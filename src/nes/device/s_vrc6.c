#include "../nestypes.h"
#include "../audiosys.h"
#include "../handler.h"
#include "../nsf6502.h"
#include "../nsdout.h"
#include "logtable.h"
#include "s_vrc6.h"

#define NES_SOUND_TAG "VRC6"

#define NES_BASECYCLES (21477270)
#define CPS_SHIFT 18

typedef struct {
	Uint32 cps;
	Int32 cycles;

	Uint32 spd;

	Uint8 regs[3];
	Uint8 update;
	Uint8 adr;
	Uint8 mute;
} VRC6_SQUARE;

typedef struct {
	Uint32 cps;
	Int32 cycles;

	Uint32 spd;
	Uint32 output;

	Uint8 regs[3];
	Uint8 update;
	Uint8 adr;
	Uint8 mute;
} VRC6_SAW;

typedef struct {
	VRC6_SQUARE square[2];
	VRC6_SAW saw;
	Uint32 mastervolume;
} VRC6SOUND;

/* ------------ */
/*  VRC6 SOUND  */
/* ------------ */

static void __fastcall VRC6SoundVolume(Uint volume);

static NES_VOLUME_HANDLER s_vrc6_volume_handler[] = {
	{ VRC6SoundVolume,NES_SOUND_TAG, },
	{ 0,0, },
};


static VRC6SOUND vrc6s;

#define DEVICE VRC6
#include "assist.h"


void VRC6SetMask(char *mask)
{
    chmask = mask;
}

void VRC6SetState(S_STATE *state)
{
    chstate = state;
}


static Int32 VRC6SoundSquareRender(VRC6_SQUARE *ch)
{
	Uint32 output;
	if (ch->update)
	{
		if (ch->update & (2 | 4))
		{
			ch->spd = (((ch->regs[2] & 0x0F) << 8) + ch->regs[1] + 0) << CPS_SHIFT;
		}
		ch->update = 0;
	}

	if (!ch->spd) return 0;

	ch->cycles -= ch->cps;
	while (ch->cycles < 0)
	{
		ch->cycles += ch->spd;
		ch->adr++;
	}
	ch->adr &= 0xF;

	if (ch->mute || !(ch->regs[2] & 0x80)) return 0;

	output = LinearToLog(ch->regs[0] & 0x0F) + vrc6s.mastervolume;
	if (!(ch->regs[0] & 0x80) && (ch->adr < ((ch->regs[0] >> 4) + 1)))
	{
#if 1
		return 0;	/* and array gate */
#else
		output++;	/* negative gate */
#endif
	}
	return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 16 - 1);
}

static Int32 VRC6SoundSawRender(VRC6_SAW *ch)
{
	Uint32 output;

	if (ch->update)
	{
		if (ch->update & (2 | 4))
		{
			ch->spd = (((ch->regs[2] & 0x0F) << 8) + ch->regs[1] + 0) << CPS_SHIFT;
		}
		ch->update = 0;
	}

	if (!ch->spd) return 0;

	ch->cycles -= ch->cps;
	while (ch->cycles < 0)
	{
		ch->cycles += ch->spd;
		ch->output += (ch->regs[0] & 0x3F);
		if (7 == ++ch->adr)
		{
			ch->adr = 0;
			ch->output = 0;
		}
	}

	if (ch->mute || !(ch->regs[2] & 0x80)) return 0;

	output = LinearToLog((ch->output >> 3) & 0x1F) + vrc6s.mastervolume;
	return LogToLinear(output, LOG_LIN_BITS - LIN_BITS - 16 - 1);
}

static Int32 __fastcall VRC6SoundRender(void)
{
	Int32 accum = 0;
    if (chmask[0])
        accum += VRC6SoundSquareRender(&vrc6s.square[0]);
    if (chmask[1])
        accum += VRC6SoundSquareRender(&vrc6s.square[1]);
    if (chmask[2])
        accum += VRC6SoundSawRender(&vrc6s.saw);
    
	return VOLGAIN(accum,s_vrc6_volume_handler[0]);
}

