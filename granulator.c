/*
 * granulator.c
 * Author: Ian Baxter
 */

#include <stdio.h>
#include <stdlib.h>
#include <portsf.h>
#include <math.h>
#include <string.h>

enum {ARG_NAME, ARG_INFILE, ARG_OUTFILE, ARG_DUR, ARG_MINGRAIN, ARG_MAXGRAIN, ARG_ATTACK, ARG_DECAY, ARG_SAMPLINGRATE, ARG_DENSITY, ARG_GAIN, ARG_PAN, ARG_WARP, ARGC};

/* Functions */
int open_input(char* name, PSF_PROPS *pProps);
int open_output(char* name, PSF_PROPS *pProps);
float* allocate_buffer(long bytes);
long copy(int infile, int outfile, float* buffer, long num_frames, long attack_frames, long decay_frames, double gain, float* stereobuffer);
void clean_up(int infile, int outfile, float* buffer, float* stereobuffer);
long attack(float *buffer, long attack_frames, double warp);
long decay(float *endBuf, long decay_frames, double warp);
float* allocate_stereobuffer(long bytes);

/* Declare global variables */
long stereoIn = 0;
long pos = 0;
long step_frames;
float* Bigbuffer;
double pan;
double warp;

int main(int argc, char *argv[])
{
    /* Declare variables */
    int i;
	double dur;
	double mingrain;
	double maxgrain;
	double attack;
	double decay;
	long samplingrate;
	long attack_frames;
	long decay_frames;
	double grainDur;
	long readPos;
	double totalTime;
	long frames;
	float density;
	long written_frames;
	double gain;
	
	
    PSF_PROPS props;
    int infile;
    int outfile;
    float* buffer;
    long num_frames;
    long bytes;
	long Total_frames;
	float* stereobuffer=0;
	

    /* Check the number of arguments */
    if(argc != ARGC)
    {
        printf("Usage: %s Input Output Duration(s) Mingrain(ms) Maxgrain(ms) Attack(ms) Decay(ms) Samplingrate Density Gain Pan Warp\n", argv[ARG_NAME]);
        return -1;
    }
    
	/* Convert arguments to floats */
	dur = atof(argv[ARG_DUR]);
	mingrain = atof(argv[ARG_MINGRAIN])/1000.0;
	maxgrain = atof(argv[ARG_MAXGRAIN])/1000.0;
	attack = atof(argv[ARG_ATTACK])/1000.0;
	decay = atof(argv[ARG_DECAY])/1000.0; /* division by 1000.0 as user gives the grain duration in milliseconds */
	samplingrate = atol(argv[ARG_SAMPLINGRATE]);
	density = atof(argv[ARG_DENSITY]);
	
	/* Check density>0 */
	if(density <= 0)
	{
		printf("density must be >0\n");
		return 0;
	}
	
	step_frames = samplingrate*1.0/density;
	gain = atof(argv[ARG_GAIN]);
	gain = pow(10.0,(gain/20.0));
	pan = atof(argv[ARG_PAN]);
	warp = atof(argv[ARG_WARP]);
	
	/* Check attack/decay is not 0*/
	if(attack <= 0)
	{
		printf("Attack must be larger than 0\n");
		return -1;
	}
	if(decay <= 0)
	{
		printf("Decay must be larger than 0\n");
		return 0;
	}
	
	/* Check attack/decay smaller than mingrain and maxgrain */
	if(attack > mingrain)
	{	
		printf("Minimum grain must be larger than attack\n");
		return 0;
	}
	if(attack > maxgrain)
	{
		printf("Maximum grain must be larger than attack\n");
		return 0;
	}
	if(decay > mingrain)
	{	printf("Minimum grain must be larger than decay\n");
		return 0;
	}
	if(decay > maxgrain)
	{
		printf("Maximum grain must be larger than decay\n");
		return 0;
	}
	
	/*Check pan>0*/
	if(pan < 0)
	{	
		printf("Pan must be larger than 0\n");
		return -1;
	}	
	
	/*Check warp >0*/
	if(warp < 0)
	{	
		printf("Warp must be larger than 0\n");
		return -1;
	}	
	
	/*Check grain size smaller then output*/
	if(dur < mingrain)
	{
		printf("Duration must be larger than minimum grainsize\n");
		return -1;
	}
	if(dur < maxgrain)
	{
		printf("Duration must be larger than maximum grainsize\n");
		return -1;
	}
		
	/* Check density is not too large */
	if(density > 10000)
	{
		printf("Density larger than 10000, use a lower value\n");
		return -1;
	}	
	
	/* Check Duration is not too large */
	if(dur > 5000)
	{
		printf("Duration larger than 5000, use a lower value\n");
		return -1;
	}	
	
	/* Check Pan is not too large */
	if(pan > 1000)
	{
		printf("Pan larger than 1000, use a lower value\n");
		return -1;
	}	
	
    /* Start portsf */
    if(psf_init())
    {
        printf("Error: unable to open portsf\n");
        return -2;
    }

	/* Open input audio file */
	infile = open_input(argv[ARG_INFILE], &props);
	if(infile < 0)
		return -5;

	if(props.chans==2.0)
	{
		stereoIn = 1;
	}
	
	/* Create output audio file */
	props.chans=2.0; /*Make output Stereo*/
	outfile = open_output(argv[ARG_OUTFILE], &props);
	if(outfile < 0)
		return -10;
		
	/* Calculate number of bytes and allocate Bigbuffer */
	Total_frames = (long)(dur* samplingrate); /* number of samples in file */
	bytes = Total_frames * props.chans * sizeof(float);/* convert to bytes */
	Bigbuffer = allocate_buffer(bytes);				/* allocate buffer */
	if(Bigbuffer == NULL)
	{
		clean_up(infile, outfile, buffer, stereobuffer);
		return -15;
	}
	
	
	/*clean Bigbuffer*/
	memset(Bigbuffer, 0, bytes);
	
	/*Grain Looping*/
	printf("Granulating %s to %s...\n", argv[ARG_INFILE],argv[ARG_OUTFILE]);
	for(written_frames=0 ; written_frames<Total_frames ;	written_frames+=step_frames )
	{
	
		/* Calculate random grain duration */
		grainDur = mingrain + (maxgrain-mingrain)*rand()/RAND_MAX;

		/* Calculate number of bytes and allocate buffer */
		num_frames = (long)(grainDur* samplingrate); /* number of samples in file */
		
		/*If tries to write over total duration*/
		if(pos+(2*num_frames) > (2*Total_frames))
		{ 
			break;
		}
		
		bytes = num_frames * sizeof(float);/* convert to bytes */
		buffer = allocate_buffer(bytes);/* allocate buffer */
		if(buffer == NULL)
		{
			clean_up(infile, outfile, buffer, stereobuffer);
			return -15;
		}
	
		if(stereoIn)
		{
			stereobuffer = allocate_buffer(bytes*props.chans);				/* allocate buffer */
			if(stereobuffer == NULL)
			{
				clean_up(infile, outfile, buffer, stereobuffer);
				return -15;
			}
		}
		
		/* calculate attack and decay frames*/
		attack_frames = attack * samplingrate;
		decay_frames = decay * samplingrate;
	
		/* Calculate random read position and seek */
		readPos = (psf_sndSize(infile)-num_frames)* (double)rand()/RAND_MAX;
		psf_sndSeek(infile, readPos, PSF_SEEK_SET);
	
		/* Copy input to output */
		copy(infile, outfile, buffer, num_frames, attack_frames, decay_frames, gain, stereobuffer);
		
		/* free grain buffer */
		free(buffer);
		buffer = NULL;
	}
	
	/* Write the output file */
	frames = psf_sndWriteFloatFrames(outfile, Bigbuffer, Total_frames);
	if(frames != Total_frames)
	{
		printf("Error writing to output\n");
	return -1;
	}
	
	/* Clean up */
	clean_up(infile, outfile, buffer, stereobuffer);
    
	/* finish portsf */
	psf_finish();

	/* send completion message */
	printf("Completed Granulation\n");
	return 0;
}

