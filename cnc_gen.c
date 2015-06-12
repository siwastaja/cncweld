#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define X 0
#define Y 1

#define ZMODE 0
#define SIZETEST 0
#define NUM_SIZETESTS 9

#define CUT()   {if(ZMODE) fprintf(gfile, "G1 Z-5.0\n"); else fprintf(gfile, "M03 S%02d\n", (int)((float)power+extrapower)); extrapower += power_increase_per_cut;}
#define CUT_MARK() {if(ZMODE) fprintf(gfile, "G1 Z-5.0\n"); else fprintf(gfile, "M03 S%02d\n", markpower);}
#define UNCUT() {if(ZMODE) fprintf(gfile, "G1 Z5.0\n");  else fprintf(gfile, "M05\n");}

#define COVER_CUT()   {if(ZMODE) fprintf(coverfile, "G1 Z-5.0\n"); else fprintf(coverfile, "M03 S%02d\n", cover_power);}
#define COVER_UNCUT() {if(ZMODE) fprintf(coverfile, "G1 Z5.0\n");  else fprintf(coverfile, "M05\n");}

#define delay(ff, dd)	{fprintf((ff), "G04 P%.3f\n", (dd));}

// Generates main cell board, one side and one front.
// Run the laser twice to obtain all 6 parts.

// Saves weld data to gcode file as comments.
// (WELDPOINT x_idx;y_idx;x_coord;y_coord) defines a center of the weld(s)
// (ALIGNPOINT x_idx;y_idx;x_coord;y_coord) defines the coordinates of the corners
// of the finished box. idx 0;0 is the bottom-left corner.
// The box can be aligned against physical restrainers on two edges (typically bottom-left)

