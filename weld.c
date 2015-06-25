#define _BSD_SOURCE  // to get freaking usleep back
#include <inttypes.h>
#include <termios.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdio_ext.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stropts.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <math.h>

#define X 0
#define Y 1

#define ALIGN_X 15.5
#define ALIGN_Y 6.0

#define TESTMODE 0

int kbhit()
{
	static const int STDIN = 0;
/*	static bool initialized = false;

	if(!initialized)
	{
		termios term;
		tcgetattr(STDIN, &term)
	}
*/
	int bytesWaiting;
	ioctl(STDIN, FIONREAD, &bytesWaiting);
	return bytesWaiting;
}

int set_interface_attribs(int fd, int speed)
{
	struct termios tty;
	memset(&tty, 0, sizeof(tty));
	if(tcgetattr(fd, &tty) != 0)
	{
		printf("error %d from tcgetattr\n", errno);
		return -1;
	}

	cfsetospeed(&tty, speed);
	cfsetispeed(&tty, speed);

	tty.c_cflag = (tty.c_cflag & ~CSIZE) | CS7;
	tty.c_iflag &= ~IGNBRK;
	tty.c_lflag = 0;
	tty.c_oflag = 0;
	tty.c_cc[VMIN] = 255; // should_block ? 1 : 0;
	tty.c_cc[VTIME] = 1; // 0.1s read timeout

//	tty.c_iflag &= ~(IXON | IXOFF | IXANY); // no software flow control
	tty.c_iflag &= ~(IXANY); tty.c_iflag |= (IXON | IXOFF); // software flow control

	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
//	tty.c_cflag |= PARENB;
	tty.c_cflag &= ~CSTOPB;
//	tty.c_cflag |= CRTSCTS; // hardware flow control, supported randomly on linux.

	if(tcsetattr(fd, TCSANOW, &tty) != 0)
	{
		printf("error %d from tcsetattr\n", errno);
		return -1;
	}
	return 0;
}

int wr(int fd, const char* buf)
{
	int len = 0;
	while(buf[len] != 0) len++;
	if(len == 0) return 0;
	write(fd, buf, len);
}

#define MAX_X_POINTS 100
#define MAX_Y_POINTS 100
#define MAX_WELDS_PER_POINT 5

#define STATE_UNINITIALIZED 0
#define STATE_INITIALIZED 1

typedef struct
{
	int state;
	float midpoint[2];
	int num_welds;
	int extra_power;
	float weldpoints[MAX_WELDS_PER_POINT][2];
} weldpoint;

int num_points[2] = {0, 0};
weldpoint points[MAX_X_POINTS][MAX_Y_POINTS];

int parse_file(FILE* datafile)
{
	int linenum = 0;
	while(1)
	{
		char line[1000];
		fgets(line, 1000, datafile);

		if(feof(datafile))
			return 1;

		linenum++;

		const char MARKER[] = "WELDPOINT";
		char* pnt = strstr(line, MARKER);
		if(!pnt)
		{
			continue;
		}
		else
		{
//			int extra_pwr = 0;
//			if(*(pnt-1) == '+')
//			{
//				extra_pwr = 1;
//			}

			pnt += strlen(MARKER);
			int idx_x, idx_y;
			float point_x, point_y;
			int ret = sscanf(pnt, " %u ; %u ; %f ; %f", &idx_x, &idx_y, &point_x, &point_y);
			if(ret != 4)
			{
				printf("Read error on line %u: %u fields out of 4 required was read.\n-->%s\n",
					linenum, ret, pnt);
				return 0;
			}

			if(idx_x < 0 || idx_x > MAX_X_POINTS-1 || idx_y < 0 || idx_y > MAX_Y_POINTS-1)
			{
				printf("Invalid weldpoint index on line %u: (%u;%u)\n",
					linenum, idx_x, idx_y);
				return 0;
			}

			if(point_x < 0.0 || point_x > 2000.0 || point_y < 0.0 || point_y > 2000.0)
			{
				printf("Invalid weldpoint coordinates on line %u: (%f ; %f)\n",
					linenum, point_x, point_y);
			}

			if((idx_x+1) > num_points[X]) num_points[X] = (idx_x+1);
			if((idx_y+1) > num_points[Y]) num_points[Y] = (idx_y+1);

			if(TESTMODE) printf("Added point %u, %u\n", idx_x, idx_y);
			points[idx_x][idx_y].state = STATE_INITIALIZED;
			points[idx_x][idx_y].midpoint[X] = point_x;
			points[idx_x][idx_y].midpoint[Y] = point_y;
			points[idx_x][idx_y].extra_power = 0;

//			if(extra_pwr)
//			{
//				points[idx_x][idx_y].extra_power = 1;
//				if(TESTMODE)
//					printf("(With extra power)\n");
//			}

		}

	}
}

