/*
 * OpenAL example
 *
 * Copyright(C) Florian Fainelli <f.fainelli@gmail.com>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include<sys/time.h>
#include<fcntl.h>
#include<math.h>

#include <AL/al.h>
#include <AL/alc.h>

#ifdef LIBAUDIO
#include <audio/wave.h>
#define BACKEND	"libaudio"
#else
#include <AL/alut.h>
#define BACKEND "alut"
#endif

long long current_timestamp();
double roundThousandth(double);

static void list_audio_devices(const ALCchar *devices) {
	const ALCchar *device = devices, *next = devices + 1;
	size_t len = 0;

	fprintf(stdout, "Devices list:\n");
	fprintf(stdout, "----------\n");
	while (device && *device != '\0' && next && *next != '\0') {
		fprintf(stdout, "%s\n", device);
		len = strlen(device);
		device += (len + 1);
		next += (len + 2);
	}
	fprintf(stdout, "----------\n");
}

static inline ALenum to_al_format(short channels, short samples) {
	bool stereo = (channels > 1);

	switch (samples) {
	case 16:
		if (stereo) return AL_FORMAT_STEREO16;
		else return AL_FORMAT_MONO16;
	case 8:
		if (stereo) return AL_FORMAT_STEREO8;
		else return AL_FORMAT_MONO8;
	default:
		return -1;
	}
}

int main(int argc, char **argv) {
	ALboolean enumeration;
	const ALCchar *devices;
	int ret;

	#ifdef LIBAUDIO
	WaveInfo *wave;
	#endif

	char *bufferData;
	ALCdevice *device;
	ALvoid *data;
	ALCcontext *context;
	ALsizei size, freq;
	ALenum format;
	ALuint buffer, source;
	ALfloat listenerOri[] = { 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f };
	ALboolean loop = AL_TRUE;
	ALCenum error;
	ALint source_state;

	fprintf(stdout, "Using " BACKEND " as audio backend\n");

	enumeration = alcIsExtensionPresent(NULL, "ALC_ENUMERATION_EXT");
	if (enumeration == AL_FALSE) fprintf(stderr, "enumeration extension not available\n");

	list_audio_devices(alcGetString(NULL, ALC_DEVICE_SPECIFIER));

	device = alcOpenDevice(alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER));
	if (!device) {
		fprintf(stderr, "unable to open default device\n");
		return -1;
	}

	fprintf(stdout, "Device: %s\n", alcGetString(device, ALC_DEVICE_SPECIFIER));

	alGetError();

	context = alcCreateContext(device, NULL);
	if (!alcMakeContextCurrent(context)) {
		fprintf(stderr, "failed to make default context\n");
		return -1;
	}

	/* set orientation */
	alListener3f(AL_POSITION, 0, 0, 1.0f);
    	alListener3f(AL_VELOCITY, 0, 0, 0);
	alListenerfv(AL_ORIENTATION, listenerOri);

	alGenSources((ALuint)1, &source);

	alSourcef(source, AL_PITCH, 1);
	alSourcef(source, AL_GAIN, 1);
	alSource3f(source, AL_POSITION, 0, 0, 0);
	alSource3f(source, AL_VELOCITY, 0, 0, 0);
	alSourcei(source, AL_LOOPING, loop);

	alGenBuffers(1, &buffer);

	#ifdef LIBAUDIO
	/* load data */
	wave = WaveOpenFileForReading(argv[1]);
	if (!wave) {
		fprintf(stderr, "failed to read wave file\n");
		return -1;
	}

	ret = WaveSeekFile(0, wave);
	if (ret) {
		fprintf(stderr, "failed to seek wave file\n");
		return -1;
	}

	bufferData = malloc(wave->dataSize);
	if (!bufferData) {
		perror("malloc");
		return -1;
	}

	ret = WaveReadFile(bufferData, wave->dataSize, wave);
	if (ret != wave->dataSize) {
		fprintf(stderr, "short read: %d, want: %d\n", ret, wave->dataSize);
		return -1;
	}

	alBufferData(buffer, to_al_format(wave->channels, wave->bitsPerSample),bufferData, wave->dataSize, wave->sampleRate);
	
	#else
	alutLoadWAVFile(argv[1], &format, &data, &size, &freq, &loop);
	alBufferData(buffer, format, data, size, freq);

	#endif

	alSourcei(source, AL_BUFFER, buffer);
	alSourcef(source, AL_PITCH, 1);

	alSourcePlay(source);

	alGetSourcei(source, AL_SOURCE_STATE, &source_state);

	double speed = 1.0; //The actual speed, not rounded
	double factor = 30.0; //How much the platter changes the speed
	double reflex = 0.000001; //How quickly it should return to the normal speed
	int readsize;
	long long prevtick,thistick,gap;
	char tickval;

	int fd = open("/dev/ttyS0",O_RDONLY|O_NOCTTY|O_SYNC);
	fcntl(fd,F_SETFL,fcntl(fd,F_GETFL,0) | O_NONBLOCK);
	while(1) {
		readsize = read(fd,&tickval,1*sizeof(char));
		if (readsize==-1) {
			if(roundThousandth(speed)<1) speed+=reflex;
			else if(roundThousandth(speed)>1) speed-=reflex;
		} else {
			thistick = current_timestamp();
			gap = thistick-prevtick;
			prevtick = thistick;
			speed = roundThousandth(1.0+(1.0/gap)*((tickval=='2')?factor:-factor));
		}
		if(roundThousandth(speed)==1.0 || !isfinite(speed)) speed = 1.0;
		if(roundThousandth(speed)<=0) speed = 0.001;
		printf("Speed: %f\r",speed);
		alSourcef(source, AL_PITCH, roundThousandth(speed));
	}

	/* exit context */
	alDeleteSources(1, &source);
	alDeleteBuffers(1, &buffer);
	device = alcGetContextsDevice(context);
	alcMakeContextCurrent(NULL);
	alcDestroyContext(context);
	alcCloseDevice(device);

	return 0;
}

long long current_timestamp() { //Gets the current timestamp in milliseconds
	struct timeval ct;
	gettimeofday(&ct, NULL);
	long long milliseconds = ct.tv_sec*1000LL + ct.tv_usec/1000;
	return milliseconds;
}

double roundThousandth(double x) { //Rounds numbers to the nearest thousandth
	return floor(x*1000+0.5)/1000.0;
}
