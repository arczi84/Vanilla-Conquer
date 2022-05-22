//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/* $Header:   F:\projects\c&c\vcs\code\audio.cpv   2.17   16 Oct 1995 16:50:20   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : AUDIO.CPP                                                    *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : September 10, 1993                                           *
 *                                                                                             *
 *                  Last Update : May 4, 1995 [JLB]                                            *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   Is_Speaking -- Checks to see if the eva voice is still playing.                           *
 *   Sound_Effect -- General purpose sound player.                                             *
 *   Sound_Effect -- Plays a sound effect in the tactical map.                                 *
 *   Speak -- Computer speaks to the player.                                                   *
 *   Speak_AI -- Handles starting the EVA voices.                                              *
 *   Stop_Speaking -- Forces the EVA voice to stop talking.                                    *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

extern unsigned char Apollo_AMMXon;

#include "function.h"
#include <memory.h>
#include <math.h>
#include "ccfile.h" 
//#include "defines.h"

//#include "mssleep.h"
// KI stuff
#include "audio_regs.c"
#include "soscomp.h"
#include "KI_header.h"
#undef Node
#undef List
#define Node NodeA
#define List ListA
#include <pthread.h>
#undef Node
#undef List

#define u16 uint16_t
#define u32 uint32_t

#define USE_DMA_FOR_SAMPLE_FINISH 1
#define VOC_TYPE_AUDIO 0
#define VOX_TYPE_AUDIO 1

unsigned short* DMACONR = (unsigned short*)0xDFF002;  //<=- this should be DMA_CON
unsigned short* DMACONR2 = (unsigned short*)0xDFF202;  //<=- this should be DMA_CONR2    

pthread_t audio_thread;
volatile int audio_thread_paused = 0;
int audio_thread_created = 0;
void* KI_audio_thread(void* ptr);
volatile int quit_audio_thread = 0;

volatile char file_to_load[50];
volatile AUDIO_SAMPLE *destination_audio_sample;
volatile int signal_audio_thread_to_load=0;
volatile int signal_main_thread_that_audio_has_loaded=0;

//unsigned short* DMACONR1_KI = (unsigned short*)0xDFF002;  //<=- this should be DMA_CONR2    

int current_audio_priorities[8] = { 0 }; // to hold priorities of the last/current sample being played.



/*
enum
{
    airstrik,
    aoi,
    ccthang,
    fwp,
    heavyg,
    ind,
    ind2,
    J1,
    jdi_v2,
    justdoit,
    linefire,
    march,
    nomercy,
    otp,
    prp,
    radio,
    rain,
    stopthem,
    target,
    trouble,
    unknown1,
    unknown2,
    warfare,
    KI_NUM_MUSIC_TRACK
}*/

//#define KI_NUM_MUSIC_TRACK 23

char* music_names[KI_NUM_MUSIC_TRACK] =
{
"music/airstrik.raw",
"music/aoi.raw"     ,
"music/ccthang.raw" ,
"music/fwp.raw"     ,
"music/heavyg.raw"  ,
"music/ind.raw"     ,
"music/ind2.raw"    ,
"music/j1.raw"      ,
"music/jdi_v2.raw"  ,
"music/justdoit.raw",
"music/linefire.raw",
"music/march.raw"   ,
"music/nomercy.raw" ,
"music/otp.raw"     ,
"music/prp.raw"     ,
"music/radio.raw"   ,
"music/rain.raw"    ,
"music/stopthem.raw",
"music/target.raw"  ,
"music/trouble.raw" ,
"music/unknown1.raw",
"music/unknown2.raw",
"music/warfare.raw" ,
};


AUDIO_SAMPLE music_samples[KI_NUM_MUSIC_TRACK];

typedef enum
{
    SCOMP_NONE = 0,     // No compression -- raw data.
    SCOMP_WESTWOOD = 1, // Special sliding window delta compression.
    SCOMP_SOS = 99      // SOS frame compression.
} SCompressType;
typedef enum
{
    SoundFormat_16Bit = 1, /*!<  16-bit PCM */
    SoundFormat_8Bit = 0,  /*!<  8-bit PCM */
    SoundFormat_PSG = 3,   /*!<  PSG (programmable sound generator?) */
    SoundFormat_ADPCM = 2  /*!<  IMA ADPCM compressed audio  */
} SoundFormat;

enum
{
    AUD_CHUNK_MAGIC_ID = 0x0000DEAF,
    VOLUME_MIN = 0,
    VOLUME_MAX = 255,
    PRIORITY_MIN = 0,
    PRIORITY_MAX = 255,
    MAX_SAMPLE_TRACKERS = 5, // C&C issue where sounds get cut off is because of the small number of trackers.
    STREAM_BUFFER_COUNT = 16,
    BUFFER_CHUNK_SIZE = 4096, // 256 * 32,
    UNCOMP_BUFFER_SIZE = 2098,
    BUFFER_TOTAL_BYTES = BUFFER_CHUNK_SIZE * 4, // 32 kb
    TIMER_DELAY = 25,
    TIMER_RESOLUTION = 1,
    TIMER_TARGET_RESOLUTION = 10, // 10-millisecond target resolution
    INVALID_AUDIO_HANDLE = -1,
    INVALID_FILE_HANDLE = -1,
    DECOMP_BUFFER_COUNT = 3,
};

class SoundTracker;


void KI_Play_Sample_VOC(void const* sample, VocType voc, VolType volume, signed short pan_value);
void KI_Play_Sample_VOX(void const* sample, VoxType vox, VolType volume, signed short pan_value);
int get_free_audio_channel(int audio_type);
void play_SAGA_audio(int channel, char* buffer, long length, VolType volume, short pan_value);
int KI_Play_MUSIC(char* name, int volume);
int load_sound(char* filename, int bitdepth, int frequency, AUDIO_SAMPLE* the_audio_sample, int multi_track);

static const signed char ZapTabTwo[4] = { -2, -1, 0, 1 };

static const signed char ZapTabFour[16] = { -9, -8, -6, -5, -4, -3, -2, -1, 0, 1, 2, 3, 4, 5, 6, 8 };

#ifndef clamp
static int clamp(int x, int low, int high)
{
    return ((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x));
}
#endif


const static int aud_ws_step_table2[] = { -2, -1, 0, 1 };

const static int aud_ws_step_table4[] =
{
    -9, -8, -6, -5, -4, -3, -2, -1,
     0,  1,  2,  3,  4,  5,  6,  8
};

const static int aud_ima_index_adjust_table[8] = { -1, -1, -1, -1, 2, 4, 6, 8 };

const static int aud_ima_step_table[89] =
{
    7,     8,     9,     10,    11,    12,     13,    14,    16,
    17,    19,    21,    23,    25,    28,     31,    34,    37,
    41,    45,    50,    55,    60,    66,     73,    80,    88,
    97,    107,   118,   130,   143,   157,    173,   190,   209,
    230,   253,   279,   307,   337,   371,    408,   449,   494,
    544,   598,   658,   724,   796,   876,    963,   1060,  1166,
    1282,  1411,  1552,  1707,  1878,  2066,   2272,  2499,  2749,
    3024,  3327,  3660,  4026,  4428,  4871,   5358,  5894,  6484,
    7132,  7845,  8630,  9493,  10442, 11487,  12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086,  29794, 32767
};
extern char* KI_buffer16_VOC[VOC_COUNT];
unsigned int KI_buffer16_length_VOC[VOC_COUNT] = { 0 }; // store the sample lengths, to make sure that the voc matches. It seems there are multiple voc indexes?

extern char* KI_buffer16_VOX[VOX_COUNT];
unsigned int KI_buffer16_length_VOX[VOX_COUNT] = { 0 }; // store the sample lengths, to make sure that the voc matches. It seems there are multiple voc indexes?

extern char* KI_buffer16_MUSIC[THEME_COUNT];
unsigned int KI_buffer16_MUSIC_length[THEME_COUNT] = { 0 }; // store the sample lengths, to make sure that the voc matches. It seems there are multiple voc indexes?

extern  AUDIO_SAMPLE sound_samples_VOC[VOC_COUNT];
extern  AUDIO_SAMPLE sound_samples_VOX[VOX_COUNT];

extern unsigned char* raw_music_data;