int main(int argc, char** argv)
{

	float part_separation = 2.0; // clearance between main board, side and front
	float thickness = 3.0; // Thickness of the material.
	float cover_thickness = 3.1; // including busbar space.

	float cell_length = 65.0;
	float hole = 18.43;
	float sizetests[NUM_SIZETESTS] = {-0.20, -0.15, -0.1, -0.05, 0, 0.05, 0.1, 0.15, 0.20};
//	float sizetests[NUM_SIZETESTS] = {-4.0, -3.0, -2.0, -1.0, 0, 1.0, 2.0, 3.0, 4.0};
	float wallgaps[4] = {4.0, 20.5-18.43, 4.0, 3.15};
	float cellgap = 20.5-hole;

	int ys[2];
	int x;
	float bonushole = 2.5;
	float bonushole_dist = 1.3; // gap to cell hole

	float end_bonusholes = 3.5;
	float end_bonusholes_dist = 0.0;

	float spacing_trim = 1.01; // Adds extra gap in x direction.

	float feedrate = 640.0;
	int power = 70; // 70 -> 20 mA
	float extrapower = 0.0;
	float power_increase_per_cut = 0.05;
	int markpower = 1;
	float lasertrim = 0.12; // How much excess does the laser burn.

	// focus = 7mm
	float cover_feedrate = 850.0;
	int cover_power = 70;
	float cover_lasertrim = 0.09;

	int do_sides = 1; // adds a side. Sides are solid, with small optional holes
			  // for wires / mounting.
	float finger_size_x = 8.0;
	float finger_size_y = 10.0;

	int do_bms_wire_holes = 1;
	int bms_wire_hole_every = 4;
	int bms_wire_hole_offset = 3;

	// Side bonusholes: small holes are inserted at every other cell location;
	// two holes, one near top, one near bottom. Can be used for wires or
	// mounting.
	// 0 = don't do, 1 = do, 2 = use marking power only
	int do_side_bonusholes = 2;
	float side_bonushole_size = 2.0; // 3.20 tapped to M4 for mounting.
	// Distance (of hole center) from the cell
	// main board. Absolute minimum is side_bonushole_size/2.
	float side_bonushole_dist = side_bonushole_size/2.0 + 15.0;

	int do_fronts = 1; // adds front and back. Front and back 
			   // look like ####, to allow air to pass with maximum
			   // structural integrity.

	// How many fingers join the sides with fronts.
	int num_side_front_fingers = 4;

	// Solid parts between the finger part and ventilation holes.
	float front_y_frame_width = 5.0;

	// Width of the structural solid parts between the ventilation holes.
	float front_mid_width = 4.0;
	// A hole is generated at every cell location y[0] (the front cells
	// from the ventilation hole viewpoint). num_front_holes_y specifies
	// how many holes are generated per cell in cell height direction.
	// Larger number adds more structural integrity.
	int num_front_holes_y = 3;

	int side_at_back = 0;

	int do_covers = 1;

	double delay_per_cell = 6.0; // seconds of delay per round hole, or one cell length worth of border.
	double cover_delay_per_cell = 4.0; // seconds of delay per round hole, or one cell length worth of border.

/*
	FRONT & BACK
                        v--- finger_size_x
           ###   ###   ###   ###   ###   ###   ###   ###   ###
	#########################################################
      #######################frame_width###########################
      ####    #     #     #    #                               ####
        ##    #     #     #    #                    ^          ##
        ##    #     #     #    #                    |num_      ##
      #########front_mid_width############# . . .   |front_    #### <-- finger_size_y
      ####    #     #     #    #    . . .           |holes_y   ####
       ###    #     #     #    #                    v          ##
        ##    #     #     #         hole at every cell (y[0])  ##
      ####    #     #            ...  <----------->            ####
      #############################################################
        #########################################################
           ###   ###   ###   ###   ###   ###   ###   ###   ###

*/


	if(argc < 5)
	{
		printf("Usage: cnc_gen <outfile_prefix> <y1> <y2> <x>\n");
		printf("Ex.: cnc_gen out 4 3 11\n");
		printf("__-_-_-_-_-_4_-_-_-_-_-__\n");
		printf("| O   O   O   O   O   O |\n");
		printf("|  .O  .O  .O  .O  .O   |\n");
		printf("| O   O   O   O   O   O |\n");
		printf("1  .O  .O  .O  .O  .O   3\n");
		printf("| O   O   O   O   O   O |\n");
		printf("|  .O  .O  .O  .O  .O   |\n");
		printf("| O   O   O   O   O   O |\n");
		printf("------------2------------\n");
		return 1;
	}

	ys[0] = atoi(argv[2]);
	if(ys[0] < 1 || ys[0] > 100) { printf("Invalid y1\n"); return 1;}

	ys[1] = atoi(argv[3]);
	if(ys[1] < 1 || ys[1] > 100) { printf("Invalid y2\n"); return 1;}
	if(ys[1] < ys[0]-1 || ys[1] > ys[0]) { printf("y2 must be y1-1 or y1\n"); return 1;}

	x = atoi(argv[4]);
	if(x < 1 || x > 100) { printf("Invalid x\n"); return 1;}

	char mainfilename[1000];
	char coverfilename[1000];
	sprintf(mainfilename, "%s_main.ngc", argv[1]);
	sprintf(coverfilename, "%s_cover.ngc", argv[1]);

	FILE* gfile = fopen(mainfilename, "wb");
	if(!gfile)
	{
		printf("Error opening file %s\n", mainfilename);
		return 1;
	}

	FILE* coverfile;

	if(do_covers)
	{
		coverfile = fopen(coverfilename, "wb");
		if(!coverfile)
		{
			printf("Error opening file %s\n", coverfilename);
			return 1;
		}
	}

	float y_step = hole + cellgap;
	float x_step = sqrt(  (3.0*(hole/2.0)*(hole/2.0)) + (2.0*(hole/2.0)*cellgap)  ) * 1.05 * spacing_trim; // todo: fix math...

	printf("y_step = %f, x_step = %f\n", y_step, x_step);

	fprintf(gfile, "(");
	for(int i = 0; i < 4; i++)
	{
		fprintf(gfile, " %s ", argv[i]);
	}
	fprintf(gfile, ")\n");
	fprintf(gfile, "(%f; %f; %f; %f; %f; %f; %f; %f;)\n(%f; %f; %f; %f; %f; %f; %f; %d;)\n(%f; %f; %d; %f; %d; %f; %d; %f)\n",
		part_separation, thickness, cell_length, hole, wallgaps[0], wallgaps[1], wallgaps[2], wallgaps[3],
		cellgap, bonushole, lasertrim, finger_size_x, finger_size_y, side_bonushole_size, side_bonushole_dist, num_side_front_fingers,
		front_y_frame_width, front_mid_width, num_front_holes_y, feedrate, power, cover_feedrate, cover_power, spacing_trim);

	fprintf(gfile, "G21\n");
	fprintf(gfile, "G61\n");
	UNCUT();
	fprintf(gfile, "G00 F%.2f\n", feedrate);
	fprintf(gfile, "M07 (air on)\n");
	delay(gfile, 5.0);

	if(do_covers)
	{
		fprintf(coverfile, "G21\n");
		fprintf(coverfile, "G61\n");
		COVER_UNCUT();
		fprintf(coverfile, "G00 F%.2f\n", cover_feedrate);
		fprintf(coverfile, "M07 (air on)\n");
		delay(coverfile, 5.0);

	}

	float front_origin_x = 10.0;
	float front_origin_y = 10.0;
	float side_origin_x = 10.0;
	float side_origin_y = 10.0;

	float cover_origin_x = 10.0;
	float cover_origin_y = 10.0;

	float origin_x = 10.0+thickness;
	float origin_y = 10.0+thickness;

	if(do_fronts)
	{
		origin_x += cell_length + part_separation;
		side_origin_x = origin_x;
	}

	if(do_sides)
	{
		if(side_at_back)
		{
			side_origin_x = origin_x + wallgaps[0] + x_step*(x-1) + hole + wallgaps[2] + part_separation
				+ thickness*2.0;
		}
		else
		{
			origin_y += cell_length + part_separation + (do_covers?(cover_thickness*2.0):0.0);
			front_origin_y = origin_y;
		}
	}

	int eka = 1;
	int cur_sizetest = 0;
	for(int curx = 0; curx < x; curx++)
	{
		int y = ys[curx%2];
		float y_offset = (curx%2)?(y_step/2.0):(0.0);
		for(int cury = 0; cury < y; cury++)
		{
			float start_x = origin_x + wallgaps[0] + x_step*curx;
			float mid_x = start_x + hole/2.0;
			float start_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 + y_offset;
			float mid_y = start_y;

			float start_x_trimmed = start_x + lasertrim;
			float start_y_trimmed = start_y;

			float offset_i = hole/2.0 - lasertrim;
			float offset_j = 0.0;

			if(SIZETEST)
			{
				start_x_trimmed -= sizetests[cur_sizetest%NUM_SIZETESTS]/2.0;
				offset_i += sizetests[cur_sizetest%NUM_SIZETESTS]/2.0;
				cur_sizetest++;
			}

			// Do end bms bonusholes:
			if(((curx == 0) || (curx == x-1)) && (cury == y-1) && end_bonusholes > 0.01)
			{
				float bonushole_midx = mid_x;
				if(curx == 0) bonushole_midx -= hole/2.0; else bonushole_midx += hole/2.0;

				float bonushole_midy = origin_y + wallgaps[1] + y_step*(ys[0]-1) + hole/2.0;
				float shift = hole/2.0 + end_bonusholes/2.0 + end_bonusholes_dist;
				bonushole_midy += shift;
				float bonushole_startx = bonushole_midx - end_bonusholes/2.0;
				float bonushole_startx_trimmed = bonushole_startx + lasertrim;
				float bonushole_starty_trimmed = bonushole_midy;

				float bonushole_offset_i = end_bonusholes/2.0 - lasertrim;
				float bonushole_offset_j = 0.0;

				fprintf(gfile, "G00 X%.2f Y%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed);
				CUT();
				fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed,
					bonushole_offset_i, bonushole_offset_j);

				UNCUT();
				delay(gfile, delay_per_cell*0.25);


			}

			// Do cell bms bonusholes:
			if((curx%2 == 0) && ((cury == y-1)) && bonushole > 0.01)
			{
				float bonushole_midx = mid_x;
//					(
//					(2.0*(origin_x + wallgaps[0] + x_step*(curx-1) + hole/2.0))+
//					(1.0*(origin_x + wallgaps[0] + x_step*curx     + hole/2.0))
//					)/3.0;
				float bonushole_midy = mid_y;
				float shift = hole/2.0 + bonushole/2.0 + bonushole_dist;
				if(cury == 0) bonushole_midy -= shift; else bonushole_midy += shift;
				float bonushole_startx = bonushole_midx - bonushole/2.0;
				float bonushole_startx_trimmed = bonushole_startx + lasertrim;
				float bonushole_starty_trimmed = bonushole_midy;

				float bonushole_offset_i = bonushole/2.0 - lasertrim;
				float bonushole_offset_j = 0.0;

				fprintf(gfile, "G00 X%.2f Y%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed);
				CUT();
				fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed,
					bonushole_offset_i, bonushole_offset_j);

				UNCUT();
				delay(gfile, delay_per_cell*0.25);
			}

			fprintf(gfile, "G00 X%.2f Y%.2f (WELDPOINT %u;%u;%.2f;%.2f)\n", start_x_trimmed,
				start_y_trimmed, curx, cury, mid_x-origin_x, mid_y-origin_y);
			CUT();
			fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f", start_x_trimmed, start_y_trimmed,
				offset_i, offset_j);

			if(eka)
				fprintf(gfile, " F%.2f", feedrate);
			eka = 0;
			fprintf(gfile, "\n");

			UNCUT();
			delay(gfile, delay_per_cell);

		}
	}

	float outline[4][2];
	float side_outline[2][2];
	float cover_outline[4][2];
	outline[0][X] = origin_x;
	outline[0][Y] = origin_y;
	outline[1][X] = origin_x + wallgaps[0] + x_step*(x-1) + hole + wallgaps[2];
	outline[1][Y] = outline[0][Y];
	outline[2][X] = outline[1][X];
	outline[2][Y] = origin_y + wallgaps[1] + y_step*(ys[0]-1) + hole + wallgaps[3];
	if(ys[1] == ys[0])
		outline[2][Y] += y_step/2.0;
	outline[3][X] = origin_x;
	outline[3][Y] = outline[2][Y];

	side_outline[0][X] = side_origin_x;
	side_outline[0][Y] = side_origin_y;
	side_outline[1][X] = side_origin_x + wallgaps[0] + x_step*(x-1) + hole + wallgaps[2];
	side_outline[1][Y] = side_outline[0][Y];

	cover_outline[0][X] = cover_origin_x;
	cover_outline[0][Y] = cover_origin_y;
	cover_outline[1][X] = cover_origin_x + wallgaps[0] + x_step*(x-1) + hole + wallgaps[2];
	cover_outline[1][Y] = cover_outline[0][Y];
	cover_outline[2][X] = cover_outline[1][X];
	cover_outline[2][Y] = cover_origin_y + wallgaps[1] + y_step*(ys[0]-1) + hole + wallgaps[3];
	if(ys[1] == ys[0])
		cover_outline[2][Y] += y_step/2.0;
	cover_outline[3][X] = cover_outline[0][X];
	cover_outline[3][Y] = cover_outline[2][Y];

	fprintf(gfile, "G00 X%.2f Y%.2f (ALIGNPOINT 0;0;%.2f;%.2f)\n", outline[0][X]-lasertrim, outline[0][Y]-lasertrim,
		outline[0][X]-(do_fronts?thickness:0.0), outline[0][Y]-(do_sides?thickness:0.0));
	CUT();

	if(do_covers)
	{
		fprintf(coverfile, "G00 X%.2f Y%.2f\n", cover_outline[0][X]-cover_lasertrim, cover_outline[0][Y]-cover_lasertrim);
		COVER_CUT();
	}

	// horizontal bottom side
	if(do_sides)
	{
		// Finger cut
		for(int curx = 0; curx < x; curx++)
		{
			float finger_start_x = origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_end_x = finger_start_x + finger_size_x;

			float cfinger_start_x = cover_origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float cfinger_end_x = cfinger_start_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[0][Y]-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[0][Y]
				-thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[0][Y]
				-thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[0][Y]-lasertrim);

			if(do_covers)
			{
				fprintf(coverfile, "G01 X%.2f Y%.2f F%.2f\n", cfinger_start_x-cover_lasertrim, cover_outline[0][Y]-cover_lasertrim, cover_feedrate);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_start_x-cover_lasertrim, cover_outline[0][Y]
					-thickness-cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_end_x+cover_lasertrim, cover_outline[0][Y]
					-thickness-cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_end_x+cover_lasertrim, cover_outline[0][Y]-cover_lasertrim);
			}

		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 1;0;%.2f;%.2f)\n", outline[1][X]+lasertrim, outline[1][Y]-lasertrim,
		outline[1][X]+(do_fronts?thickness:0.0), outline[1][Y]-(do_sides?thickness:0.0));

	if(do_covers)
	{
		fprintf(coverfile, "G01 X%.2f Y%.2f F%.2f\n", cover_outline[1][X]+cover_lasertrim, cover_outline[1][Y]-cover_lasertrim, cover_feedrate);

	}

	UNCUT();
	delay(gfile, delay_per_cell*x);
	CUT();
	if(do_covers)
	{
		COVER_UNCUT();
		delay(coverfile, cover_delay_per_cell*x);
		COVER_CUT();
	}


	// vertical right side
	if(do_fronts)
	{
		// Finger cut
		for(int cury = 0; cury < ys[0]; cury++)
		{
			float finger_start_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_end_y = finger_start_y + finger_size_y;
			float cfinger_start_y = cover_origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float cfinger_end_y = cfinger_start_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_end_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_end_y+lasertrim);

			if(do_covers)
			{
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[1][X]+cover_lasertrim, cfinger_start_y-cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[1][X]+thickness+cover_lasertrim, cfinger_start_y-cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[1][X]+thickness+cover_lasertrim, cfinger_end_y+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[1][X]+cover_lasertrim, cfinger_end_y+cover_lasertrim);

			}
		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 1;1;%.2f;%.2f)\n", outline[2][X]+lasertrim, outline[2][Y]+lasertrim,
		outline[2][X]+(do_fronts?thickness:0.0), outline[2][Y]+(do_sides?thickness:0.0));

	if(do_covers)
	{
		fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[2][X]+cover_lasertrim, cover_outline[2][Y]+cover_lasertrim);
	}

	UNCUT();
	delay(gfile, delay_per_cell*ys[0]);
	CUT();
	if(do_covers)
	{
		COVER_UNCUT();
		delay(coverfile, cover_delay_per_cell*ys[0]);
		COVER_CUT();
	}



	// horizontal top side

	if(do_sides)
	{
		// Finger cut
		for(int curx = x-1; curx >= 0; curx--)
		{
			float finger_end_x = origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_start_x = finger_end_x + finger_size_x;
			float cfinger_end_x = cover_origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float cfinger_start_x = cfinger_end_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[2][Y]+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[2][Y]+thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[2][Y]+thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[2][Y]+lasertrim);

			if(do_covers)
			{
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_start_x+cover_lasertrim, cover_outline[2][Y]+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_start_x+cover_lasertrim, cover_outline[2][Y]+thickness+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_end_x-cover_lasertrim, cover_outline[2][Y]+thickness+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cfinger_end_x-cover_lasertrim, cover_outline[2][Y]+cover_lasertrim);
			}
		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 0;1;%.2f;%.2f)\n", outline[3][X]-lasertrim, outline[3][Y]+lasertrim,
		outline[3][X]-(do_fronts?thickness:0.0), outline[3][Y]+(do_sides?thickness:0.0));

	if(do_covers)
	{
		fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[3][X]-cover_lasertrim, cover_outline[3][Y]+cover_lasertrim);
	}

	UNCUT();
	delay(gfile, delay_per_cell*x);
	CUT();
	if(do_covers)
	{
		COVER_UNCUT();
		delay(coverfile, cover_delay_per_cell*x);
		COVER_CUT();
	}

	// Vertical left side

	if(do_fronts)
	{
		// Finger cut
		for(int cury = ys[0]-1; cury >=0; cury--)
		{
			float finger_end_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_start_y = finger_end_y + finger_size_y;
			float cfinger_end_y = cover_origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float cfinger_start_y = cfinger_end_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-thickness-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-thickness-lasertrim, finger_end_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-lasertrim, finger_end_y-lasertrim);

			if(do_covers)
			{
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[3][X]-cover_lasertrim, cfinger_start_y+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[3][X]-thickness-cover_lasertrim, cfinger_start_y+cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[3][X]-thickness-cover_lasertrim, cfinger_end_y-cover_lasertrim);
				fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[3][X]-cover_lasertrim, cfinger_end_y-cover_lasertrim);
			}
		}

	}


	fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, outline[0][Y]-lasertrim);

	if(do_covers)
	{
		fprintf(coverfile, "G01 X%.2f Y%.2f\n", cover_outline[0][X]-cover_lasertrim, cover_outline[0][Y]-cover_lasertrim);

	}

	UNCUT();
	delay(gfile, delay_per_cell*ys[0]);
	if(do_covers)
	{
		COVER_UNCUT();
		delay(coverfile, cover_delay_per_cell*ys[0]);
	}



	printf("Main panel size without fingers: %.2f x %.2f\n", outline[1][X]-outline[0][X], outline[2][Y]-outline[1][Y]);
	printf("Total box size: %.2f x %.2f x %.2f\n", outline[1][X]-outline[0][X]+2.0*thickness, outline[2][Y]-outline[1][Y]+2.0*thickness, cell_length+2.0*cover_thickness);
	printf("Sheet needed: %.2f x %.2f\n", outline[1][X]-outline[0][X]+2.0*thickness+cell_length+part_separation, outline[2][Y]-outline[1][Y]+2.0*thickness+cell_length+2.0*cover_thickness+part_separation);
	if(do_covers)
		printf("Cover size with fingers (sheet needed): %.2f x %.2f\n", cover_outline[1][X]-cover_outline[0][X]+2.0*thickness, cover_outline[2][Y]-cover_outline[1][Y]+2.0*thickness);