int process_file(int* n_weld_points, float* point_distance, int parallel_rows, int* powers)
{
	float point_offsets[2][5][2];

	for(int i = 0; i < 5; i++)
	{
		for(int o = 0; o < 2; o++)
		{
			switch(n_weld_points[o])
			{
			case 1:
				point_offsets[o][0][X] = 0.0;
				point_offsets[o][0][Y] = 0.0;
				break;
			case 2:
				point_offsets[o][0][X] = point_distance[o]/-2.0;
				point_offsets[o][0][Y] = 0.0;
				point_offsets[o][1][X] = point_distance[o]/2.0;
				point_offsets[o][1][Y] = 0.0;
				break;
			case 3:
				point_offsets[o][0][X] = point_distance[o]/-2.0;
				point_offsets[o][0][Y] = point_distance[o]/(-2.0*sqrt(3.0));
				point_offsets[o][1][X] = point_distance[o]/2.0;
				point_offsets[o][1][Y] = point_distance[o]/(-2.0*sqrt(3.0));
				point_offsets[o][2][X] = 0.0;
				point_offsets[o][2][Y] = point_distance[o]/(sqrt(3.0));
				break;

			case 4:
				point_offsets[o][0][X] = point_distance[o]/-2.0;
				point_offsets[o][0][Y] = point_distance[o]/-2.0;
				point_offsets[o][1][X] = point_distance[o]/2.0;
				point_offsets[o][1][Y] = point_distance[o]/-2.0;
				point_offsets[o][2][X] = point_distance[o]/2.0;
				point_offsets[o][2][Y] = point_distance[o]/2.0;
				point_offsets[o][3][X] = point_distance[o]/-2.0;
				point_offsets[o][3][Y] = point_distance[o]/2.0;
				break;
			case 5: point_offsets[o][0][X] = 0.0;
				point_offsets[o][0][Y] = 0.0;
				point_offsets[o][1][X] = point_distance[o]/-sqrt(2.0);
				point_offsets[o][1][Y] = point_distance[o]/-sqrt(2.0);
				point_offsets[o][2][X] = point_distance[o]/sqrt(2.0);
				point_offsets[o][2][Y] = point_distance[o]/-sqrt(2.0);
				point_offsets[o][3][X] = point_distance[o]/sqrt(2.0);
				point_offsets[o][3][Y] = point_distance[o]/sqrt(2.0);
				point_offsets[o][4][X] = point_distance[o]/-sqrt(2.0);
				point_offsets[o][4][Y] = point_distance[o]/sqrt(2.0);
				break;
			default:
				break;
			}
		}

	}

	int size_select = 1; // swapped to 0 on the first round before anything happens.
	for(int curx=0; curx<num_points[X]; curx++)
	{
		if((curx % parallel_rows) == 0)
		{
			size_select = size_select?0:1;
		}

		for(int cury=0; cury<num_points[Y]; cury++)
		{
			points[curx][cury].num_welds = n_weld_points[size_select];
			points[curx][cury].extra_power = powers[size_select];
			for(int dot = 0; dot < n_weld_points[size_select]; dot++)
			{
				for(int coord = 0; coord < 2; coord++)
					points[curx][cury].weldpoints[dot][coord] =
						points[curx][cury].midpoint[coord] + point_offsets[size_select][dot][coord];
			}
		}
	}
}