typedef struct
{
    uint16_t Rate;       // Playback rate (hertz).
    int32_t Size;        // Size of data (bytes).
    int32_t UncompSize;  // Size of data (bytes).
    uint8_t Flags;       // Holds flags for info
                         //  1: Is the sample stereo?
                         //  2: Is the sample 16 bits?
    uint8_t Compression; // What kind of compression for this sample?
} KI_AUDHeaderType;


/***************************************************************************
**	Controls what special effects may occur on the sound effect.
*/
typedef enum
{
    IN_NOVAR, // No variation or alterations allowed.
    IN_JUV,   // Juvenile sound effect alternate option.
    IN_VAR,   // Infantry variance response modification.
} ContextType;

// static struct { // MBL 02.21.2019
// Had to name the struct for VS 2017 distributed build. ST - 4/10/2019 3:59PM
struct SoundEffectNameStruct
{
    char const* Name;  // Digitized voice file name.
    int Priority;      // Playback priority of this sample.
    ContextType Where; // In what game context does this sample exist.
} SoundEffectName[VOC_COUNT] = {

    /*
    **	Special voices (typically associated with the commando).
    */
    {"BOMBIT1", 20, IN_NOVAR},  //	VOC_RAMBO_PRESENT		"I've got a present for	ya"
    {"CMON1", 20, IN_NOVAR},    //	VOC_RAMBO_CMON			"c'mon"
    {"GOTIT1", 20, IN_NOVAR},   //	VOC_RAMBO_UGOTIT		"you got it" *
    {"KEEPEM1", 20, IN_NOVAR},  //	VOC_RAMBO_COMIN		"keep 'em commin'"
    {"LAUGH1", 20, IN_NOVAR},   //	VOC_RAMBO_LAUGH		"hahaha"
    {"LEFTY1", 20, IN_NOVAR},   //	VOC_RAMBO_LEFTY		"that was left handed" *
    {"NOPRBLM1", 20, IN_NOVAR}, //	VOC_RAMBO_NOPROB		"no problem"
                                //	{"OHSH1",		20, IN_NOVAR},		//	VOC_RAMBO_OHSH			"oh shiiiiii...."
    {"ONIT1", 20, IN_NOVAR},    //	VOC_RAMBO_ONIT			"I'm on it"
    {"RAMYELL1", 20, IN_NOVAR}, //	VOC_RAMBO_YELL			"ahhhhhhh"
    {"ROKROLL1", 20, IN_NOVAR}, //	VOC_RAMBO_ROCK			"time to rock and roll"
    {"TUFFGUY1", 20, IN_NOVAR}, //	VOC_RAMBO_TUFF			"real tuff guy" *
    {"YEAH1", 20, IN_NOVAR},    //	VOC_RAMBO_YEA			"yea" *
    {"YES1", 20, IN_NOVAR},     //	VOC_RAMBO_YES			"yes" *
    {"YO1", 20, IN_NOVAR},      //	VOC_RAMBO_YO			"yo"

    /*
    **	Civilian voices (technicians too).
    */
    {"GIRLOKAY", 20, IN_NOVAR}, //	VOC_GIRL_OKAY
    {"GIRLYEAH", 20, IN_NOVAR}, //	VOC_GIRL_YEAH
    {"GUYOKAY1", 20, IN_NOVAR}, //	VOC_GUY_OKAY
    {"GUYYEAH1", 20, IN_NOVAR}, //	VOC_GUY_YEAH

    /*
    **	Infantry and vehicle responses.
    */
    {"2DANGR1", 10, IN_VAR}, //	VOC_2DANGER			"negative, too dangerous"
    {"ACKNO", 10, IN_VAR},   //	VOC_ACKNOWL			"acknowledged"
    {"AFFIRM1", 10, IN_VAR}, //	VOC_AFFIRM			"affirmative"
    {"AWAIT1", 10, IN_VAR},  //	VOC_AWAIT1			"awaiting orders"
                             //	{"BACKUP",		10,	IN_VAR},	// VOC_BACKUP			"send backup"
                             //	{"HELP",			10,	IN_VAR},	// VOC_HELP				"send help"
    {"MOVOUT1", 10, IN_VAR}, //	VOC_MOVEOUT			"movin' out"
    {"NEGATV1", 10, IN_VAR}, //	VOC_NEGATIVE		"negative"
    {"NOPROB", 10, IN_VAR},  // VOC_NO_PROB			"not a problem"
    {"READY", 10, IN_VAR},   // VOC_READY			"ready and waiting"
    {"REPORT1", 10, IN_VAR}, //	VOC_REPORT			"reporting"
    {"RITAWAY", 10, IN_VAR}, // VOC_RIGHT_AWAY		"right away sir"
    {"ROGER", 10, IN_VAR},   // VOC_ROGER			"roger"
                             //	{"SIR1",			10,	IN_VAR},	//	VOC_SIR1				"sir?"
                             //	{"SQUAD1",		10,	IN_VAR},	//	VOC_SQUAD1			"squad reporting"
                             //	{"TARGET1",		10,	IN_VAR},	//	VOC_PRACTICE		"target practice"
    {"UGOTIT", 10, IN_VAR},  // VOC_UGOTIT			"you got it"
    {"UNIT1", 10, IN_VAR},   //	VOC_UNIT1			"unit reporting"
    {"VEHIC1", 10, IN_VAR},  //	VOC_VEHIC1			"vehicle reporting"
    {"YESSIR1", 10, IN_VAR}, //	VOC_YESSIR			"yes sir"

    /*
    **	Sound effects that have a juvenile counterpart.
    */
    {"BAZOOK1", 1, IN_JUV},   // VOC_BAZOOKA			Gunfire
    {"BLEEP2", 1, IN_JUV},    // VOC_BLEEP			Clean metal bing
    {"BOMB1", 1, IN_JUV},     // VOC_BOMB1			Crunchy parachute bomb type explosion
    {"BUTTON", 1, IN_JUV},    // VOC_BUTTON			Dungeon Master button click
    {"COMCNTR1", 10, IN_JUV}, // VOC_RADAR_ON		Elecronic static with beeps
    {"CONSTRU2", 10, IN_JUV}, // VOC_CONSTRUCTION	construction sounds
    {"CRUMBLE", 1, IN_JUV},   // VOC_CRUMBLE			muffled crumble sound
    {"FLAMER2", 4, IN_JUV},   // VOC_FLAMER1			flame thrower
    {"GUN18", 4, IN_JUV},     // VOC_RIFLE			rifle shot
    {"GUN19", 4, IN_JUV},     // VOC_M60				machine gun burst -- 6 rounds
    {"GUN20", 4, IN_JUV},     // VOC_GUN20			bat hitting heavy metal door
    {"GUN5", 4, IN_JUV},      // VOC_M60A				medium machine gun burst
    {"GUN8", 4, IN_JUV},      // VOC_MINI				mini gun burst
    {"GUNCLIP1", 1, IN_JUV},  // VOC_RELOAD			gun clip reload
    {"HVYDOOR1", 5, IN_JUV},  // VOC_SLAM				metal plates slamming together
    {"HVYGUN10", 1, IN_JUV},  //	VOC_HVYGUN10		loud sharp cannon
    {"ION1", 1, IN_JUV},      // VOC_ION_CANNON		partical beam
    {"MGUN11", 1, IN_JUV},    // VOC_MGUN11			alternate tripple burst
    {"MGUN2", 1, IN_JUV},     // VOC_MGUN2			M-16 tripple burst
    {"NUKEMISL", 1, IN_JUV},  //	VOC_NUKE_FIRE		long missile sound
    {"NUKEXPLO", 1, IN_JUV},  //	VOC_NUKE_EXPLODE	long but not loud explosion
    {"OBELRAY1", 1, IN_JUV},  //	VOC_LASER			humming laser beam
    {"OBELPOWR", 1, IN_JUV},  // VOC_LASER_POWER	warming-up sound of laser beam
    {"POWRDN1", 1, IN_JUV},   //	VOC_RADAR_OFF		doom door slide
    {"RAMGUN2", 1, IN_JUV},   //	VOC_SNIPER			silenced rifle fire
    {"ROCKET1", 1, IN_JUV},   //	VOC_ROCKET1			rocket launch variation #1
    {"ROCKET2", 1, IN_JUV},   //	VOC_ROCKET2			rocket launch variation #2
    {"SAMMOTR2", 1, IN_JUV},  // VOC_MOTOR			dentists drill
    {"SCOLD2", 1, IN_JUV},    // VOC_SCOLD			cannot perform action feedback tone
    {"SIDBAR1C", 1, IN_JUV},  // VOC_SIDEBAR_OPEN	xylophone clink
    {"SIDBAR2C", 1, IN_JUV},  // VOC_SIDEBAR_CLOSE	xylophone clink
    {"SQUISH2", 1, IN_JUV},   // VOC_SQUISH2			crushing infantry
    {"TNKFIRE2", 1, IN_JUV},  // VOC_TANK1			sharp tank fire with recoil
    {"TNKFIRE3", 1, IN_JUV},  // VOC_TANK2			sharp tank fire
    {"TNKFIRE4", 1, IN_JUV},  // VOC_TANK3			sharp tank fire
    {"TNKFIRE6", 1, IN_JUV},  // VOC_TANK4			big gun tank fire
    {"TONE15", 0, IN_JUV},    // VOC_UP				credits counting up
    {"TONE16", 0, IN_JUV},    // VOC_DOWN				credits counting down
    {"TONE2", 1, IN_JUV},     // VOC_TARGET			target sound
    {"TONE5", 10, IN_JUV},    // VOC_SONAR			sonar echo
    {"TOSS", 1, IN_JUV},      // VOC_TOSS				air swish
    {"TRANS1", 1, IN_JUV},    // VOC_CLOAK			stealth tank
    {"TREEBRN1", 1, IN_JUV},  // VOC_BURN				burning crackle
    {"TURRFIR5", 1, IN_JUV},  // VOC_TURRET			muffled gunfire
    {"XPLOBIG4", 5, IN_JUV},  //	VOC_XPLOBIG4		very long muffled explosion
    {"XPLOBIG6", 5, IN_JUV},  //	VOC_XPLOBIG6		very long muffled explosion
    {"XPLOBIG7", 5, IN_JUV},  //	VOC_XPLOBIG7		very long muffled explosion
    {"XPLODE", 1, IN_JUV},    // VOC_XPLODE			long soft muffled explosion
    {"XPLOS", 4, IN_JUV},     // VOC_XPLOS			short crunchy explosion
    {"XPLOSML2", 5, IN_JUV},  //	VOC_XPLOSML2		muffled mechanical explosion

    /*
    **	Generic sound effects (no variations).
    */
    {"NUYELL1", 10, IN_NOVAR},  // VOC_SCREAM1			short infantry scream
    {"NUYELL3", 10, IN_NOVAR},  // VOC_SCREAM3			short infantry scream
    {"NUYELL4", 10, IN_NOVAR},  // VOC_SCREAM4			short infantry scream
    {"NUYELL5", 10, IN_NOVAR},  // VOC_SCREAM5			short infantry scream
    {"NUYELL6", 10, IN_NOVAR},  // VOC_SCREAM6			short infantry scream
    {"NUYELL7", 10, IN_NOVAR},  // VOC_SCREAM7			short infantry scream
    {"NUYELL10", 10, IN_NOVAR}, // VOC_SCREAM10		short infantry scream
    {"NUYELL11", 10, IN_NOVAR}, // VOC_SCREAM11		short infantry scream
    {"NUYELL12", 10, IN_NOVAR}, // VOC_SCREAM12		short infantry scream
    {"YELL1", 1, IN_NOVAR},     // VOC_YELL1			long infantry scream

    {"MYES1", 10, IN_NOVAR},   // VOC_YES				"Yes?"
    {"MCOMND1", 10, IN_NOVAR}, // VOC_COMMANDER		"Commander?"
    {"MHELLO1", 10, IN_NOVAR}, //	VOC_HELLO			"Hello?"
    {"MHMMM1", 10, IN_NOVAR},  //	VOC_HMMM				"Hmmm?"
                               //	{"MHASTE1", 	10,	IN_NOVAR},	//	VOC_PROCEED1		"I will proceed, post haste."
                               //	{"MONCE1", 		10,	IN_NOVAR},	//	VOC_PROCEED2		"I will proceed, at once."
                               //	{"MIMMD1", 		10,	IN_NOVAR},	//	VOC_PROCEED3		"I will proceed, immediately."
                               //	{"MPLAN1", 		10,	IN_NOVAR},	//	VOC_EXCELLENT1		"That is an excellent plan."
                               //	{"MPLAN2", 		10,	IN_NOVAR},	//	VOC_EXCELLENT2		"Yes, that is an excellent plan."
    {"MPLAN3", 10, IN_NOVAR},  //	VOC_EXCELLENT3		"A wonderful plan."
                               //	{"MACTION1", 	10,	IN_NOVAR},	//	VOC_EXCELLENT4		"Astounding plan of action commander."
                               //	{"MREMARK1", 	10,	IN_NOVAR},	// VOC_EXCELLENT5		"Remarkable contrivance."
    {"MCOURSE1", 10, IN_NOVAR}, // VOC_OF_COURSE		"Of course."
    {"MYESYES1", 10, IN_NOVAR}, // VOC_YESYES			"Yes yes yes."
    {"MTIBER1", 10, IN_NOVAR},  //	VOC_QUIP1			"Mind the Tiberium."
    //	{"MMG1", 		10,	IN_NOVAR},	//	VOC_QUIP2			"A most remarkable  Metasequoia Glyptostroboides."
    {"MTHANKS1", 10, IN_NOVAR}, //	VOC_THANKS			"Thank you."

    {"CASHTURN", 1, IN_NOVAR},  //	VOC_CASHTURN		Sound of money being piled up.
    {"BLEEP2", 10, IN_NOVAR},   //	VOC_BLEEPY3			Clean computer bleep sound.
    {"DINOMOUT", 10, IN_NOVAR}, //	VOC_DINOMOUT		Movin' out in dino-speak.
    {"DINOYES", 10, IN_NOVAR},  //	VOC_DINOYES			Yes Sir in dino-speak.
    {"DINOATK1", 10, IN_NOVAR}, //	VOC_DINOATK1		Dino attack sound.
    {"DINODIE1", 10, IN_NOVAR}, //	VOC_DINODIE1		Dino die sound.
    {"BEACON", 10, IN_NOVAR},   //	VOC_BEACON			Beacon sound.

#ifdef PETROGLYPH_EXAMPLE_MOD
    {"NUKE_LOB", 10, IN_NOVAR} //	VOC_NUKE_LOB		Mod expansion unit firing sound
#endif                         // PETROGLYPH_EXAMPLE_MOD

};

