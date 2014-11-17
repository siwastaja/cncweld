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


#define TESTMODE 1

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

	tty.c_iflag &= ~(IXON | IXOFF | IXANY);
	tty.c_cflag |= (CLOCAL | CREAD);
	tty.c_cflag &= ~(PARENB | PARODD);
	tty.c_cflag |= PARENB;
	tty.c_cflag &= ~CSTOPB;
//	tty.c_cflag |= CCTS_OFLOW; //CRTSCTS;

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

		const char MARKER[] = "(WELDPOINT";
		char* pnt = strstr(line, MARKER);
		if(!pnt)
		{
			continue;
		}
		else
		{
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
		}

	}
}

int process_file(int* n_weld_points, float* point_distance, int parallel_rows)
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
#define Z_VALVE_DOWN 1
#define Z_VALVE_UP 2
#define WELDER_ON 4

void automove_outp(int fd, int val)
{
	char buf[1000];
	sprintf(buf, "CD %u;", val);
	if(TESTMODE)
		puts(buf);
	else
		wr(fd, buf);
}

void automove_wait(int fd, float seconds)
{
	char buf[1000];
	sprintf(buf, "WA %u;", (int)(seconds*1000.0));
	if(TESTMODE)
		puts(buf);
	else
		wr(fd, buf);
}

#define MM_TO_MILS 39.3700787

void automove_goto(int fd, float x_mm, float y_mm)
{
	float x_mil = x_mm * MM_TO_MILS;
	float y_mil = y_mm * MM_TO_MILS;

	char buf[1000];
	sprintf(buf, "MA %u,%u;", (int)x_mil, (int)y_mil);
	if(TESTMODE)
		puts(buf);
	else
		wr(fd, buf);
}

int main(int argc, char** argv)
{
	if(argc < 5)
	{
		printf("Usage: weld <automove_handle> <weld_data_file> <parallel_rows> <+|- (start)>\n");
		printf("Data file as generated from cnc_gen: use (ALIGNPOINT <idx_x>;<idx_y>;<x>;<y>)\n");
		printf("     and (WELDPOINT <idx_x>;<idx_y>;<x>;<y>)\n");
		printf("num_weld_points can currently be 1...5\n");
		printf("+|- defines whether welding starts from + (smaller weld) or - (larger weld)\n");
		return 1;
	}

	int n_weld_points[2]    = {3, 5};
	float point_distance[2] = {4.0, 5.0};

	int parallel_rows = 0;

	int automove;

	if(!TESTMODE)
	{
		automove = open(argv[1], O_RDWR | O_NOCTTY | O_SYNC);
		if(automove < 0)
		{
			printf("error %d opening %s: %s\n", errno, argv[1], strerror(errno));
			return 1;
		}

		set_interface_attribs(automove, B9600);
	}

	FILE* datafile = fopen(argv[2], "rb");
	if(!datafile)
	{
		printf("Couldn't open %s\n", argv[2]);
		return 1;
	}

	parallel_rows = atoi(argv[3]);
	if(parallel_rows < 1 || parallel_rows > 20)
		{printf("invalid parallel_rows\n"); return 1;}

	if(argv[4][0] != '+' && argv[4][0] != '-')
		{printf("must define start argument (+ or -)\n"); return 1;}

	if(argv[4][0] == '-')
	{
		int tmp = n_weld_points[0];
		n_weld_points[0] = n_weld_points[1];
		n_weld_points[1] = tmp;
		float tmpf = point_distance[0];
		point_distance[0] = point_distance[1];
		point_distance[1] = tmpf;
	}

	if(!parse_file(datafile))
	{
		printf("Fatal error parsing datafile. Stop.\n");
		return 0;
	}

	if(!process_file(n_weld_points, point_distance, parallel_rows))
	{
		printf("Fatal error processing weldpoints. Stop.\n");
		return 0;
	}

	// Start welding.

	int max_dots = n_weld_points[0];
	if(n_weld_points[1] > max_dots)
		max_dots = n_weld_points[1];

	automove_outp(automove, GAS_VALVE);
	automove_wait(automove, 1.0);
	automove_outp(automove, 0);
	sleep(2);

	for(int dot = 0; dot < max_dots; dot++)
	{
		for(int curx = 0; curx < num_points[X]; curx++)
		{
			for(int cury = 0; cury < num_points[Y]; cury++)
			{

				if(points[curx][cury].state != STATE_INITIALIZED)
					continue;
				if(dot >= points[curx][cury].num_welds)
					continue;


				automove_goto(automove, points[curx][cury].weldpoints[dot][X],
					      points[curx][cury].weldpoints[dot][Y]);
				sleep((cury==0)?2:1);
				automove_outp(automove, GAS_VALVE);
				automove_wait(automove, 0.5);
				automove_outp(automove, GAS_VALVE | Z_VALVE_DOWN);
				automove_wait(automove, 1.0);
				automove_outp(automove, GAS_VALVE | Z_VALVE_DOWN | WELDER_ON);
				automove_wait(automove, 1.0);
				automove_outp(automove, GAS_VALVE | Z_VALVE_DOWN);
				automove_wait(automove, 0.5);
				automove_outp(automove, GAS_VALVE);
				automove_wait(automove, 0.5);
				automove_outp(automove, Z_VALVE_UP);
				automove_wait(automove, 0.5);
				automove_outp(automove, 0);
				sleep(5);
			}
		}
	}

	return 1;
}

