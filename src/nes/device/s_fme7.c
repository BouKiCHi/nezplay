#include "../nestypes.h"
#include "../audiosys.h"
#include "../handler.h"
#include "../nsf6502.h"
#include "../nsdout.h"
#include "logtable.h"
#include "s_fme7.h"
#include "s_psg.h"

#define NES_SOUND_TAG "FME7"

#define BASECYCLES_ZX  (3579545)/*(1773400)*/
#define BASECYCLES_AMSTRAD  (2000000)
#define BASECYCLES_MSX (3579545)
#define BASECYCLES_NES (21477270)

typedef struct {
	KMIF_SOUND_DEVICE *psgp;
	Uint8 adr;
} PSGSOUND;



static void __fastcall PSGSoundVolume(Uint volume);

static NES_VOLUME_HANDLER s_psg_volume_handler[] = {
	{ PSGSoundVolume, NES_SOUND_TAG, },
	{ 0, 0, },
};

static PSGSOUND psgs = { 0, 0 };

static Int32 __fastcall PSGSoundRender(void)
{
	Int32 b[2] = {0, 0};
	psgs.psgp->synth(psgs.psgp->ctx, b);
	return VOLGAIN(b[0], s_psg_volume_handler[0]);
}

static void __fastcall PSGSoundRender2(Int32 *d)
{
	Int32 b[2] = {0, 0};
	psgs.psgp->synth(psgs.psgp->ctx, b);
	VOLGAIN2(d, b, s_psg_volume_handler[0]);
}

static NES_AUDIO_HANDLER s_psg_audio_handler[] = {
	{ 3, PSGSoundRender, PSGSoundRender2, }, 
	{ 0, 0, 0, }, 
};

static void __fastcall PSGSoundVolume(Uint volume)
{
	psgs.psgp->volume(psgs.psgp->ctx, volume);
}


static Uint __fastcall PSGSoundReadData(Uint address)
{
	return psgs.psgp->read(psgs.psgp->ctx, 0);
}

static void __fastcall PSGSoundWrireAddr(Uint address, Uint value)
{
	WRITE_LOG();

	psgs.adr = value;
	psgs.psgp->write(psgs.psgp->ctx, 0, value);
}
static void __fastcall PSGSoundWrireData(Uint address, Uint value)
{
	WRITE_LOG();

	if (NSD_out_mode) NSDWrite(NSD_FME7, psgs.adr, value);
	psgs.psgp->write(psgs.psgp->ctx, 1, value);
}

static NES_WRITE_HANDLER s_fme7_write_handler[] =
{
	{ 0xC000, 0xC000, PSGSoundWrireAddr, },
	{ 0xE000, 0xE000, PSGSoundWrireData, },
	{ 0,      0,      0, },
};

static void __fastcall FME7SoundReset(void)
{
	psgs.psgp->reset(psgs.psgp->ctx, BASECYCLES_NES / 12, NESAudioFrequencyGet());
}

static NES_RESET_HANDLER s_fme7_reset_handler[] = {
	{ NES_RESET_SYS_NOMAL, FME7SoundReset, }, 
	{ 0,                   0, }, 
};

static void __fastcall PSGSoundTerm(void)
{
	if (psgs.psgp)
	{
		psgs.psgp->release(psgs.psgp->ctx);
		psgs.psgp = 0;
	}
}

static NES_TERMINATE_HANDLER s_psg_terminate_handler[] = {
	{ PSGSoundTerm, }, 
	{ 0, }, 
};

void FME7SetMask(char *mask)
{
    if (psgs.psgp)
        psgs.psgp->setmask(psgs.psgp->ctx, 0, mask);
}


void FME7SetState(S_STATE *state)
{
    if (psgs.psgp)
        psgs.psgp->setchstate(psgs.psgp->ctx, 0, state);
}


void FME7SoundInstall(void)
{
	psgs.psgp = PSGSoundAlloc(PSG_TYPE_AY_3_8910);
	if (!psgs.psgp) return;

	LogTableInitialize();
	NESTerminateHandlerInstall(s_psg_terminate_handler);
	NESVolumeHandlerInstall(s_psg_volume_handler);

	NESAudioHandlerInstall(s_psg_audio_handler);
	NESWriteHandlerInstall(s_fme7_write_handler);
	NESResetHandlerInstall(s_fme7_reset_handler);
}
