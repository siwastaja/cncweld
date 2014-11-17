#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define X 0
#define Y 1

#define ZMODE 0

#define CUT()   {if(ZMODE) fprintf(gfile, "G1 Z-5.0\n"); else fprintf(gfile, "M03\n");}
#define UNCUT() {if(ZMODE) fprintf(gfile, "G1 Z5.0\n");  else fprintf(gfile, "M05\n");}

// Generates main cell board, one side and one front.
// Run the laser twice to obtain all 6 parts.

// Saves weld data to gcode file as comments.
// (WELDPOINT x_idx;y_idx;x_coord;y_coord) defines a center of the weld(s)
// (ALIGNPOINT x_idx;y_idx;x_coord;y_coord) defines the coordinates of the corners
// of the finished box. idx 0;0 is the bottom-left corner.
// The box can be aligned against physical restrainers on two edges (typically bottom-left)

int main(int argc, char** argv)
{

	float part_separation = 5.0; // clearance between main board, side and front
	float thickness = 4.0; // Thickness of the material.
	float cell_length = 65.0;
	float hole = 18.2;
	float wallgaps[4] = {7.0, 1.8, 7.0, 1.8};
	float cellgap = 1.8;
	int ys[2];
	int x;
	float bonushole = 3.20; // Small extra holes in the main cell board. Manually
				// make sure these are small enough not to overlap
				// with cell holes. Can be used for wires or
				// mounting. 3.20mm can be tapped to M4.

	float lasertrim = 0.05; // How much excess does the laser burn.


	int do_sides = 1; // adds a side. Sides are solid, with small optional holes
			  // for wires / mounting.
	float finger_size_x = 8.0;
	float finger_size_y = 10.0;

	// Side bonusholes: small holes are inserted at every other cell location;
	// two holes, one near top, one near bottom. Can be used for wires or
	// mounting.
	int do_side_bonusholes = 1;
	float side_bonushole_size = 3.20; // 3.20 tapped to M4 for mounting.
	// Distance (of hole center) from the cell
	// main board. Absolute minimum is side_bonushole_size/2.
	float side_bonushole_dist = side_bonushole_size/2.0 + 5.0;

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


	if(argc < 7)
	{
		printf("Usage: cnc_gen <outfile> <y1> <y2> <x> <do_sides> <do_fronts>\n");
		printf("Ex.: cnc_gen out.g 4 3 11\n");
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

	do_sides = atoi(argv[5]);
	if(do_sides < 0 || do_sides > 1) { printf("do_sides must be 0 or 1\n"); return 1;}

	do_fronts = atoi(argv[6]);
	if(do_fronts < 0 || do_fronts > 1) { printf("do_fronts must be 0 or 1\n"); return 1;}

	FILE* gfile = fopen(argv[1], "wb");
	if(!gfile)
	{
		printf("Error opening file %s\n", argv[1]);
		return 1;
	}

	float y_step = hole + cellgap;
	float x_step = sqrt((3.0*(hole/2.0)*(hole/2.0)) + (2.0*(hole/2.0)*cellgap));

	printf("y_step = %f, x_step = %f\n", y_step, x_step);

	fprintf(gfile, "(");
	for(int i = 0; i < 7; i++)
	{
		fprintf(gfile, " %s ", argv[i]);
	}
	fprintf(gfile, ")\n");
	fprintf(gfile, "(%f; %f; %f; %f; %f; %f; %f; %f;)\n(%f; %f; %f; %f; %f; %f; %f; %d;)\n(%f; %f; %d)\n",
		part_separation, thickness, cell_length, hole, wallgaps[0], wallgaps[1], wallgaps[2], wallgaps[3],
		cellgap, bonushole, lasertrim, finger_size_x, finger_size_y, side_bonushole_size, side_bonushole_dist,
		num_side_front_fingers,
		front_y_frame_width, front_mid_width, num_front_holes_y);

	fprintf(gfile, "G96 S1\n");
	fprintf(gfile, "G21\n");
	fprintf(gfile, "G61\n");
	UNCUT();

	float front_origin_x = 10.0;
	float front_origin_y = 10.0;
	float side_origin_x = 10.0;
	float side_origin_y = 10.0;

	float origin_x = 10.0+thickness;
	float origin_y = 10.0+thickness;

	if(do_fronts)
	{
		origin_x += cell_length + part_separation;
		side_origin_x = origin_x;
	}

	if(do_sides)
	{
		origin_y += cell_length + part_separation;
		front_origin_y = origin_y;
	}

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

			if(curx%2 && bonushole > 0.01)
			{
				float bonushole_midx = (
					(2.0*(origin_x + wallgaps[0] + x_step*(curx-1) + hole/2.0))+
					(1.0*(origin_x + wallgaps[0] + x_step*curx     + hole/2.0))
					)/3.0;
				float bonushole_midy = mid_y;
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
			}

			fprintf(gfile, "G00 X%.2f Y%.2f (WELDPOINT %u;%u;%.2f;%.2f)\n", start_x_trimmed,
				start_y_trimmed, curx, cury, mid_x-origin_x, mid_y-origin_y);
			CUT();
			fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f\n", start_x_trimmed, start_y_trimmed,
				offset_i, offset_j);
			UNCUT();
		}
	}

	float outline[4][2];
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

	fprintf(gfile, "G00 X%.2f Y%.2f (ALIGNPOINT 0;0;%.2f;%.2f)\n", outline[0][X]-lasertrim, outline[0][Y]-lasertrim,
		outline[0][X]-(do_fronts?thickness:0.0), outline[0][Y]-(do_sides?thickness:0.0));
	CUT();


	// horizontal bottom side
	if(do_sides)
	{
		// Finger cut
		for(int curx = 0; curx < x; curx++)
		{
			float finger_start_x = origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_end_x = finger_start_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[0][Y]-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, outline[0][Y]-thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[0][Y]-thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, outline[0][Y]-lasertrim);
		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 1;0;%.2f;%.2f)\n", outline[1][X]+lasertrim, outline[1][Y]-lasertrim,
		outline[1][X]+(do_fronts?thickness:0.0), outline[1][Y]-(do_sides?thickness:0.0));

	// vertical right side
	if(do_fronts)
	{
		// Finger cut
		for(int cury = 0; cury < ys[0]; cury++)
		{
			float finger_start_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_end_y = finger_start_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_end_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_end_y+lasertrim);
		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 1;1;%.2f;%.2f)\n", outline[2][X]+lasertrim, outline[2][Y]+lasertrim,
		outline[2][X]+(do_fronts?thickness:0.0), outline[2][Y]+(do_sides?thickness:0.0));

	// horizontal top side

	if(do_sides)
	{
		// Finger cut
		for(int curx = x-1; curx >= 0; curx--)
		{
			float finger_end_x = origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_start_x = finger_end_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[2][Y]+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, outline[2][Y]+thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[2][Y]+thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, outline[2][Y]+lasertrim);
		}
	}

	fprintf(gfile, "G01 X%.2f Y%.2f (ALIGNPOINT 0;1;%.2f;%.2f)\n", outline[3][X]-lasertrim, outline[3][Y]+lasertrim,
		outline[3][X]-(do_fronts?thickness:0.0), outline[3][Y]+(do_sides?thickness:0.0));

	// Vertical left side

	if(do_fronts)
	{
		// Finger cut
		for(int cury = ys[0]-1; cury >=0; cury--)
		{
			float finger_end_y = origin_y + wallgaps[1] + y_step*cury + hole/2.0 - finger_size_y/2.0;
			float finger_start_y = finger_end_y + finger_size_y;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-thickness-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-thickness-lasertrim, finger_end_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[3][X]-lasertrim, finger_end_y-lasertrim);
		}

	}


	fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, outline[0][Y]-lasertrim);
	UNCUT();


	printf("Main panel size without fingers: %.2f x %.2f\n", outline[1][X]-outline[0][X], outline[2][Y]-outline[1][Y]);


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
		fprintf(gfile, "G00 X%.2f Y%.2f\n", outline[0][X]-lasertrim, side_origin_y-lasertrim);
		CUT();
		// Bottom horizontal
		for(int curx = 0; curx < x; curx++)
		{
			float finger_start_x = side_origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_end_x = finger_start_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, side_origin_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x+lasertrim, side_origin_y+thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, side_origin_y+thickness-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x-lasertrim, side_origin_y-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, side_origin_y-lasertrim);

		// Right vertical
		for(int cury = 0; cury < num_side_front_fingers; cury++)
		{
			float finger_start_y = side_origin_y + thickness +
				side_front_finger_step*cury;
			float finger_end_y = finger_start_y + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_start_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+thickness+lasertrim, finger_end_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, finger_end_y+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[1][X]+lasertrim, side_origin_y+cell_length+lasertrim);

		// Top horizontal
		for(int curx = x-1; curx >= 0; curx--)
		{
			float finger_end_x = origin_x + wallgaps[0] + x_step*curx + hole/2.0 - finger_size_x/2.0;
			float finger_start_x = finger_end_x + finger_size_x;

			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, side_origin_y+cell_length+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_start_x-lasertrim, side_origin_y+cell_length-thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, side_origin_y+cell_length-thickness+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", finger_end_x+lasertrim, side_origin_y+cell_length+lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, side_origin_y+cell_length+lasertrim);

		// Left vertical

		for(int cury = num_side_front_fingers-1; cury >= 0; cury--)
		{
			float finger_end_y = side_origin_y + thickness +
				side_front_finger_step*cury;
			float finger_start_y = finger_end_y + side_front_finger_step/2.0;
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-thickness-lasertrim, finger_start_y+lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-thickness-lasertrim, finger_end_y-lasertrim);
			fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, finger_end_y-lasertrim);
		}

		fprintf(gfile, "G01 X%.2f Y%.2f\n", outline[0][X]-lasertrim, side_origin_y-lasertrim);

		UNCUT();

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
						float bonushole_midx = origin_x + wallgaps[0] + x_step*curx + hole/2.0;
						float bonushole_midy = cury?
							(side_origin_y+cell_length-thickness-side_bonushole_dist):
							(side_origin_y+thickness+side_bonushole_dist);
						float bonushole_startx = bonushole_midx - side_bonushole_size/2.0;
						float bonushole_startx_trimmed = bonushole_startx + lasertrim;
						float bonushole_starty_trimmed = bonushole_midy;

						float bonushole_offset_i = side_bonushole_size/2.0 - lasertrim;
						float bonushole_offset_j = 0.0;

						fprintf(gfile, "G00 X%.2f Y%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed);
						CUT();
						fprintf(gfile, "G02 X%.2f Y%.2f I%.2f J%.2f\n", bonushole_startx_trimmed, bonushole_starty_trimmed,
							bonushole_offset_i, bonushole_offset_j);
						UNCUT();
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
			}
		}

	}

	// OpenSCAM bug workaround:
	fprintf(gfile, "G00 X%.2f Y%.2f\n", origin_x, origin_y);

	fclose(gfile);

	return 0;
}