#define GAS_VALVE 16
#define Z_VALVE_DOWN 2
#define Z_VALVE_UP_RELEASE 1
#define WELDER_ON 4
#define EXTRA_PWR_RELAY 8

void automove_outp(int fd, int val)
{
	char buf[1000];
	sprintf(buf, "CD %u;", val);
	if(TESTMODE)
		puts(buf);
	if(TESTMODE != 1)
		wr(fd, buf);
}

void automove_init(int fd)
{
	wr(fd, ";IN;");
}

void automove_find_home(int fd)
{
	wr(fd, "FH;");
}

void automove_wait(int fd, float seconds)
{
	char buf[1000];
	sprintf(buf, "WA %.3f;", seconds);
	if(TESTMODE)
		puts(buf);
	if(TESTMODE != 1)
		wr(fd, buf);
}

//#define MM_TO_MILS 39.3700787
//#define MM_TO_MILS 50

void automove_goto(int fd, float x_mm, float y_mm)
{
	static float prev_x_mil = 0.0;
	static float prev_y_mil = 0.0;

	float x_mil = x_mm * 48.66; // MILLENNIUM-tekstin suuntainen suunta (leveys)
	float y_mil = y_mm * 49.26; // syvyyssuunta kayttajasta poispain

	int sleep_time = 400000 + 130*sqrtf(powf(fabs(prev_x_mil-x_mil),2) + powf(fabs(prev_y_mil-y_mil), 2));

	prev_x_mil = x_mil;
	prev_y_mil = y_mil;


	char buf[1000];
	sprintf(buf, "MA %u,%u;", (int)x_mil, (int)y_mil);
	if(TESTMODE)
	{
		puts(buf);
		printf("\nSleeping %d us", sleep_time); fflush(stdout);
	}
	else
		wr(fd, buf);


	usleep(sleep_time);

	if(TESTMODE)
		printf("... done\n");

}