//
// External handlers. ST - 2/20/2019 3:41PM
//
extern void On_Sound_Effect(int sound_index, int variation, COORDINATE coord);
// extern void On_Speech(int speech_index); // MBL 02.06.2020
extern void On_Speech(int speech_index, HouseClass* house);
extern void On_Ping(const HouseClass* player_ptr, COORDINATE coord);

/***********************************************************************************************
 * Sound_Effect -- Plays a sound effect in the tactical map.                                   *
 *                                                                                             *
 *    This routine is used when a sound effect occurs in the game world. It handles fading     *
 *    the sound according to distance.                                                         *
 *                                                                                             *
 * INPUT:   voc   -- The sound effect number to play.                                          *
 *                                                                                             *
 *          coord -- The world location that the sound originates from.                        *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/12/1994 JLB : Created.                                                                 *
 *   01/05/1995 JLB : Reduces sound more dramatically when off screen.                         *
 *=============================================================================================*/
void Sound_Effect(VocType voc, COORDINATE coord, int variation)
{
#ifdef REMASTER_BUILD
    //
    // Intercept sound effect calls. ST - 2/20/2019 3:37PM
    //
    On_Sound_Effect((int)voc, variation, coord);

#else
    unsigned distance;
    CELL cell_pos;
    int pan_value;

    if (!Options.Volume || voc == VOC_NONE || !SoundOn || SampleType == SAMPLE_NONE) {
        return;
    }
    if (coord) {
        cell_pos = Coord_Cell(coord);
    }

    distance = 0xFF;
    pan_value = 0;
    if (coord && !Map.In_View(cell_pos)) {
        distance = Map.Cell_Distance(cell_pos, Coord_Cell(Map.TacticalCoord));
        distance = (unsigned int)MIN((int)distance, (int)MAP_CELL_W);
        distance = Cardinal_To_Fixed(MAP_CELL_W, distance);
        distance = MIN(distance, 0xFFu);
        distance ^= 0xFF;

        distance /= 2;
        distance = MAX(distance, 25U);

        pan_value = Cell_X(cell_pos);
        pan_value -= Coord_XCell(Map.TacticalCoord) + (Lepton_To_Cell(Map.TacLeptonWidth) >> 1);
        if (ABS(pan_value) > Lepton_To_Cell(Map.TacLeptonWidth >> 1)) {
            pan_value *= 0x8000;
            pan_value /= (MAP_CELL_W >> 2);
            pan_value = Bound(pan_value, -0x7FFF, 0x7FFF);
            //			pan_value  = MAX((int)pan_value, (int)-0x7FFF);
            //			pan_value  = MIN((int)pan_value, 0x7FFF);
        } else {
            pan_value = 0;
        }
    }

    Sound_Effect(voc, (VolType)Fixed_To_Cardinal(distance, Options.Volume), variation, pan_value);
#endif
}