/*FUNCTIONS*/

/*Opens input audio file*/				
int open_input(char* name, PSF_PROPS *pProps)
{
    int infile;

	/* Read the properties of the wav file */
	infile = psf_sndOpen(name, pProps, 0);
	if(infile < 0)
	{
			printf("Error: unable to read input file %s\n", name);
	}

	return infile;
}

/*Creates output audio file*/				
int open_output(char* name, PSF_PROPS *pProps)
{
    int outfile;

    outfile = psf_sndCreate(name, pProps, 0, 0, PSF_CREATE_RDWR);
    if(outfile < 0)
    {
        printf("Error: unable to create output file %s\n", name);
    }
    
    return outfile;
}

/*Allocate buffer function*/				
float* allocate_buffer(long bytes)
{   
	float *buffer;
	
   	buffer = (float*)malloc(bytes);
	if(buffer == 0)
	{
		printf("Error: unable to allocate buffer\n");
	}
	
	return buffer;
}

/*Allocate stereobuffer function*/	
float* allocate_stereobuffer(long bytes)
{   
	float *stereobuffer;
	
   	stereobuffer = (float*)malloc(bytes);
	if(stereobuffer == 0)
	{
		printf("Error: unable to allocate stereobuffer\n");
	}
	
	return stereobuffer;
}

