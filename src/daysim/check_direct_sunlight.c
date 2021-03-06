/*  Copyright (c) 2002
 *  written by Christoph Reinhart
 *  National Research Council Canada
 *  Institute for Research in Construction
 *
 *	Update by Nathaniel Jones at MIT, March 2016
 */

#include <stdio.h>

#include  "paths.h"
#include  "fropen.h"
#include  "check_direct_sunlight.h"
#include  "read_in_header.h"
#include  "ds_constants.h"

int number_of_active_sensors=0;
double alt,azi;
float** points;
float dir,dif;

/* function that test for each time step in the wea_data_short_file whether any of the sensors is in direct view of the sun */

int  check_direct_sunlight(char *octree)
{
	int i=0;
	char befehl[5024]="";
	char befehl1[5024]="";
	float r=0;
	int shading=0;
    FILE *COMMAND;

		//sprintf(befehl,"echo -e ");
		sprintf(befehl,"echo ");
		for (i=0 ; i<number_of_active_sensors ; i++){
				//sprintf(befehl1,"%s %f %f %f %f %f %f\'\\n\'",befehl, points[0][i],points[1][i],points[2][i],-cos(alt)*sin(azi),-cos(alt)*cos(azi),sin(alt) );
				sprintf(befehl1,"%s %f %f %f %f %f %f ",befehl, points[0][i],points[1][i],points[2][i],-cos(alt)*sin(azi),-cos(alt)*cos(azi),sin(alt) );
				strcpy(befehl,befehl1);
		}

		/* test direct sunlight */
		sprintf(befehl1,"%s | %srtrace_dc -ab 0 -h  \"%s\"  \n",befehl,"",octree);
		//printf("%s ",befehl1);
		COMMAND= popen(befehl1,"r");
		while(fscanf(COMMAND,"%f",&r) != EOF){
			if ( (r/1000.0*dir) >DirectIrradianceGlareThreshold) shading=1;
			fscanf(COMMAND,"%*[^\n]");
			fscanf(COMMAND,"%*[\n\r]");
		}
		pclose(COMMAND);

return shading;
}




void  calculate_shading_status(char *octree, char *long_sensor_file, char *direct_sunlight_file_tmp)
{
	// function starts an rtrace run that determines for the "annual point file" whether
	// direct glare appears or not.
	char befehl1[5024]="";
    char buf[1024];
	FILE *COMMAND;

		/* test direct sunlight */
		sprintf(befehl1,"rtrace_dc -ab 0 -h -lr 6 -dt 0 \"%s\" < %s > %s\n",octree,long_sensor_file,direct_sunlight_file_tmp);
		printf("%s ",befehl1);
		COMMAND=popen(befehl1,"r");
		while( fscanf( COMMAND, "%s", buf ) != EOF )
			printf("%s \n",buf);
		pclose(COMMAND);

}




void  calculate_shading_status_ab0(char *octree, char *direct_sunlight_file_tmp, int LightSourceCounter)
{
	// function starts an rtrace_dc run that determines the direct-direct dayligth ceoffcient fro each applicalbe
	// sky condition in the input wheather file.
	char befehl1[5024]="";
    FILE *COMMAND;

		/* test direct sunlight */
		sprintf(befehl1,"rtrace_dc_2305 -h -I+ -ab 0 -oc  -N %d -lr 6 -dt 0 -Dm -L 1000 -aa 0.01 -ad 1500 -ar 300 -as 20 -dj 0.00 -dr 2 -ds 0.2  -lw 0.004 -sj 1.00 -st 0.15 %s < %s > %s\n",LightSourceCounter,octree,sensor_file,direct_sunlight_file_tmp);

		//printf("%s\n",befehl1);
		COMMAND=popen(befehl1,"r");
		pclose(COMMAND);
}


/* function that test for each time step in the wea_data_short_file whether any of the sensors is in direct view of the sun */

