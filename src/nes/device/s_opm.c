
#include "../audiosys.h"

#include "kmsnddev.h"
#include "divfix.h"
#include "s_opm.h"
#include "s_logtbl.h"


#include "ym2151/ym2151.h"

#include <stdio.h>

#define CPS_SHIFT 18
#define CPS_ENVSHIFT 12

typedef struct
{
	KMIF_SOUND_DEVICE kmif;

	struct {
		Int32 mastervolume;
		Uint32 davolume;
		Uint32 envout;
		Uint8 daenable;
		Uint8 regs[1];
		Uint8 rngout;
		Uint8 adr;
	} common;
	Uint8 type;
	
	Uint8 psg_adr;
	Uint8 opm_adr[2];

	void *opm_p[2];
	char *mask[2];
	
} OPMSOUND;


// #define VOL_OPM 128
#define VOL_OPM 128

static void sndsynth(void *p, Int32 *dest)
{
	int i;
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
	SAMP *bufp[2];
	SAMP buf[2];
	
	bufp[0] = buf;
	bufp[1] = buf+1;

	for(i=0; i < 2; i++)
	{
		YM2151UpdateOne(sndp->opm_p[i],bufp,1);
		dest[0] += (buf[0] * VOL_OPM);
		dest[1] += (buf[1] * VOL_OPM);
	}
}

static void sndvolume(void *p, Int32 volume)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);

	volume = (volume << (LOG_BITS - 8)) << 1;
	sndp->common.mastervolume = volume;
}


static Uint32 sndread(void *p, Uint32 a)
{
	// printf("a:%04x\n",a);

	return 0;
}

static void sndwrite(void *p, Uint32 a, Uint32 v)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);

	int chip = a / 8;
		
	switch( a & 0x17 )
	{
		// OPM
		case 0x00:
			sndp->opm_adr[chip] = v;
		break;
		case 0x01:
			if (nes_logfile)
				fprintf(nes_logfile,"OPM:%d:%02X:%02X\n",
					chip, sndp->opm_adr[chip], v);

			YM2151WriteReg( sndp->opm_p[chip] , sndp->opm_adr[chip] , v );
		break;
	}
}

static void sndreset(void *p, Uint32 clock, Uint32 freq)
{
	int i;
	int bc = 4000000;
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
	// printf("clk:%d freq:%d\n", clock, freq);
	
	for ( i = 0; i < 2; i++ )
	{
		if ( sndp->opm_p[i] )
		{
			YM2151Shutdown( sndp->opm_p[i] );
			sndp->opm_p[i] = NULL;
		}
		
		sndp->opm_p[i] = YM2151Init(1, bc, freq);
		YM2151ResetChip( sndp->opm_p[i]  );
		
		if (sndp->mask[i])
			YM2151SetMask(sndp->opm_p[i], sndp->mask[i]);

	}
}

static void sndrelease(void *p)
{
	OPMSOUND *sndp = (OPMSOUND *)(p);
	
	int i;
	
	for ( i = 0; i < 2; i++ )
	{
		if ( sndp->opm_p[i] )
		{
			YM2151Shutdown( sndp->opm_p[i] );
			sndp->opm_p[i] = NULL;
		}
	}

	XFREE(sndp);
}

static void setmask(void *p, int dev, char *mask)
{
    OPMSOUND *sndp = (OPMSOUND *)(p);
    
	if (sndp->opm_p[dev])
	{
		YM2151SetMask(sndp->opm_p[dev], mask);
	}
	sndp->mask[dev] = mask;
}

static void setinst(void *ctx, Uint32 n, void *p, Uint32 l){}

KMIF_SOUND_DEVICE *OPMSoundAlloc(void)
{
	OPMSOUND *sndp;
	sndp = (OPMSOUND *)(XMALLOC(sizeof(OPMSOUND)));
	if (!sndp) return 0;

	sndp = (OPMSOUND *)(XMEMSET(sndp,0,sizeof(OPMSOUND)));


	sndp->kmif.ctx = sndp;
	sndp->kmif.release = sndrelease;
	sndp->kmif.reset = sndreset;
	sndp->kmif.synth = sndsynth;
	sndp->kmif.volume = sndvolume;
	sndp->kmif.write = sndwrite;
	sndp->kmif.read = sndread;
	sndp->kmif.setinst = setinst;
    sndp->kmif.setmask = setmask;
	
	return &sndp->kmif;
}