/***********************************************************************************************
 * Sound_Effect -- General purpose sound player.                                               *
 *                                                                                             *
 *    This is used for general purpose sound effects. These are sounds that occur outside      *
 *    of the game world. They do not have a corresponding game world location as their source. *
 *                                                                                             *
 * INPUT:   voc      -- The sound effect number to play.                                       *
 *                                                                                             *
 *          volume   -- The volume to assign to this sound effect.                             *
 *                                                                                             *
 * OUTPUT:  Returns with the sound handle (-1 if no sound was played).                         *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/12/1994 JLB : Created.                                                                 *
 *   11/12/1994 JLB : Handles cache logic.                                                     *
 *   05/04/1995 JLB : Variation adjustments.                                                   *
 *=============================================================================================*/
int Sound_Effect(VocType voc, VolType volume, int variation, signed short pan_value)
{
    char name[_MAX_FNAME + _MAX_EXT]; // Working filename of sound effect.

    if (!Options.Volume || voc == VOC_NONE || !SoundOn || SampleType == SAMPLE_NONE) {
        return (-1);
    }

    /*
    **	Fetch a pointer to the sound effect data. Modify the sound as appropriate and desired.
    */
    char const* ext = ".AUD";
    if (Special.IsJuvenile && SoundEffectName[voc].Where == IN_JUV) {
        ext = ".JUV";
    } else {
        if (SoundEffectName[voc].Where == IN_VAR) {

            /*
            **	For infantry, use a variation on the response. For vehicles, always
            **	use the vehicle response table.
            */
            if (variation < 0) {
                if (ABS(variation) % 2) {
                    ext = ".V00";
                } else {
                    ext = ".V02";
                }
            } else {
                if (variation % 2) {
                    ext = ".V01";
                } else {
                    ext = ".V03";
                }
            }
        }
    }
    _makepath(name, NULL, NULL, SoundEffectName[voc].Name, ext);
    void const* ptr = MFCD::Retrieve(name);

    /*
    **	If the sound data pointer is not null, then presume that it is valid.
    */
    if (ptr) {
#ifdef AMIGA
		if (Apollo_AMMXon) {
            KI_Play_Sample_VOC(ptr, voc, volume, pan_value);
            return 0;
		} else
#endif
		{
        	return (Play_Sample(ptr, Fixed_To_Cardinal(SoundEffectName[voc].Priority, (int)volume), (int)volume, pan_value));  
		}
    }
    return (-1);
}

/*
**	This elaborates all the EVA speech voices.
*/
char const* Speech[VOX_COUNT] = {
    "ACCOM1",   //	mission accomplished
    "FAIL1",    //	your mission has failed
    "BLDG1",    //	unable to comply, building in progress
    "CONSTRU1", //	construction complete
    "UNITREDY", //	unit ready
    "NEWOPT1",  //	new construction options
    "DEPLOY1",  //	cannot deploy here
    "GDIDEAD1", //	GDI unit destroyed
    "NODDEAD1", //	Nod unit destroyed
    "CIVDEAD1", //	civilian killed
                //	"EVAYES1",		//	affirmative
                //	"EVANO1",		//	negative
                //	"UPUNIT1",		//	upgrade complete, new unit available
                //	"UPSTRUC1",		//	upgrade complete, new structure available
    "NOCASH1",  //	insufficient funds
    "BATLCON1", //	battle control terminated
    "REINFOR1", //	reinforcements have arrived
    "CANCEL1",  //	canceled
    "BLDGING1", //	building
    "LOPOWER1", //	low power
    "NOPOWER1", //	insufficient power
    "MOCASH1",  //	need more funds
    "BASEATK1", //	our base is under attack
    "INCOME1",  //	incoming missile
    "ENEMYA",   //	enemy planes approaching
    "NUKE1",    //	nuclear warhead approaching - VOX_INCOMING_NUKE
                //	"RADOK1",		//	radiation levels are acceptable
                //	"RADFATL1",		//	radiation levels are fatal
    "NOBUILD1", //	unable to build more
    "PRIBLDG1", //	primary building selected
                //	"REPDONE1",		//	repairs completed
    "NODCAPT1", //	Nod building captured
    "GDICAPT1", //	GDI building captured
                //	"SOLD1",			//	structure sold
    "IONCHRG1", //	ion cannon charging
    "IONREDY1", //	ion cannon ready
    "NUKAVAIL", //	nuclear weapon available
    "NUKLNCH1", //	nuclear weapon launched - VOX_NUKE_LAUNCHED
    "UNITLOST", // unit lost
    "STRCLOST", // structure lost
    "NEEDHARV", //	need harvester
    "SELECT1",  // select target
    "AIRREDY1", // airstrike ready
    "NOREDY1",  //	not ready
    "TRANSSEE", // Nod transport sighted
    "TRANLOAD", // Nod transport loaded
    "ENMYAPP1", //	enemy approaching
    "SILOS1",   //	silos needed
    "ONHOLD1",  //	on hold
    "REPAIR1",  //	repairing
    "ESTRUCX",  //	enemy structure destroyed
    "GSTRUC1",  //	GDI structure destroyed
    "NSTRUC1",  //	NOD structure destroyed
    "ENMYUNIT", // Enemy unit destroyed
    //	"GUKILL1",		//	gold unit destroyed
    //	"GSTRUD1",		//	gold structure destroyed
    //	"GONLINE1",		//	gold player online
    //	"GLEFT1",		//	gold player has departed
    //	"GOLDKILT",		//	gold player destroyed
    //	"GOLDWIN",		//	gold player is victorious
    //	"RUKILL1",		//	red unit destroyed
    //	"RSTRUD1",		//	red structure destroyed
    //	"RONLINE1",		//	red player online
    //	"RLEFT1",		//	red player has departed
    //	"REDKILT",		//	red player destroyed
    //	"REDWIN",		//	red player is victorious
    //	"GYUKILL1",		//	grey unit destroyed
    //	"GYSTRUD1",		//	grey structure destroyed
    //	"GYONLINE",		//	grey player online
    //	"GYLEFT1",		//	grey player has departed
    //	"GREYKILT",		//	grey player destroyed
    //	"GREYWIN",		//	grey player is victorious
    //	"OUKILL1",		//	orange unit destroyed
    //	"OSTRUD1",		//	orange structure destroyed
    //	"OONLINE1",		//	orange player online
    //	"OLEFT1",		//	orange player has departed
    //	"ORANKILT",		//	orange player destroyed
    //	"ORANWIN",		//	orange player is victorious
    //	"GNUKILL1",		//	green unit destroyed
    //	"GNSTRUD1",		//	green structure destroyed
    //	"GNONLINE",		//	green player online
    //	"GNLEFT1",		//	green player has departed
    //	"GRENKILT",		//	green player destroyed
    //	"GRENWIN",		//	green player is victorious
    //	"BUKILL1",		//	blue unit destroyed
    //	"BSTRUD1",		//	blue structure destroyed
    //	"BONLINE1",		//	blue player online
    //	"BLEFT1",		//	blue player has departed
    //	"BLUEKILT",		//	blue player destroyed
    //	"BLUEWIN"		//	blue player is victorious
};
static VoxType CurrentVoice = VOX_NONE;