int main(int argc, char** argv)
{
	if(argc < 4)
	{
		printf("Usage: weld <weld_data_file> <parallel_rows> <+|- (start)>[s]\n");
		printf("Data file as generated from cnc_gen: use (ALIGNPOINT <idx_x>;<idx_y>;<x>;<y>)\n");
		printf("     and (WELDPOINT <idx_x>;<idx_y>;<x>;<y>)\n");
		printf("num_weld_points can currently be 1...5\n");
		printf("+|- defines whether welding starts from + (smaller weld) or - (larger weld)\n");
		printf("s = simulate (no gas, no weld) S = simulate with midpoints\n");
		return 1;
	}

	int n_weld_points[2]    = {3, 5};
	int powers[2]           = {1, 0};
	float point_distance[2] = {3.0, 4.0};

	int parallel_rows = 0;

	int automove;

	int simu = 0;
	int midsimu = 0;

	const char handle[] = "/dev/ttyS0";

	if(!TESTMODE)
	{
		automove = open(handle, O_RDWR | O_NOCTTY | O_SYNC);
		if(automove < 0)
		{
			printf("error %d opening %s: %s\n", errno, handle, strerror(errno));
			return 1;
		}

		set_interface_attribs(automove, B9600);
	}

	FILE* datafile = fopen(argv[1], "rb");
	if(!datafile)
	{
		printf("Couldn't open %s\n", argv[1]);
		return 1;
	}

	parallel_rows = atoi(argv[2]);
	if(parallel_rows < 1 || parallel_rows > 20)
		{printf("invalid parallel_rows\n"); return 1;}

	if(argv[3][0] != '+' && argv[3][0] != '-')
		{printf("must define start argument (+ or -)\n"); return 1;}

	if(argv[3][0] == '-')
	{
		int tmp = n_weld_points[0];
		n_weld_points[0] = n_weld_points[1];
		n_weld_points[1] = tmp;

		tmp = powers[0];
		powers[0] = powers[1];
		powers[1] = tmp;

		float tmpf = point_distance[0];
		point_distance[0] = point_distance[1];
		point_distance[1] = tmpf;
	}


	if(argv[3][1] == 's' || argv[3][1] == 'S')
		{printf("Simulation mode\n"); simu = 1;}

	if(argv[3][1] == 'S')
		midsimu = 1;


	if(!parse_file(datafile))
	{
		printf("Fatal error parsing datafile. Stop.\n");
		return 0;
	}

	if(!process_file(n_weld_points, point_distance, parallel_rows, powers))
	{
		printf("Fatal error processing weldpoints. Stop.\n");
		return 0;
	}

	// Start welding.

	int max_dots = n_weld_points[0];
	if(n_weld_points[1] > max_dots)
		max_dots = n_weld_points[1];

	automove_init(automove);

	automove_outp(automove, 0);
	automove_wait(automove, 0.5);
	automove_find_home(automove);
	sleep(5);

	if(!simu)
	{
		automove_outp(automove, GAS_VALVE);
		automove_wait(automove, 1.0);
		automove_outp(automove, 0);
		sleep(2);
	}

	for(int dot = 0; dot < max_dots; dot++)
	{
		for(int curx = 0; curx < num_points[X]; curx++)
		{
			for(int cury = 0; cury < num_points[Y]; cury++)
			{
				int cmd;
				if(points[curx][cury].state != STATE_INITIALIZED)
					continue;
				if(dot >= points[curx][cury].num_welds)
					continue;

				printf("x=%2u/%2u  y=%2u/%2u  dot=%u/%u   %c   \r", curx+1, num_points[X], cury+1, num_points[Y], dot+1,
					points[curx][cury].num_welds, (points[curx][cury].extra_power)?'P':' ');
				fflush(stdout);

				if(midsimu)
					automove_goto(automove, ALIGN_X+points[curx][cury].midpoint[X],
					      ALIGN_Y+points[curx][cury].midpoint[Y]);
				else
					automove_goto(automove, ALIGN_X+points[curx][cury].weldpoints[dot][X],
					      ALIGN_Y+points[curx][cury].weldpoints[dot][Y]);

				cmd = Z_VALVE_UP_RELEASE;
				if(!simu) cmd |= GAS_VALVE;
				automove_outp(automove, cmd); // let it drop with gravity, put gas on.
				automove_wait(automove, 0.4);

				cmd = Z_VALVE_UP_RELEASE | Z_VALVE_DOWN; // apply force, keep gas on.
				if(!simu) cmd |= GAS_VALVE;
				if(points[curx][cury].extra_power) cmd |= EXTRA_PWR_RELAY;
				automove_outp(automove, cmd);
				automove_wait(automove, 0.3);

				cmd = Z_VALVE_UP_RELEASE | Z_VALVE_DOWN; // Weld while applying force
				if(!simu) cmd |= GAS_VALVE | WELDER_ON;
				if(points[curx][cury].extra_power) cmd |= EXTRA_PWR_RELAY;
				automove_outp(automove, cmd);
				automove_wait(automove, 0.5); // weld stops once energy level is reached, but this is a safety timeout.

				cmd = Z_VALVE_UP_RELEASE | Z_VALVE_DOWN;
				if(!simu) cmd |= GAS_VALVE;
				automove_outp(automove, cmd); // keep pressure and gas
				automove_wait(automove, 0.3);

				cmd = 0;
				if(!simu) cmd |= GAS_VALVE;
				automove_outp(automove, cmd); // keep gas for a little bit to purge smoke and cool the electrode
				automove_wait(automove, 0.3);

				automove_outp(automove, 0);  // all off, welder up.
				sleep(3);
			}
		}
	}

	printf("\n");

	return 1;
}