/*

|||| thickness
c ||||  < 0.5 steps
e ||    ^ 1
l ||||  v step
l ||    ^ 1
l ||||  v step
e ||    ^ 1
n ||||  v step
|||| thickness

*/
	float side_front_finger_step = (cell_length - 2.0*thickness) / ((float)num_side_front_fingers-0.5);

	// Do side panel
	if(do_sides)
	{
		fprintf(gfile, "G00 X%.2f Y%.2f\n", side_outline[0][X]-lasertrim, side_origin_y-(do_covers?cover_thickness:0.0)-lasertrim);
		CUT();
		// Bottom horizontal
		for(int curx = 0; curx < x; curx++)
		{
			float finger_start_x = side_origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_end_x = finger_start_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, side_origin_y-(do_covers?cover_thickness:0.0)-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, side_origin_y+thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, side_origin_y+thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, side_origin_y-(do_covers?cover_thickness:0.0)-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+lasertrim, side_origin_y-(do_covers?cover_thickness:0.0)-lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*x);
		CUT();

		// Right vertical
		for(int cury = 0; cury < num_side_front_fingers; cury++)
		{
			float finger_start_y = side_origin_y + thickness +
				side_front_finger_step*cury;
			float finger_end_y = finger_start_y + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+thickness+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+thickness+lasertrim, finger_end_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+lasertrim, finger_end_y+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[1][X]+lasertrim, side_origin_y+cell_length+(do_covers?cover_thickness:0.0)+lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*3);
		CUT();

		// Top horizontal
		for(int curx = x-1; curx >= 0; curx--)
		{
			float finger_end_x = side_origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_start_x = finger_end_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, side_origin_y+cell_length+(do_covers?cover_thickness:0.0)+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, side_origin_y+cell_length-thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, side_origin_y+cell_length-thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, side_origin_y+cell_length+(do_covers?cover_thickness:0.0)+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-lasertrim, side_origin_y+cell_length+(do_covers?cover_thickness:0.0)+lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*x);
		CUT();

		// Left vertical

		for(int cury = num_side_front_fingers-1; cury >= 0; cury--)
		{
			float finger_end_y = side_origin_y + thickness +
				side_front_finger_step*cury;
			float finger_start_y = finger_end_y + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-thickness-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-thickness-lasertrim, finger_end_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-lasertrim, finger_end_y-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", side_outline[0][X]-lasertrim, side_origin_y-(do_covers?cover_thickness:0.0)-lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*3);

		if(do_side_bonusholes)
		{
			for(int curx = 0; curx < x; curx++)
			{
				int y = ys[curx%2];
				float y_offset = (curx%2)?(y_step/2.0):(0.0);
				for(int cury = 0; cury < 2; cury++)
				{
					if(curx%2)
					{
						float bonushole_midx = side_origin_x + wallgaps[0] + x_step*curx + hole/2.0;
						float bonushole_midy = cury?
							(side_origin_y+cell_length-thickness-side_bonushole_dist):
							(side_origin_y+thickness+side_bonushole_dist);
						float bonushole_startx = bonushole_midx - side_bonushole_size/2.0;
						float bonushole_startx_trimmed = bonushole_startx + lasertrim;
						float bonushole_starty_trimmed = bonushole_midy;

						float bonushole_offset_i = side_bonushole_size/2.0 - lasertrim;
						float bonushole_offset_j = 0.0;

						fprintf(gfile, "G00 X%.2f Y%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed);
						if(do_side_bonusholes == 2)
						{
							CUT_MARK();
						}
						else
						{
							CUT();
						}
						fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed,
							bonushole_offset_i, bonushole_offset_j);
						UNCUT();
						if(do_side_bonusholes != 2)
						{
							delay(gfile, delay_per_cell*0.25);
						}
					}
				}
			}
		} // end do_side_bonusholes

	} // end do side panel

	// Do front panel
	if(do_fronts)
	{
		// Left vertical (joins bottom main cell board)
		fprintf(gfile, "G00 X%.2f Y%.2f\n", front_origin_x-lasertrim, outline[3][Y]+thickness+lasertrim);
		CUT();
		for(int cury = ys[0]-1; cury >=0; cury--)
		{
			float finger_end_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_start_y = finger_end_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x-lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+thickness-lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+thickness-lasertrim, finger_end_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x-lasertrim, finger_end_y+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x-lasertrim, outline[0][Y]-thickness-lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*ys[0]);
		CUT();

		// Bottom horizontal (joins to a side board)

		for(int curx = 0; curx < num_side_front_fingers; curx++)
		{
			float finger_start_x = front_origin_x + thickness +
				side_front_finger_step*curx;
			float finger_end_x = finger_start_x + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[0][Y]-thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[0][Y]-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[0][Y]-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[0][Y]-thickness-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length+lasertrim, outline[0][Y]-thickness-lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*3);
		CUT();

		// Right vertical (joins to top main cell board)

		for(int cury = 0; cury < ys[0]; cury++)
		{
			float finger_start_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_end_y = finger_start_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length+lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length-thickness+lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length-thickness+lasertrim, finger_end_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length+lasertrim, finger_end_y-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x+cell_length+lasertrim, outline[2][Y]+thickness+lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*ys[0]);
		CUT();

		// Top horizontal (joins to another side board)

		for(int curx = num_side_front_fingers-1; curx >= 0; curx--)
		{
			float finger_end_x = front_origin_x + thickness +
				side_front_finger_step*curx;
			float finger_start_x = finger_end_x + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[2][Y]+thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[2][Y]+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[2][Y]+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[2][Y]+thickness+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", front_origin_x-lasertrim, outline[3][Y]+thickness+lasertrim);

		UNCUT();
		delay(gfile, delay_per_cell*3);

		// Cut ventilation holes

		float fhole_width = (hole+cellgap) - front_mid_width;
		float fhole_hstep = (cell_length - 2.0*thickness - 2.0*front_y_frame_width)/((float)num_front_holes_y);
		float fhole_height = fhole_hstep-front_mid_width;

		for(int cury = 0; cury < ys[0]; cury++)
		{
			float fhole_start_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - fhole_width/2.0;
			if(ys[0] == ys[1])
				fhole_start_y += (hole+cellgap)/4.0;

			float fhole_end_y = fhole_start_y + fhole_width;

			for(int curx = 0; curx < num_front_holes_y; curx++)
			{
				float fhole_start_x = front_origin_x + thickness + front_y_frame_width + fhole_hstep*curx
						+ front_mid_width/2.0;
				float fhole_end_x = fhole_start_x + fhole_height;

				fprintf(gfile, "G00 X%.2f Y%.2f\n", fhole_start_x+lasertrim, fhole_start_y+lasertrim);
				CUT();
				fprintf(gfile, "G01 X%.2f Y%.2f\n", fhole_end_x-lasertrim, fhole_start_y+lasertrim);
				fprintf(gfile, "G01 X%.2f Y%.2f\n", fhole_end_x-lasertrim, fhole_end_y-lasertrim);
				fprintf(gfile, "G01 X%.2f Y%.2f\n", fhole_start_x+lasertrim, fhole_end_y-lasertrim);
				fprintf(gfile, "G01 X%.2f Y%.2f\n", fhole_start_x+lasertrim, fhole_start_y+lasertrim);
				UNCUT();
				delay(gfile, delay_per_cell);

			}
		}

	}

	fprintf(gfile, "G00 X%.2f Y%.2f\n", origin_x, origin_y);
	delay(gfile, 15.0);
	fprintf(gfile, "M2\n%%\n");
	fclose(gfile);

	printf("Power rise during cutting: %d -> %d\n", (int)((float)power), (int)((float)power+extrapower));

	if((int)((float)power+extrapower) > 99)
		printf("Warning: Power overflows!\n");

	if(do_covers)
	{
		fprintf(coverfile, "G00 X%.2f Y%.2f\n", cover_origin_x, cover_origin_y);
		delay(coverfile, 15.0);
		fprintf(coverfile, "M2\n%%\n");
		fclose(coverfile);
	}

	return 0;
}
