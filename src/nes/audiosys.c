#include "audiosys.h"

/* ---------------------- */
/*  Audio Render Handler  */
/* ---------------------- */

#define SHIFT_BITS 8

static Uint frequency = 44100;
static Uint channel = 1;

static NES_AUDIO_HANDLER *nah = 0;
static NES_VOLUME_HANDLER *nvh = 0;

int nes_debug_pause = 0;
int nes_debug_step = 0;
int nes_debug_frame_step = 0;
int nes_debug_bp = -1;


int nes_debug_mode = 0;
int nes_strict_mode = 1;

FILE *nes_logfile = NULL;

static Uint naf_type = NES_AUDIO_FILTER_LOWPASS;
static Uint32 naf_prev[2] = { 0x8000, 0x8000 };

NES_DEVICE_INTERFACE nes_devif;


void NESAudioSetDebug(int level)
{
	nes_debug_mode = level;
}

void NESAudioSetStrict(int level)
{
	nes_strict_mode = level;
}


void NESAudioOpenLog(const char *filename)
{
	nes_logfile = fopen(filename,"w");
}

void NESAudioCloseLog(void)
{
	if (nes_logfile)
		fclose(nes_logfile);
		
	nes_logfile = NULL;
}

void NESAudioFilterSet(Uint filter)
{
	naf_type = filter;
	naf_prev[0] = 0x8000;
	naf_prev[1] = 0x8000;
}

void NESAudioRender(Int16 *bufp, Uint buflen)
{
	Uint maxch = NESAudioChannelGet();
	while (buflen--)
	{
		NES_AUDIO_HANDLER *ph;
		Int32 accum[2] = { 0, 0 };
		Uint ch;

		for (ph = nah; ph; ph = ph->next)
		{
			if (!(ph->fMode & 1) || bufp)
			{
				if (channel == 2 && ph->fMode & 2)
				{
					ph->Proc2(accum);
				}
				else
				{
					Int32 devout = ph->Proc();
					accum[0] += devout;
					accum[1] += devout;
				}
			}
		}

		if (bufp)
		{
			for (ch = 0; ch < maxch; ch++)
			{
				Uint32 output[2];
				accum[ch] += (0x8000 << SHIFT_BITS);
				if (accum[ch] < 0)
					output[ch] = 0;
				else if (accum[ch] > (0x10000 << SHIFT_BITS) - 1)
					output[ch] = (0x10000 << SHIFT_BITS) - 1;
				else
					output[ch] = accum[ch];
				output[ch] >>= SHIFT_BITS;
				switch (naf_type)
				{
					case NES_AUDIO_FILTER_LOWPASS:
						{
							Uint32 prev = naf_prev[ch];
							naf_prev[ch] = output[ch];
							output[ch] = (output[ch] + prev) >> 1;
						}
						break;
					case NES_AUDIO_FILTER_WEIGHTED:
						{
							Uint32 prev = naf_prev[ch];
							naf_prev[ch] = output[ch];
							output[ch] = (output[ch] + output[ch] + output[ch] + prev) >> 2;
						}
						break;
				}
				*bufp++ = ((Int32)output[ch]) - 0x8000;
			}
		}
	}
}

void NESVolume(Uint volume)
{
	NES_VOLUME_HANDLER *ph;
	for (ph = nvh; ph; ph = ph->next)
		ph->Proc(volume);
}

float NESGetDeviceVolume(const char *name)
{
	NES_VOLUME_HANDLER *ph;
	for (ph = nvh; ph; ph = ph->next)
	{
		if ( strcmp(ph->name, name) == 0)
		{
			return ph->volume;
		}
	}
	return -1;
}


void NESSetAllDeviceVolume(float volume)
{
	NES_VOLUME_HANDLER *ph;
	for (ph = nvh; ph; ph = ph->next)
	{
			ph->volume = volume;
	}
}

void NESSetDeviceVolume(const char *name, float volume)
{
	NES_VOLUME_HANDLER *ph;
	for (ph = nvh; ph; ph = ph->next)
	{
		if ( strcmp(ph->name, name) == 0)
		{
			ph->volume = volume;
		}
	}
}

static void NESAudioHandlerInstallOne(NES_AUDIO_HANDLER *ph)
{
	/* Add to tail of list*/
	ph->next = 0;
	if (nah)
	{
		NES_AUDIO_HANDLER *p = nah;
		while (p->next) p = p->next;
		p->next = ph;
	}
	else
	{
		nah = ph;
	}
}
void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph)
{
	for (;(ph->fMode&2)?(!!ph->Proc2):(!!ph->Proc);ph++) NESAudioHandlerInstallOne(ph);
}
void NESVolumeHandlerInstall(NES_VOLUME_HANDLER *ph)
{
	for (;ph->Proc;ph++)
	{
		/* Add to top of list*/
		ph->volume = 1;
		ph->next = nvh;
		nvh = ph;
	}
}

/* 
 Debug
*/

nes_funcs_t nes_funcs =
{
    NULL,NULL,NULL,NULL,
    NULL,NULL,NULL,NULL,
    NULL
};

void NESSetHandlers(nes_funcs_t *pfuncs)
{
    if (pfuncs)
        nes_funcs = *pfuncs;
    else
        memset(&nes_funcs,0,sizeof(nes_funcs));
}

void NESSetLinearHandler(RDFUNC plinear)
{
    nes_funcs.rdlinear = plinear;
}

Uint NESReadLinearMemory(Uint A)
{
    if (nes_funcs.rdlinear)
        return nes_funcs.rdlinear(A);
    return 0;
}

Uint NESReadMemory(Uint A)
{
    if (nes_funcs.rdmem)
        return nes_funcs.rdmem(A);
    return 0;
}


Uint NESReadRegister(Uint A)
{
    if (nes_funcs.rdreg)
        return nes_funcs.rdreg(A);
    return 0;
}

void NESSeekFrame(Uint A)
{
    if (nes_funcs.seek)
        nes_funcs.seek(A);
}

int NESGetFrame(void)
{
    if (nes_funcs.frame)
        return nes_funcs.frame();
    return -1;
}

int NESGetFramePerSeconds(void)
{
    if (nes_funcs.fps)
        return nes_funcs.fps();
    return -1;
}

int NESGetAvgCycle(void)
{
    if (nes_funcs.avg)
        return nes_funcs.avg();
    return -1;
}

int NESGetMaxCycle(void)
{
    if (nes_funcs.max)
        return nes_funcs.max();
    return -1;
}

int NESGetMaxPosition(void)
{
    if (nes_funcs.maxpos)
        return nes_funcs.maxpos();
    return -1;
}


int NESGetFrameCycle(void)
{
    if (nes_funcs.fcycle)
        return nes_funcs.fcycle();
    return -1;
}


void NESDebugPause(void)
{
    nes_debug_pause = 1;
    nes_debug_bp = -1;
}

void NESDebugRun(void)
{
    nes_debug_pause = 0;
}

void NESDebugStep(void)
{
    nes_debug_step = 1;
}

void NESDebugFrameStep(void)
{
    nes_debug_frame_step = 1;
}

void NESDebugSetBP(int address)
{
    nes_debug_bp = address;
}

void NESAudioHandlerInitialize(void)
{
	nah = 0;
	nvh = 0;
}

void NESAudioFrequencySet(Uint freq)
{
	frequency = freq;
}
Uint NESAudioFrequencyGet(void)
{
	return frequency;
}

void NESAudioChannelSet(Uint ch)
{
	channel = ch;
}
Uint NESAudioChannelGet(void)
{
	return channel;
}