static NES_AUDIO_HANDLER s_vrc6_audio_handler[] = {
	{ 1, VRC6SoundRender, }, 
	{ 0, 0, }, 
};

static void __fastcall VRC6SoundVolume(Uint volume)
{
	volume += 64;
	vrc6s.mastervolume = (volume << (LOG_BITS - 8)) << 1;
}

static void __fastcall VRC6SoundWrite9000(Uint address, Uint value)
{
	WRITE_LOG();

	if (NSD_out_mode) NSDWrite(NSD_VRC6, address, value);
	vrc6s.square[0].regs[address & 3] = value;
	vrc6s.square[0].update |= 1 << (address & 3);
    
    int wl =
        ((vrc6s.square[0].regs[2] & 0x0f) << 8) |
         (vrc6s.square[0].regs[1]);
    
    if (wl)
        chstate[0].freq = NES_CPUCLOCK / ((wl+1) << 4) ;
    else
        chstate[0].freq = 0;

}
static void __fastcall VRC6SoundWriteA000(Uint address, Uint value)
{
	WRITE_LOG();

	if (NSD_out_mode) NSDWrite(NSD_VRC6, address, value);
	vrc6s.square[1].regs[address & 3] = value;
	vrc6s.square[1].update |= 1 << (address & 3);

    int wl =
        ((vrc6s.square[1].regs[2] & 0x0f) << 8) |
         (vrc6s.square[1].regs[1]) ;
    if (wl)
        chstate[1].freq = NES_CPUCLOCK / ((wl+1) << 4) ;
    else
        chstate[1].freq = 0;

}
static void __fastcall VRC6SoundWriteB000(Uint address, Uint value)
{
	WRITE_LOG();

	if (NSD_out_mode) NSDWrite(NSD_VRC6, address, value);
	vrc6s.saw.regs[address & 3] = value;
	vrc6s.saw.update |= 1 << (address & 3);
    
    int wl =
    ((vrc6s.saw.regs[2] & 0x0f) << 8) |
    (vrc6s.saw.regs[1]) ;
    if (wl)
        chstate[2].freq = NES_CPUCLOCK / ((wl+1) * 14) ;
    else
        chstate[2].freq = 0;

}

static NES_WRITE_HANDLER s_vrc6_write_handler[] =
{
	{ 0x9000, 0x9002, VRC6SoundWrite9000, },
	{ 0xA000, 0xA002, VRC6SoundWriteA000, },
	{ 0xB000, 0xB002, VRC6SoundWriteB000, },
	{ 0,      0,      0, },
};

static Uint32 DivFix(Uint32 p1, Uint32 p2, Uint32 fix)
{
	Uint32 ret;
	ret = p1 / p2;
	p1  = p1 % p2;/* p1 = p1 - p2 * ret; */
	while (fix--)
	{
		p1 += p1;
		ret += ret;
		if (p1 >= p2)
		{
			p1 -= p2;
			ret++;
		}
	}
	return ret;
}

static void VRC6SoundSquareReset(VRC6_SQUARE *ch)
{
	ch->cps = DivFix(NES_BASECYCLES, 12 * NESAudioFrequencyGet(), CPS_SHIFT);
}

static void VRC6SoundSawReset(VRC6_SAW *ch)
{
	ch->cps = DivFix(NES_BASECYCLES, 24 * NESAudioFrequencyGet(), CPS_SHIFT);
}

static void __fastcall VRC6SoundReset(void)
{
	XMEMSET(&vrc6s, 0, sizeof(VRC6SOUND));
	VRC6SoundSquareReset(&vrc6s.square[0]);
	VRC6SoundSquareReset(&vrc6s.square[1]);
	VRC6SoundSawReset(&vrc6s.saw);
}

static NES_RESET_HANDLER s_vrc6_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, VRC6SoundReset, }, 
	{ 0,                   0, }, 
};

void VRC6SoundInstall(void)
{
	LogTableInitialize();
	NESAudioHandlerInstall(s_vrc6_audio_handler);
	NESVolumeHandlerInstall(s_vrc6_volume_handler);
	NESWriteHandlerInstall(s_vrc6_write_handler);
	NESResetHandlerInstall(s_vrc6_reset_handler);
}