/***********************************************************************************************
 * Speak -- Computer speaks to the player.                                                     *
 *                                                                                             *
 *    This routine is used to have the game computer (EVA) speak to the player.                *
 *                                                                                             *
 * INPUT:   voice -- The voice number to speak (see defines.h).                                *
 *                                                                                             *
 * OUTPUT:  Returns with the handle of the playing speech (-1 if no voice started).            *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   11/12/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void Speak(VoxType voice, HouseClass* house, COORDINATE coord)
{
#ifdef REMASTER_BUILD
    // MBL 02.22.2019
    if (voice == VOX_NONE) {
        return;
    }

    //
    // Intercept speech calls. ST - 2/20/2019 3:37PM
    //
    // On_Speech((int)voice); // MBL 02.06.2020
    On_Speech((int)voice, house);
    if (coord) {
        On_Ping(house, coord);
    }

#else
    if (Options.Volume && SampleType != 0 && voice != VOX_NONE && voice != SpeakQueue && voice != CurrentVoice
        && SpeakQueue == VOX_NONE) {
        SpeakQueue = voice;
    }
#endif
}

/***********************************************************************************************
 * Speak_AI -- Handles starting the EVA voices.                                                *
 *                                                                                             *
 *    This starts the EVA voice talking as well. If there is any speech request in the queue,  *
 *    it will be started when the current voice is finished. Call this routine as often as     *
 *    possible (once per game tick is sufficient).                                             *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/27/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void Speak_AI(void)
{
// MBL 06.17.2019 KO
#ifndef REMASTER_BUILD
    static VoxType _last = VOX_NONE;
    if (SampleType == 0)
        return;

    if (!Is_Sample_Playing(SpeechBuffer)) {
        CurrentVoice = VOX_NONE;
        if (SpeakQueue != VOX_NONE) {
            if (SpeakQueue != _last) {
                char name[_MAX_FNAME + _MAX_EXT];

                _makepath(name, NULL, NULL, Speech[SpeakQueue], ".AUD");
                if (CCFileClass(name).Read(SpeechBuffer, SPEECH_BUFFER_SIZE)) {
#ifdef AMIGA            
				if (Apollo_AMMXon)
                    KI_Play_Sample_VOX(SpeechBuffer, SpeakQueue, Options.Volume,0);
                else 
#endif
                    Play_Sample(SpeechBuffer, 254, Options.Volume);                  
                }
                _last = SpeakQueue;
            } else {
#ifdef AMIGA
				if (Apollo_AMMXon)
					KI_Play_Sample_VOX(SpeechBuffer, SpeakQueue, Options.Volume,0);
				else
#endif
                	Play_Sample(SpeechBuffer, 254, Options.Volume);
            }
            SpeakQueue = VOX_NONE;
        }
    }
#endif
}

/***********************************************************************************************
 * Stop_Speaking -- Forces the EVA voice to stop talking.                                      *
 *                                                                                             *
 *    Use this routine to immediately stop the EVA voice from speaking. It also clears out     *
 *    the pending voice queue.                                                                 *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/27/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void Stop_Speaking(void)
{
    SpeakQueue = VOX_NONE;
    if (SampleType != 0) {
        Stop_Sample_Playing(SpeechBuffer);
    }
}

/***********************************************************************************************
 * Is_Speaking -- Checks to see if the eva voice is still playing.                             *
 *                                                                                             *
 *    Call this routine when the EVA voice being played needs to be checked. A typical use     *
 *    of this would be when some action needs to be delayed until the voice has finished --    *
 *    say the end of the game.                                                                 *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  bool; Is the EVA voice still playing?                                              *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   03/12/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
bool Is_Speaking(void)
{
    Speak_AI();
    if (SampleType != 0 && (SpeakQueue != VOX_NONE || Is_Sample_Playing(SpeechBuffer))) {
        return (true);
    }
    return (false);
}
#ifdef AMIGA
void KI_Play_Sample_VOC(void const* sample, VocType voc, VolType volume, signed short pan_value)
{
    //return;
    int address;
    KI_AUDHeaderType raw_header;

    if (voc >= VOC_COUNT || voc < 0)
        return;


    memcpy(&raw_header, sample, sizeof(raw_header));
    raw_header.Rate = le16toh(raw_header.Rate);

    raw_header.Size = le32toh(raw_header.Size);
    raw_header.UncompSize = le32toh(raw_header.UncompSize);

    _SOS_COMPRESS_INFO sosinfo;

    sosinfo.wChannels = (raw_header.Flags & 1) + 1;
    sosinfo.wBitSize = raw_header.Flags & 2 ? 16 : 8;
    sosinfo.dwCompSize = raw_header.Size;
    sosinfo.dwUnCompSize = raw_header.Size * (sosinfo.wBitSize / 4);

    // Change to 64 bit aligned samples...
    if (sound_samples_VOC[voc].file_data == NULL)
    {
        KI_buffer16_length_VOC[voc] = raw_header.UncompSize; // store the size, for checking the sample later
        sound_samples_VOC[voc].file_data = (unsigned char*)calloc(1, raw_header.UncompSize * 2 + 4); // add 4 bytes, for 64-bit alignmenty

        if (sound_samples_VOC[voc].file_data == NULL)
            return;

        // align to 4 byte boundary...
        address = (int)sound_samples_VOC[voc].file_data + 4;
        address = address & ~4;
        sound_samples_VOC[voc].data = (unsigned char*)address;

        int loop = floor(raw_header.Size / 520);
        //int remain = raw_header.UncompSize-(loop * 520);

        for (int i = 0; i < (loop); i++)
        {

            sosinfo.lpSource = Add_Long_To_Pointer(sample, sizeof(raw_header) + (i + 1) * (8) + i * 512);
            sosinfo.lpDest = Add_Long_To_Pointer(sound_samples_VOC[voc].data, (i * 2048));
            sosCODECDecompressData(&sosinfo, 2048);
        }

        int remain = raw_header.Size - (loop * 520);
        if (remain < 0 || remain>512)
        {
            //printf("AUDIO VOC SIZE ERROR: remain=%d\n;", remain);
        }
        else
        {
            sosinfo.lpSource = Add_Long_To_Pointer(sample, sizeof(raw_header) + (loop + 1) * (8) + loop * 512);
            sosinfo.lpDest = Add_Long_To_Pointer(sound_samples_VOC[voc].data, (loop * 2048));
            sosCODECDecompressData(&sosinfo, remain * 4);
        }

        // sample should be decoded into out AUDIO_SAMPLE data structure...
        sound_samples_VOC[voc].bits = 16;
        sound_samples_VOC[voc].frequency = 22050;
        sound_samples_VOC[voc].length = KI_buffer16_length_VOC[voc];
        sound_samples_VOC[voc].multi_track = 0;

        //if (load_sound((char*)file_to_load, 16, 22050, (AUDIO_SAMPLE*)destination_audio_sample, 0) != 0)
    }

    int channel = get_free_audio_channel(VOC_TYPE_AUDIO);

    //printf("Audio channel: %d\n", channel);
    if (channel > 0)
    {
        play_SAGA_audio(channel, (char*)sound_samples_VOC[voc].data, KI_buffer16_length_VOC[voc], volume, pan_value);
    }
    else
    {
        //printf("Audio channel returned -1\n");
    }


}

void KI_Play_Sample_VOX(void const* sample, VoxType vox, VolType volume, signed short pan_value)
{
    int address;
    KI_AUDHeaderType raw_header;

    if (vox >= VOX_COUNT || vox < 0)
        return;

    memcpy(&raw_header, sample, sizeof(raw_header));
    raw_header.Rate = le16toh(raw_header.Rate);

    raw_header.Size = le32toh(raw_header.Size);
    raw_header.UncompSize = le32toh(raw_header.UncompSize);

    _SOS_COMPRESS_INFO sosinfo;

    sosinfo.wChannels = (raw_header.Flags & 1) + 1;
    sosinfo.wBitSize = raw_header.Flags & 2 ? 16 : 8;
    sosinfo.dwCompSize = raw_header.Size;
    sosinfo.dwUnCompSize = raw_header.Size * (sosinfo.wBitSize / 4);

    // Change to 64 bit aligned samples...
    if (sound_samples_VOX[vox].file_data == NULL)
    {
        KI_buffer16_length_VOX[vox] = raw_header.UncompSize; // store the size, for checking the sample later
        sound_samples_VOX[vox].file_data = (unsigned char*)calloc(1, raw_header.UncompSize * 2+4); // add 4 bytes, for 64-bit alignmenty

        if (sound_samples_VOX[vox].file_data == NULL)
            return;

        // align to 4 byte boundary...
        address = (int)sound_samples_VOX[vox].file_data + 4;
        address = address & ~4;
        sound_samples_VOX[vox].data = (unsigned char*)address;

        int loop = floor(raw_header.Size / 520);
        //int remain = raw_header.UncompSize-(loop * 520);

        for (int i = 0; i < (loop); i++)
        {

            sosinfo.lpSource = Add_Long_To_Pointer(sample, sizeof(raw_header) + (i + 1) * (8) + i * 512);
            sosinfo.lpDest = Add_Long_To_Pointer(sound_samples_VOX[vox].data, (i * 2048));
            sosCODECDecompressData(&sosinfo, 2048);
        }

        int remain = raw_header.Size - (loop * 520);
        if (remain < 0 || remain>512)
        {
            //printf("AUDIO VOX SIZE ERROR: remain=%d\n;", remain);
        }
        else
        {
            sosinfo.lpSource = Add_Long_To_Pointer(sample, sizeof(raw_header) + (loop + 1) * (8) + loop * 512);
            sosinfo.lpDest = Add_Long_To_Pointer(sound_samples_VOX[vox].data, (loop * 2048));
            sosCODECDecompressData(&sosinfo, remain * 4);
        }

        // sample should be decoded into out AUDIO_SAMPLE data structure...
        sound_samples_VOX[vox].bits = 16;
        sound_samples_VOX[vox].frequency = 22050;
        sound_samples_VOX[vox].length = KI_buffer16_length_VOX[vox];
        sound_samples_VOX[vox].multi_track = 0;

        //if (load_sound((char*)file_to_load, 16, 22050, (AUDIO_SAMPLE*)destination_audio_sample, 0) != 0)
    }
    
    int channel = get_free_audio_channel(VOX_TYPE_AUDIO);

    //printf("Audio channel: %d\n", channel);
    if (channel > 0)
    {
        //play_SAGA_audio(channel, KI_buffer16_VOX[vox], KI_buffer16_length_VOX[vox], volume, pan_value);
        play_SAGA_audio(channel, (char *)sound_samples_VOX[vox].data, KI_buffer16_length_VOX[vox], volume, pan_value);
    }
    else
    {
        //printf("Audio channel returned -1\n");
    }
}


int KI_Play_MUSIC(char* name, int volume)
{
    char temp_name[40];
    int name_length;
    static char last_name[40] = { "" };
    static int last_track_found = 0;

    if (signal_audio_thread_to_load == 1)
    {
        //printf(" - waiting loading - ");
        return 0; // skip this loop if we are waiting for a sample to load...
    }

    if (signal_main_thread_that_audio_has_loaded == 1)
    {
        //printf("Playing AFTER thread loading %s...\n", file_to_load);
        play_SAGA_audio(1, (char*)destination_audio_sample->data, destination_audio_sample->length, volume, 0);
        signal_main_thread_that_audio_has_loaded = 0;
        return 0; // all is good...
    }

    for (size_t i = 0; i < strlen(name); ++i) {
        name[i]=tolower(name[i]);
    }

    // check to see if the song has changed...
    if (strcmp(name, last_name) == 0)
    {
        // check to see if the song is still playing. If it isn't return value of 1 to say it has finished...

        uint16_t the_CON = *DMACONR;
        if ((the_CON & 0x2) > 0)
        {
            // set the volume value, in case it has been changed by the user
            *AUD1VOL = volume << 8 | volume;
            return 0; // same song, nothing to see here...
        }

        //printf("DMA finished\n");
        // DMA finshed. Return 1
        if (last_track_found == 0)
            return 2;
        else
        return 1;
        
    }

    // turn audio off...
    *DMACON1 = DMAF_AUD1;
    *INTENA1 = INTF_AUD1;

    strcpy(last_name, name);

    //printf("KI_Play_MUSIC() requested: %s\n", name);

    name_length = strlen(name);
    strcpy(temp_name, "music/");
    strncat(temp_name, name, name_length - 4);
    strcat(temp_name, ".raw");

    int i;
    while (i < KI_NUM_MUSIC_TRACK)
    {
        if(strcmp(temp_name, music_names[i])==0)
        {
            break;
        }
        i++;
    }

    if (i >= KI_NUM_MUSIC_TRACK)
    {
        //printf("Track not found in list.\n");
        last_track_found = 0;
        return - 1;
    }

    // force it for it for testing.
    //strcpy(temp_name, "music/airstrik.raw");
    //printf("KI_Play_MUSIC() target -> %s\n", temp_name);

    // load up file, if it hasn't been loaded already...
    if (music_samples[i].file_data == 0)
    {
        //printf("load_sound() %s\n", temp_name);

        last_track_found = 1;

        // Now, user the audio_thread to load up the data... use some flags to tell the thread that data is ready to load...
        strcpy((char *)file_to_load, temp_name);
        destination_audio_sample = &music_samples[i];
        
        // tell audio thread to load data...
        signal_audio_thread_to_load = 1;

        /*
        if (load_sound(temp_name, 16, 22050, &music_samples[i], 0) != 0)
        {
            //printf("KI_Play_MUSIC() could not load -> %s\n", temp_name);
            return - 1;
        }*/
        return 0;
    }
    else
    {
        last_track_found = 1;
        //printf("Playing already loaded track %s[%d]...\n", temp_name, i);
        play_SAGA_audio(1, (char*)music_samples[i].data, music_samples[i].length, volume, 0);
    }
    return 0;  // all is well....
    
    {
        // called in theme.cpp. But not quite working right, due to OpenAL needing volume up to be playing, etc.
        int fh = Open_File(name, 1);

        if (fh == INVALID_FILE_HANDLE) {
            //printf("Could not open file in KI_Pay_MUSIC\n");
            return -1;
        }

        if (raw_music_data)
            free(raw_music_data);

        raw_music_data = malloc(File_Size(fh));
        if (!raw_music_data)
        {
            Close_File(fh);
            return 0;
        }

        Read_File(fh, raw_music_data, File_Size(fh));

        Close_File(fh);

        KI_AUDHeaderType raw_header;


        memcpy(&raw_header, raw_music_data, sizeof(raw_header));
        raw_header.Rate = le16toh(raw_header.Rate);

        raw_header.Size = le32toh(raw_header.Size);
        raw_header.UncompSize = le32toh(raw_header.UncompSize);

        _SOS_COMPRESS_INFO sosinfo;

        sosinfo.wChannels = (raw_header.Flags & 1) + 1;
        sosinfo.wBitSize = raw_header.Flags & 2 ? 16 : 8;
        sosinfo.dwCompSize = raw_header.Size;
        sosinfo.dwUnCompSize = raw_header.Size * (sosinfo.wBitSize / 4);

        // kludge into one music slot, for the moment.
        int vox = 1;
        if (KI_buffer16_MUSIC[vox])
            free(KI_buffer16_MUSIC[vox]);

        if (KI_buffer16_MUSIC[vox] == NULL)
        {
            KI_buffer16_MUSIC_length[vox] = raw_header.UncompSize; // store the size, for checking the sample later
            KI_buffer16_MUSIC[vox] = (char*)calloc(1, raw_header.UncompSize * 2);
            if (KI_buffer16_MUSIC[vox] == NULL)
                return 0;

            int loop = floor(raw_header.Size / 520);
            //int remain = raw_header.UncompSize-(loop * 520);

            for (int i = 0; i < (loop); i++)
            {

                sosinfo.lpSource = Add_Long_To_Pointer(raw_music_data, sizeof(raw_header) + (i + 1) * (8) + i * 512);
                sosinfo.lpDest = Add_Long_To_Pointer(KI_buffer16_MUSIC[vox], (i * 2048));
                sosCODECDecompressData(&sosinfo, 2048);
            }

            int remain = raw_header.Size - (loop * 520);
            if (remain < 0 || remain>512)
            {
                //printf("AUDIO MUSIC SIZE ERROR: remain=%d\n;", remain);
            }
            else
            {
                sosinfo.lpSource = Add_Long_To_Pointer(raw_music_data, sizeof(raw_header) + (loop + 1) * (8) + loop * 512);
                sosinfo.lpDest = Add_Long_To_Pointer(KI_buffer16_MUSIC[vox], (loop * 2048));
                sosCODECDecompressData(&sosinfo, remain * 4);
            }
        }

        play_SAGA_audio(1, KI_buffer16_MUSIC[vox], KI_buffer16_MUSIC_length[vox], 255, 0);


        //long Read_File(int handle, void* buf, unsigned long bytes)



        //_makepath(name, NULL, NULL, SoundEffectName[voc].Name, ext);
        //void const* ptr = MFCD::Retrieve(name);

        /*
        **	If the sound data pointer is not null, then presume that it is valid.
        */
        //if (ptr) {
        //    KI_Play_Sample_VOC(ptr, voc, volume, pan_value);
    }


}

