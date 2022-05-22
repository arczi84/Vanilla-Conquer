// KI_Header.h

#define KI_NUM_MUSIC_TRACK 23

typedef struct my_audio
{
    unsigned char* data;
    unsigned char* file_data; // for actual malloc'd memory address. Need to align, then put into data.
    unsigned long length; // in bytes
    long frequency;
    long bits; // 8 or 16
    long multi_track; // 0 or 1. Is this a mutiple-part file to play?
    long bob;
} AUDIO_SAMPLE;

#define	KI_random(x)	(rand()/(RAND_MAX/x))