/*Copy function, copies input to output*/		
long copy(int infile, int outfile, float* buffer, long num_frames, long attack_frames, long decay_frames, double gain, float* stereobuffer)
{
	double increment;
	double decrement;
	long frames;
	float *endBuf = buffer + (num_frames - decay_frames);
	long i;
	float x;
	float Aleft;
	float Aright;
	
	if(stereoIn)
	{
		/* Read the input file */
		frames = psf_sndReadFloatFrames(infile, stereobuffer, num_frames);
	
		/*Read input into stereo */
		for(i=0 ; i<num_frames ; i++)
		{
			buffer[i] = 0.5*(stereobuffer[2*i]+stereobuffer[2*i+1]);
		
		
		}
	}
	
	else
	/* Read the input file */
	frames = psf_sndReadFloatFrames(infile, buffer, num_frames);
	
	if(frames != num_frames)
	{
		printf("Error reading input\n");
		return -1;
	}
	
    increment = 1.0 / attack_frames;
	attack(buffer, attack_frames, warp);
	decrement = 1.0 / decay_frames;
	decay(endBuf, decay_frames, warp);
	
	
	
	/*Panning*/
	x = pan*((((float)rand())/RAND_MAX)-0.5)*2;
	
	
	float balanceL = (1.0-x);
	float balanceR = (1.0+x);
		
	if((balanceL)<-2.0)
	{
		balanceR = 0;
		balanceL = 2.0;
	}
	if((balanceL)>2.0)
	{
		balanceR = 0;
		balanceL = 2.0;
	}
	if((balanceR)>2.0)
	{
		balanceR = 2.0;
		balanceL = 0;
	}
	if((balanceR)<-2.0)
	{
		balanceR = 2.0;
		balanceL = 0;
	}
	
	Aleft = (sqrt(2.0)/2.0)*((balanceL)/sqrt(1.0+(x*x)));
	Aright = (sqrt(2.0)/2.0)*((balanceR)/sqrt(1.0+(x*x)));
	
	
	/* implement mixing*/
    for(i=0; i<num_frames; i++)
	{
		Bigbuffer[pos+(2*i)] += Aleft*gain*buffer[i];
		Bigbuffer[pos+(2*i+1)] += Aright*gain*buffer[i];
	}

	pos+=step_frames*2;
	
	return frames;
}



/*Void clean_up function closes files and frees buffer*/	
void clean_up(int infile, int outfile, float* buffer, float* stereobuffer)
{
	/* Close files */
	if(infile >= 0)
	{
		if(psf_sndClose(infile))
		{
			printf("Warning: error closing input\n");
		}
	}

	if(outfile >= 0)
	{
		if (psf_sndClose(outfile))
		{
			printf("Warning: error closing output\n");
		}
	}
	
	/* Free the buffer */
	if(buffer)
	{
		free(buffer);
	}
}

/*Create attack function*/
long attack(float *buffer, long attack_frames, double warp)
{
	long i;
	
	double factor = 0.0; /* attack factor begins at 0 */
	double increment = 1.0 / attack_frames;

	for(i = 0 ; i < attack_frames ; i++ )
	{
		buffer[i] = pow(factor, warp) * buffer[i];
		factor += increment; /* increase factor */
	}
	return i; /* return the number of frames created */
}

/*Create decay function*/
long decay(float *endBuf, long decay_frames, double warp)
{
	long i;
	
	double factor = 1.0; /* decay factor begins at 1 */
	double decrement = 1.0 / decay_frames;
	
	for(i = 0 ; i < decay_frames ; i++ )
	{
		endBuf[i] = pow(factor, warp) * endBuf[i];
		factor -= decrement; /* decrease factor */
	}
	return i; /* return the number of frames created */
}

