#define CUSTOM_REGBASE          0xdff000
#define CUSTOM_DMACON           (CUSTOM_REGBASE + 0x096)
#define CUSTOM_INTENA           (CUSTOM_REGBASE + 0x09a)
#define CUSTOM_INTENAR           (CUSTOM_REGBASE + 0x01c)

#define CUSTOM_INTREQ          (CUSTOM_REGBASE + 0x09c)
#define CUSTOM_INTREQR           (CUSTOM_REGBASE + 0x01e)


#define CUSTOM_DMACON2          (CUSTOM_DMACON  + 0x200)
#define CUSTOM_INTENA2          (CUSTOM_INTENA  + 0x200)

#define AUDIO_MODEF_16          (1<<0)
#define AUDIO_MODEF_ONESHOT     (1<<1)

#define  INTF_SETCLR	(1L<<15)
#define  INTF_INTEN	(1L<<14)
#define  INTF_EXTER	(1L<<13)
#define  INTF_DSKSYNC	(1L<<12)
#define  INTF_RBF	(1L<<11)
#define  INTF_AUD3	(1L<<10)
#define  INTF_AUD2	(1L<<9)
#define  INTF_AUD1	(1L<<8)
#define  INTF_AUD0	(1L<<7)
#define  INTF_BLIT	(1L<<6)
#define  INTF_VERTB	(1L<<5)
#define  INTF_COPER	(1L<<4)
#define  INTF_PORTS	(1L<<3)
#define  INTF_SOFTINT	(1L<<2)
#define  INTF_DSKBLK	(1L<<1)
#define  INTF_TBE	(1L<<0)

/* write definitions for dmaconw */
#define DMAF_SETCLR  0x8000
#define DMAF_AUDIO   0x000F   /* 4 bit mask */
#define DMAF_AUD0    0x0001
#define DMAF_AUD1    0x0002
#define DMAF_AUD2    0x0004
#define DMAF_AUD3    0x0008
#define DMAF_DISK    0x0010
#define DMAF_SPRITE  0x0020
#define DMAF_BLITTER 0x0040
#define DMAF_COPPER  0x0080
#define DMAF_RASTER  0x0100
#define DMAF_MASTER  0x0200
#define DMAF_BLITHOG 0x0400
#define DMAF_ALL     0x01FF   /* all dma channels */

unsigned short *DMACON1 = (unsigned short*)CUSTOM_DMACON;
unsigned short *DMACON2 = (unsigned short*)CUSTOM_DMACON2;

//unsigned short *PAMELACON = (unsigned short*)0xDFF29E;		//Pamela Control to play 8 or 16-bit data

unsigned long *AUD0LCH = (unsigned long*)0xdff400;		//Pointer to audio data
unsigned short *AUD0LEN = (unsigned short*)0xdff404;		//Length in WORDs of the audio data
unsigned short *AUD0LEN2 = (unsigned short*)0xdff406;		//Length in WORDs of the audio data
unsigned short *AUD0PER = (unsigned short*)0xdff40c;		//Period time to play the audio data
unsigned short *AUD0VOL = (unsigned short*)0xdff408;
unsigned short *AUD0MODE = (unsigned short*)0xdff40a;

/*unsigned long *AUD0LCH = (unsigned long*)0xdff0a0;		//Pointer to audio data
unsigned short *AUD0LEN = (unsigned short*)0xdff0a4;		//Length in WORDs of the audio data
unsigned short *AUD0PER = (unsigned short*)0xdff0a6;		//Period time to play the audio data
unsigned short *AUD0VOL = (unsigned short*)0xdff0a8;
unsigned short *AUD0VOL2 = (unsigned short*)0xdff0ac;
unsigned short *AUD0LEN2 = (unsigned short*)0xdff0ae;
*/

unsigned long *AUD1LCH = (unsigned long*)0xdff410;		//Pointer to audio data
unsigned short *AUD1LEN = (unsigned short*)0xdff414;		//Length in WORDs of the audio data
unsigned short *AUD1LEN2 = (unsigned short*)0xdff416;		//Length in WORDs of the audio data
unsigned short *AUD1PER = (unsigned short*)0xdff41c;		//Period time to play the audio data
unsigned short *AUD1VOL = (unsigned short*)0xdff418;
unsigned short *AUD1MODE = (unsigned short*)0xdff41a;

