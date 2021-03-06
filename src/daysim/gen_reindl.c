/*	This program has been written by Oliver Walkenhorst at the
 *	Fraunhofer Institute for Solar Energy Systems in Freiburg, Germany
 *	last changes were added in January 2001
 *
 *	Update by Nathaniel Jones at MIT, March 2016
 */

#include  <stdio.h>
#include  <string.h>
#include  <rtmath.h>
#include  <stdlib.h>
#include  <rterror.h>
#include  <paths.h>

#include "version.h"
#include "fropen.h"
#include "sun.h"
#include "ds_constants.h"


char *header;
FILE *HEADER;               /*  header file  */
FILE *HOURLY_DATA;          /*  input weather data file  */
FILE *SHORT_TERM_DATA;      /*  input weather data shortterm file  */

/*  global variables for the header file key words and their default values */

char input_weather_data[200]="";
char input_weather_data_shortterm[200]="";   /*  default value: input_weather_data_shortterm = input_weather_data."shortterm_timestep"min  */
int shortterm_timestep=60;                /*  in minutes  */
//int input_units_genshortterm;
//int output_units_genshortterm=1;
//int input_timestep=60;                /*  in minutes  */
//int solar_time=0;                     /*  0=LST ; 1=solar time  */
//long random_seed=-10;                 /*  seed for the pseudo-random-number generator, random_seed has to be a negative integer  */
//int new=1;

/*  global variables for the header file key words representing station specific data  */

double latitude=45.32;
double longitude = 75.67;
double time_zone = 75.0;
//float site_elevation=0.0;                   /*  in metres  */
//float linke_turbidity_factor_am2[12];       /*  monthly means for jan-dec  */
//char horizon_data_in[200];            /*  name of the horizon data file for the station where the input irradiance data were collected  */
                                      /*  (the file contains 36 horizon heights in degrees starting from N to E)                        */
//char horizon_data_out[200];	      /*  name of the horizon data file for the location the output irradiance data are computed for    */
                                      /*  (the file contains 36 horizon heights in degrees starting from N to E)                        */

/*  other global variables  */

int sph=60;                               /*  sph=steps per hour: if shortterm_timestep < 60 1-min-data are generated  */
//int horizon_in=0;                         /*  indicates if an input horizon data file is specified   */
//int horizon_out=0;                        /*  indicates if an output horizon data file is specified  */
//float horizon_azimuth_in[36];             /*  divide [-180�,180�] of the input horizon in 36 azimuth classes  */
                                          /*  (south=0�, horizon heights in degrees)                          */
float horizon_azimuth_out[36];            /*  divide [-180�,180�] of the output horizon in 36 azimuth classes */
                                          /*  (south=0�, horizon heights in degrees)                          */
int linke_estimation=1;                   /*  flag that indicates if estimation of the monthly linke factors is necessary  */


