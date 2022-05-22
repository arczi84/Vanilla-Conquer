// Copyright Kenji Irie (Quetzal) 2020.
#ifdef  AMIGA
#ifndef TIMER_AMIGA_INCLUDED
    #define TIMER_AMIGA_INCLUDED
#define NULL 0

#include	<proto/exec.h>
#include	<proto/dos.h>
#define Node NodeA
//#include	<proto/intuition.h>
//#include	<proto/timer.h>
#include	<proto/graphics.h>
//    #include	<proto/picasso96.h>
//#include <cybergraphx/cybergraphics.h>
//#include	<proto/cybergraphics.h>
#define NodeA Node
#include	<intuition/screens.h>
#undef NodeA
#include    <devices/keyboard.h>
#if defined(__GNUC__)
#include	<clib/macros.h>
#endif
//#include <devices/timer.h>
#include <devices/input.h>
#include <devices/inputevent.h>
#include <clib/timer_protos.h>
//#include <inline/mathtrans_protos.h>
#include <clib/exec_protos.h>
//#include <inline/exec_protos.h>

#include	<math.h>
#include <stdio.h>
#include	<stdlib.h>


#endif

#include "timer_amiga.h"
//#ifdef AMIGA

//#include "main_amiga_headers.h" 


struct Library *TimerBase;
static struct timerequest *TimerIO=NULL;
static struct EClockVal startEClock, endEClock;

//extern struct ExecBase *SysBase;

// For keyboard control...
static struct IOStdReq *KeyIO;
static struct MsgPort *KeyMP=NULL;
unsigned char *keyMatrix;

struct timeval a, b;

int init_keyboard(void)
{
    // This isn't used?!?!
    return 0;

    
    //if ( KeyMP = AllocSysObjectTags ( ASOT_PORT, TAG_END ) )
   if(KeyMP = (struct MsgPort*)AllocMem(sizeof(struct MsgPort), MEMF_CLEAR))
   // if ( KeyMP = (struct *MsgPort)AllocMem(sizeof(structMsgPort), MEMF_CLEAR))
   {}
    else printf( "Error: Could not create message port\n" );

   return 0;
}

int init_timer(void)
{
    TimerIO=(struct timerequest *)AllocMem(sizeof(struct timerequest),
                                      MEMF_PUBLIC | MEMF_CLEAR);
    OpenDevice("timer.device", 0, (struct IORequest*)TimerIO, 0);
    //OpenDevice(TIMERNAME,UNIT_MICROHZ, (struct IORequest *)TimerIO,0);
    TimerBase = (struct Library *)TimerIO->tr_node.io_Device;
    if(TimerIO==NULL)
    {
        printf("ERROR: Timer not opened\n");
        int bob;
        scanf("Crap %d", bob);
    }
    else
         printf("Timer OK\n");
    return 0;
}

void set_keyboard_repeat_rate(unsigned long seconds)
{
   /* struct timerequest *InputTime;
    InputTime=(struct timerequest *)AllocMem(sizeof(struct timerequest),
                                      MEMF_PUBLIC | MEMF_CLEAR);
    InputTime->tr_time.tv_secs=(ULONG)seconds;
   // InputTime->Request.io_Command = IND_SETPERIOD;
     InputTime->tr_node.io_Command = IND_SETPERIOD;
    */

     TimerIO->tr_time.tv_secs=(ULONG)seconds;
    // InputTime->Request.io_Command = IND_SETPERIOD;
     TimerIO->tr_node.io_Command = IND_SETTHRESH;
     DoIO((struct IORequest *)TimerIO);
     TimerIO->tr_node.io_Command = IND_SETPERIOD;
     DoIO((struct IORequest *)TimerIO);

     printf("Set keyboard function entered.\n");
}
void close_timer(void)
{
    if(TimerIO)
        CloseDevice( (struct IORequest *) TimerIO );
  
    FreeMem(TimerIO, sizeof(struct timerequest));
    return;
}

void start_fast_timer(void)
{
    //return;

    if(TimerIO==NULL)
    {
        //printf("ERROR: TIMER==NULL\n");
        return;
    }

   if(TimerIO) 
   {
       ReadEClock (&startEClock);
   }
}

double stop_fast_timer(void) // returns the number of milliseconds passed since start_fast_timer() was called, up to ~32bits/700k, apparently.
{
    //return 0;

    ULONG eClockDiff, E_Freq;
    if(TimerIO==NULL)
    {    //printf("ERROR: TIMER==NULL\n");
        return -1;
    }
    if(TimerIO)
    {
         E_Freq=ReadEClock(&endEClock);
         eClockDiff=endEClock.ev_lo-startEClock.ev_lo; // only using lower 32 bits, so don't have to do 64-bit maths. Only works for a couple of seconds.

      //  printf("%ld\n", E_Freq);
        return (double)(eClockDiff*1000)/(double)E_Freq;
       
    }
    else
        return -1; 
}

void start_normal_timer(void)
{
    // store current time in a.
    GetSysTime(&a);
    //GetSysTime(&b);
    //printf("START -%d  %d-\n", a.tv_micro, b.tv_micro);
}

long stop_normal_timer(void)
{
    if (TimerIO == NULL)
    {
        //printf("ERROR: TIMER==NULL\n");     
    }
    GetSysTime(&b);
    SubTime(&b, &a);

    long micros = b.tv_secs + b.tv_micro;
    //printf("STOP -%d  %d- MICROSECS %d\n", a.tv_micro, b.tv_micro, micros);

    return micros;
}

#endif // AMIGA