int get_free_audio_channel(int audio_type)
{
    static int last_channel = 2;
    int first_channel_of_lowest_priority = -1; // this is so we can force a low priority vox onto a channel.
    int lowest_priority_found = 255;

    if (USE_DMA_FOR_SAMPLE_FINISH)
    {
        int channels_checked = 0;
        uint16_t the_CON;;
        int curr_audio_pri;

        if (audio_type == VOC_TYPE_AUDIO)
            curr_audio_pri = 100;
        else
            curr_audio_pri = 1;

        // look for a free channel, starting with last_channel+1;
        while (channels_checked < 6)
        {
            if (++last_channel > 7)
                last_channel = 2; // JUST FOR TESTING. Use Arne only, as music is probably on Paula.

            // get channel priority... currently just use voc/vox, but set up priority eventually.
            if (current_audio_priorities[last_channel] < lowest_priority_found)
            {
                lowest_priority_found = current_audio_priorities[last_channel];
                first_channel_of_lowest_priority = last_channel;
            }


            if (last_channel < 4)
            {
                the_CON = *DMACONR;
                //printf(" last_chan %d, CON=%x (%x). ", last_channel, the_CON, (1 << last_channel));
             
                if ( (the_CON & (1 << last_channel)) == 0) // i.e., DMA==0 when finished playing
                {
                 //   printf("DMA1 found free %d\n", last_channel);
                    current_audio_priorities[last_channel] = curr_audio_pri; 
                    return last_channel;
                }
            }
            else
            {
                the_CON = *DMACONR2;
                //printf(" last_chan %d, CON2=%x", last_channel, the_CON);

                if ( (the_CON & (1 << (last_channel-4))) == 0) // i.e., DMA==0 when finished playing
                {
                 //   printf("DMA2 found free %d\n", last_channel);

                    current_audio_priorities[last_channel] = curr_audio_pri;
                    return last_channel;
                }
            }

            channels_checked++;
        }

        // if we get here, then all channels are full... let's return the one with the lowest priority, that was found first (oldest sample)
        if (first_channel_of_lowest_priority >= 0)
        {
            
            if (curr_audio_pri > current_audio_priorities[first_channel_of_lowest_priority])
            {
                current_audio_priorities[first_channel_of_lowest_priority] = curr_audio_pri; 
                return first_channel_of_lowest_priority;
            }
            return -1;

        }
    }

    // no channels free...
    return -1;
}