/*  main program  */
int main(int argc, char *argv[])
{
	int i;
	int month, day, jday;
	//int *daylight_status;     /*  0=night hour, 1=sunrise/sunset hour, 2=innerday hour  */
	int command_line = 0;
	int file_input_output = 0;

	double time;
	double irrad_glo, irrad_beam_nor, irrad_dif;     /* in W/m� */
	double solar_elevation, solar_azimuth, eccentricity_correction;



	/* get the arguments */
	if (argc > 1)
	{

		for (i = 1; i < argc; i++) {
			if (argv[i] == NULL || argv[i][0] != '-')
				break;			/* break from options */
			if (!strcmp(argv[i], "-version")) {
				puts(VersionID);
				exit(0);
			}
			switch (argv[i][1])
			{
				case 'a':
					latitude = atof(argv[++i]);
					break;
				case 'm':
					time_zone = atof(argv[++i]);
					break;
				case 'l':
					longitude = atof(argv[++i]);
					break;

				case 'i':
					strcpy(input_weather_data, argv[++i]);
					file_input_output++;
					break;
				case 'o':
					strcpy(input_weather_data_shortterm, argv[++i]);
					file_input_output++;
					break;
				case 'g':
					month = atoi(argv[++i]);
					day = atoi(argv[++i]);
					time = atof(argv[++i]);
					irrad_glo = atof(argv[++i]);
					command_line = 1;
					break;

			}
		}
	}

	if ((!command_line && (file_input_output<2)) || argc == 1)
	{
		char *progname = fixargv0(argv[0]);
		fprintf(stdout, "\n%s: \n", progname);
		printf("Program that splits input global irradiances into direct normal and diffuse horizontal irradiances using the Reindl model. ");
		printf("The program has a command line and an input file option depending on wheter a single or mutliple irradiances are to be converted.\n");
		printf("\n");
		printf("Supported options are: \n");
		printf("-m\t time zone [DEG, West is positive] (required input)\n");
		printf("-l\t longitude [DEG, West is positive] (required input)\n");
		printf("-a\t latitude [DEG, North is positive] (required input)\n\n");
		printf("-g\t month day hour global_irradiation [W/m2] (command line version)\n");
		printf("-i\t input file [format: month day hour global_irradiation] \n");
		printf("-o\t output file [format: month day hour dir_norm_irrad dif_hor_irrad] \n");
		printf("Example: \n");
		printf("\t%s -a 42.3 -o 71 -m 75 -g 6 21 12.0 800 \n", progname);
		printf("\t%s -a 42.3 -o 71 -m 75 -i input_file.txt -o output_file.txt\n", progname);
		exit(0);
	}



	//for (i = 0; i < 36; i++)
	//	horizon_azimuth_in[i] = 0;


	if (command_line)
	{
		jday = jdate(month, day);
		if (irrad_glo < 0 || irrad_glo > SOLAR_CONSTANT_E)          /*  check irradiances and exit if necessary  */
			irrad_glo = SOLAR_CONSTANT_E;

		solar_elev_azi_ecc(latitude, longitude, time_zone, jday, time, 0, &solar_elevation, &solar_azimuth, &eccentricity_correction);
		irrad_dif = diffuse_fraction(irrad_glo, solar_elevation, eccentricity_correction)*irrad_glo;
		if (solar_elevation > 5.0)
		{
			irrad_beam_nor = (irrad_glo - irrad_dif) / sin(radians(solar_elevation));
		}
		else{
			irrad_beam_nor = 0;
			irrad_dif = irrad_glo;
		}
		if (irrad_beam_nor > SOLAR_CONSTANT_E)
		{
			irrad_beam_nor = SOLAR_CONSTANT_E;
			irrad_dif = irrad_glo - irrad_beam_nor * sin(radians(solar_elevation));
		}
		fprintf(stdout, "%.0f %.0f\n", irrad_beam_nor, irrad_dif);

	}else{	
		HOURLY_DATA = open_input(input_weather_data);
		SHORT_TERM_DATA = open_output(input_weather_data_shortterm);

		while (EOF != fscanf(HOURLY_DATA, "%d %d %lf %lf", &month, &day, &time, &irrad_glo))
		{
			jday = jdate(month, day);
			if (irrad_glo < 0 || irrad_glo > SOLAR_CONSTANT_E)          /*  check irradiances and exit if necessary  */
				irrad_glo = SOLAR_CONSTANT_E;

			solar_elev_azi_ecc(latitude, longitude, time_zone, jday, time, 0, &solar_elevation, &solar_azimuth, &eccentricity_correction);

			irrad_dif = diffuse_fraction(irrad_glo, solar_elevation, eccentricity_correction)*irrad_glo;
			if (solar_elevation > 5.0)
			{
				irrad_beam_nor = (irrad_glo - irrad_dif) / sin(radians(solar_elevation));
			}else{
				irrad_beam_nor = 0;
				irrad_dif = irrad_glo;
			}
			if (irrad_beam_nor > SOLAR_CONSTANT_E)
			{
				irrad_beam_nor = SOLAR_CONSTANT_E;
				irrad_dif = irrad_glo - irrad_beam_nor * sin(radians(solar_elevation));
			}
			fprintf(SHORT_TERM_DATA, "%d %d %.3f %.0f %.0f\n", month, day, time, irrad_beam_nor, irrad_dif);
		}
		close_file(HOURLY_DATA);
		close_file(SHORT_TERM_DATA);
	}

	return 0;
}