void calculate_visible_sky_angle(char *octree)
{
	int j=0, i=0,k=0,NumOfRingDivisions=0;
	float RingAltitude=0,RingAzimuth=0,r=0;
	char befehl1[1024]="";
	float DirectDC[2305][2];
	char hemisphere_sensors[200]="";
    int number_of_sensors_that_see_the_sky=0;
    int band_number_of_sensors_that_see_the_sky=0;
    FILE *COMMAND;
    FILE *RESULTS_FILE;
	FILE *DIRECT_POINTS_FILE;


	sprintf(hemisphere_sensors,"%shemisphere_sensors",tmp_directory);


	for(j=0; j< 2305; j++)
	{
		DirectDC[j][0]=0.0;
		DirectDC[j][1]=0.0;
	}

	// divide the celestial hemisphere into 2305 patches of equal size
	for(j=0; j< 29; j++)
	{ //ring index
		//assign ring latitude
		if(j<28)
			RingAltitude=(j+0.5f)*(90.0f/28.5f);
		else
			RingAltitude=90.0;
		if(RingAltitude<2.0)
			RingAltitude=2.0;
		if(j<8)
			NumOfRingDivisions=30*4;
		else if(j<16)
			NumOfRingDivisions=24*4;
		else if (j<20)
			NumOfRingDivisions=18*4;
		else if (j<24)
			NumOfRingDivisions=12*4;
		else if (j<28)
			NumOfRingDivisions=6*4;
		else
			NumOfRingDivisions=1;

		//assign azimuth
		for(i=0; i< NumOfRingDivisions; i++){ //azimuth
			RingAzimuth=(i+0.5f)*(360.0f/NumOfRingDivisions);
			if(RingAzimuth<=90)
				RingAzimuth=-90-RingAzimuth;
			if(RingAzimuth>90 && RingAzimuth<=360)
				RingAzimuth=270-RingAzimuth;
				DirectDC[k][0]=RingAzimuth;
			DirectDC[k][1]=RingAltitude;
			k++;
			//printf("%d azi %f alt %f \n",k,RingAzimuth,RingAltitude);
		}
	}


	//write our seonsor point file for rtrace
	DIRECT_POINTS_FILE=open_output(hemisphere_sensors);
	for (i=0 ; i<number_of_active_sensors ; i++)
	{
		for (j=0 ; j<2305 ; j++)
		{
			alt = radians(90.0 - DirectDC[j][1]);
			azi = -radians(DirectDC[j][0] + 90.0);
			fprintf(DIRECT_POINTS_FILE,"%f %f %f\t%f %f %f\n", points[0][i],points[1][i],points[2][i],sin(alt)*cos(azi),sin(alt)*sin(azi),cos(alt));
		}
	}
	close_file(DIRECT_POINTS_FILE);


	//run rtace and writeo ut results
	RESULTS_FILE=open_output(percentage_of_visible_sky_file);
	sprintf(befehl1,"rtrace_dc -ab 0 -h  \"%s\" < %s \n",octree,hemisphere_sensors);
	COMMAND=popen(befehl1,"r");

	//write header
	fprintf(RESULTS_FILE,"# This file was generated by Daysim 3.0 or higher.\n");
	fprintf(RESULTS_FILE,"# It provides for each workplane sensor a percentage of how much celestial hemisphere the sensor sees.\n");
	fprintf(RESULTS_FILE,"# A value of '100' indicates that the sensor is completely unobstructed. A value of '0' corresponds to fully obstructed.\n");
	fprintf(RESULTS_FILE,"# Individual values are given for the full hemisphere (0-90) as well as six altitude bands.\n");
	fprintf(RESULTS_FILE,"#x-coordinate\ty-coordinate\tz-coordinate\t0-16\t17-32\t33-47\t48-63\t64-79\t80-90\t0-90\n");


	for (i=0 ; i<number_of_active_sensors ; i++)
	{
		fprintf(RESULTS_FILE,"%f\t%f\t%f\t", points[0][i],points[1][i],points[2][i]);
		number_of_sensors_that_see_the_sky=0;
		for (j=0 ; j<2305 ; j++)
		{
			fscanf(COMMAND,"%f",&r);
			//printf ("%f\n",r);
			fscanf(COMMAND,"%*[^\n]");
			fscanf(COMMAND,"%*[\n\r]");
			if ( r>0.0)
			{
				number_of_sensors_that_see_the_sky++;
				band_number_of_sensors_that_see_the_sky++;
			}

			if(j==599)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//600
				band_number_of_sensors_that_see_the_sky=0;
			}
			if(j==1151)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//552
				band_number_of_sensors_that_see_the_sky=0;
			}
			if(j==1631)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//480
				band_number_of_sensors_that_see_the_sky=0;
			}
			if(j==2015)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//384
				band_number_of_sensors_that_see_the_sky=0;
			}
			if(j==2231)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//216
				band_number_of_sensors_that_see_the_sky=0;
			}
			if(j==2304)
			{
				fprintf(RESULTS_FILE,"%0.0f\t",100.0*(band_number_of_sensors_that_see_the_sky/2305.0));//73
				band_number_of_sensors_that_see_the_sky=0;
			}


		}
		fprintf(RESULTS_FILE,"%0.0f\n",100.0*(number_of_sensors_that_see_the_sky/2305.0));
	}


	//popen(COMMAND, "r");
	close_file(RESULTS_FILE);
}