void play_SAGA_audio(int channel, char* buffer, long length, VolType volume, short pan_value)
{
    int SAGA_volume= volume / 8; // 40 is pretty loud on V4. So let's divide 0xFF by 8

    int vol_left;// = volume;
    int vol_right;// = volume;
    int modified_freq=22050;

    if (channel != 1)
    {
        float multiplier;
        
        if (pan_value == 0)
        {
            multiplier = (float)KI_random(200) / 1000;
            vol_left = (int)((0.7 - multiplier) * (float)SAGA_volume);
            vol_right = (int)((0.7 + multiplier) * (float)SAGA_volume);
        }
        else
        {
            // we have an off-screen thing. Maybe we push it all to one channel?
            if (pan_value < 0)
            {
                vol_left = SAGA_volume;;
                vol_right = (int) 0.25* SAGA_volume;
            }
            else
            {
                vol_left = (int)0.25 * SAGA_volume;
                vol_right = SAGA_volume;
            }
        }
        modified_freq = 20500 + KI_random(3100);

        //printf("L/R=%d/%d, multiplier=%f\n", vol_left, vol_right, multiplier);
    }
    else
    {
        //Is music, centre
        vol_left= SAGA_volume;
        vol_right = SAGA_volume;

    }
    //pan_value *= 0x8000;
    //pan_value /= (MAP_CELL_W >> 2);
    //pan_value = Bound(pan_value, -0x7FFF, 0x7FFF);


    //channel = 7;
    //printf("AUDIO: %d\n", channel);
    switch (channel)
    {
    case 0:
        *DMACON1 = DMAF_AUD0;
        *INTENA1 = INTF_AUD0;
        *AUD0MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD0LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD0LEN2 = (length >> 2) & 0xFFFF;
        *AUD0LEN = (length >> 2) >> 16;
        *AUD0PER = (unsigned long)(3546895 / 22050);
        *AUD0VOL = vol_left << 8 | vol_right;
        *DMACON1 = 0x8000 | DMAF_AUD0 ;// DMAF_SETCLR | DMAF_AUD0;
        break;

    case 1:
        *DMACON1 = DMAF_AUD1;
        *INTENA1 = INTF_AUD1;
        *AUD1MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD1LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD1LEN2 = (length >> 2) & 0xFFFF;
        *AUD1LEN = (length >> 2) >> 16;
        *AUD1PER = (unsigned long)(3546895 / modified_freq);
        *AUD1VOL = vol_left << 8 | vol_right;
        *DMACON1 = 0x8000 | DMAF_AUD1;// DMAF_SETCLR | DMAF_AUD0;
        break;
    case 2:
        *DMACON1 = DMAF_AUD2;
        *INTENA1 = INTF_AUD2;
        *AUD2MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD2LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD2LEN2 = (length >> 2) & 0xFFFF;
        *AUD2LEN = (length >> 2) >> 16;
        *AUD2PER = (unsigned long)(3546895 / modified_freq);
        *AUD2VOL = vol_left << 8 | vol_right;
        *DMACON1 = 0x8000 | DMAF_AUD2;// DMAF_SETCLR | DMAF_AUD0;
        break;
    case 3:
       * DMACON1 = DMAF_AUD3;
        *INTENA1 = INTF_AUD3;
        *AUD3MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD3LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD3LEN2 = (length >> 2) & 0xFFFF;
        *AUD3LEN = (length >> 2) >> 16;
        *AUD3PER = (unsigned long)(3546895 / modified_freq);
        *AUD3VOL = vol_left << 8 | vol_right;
        *DMACON1 = 0x8000 | DMAF_AUD3;// DMAF_SETCLR | DMAF_AUD0;
        break;
    case 4:
        *DMACON2 = DMAF_AUD0;
        *INTENA2 = INTF_AUD0;
        *AUD4MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD4LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD4LEN2 = (length >> 2) & 0xFFFF;
        *AUD4LEN = (length >> 2) >> 16;
        *AUD4PER = (unsigned long)(3546895 / modified_freq);
        *AUD4VOL = vol_left << 8 | vol_right;
        *DMACON2 = 0x8000 | DMAF_AUD0;// DMAF_SETCLR | DMAF_AUD0;
        break;

    case 5:
        *DMACON2 = DMAF_AUD1;
        *INTENA2 = INTF_AUD1;
        *AUD5MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD5LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD5LEN2 = (length >> 2) & 0xFFFF;
        *AUD5LEN = (length >> 2) >> 16;
        *AUD5PER = (unsigned long)(3546895 / modified_freq);
        *AUD5VOL = vol_left << 8 | vol_right;
        *DMACON2 = 0x8000 | DMAF_AUD1;// DMAF_SETCLR | DMAF_AUD0;
        break;
    case 6:
        *DMACON2 = DMAF_AUD2;
        *INTENA2 = INTF_AUD2;
        *AUD6MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD6LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD6LEN2 = (length >> 2) & 0xFFFF;
        *AUD6LEN = (length >> 2) >> 16;
        *AUD6PER = (unsigned long)(3546895 / modified_freq);
        *AUD6VOL = vol_left << 8 | vol_right;
        *DMACON2 = 0x8000 | DMAF_AUD2;// DMAF_SETCLR | DMAF_AUD0;
        break;
    
    case 7:
        *DMACON2= DMAF_AUD3;
        *INTENA2 = INTF_AUD3;// INTF_AUD3;
        *AUD7MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)
        *AUD7LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
        *AUD7LEN2 = (length >> 2) & 0xFFFF;
        *AUD7LEN = (length >> 2) >> 16;
        *AUD7PER = (unsigned long)(3546895 / modified_freq);
        *AUD7VOL = vol_left << 8 | vol_right;
        *DMACON2 = 0x8000 | DMAF_AUD3;// DMAF_SETCLR | DMAF_AUD0;

        break;
    }

  /*
    pan_value *= 0x8000;
    pan_value /= (MAP_CELL_W >> 2);
    pan_value = Bound(pan_value, -0x7FFF, 0x7FFF);*/


    /*
    *DMACON2 = 0x0008; // DMAF_AUD3; //disable DMA
    *INTENA2 = (1 << 10);// INTF_AUD3;

    *AUD7MODE = 1 | 1 << 1; // 8 bit (bit0=0), single shot (bit1=1)

    *AUD7LCH = (unsigned long)buffer;// (unsigned long)dummy_buff;
    *AUD7LEN2 = (length >> 2) & 0xFFFF;
    *AUD7LEN = (length >> 2) >> 16;
    *AUD7PER = (unsigned long)(3546895 / 22050);
    *AUD7VOL = 40 << 8 | 40;
    *DMACON2 = 0x8000 | 0x0008;// DMAF_SETCLR | DMAF_AUD3;
    */
}