unsigned long *AUD2LCH = (unsigned long*)0xdff420;		//Pointer to audio data
unsigned short *AUD2LEN = (unsigned short*)0xdff424;		//Length in WORDs of the audio data
unsigned short *AUD2LEN2 = (unsigned short*)0xdff426;		//Length in WORDs of the audio data
unsigned short *AUD2PER = (unsigned short*)0xdff42c;		//Period time to play the audio data
unsigned short *AUD2VOL = (unsigned short*)0xdff428;
unsigned short *AUD2MODE = (unsigned short*)0xdff42a;

unsigned long *AUD3LCH = (unsigned long*)0xdff430;		//Pointer to audio data
unsigned short *AUD3LEN = (unsigned short*)0xdff434;		//Length in WORDs of the audio data
unsigned short *AUD3LEN2 = (unsigned short*)0xdff436;		//Length in WORDs of the audio data
unsigned short *AUD3PER = (unsigned short*)0xdff43c;		//Period time to play the audio data
unsigned short *AUD3VOL = (unsigned short*)0xdff438;
unsigned short *AUD3MODE = (unsigned short*)0xdff43a;

unsigned long* AUD4LCH = (unsigned long*)0xdff440;		//Pointer to audio data
unsigned short* AUD4LEN = (unsigned short*)0xdff444;		//Length in WORDs of the audio data
unsigned short* AUD4LEN2 = (unsigned short*)0xdff446;		//Length in WORDs of the audio data
unsigned short* AUD4PER = (unsigned short*)0xdff44c;		//Period time to play the audio data
unsigned short* AUD4VOL = (unsigned short*)0xdff448;
unsigned short* AUD4MODE = (unsigned short*)0xdff44a;

unsigned long *AUD5LCH = (unsigned long*)0xdff450;		//Pointer to audio data
unsigned short *AUD5LEN = (unsigned short*)0xdff454;		//Length in WORDs of the audio data
unsigned short* AUD5LEN2 = (unsigned short*)0xdff456;		//Length in WORDs of the audio data
unsigned short *AUD5PER = (unsigned short*)0xdff45c;		//Period time to play the audio data
unsigned short *AUD5VOL = (unsigned short*)0xdff458;
unsigned short* AUD5MODE = (unsigned short*)0xdff45a;

unsigned long*  AUD6LCH = (unsigned long*)0xdff460;		//Pointer to audio data
unsigned short* AUD6LEN = (unsigned short*)0xdff464;		//Length in WORDs of the audio data
unsigned short* AUD6LEN2 = (unsigned short*)0xdff466;		//Length in WORDs of the audio data
unsigned short* AUD6PER = (unsigned short*)0xdff46c;		//Period time to play the audio data
unsigned short* AUD6VOL = (unsigned short*)0xdff468;
unsigned short* AUD6MODE = (unsigned short*)0xdff46a;

unsigned long*  AUD7LCH = (unsigned long*)0xdff470;		//Pointer to audio data
unsigned short* AUD7LEN = (unsigned short*)0xdff474;		//Length in WORDs of the audio data
unsigned short* AUD7LEN2 = (unsigned short*)0xdff476;		//Length in WORDs of the audio data
unsigned short* AUD7PER = (unsigned short*)0xdff47c;		//Period time to play the audio data
unsigned short* AUD7VOL = (unsigned short*)0xdff478;
unsigned short* AUD7MODE = (unsigned short*)0xdff47a;

unsigned short *INTENA1 = (unsigned short*)CUSTOM_INTENA;
unsigned short* INTENAR1 = (unsigned short*)CUSTOM_INTENAR;
unsigned short *INTREQ = (unsigned short*)CUSTOM_INTREQ;
unsigned short* INTREQR = (unsigned short*)CUSTOM_INTREQR;

unsigned short *INTENA2 = (unsigned short*)CUSTOM_INTENA2; // PAMELA AUDIO
unsigned short *INTREQ2 = (unsigned short*)0xdff29C;






// DFF4_A = (W) MODE (Bit0=16bit, Bit1=OneShot)
// sndBank[ channel ].ac_vol = ( UWORD  ) ( vol2 << 8 ) | vol1; // max = 128.128