int load_sound(char* filename, int bitdepth, int frequency, AUDIO_SAMPLE* the_audio_sample, int multi_track)
{
    FILE* fp;
    int address;
    fp = fopen(filename, "r");

    //printf("Loading sound: %s\n", filename);
    // initialise samples...
    the_audio_sample->data = 0;

    if (fp)
    {
        fseek(fp, 0L, SEEK_END);
        the_audio_sample->length = ftell(fp);
        //the_audio_sample->file_data = (unsigned char*)AllocMem(the_audio_sample->length + 4, MEMF_FAST | MEMF_CLEAR);
        the_audio_sample->file_data = (unsigned char*)malloc(the_audio_sample->length + 4);
        //printf("filename: %s, address: %x\n", filename, the_audio_sample);
        //the_audio_sample->data =(unsigned char*) malloc( the_audio_sample->length);


        // align to 4 byte boundary...
        address = (int)the_audio_sample->file_data + 4;
        address = address & ~4;
        the_audio_sample->data = (unsigned char*)address;

        //printf("New audio sample mem: %x\n", address);

        fseek(fp, 0L, SEEK_SET);
        fread(the_audio_sample->data, the_audio_sample->length, 1, fp);
        fclose(fp);

        // swap endians, PC software used to write out 16-bit sample
        int i;
        unsigned char tempy;
        for (i = 0; i < the_audio_sample->length / 2; i++)
        {
            tempy = *(the_audio_sample->data + i * 2);
            *(the_audio_sample->data + i * 2) = *(the_audio_sample->data + i * 2 + 1);
            *(the_audio_sample->data + i * 2 + 1) = tempy;
        }
    }
    else
    {
        //printf("File error.\n");
        return -1;
    }

    the_audio_sample->bits = bitdepth;
    the_audio_sample->frequency = frequency;
    the_audio_sample->multi_track = multi_track;
    //printf(" the_audio_sample->multi_track=%d\n", the_audio_sample->multi_track);
    return 0;
}


/*
typedef enum VolType : unsigned char
{
    VOL_OFF = 0,
    VOL_0 = VOL_OFF,
    VOL_1 = 0x19,
    VOL_2 = 0x32,
    VOL_3 = 0x4C,
    VOL_4 = 0x66,
    VOL_5 = 0x80,
    VOL_6 = 0x9A,
    VOL_7 = 0xB4,
    VOL_8 = 0xCC,
    VOL_9 = 0xE6,
    VOL_10 = 0xFF,
    VOL_FULL = VOL_10
} VolType;*/

/*
THEME_PICK_ANOTHER = -2,
THEME_NONE = -1,
THEME_AIRSTRIKE,
THEME_80MX,
THEME_CHRG,
THEME_CREP,
THEME_DRIL,
THEME_DRON,
THEME_FIST,
THEME_RECON,
THEME_VOICE,
THEME_HEAVYG,
THEME_J1,
THEME_JDI_V2,
THEME_RADIO,
THEME_RAIN,
THEME_AOI,      // Act On Instinct
THEME_CCTHANG,  //	C&C Thang
THEME_DIE,      //	Die!!
THEME_FWP,      //	Fight, Win, Prevail
THEME_IND,      //	Industrial
THEME_IND2,     //	Industrial2
THEME_JUSTDOIT, //	Just Do It!
THEME_LINEFIRE, //	In The Line Of Fire
THEME_MARCH,    //	March To Your Doom
THEME_MECHMAN,  // Mechanical Man
THEME_NOMERCY,  //	No Mercy
THEME_OTP,      //	On The Prowl
THEME_PRP,      //	Prepare For Battle
THEME_ROUT,     //	Reaching Out
THEME_HEART,    //
THEME_STOPTHEM, //	Stop Them
THEME_TROUBLE,  //	Looks Like Trouble
THEME_WARFARE,  //	Warfare
THEME_BFEARED,  //	Enemies To Be Feared
THEME_IAM,      // I Am
THEME_WIN1,     //	Great Shot!
THEME_MAP1,     // Map subliminal techno "theme".
THEME_VALKYRIE, // Ride of the valkyries.

THEME_COUNT,
THEME_LAST = THEME_BFEARED,
THEME_FIRST = 0
*/

void* KI_audio_thread(void* ptr)
{
    // detach from the calling thread...
    pthread_detach(pthread_self());
    //printf("KI_audio_thread() started\n");

    while (!quit_audio_thread) 
    {
        if (signal_audio_thread_to_load)
        {
            //printf("KI_audio_thread() loading file: %s\n", (char*)file_to_load);
            if (load_sound((char *)file_to_load, 16, 22050, (AUDIO_SAMPLE *)destination_audio_sample, 0) != 0)
            {
                //printf("KI_Play_MUSIC() could not load -> %s\n", (char*)file_to_load);
               // return -1;
            }
            signal_main_thread_that_audio_has_loaded = 1;
            signal_audio_thread_to_load = 0;
        }
        else
             usleep(50000);
    }
    //pthread_exit(NULL);
    return NULL;
}

void KI_stop_all_audio()
{
    KI_Play_MUSIC(NULL, 0);

    *DMACON1 = DMAF_AUD0;
    *INTENA1 = INTF_AUD0;
    *DMACON1 = DMAF_AUD1;
    *INTENA1 = INTF_AUD1;
    *DMACON1 = DMAF_AUD2;
    *INTENA1 = INTF_AUD2;
    *DMACON1 = DMAF_AUD3;
    *INTENA1 = INTF_AUD3;
    *DMACON2 = DMAF_AUD0;
    *INTENA2 = INTF_AUD0;
    *DMACON2 = DMAF_AUD1;
    *INTENA2 = INTF_AUD1;
    *DMACON2 = DMAF_AUD2;
    *INTENA2 = INTF_AUD2;
    *DMACON2 = DMAF_AUD3;
    *INTENA2 = INTF_AUD3;
}

#else

void KI_Play_Sample_VOC(void const* sample, VocType voc, VolType volume, signed short pan_value) {}
void KI_Play_Sample_VOX(void const* sample, VoxType vox, VolType volume, signed short pan_value) {}
int KI_Play_MUSIC(char* name, int volume) {return 0;}
int get_free_audio_channel(int audio_type) {return 0;}
void play_SAGA_audio(int channel, char* buffer, long length, VolType volume, short pan_value) {}
int load_sound(char* filename, int bitdepth, int frequency, AUDIO_SAMPLE* the_audio_sample, int multi_track) {return 0;}
void* KI_audio_thread(void* ptr){}
void KI_stop_all_audio() {}

#endif