#ifndef AUDIOSYS_H__
#define AUDIOSYS_H__

#include <stdio.h>

#include "nestypes.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (__fastcall *AUDIOHANDLER2)(Int32 *p);
typedef Int32 (__fastcall *AUDIOHANDLER)(void);
typedef struct NES_AUDIO_HANDLER_TAG {
	Uint fMode;
	AUDIOHANDLER Proc;
	AUDIOHANDLER2 Proc2;
	struct NES_AUDIO_HANDLER_TAG *next;
} NES_AUDIO_HANDLER;

typedef void (__fastcall *VOLUMEHANDLER)(Uint volume);
typedef struct NES_VOLUME_HANDLER_TAG {
	VOLUMEHANDLER Proc;
	char *name;
	float volume;
	struct NES_VOLUME_HANDLER_TAG *next;
} NES_VOLUME_HANDLER;

enum
{
   NES_AUDIO_FILTER_NONE,
   NES_AUDIO_FILTER_LOWPASS,
   NES_AUDIO_FILTER_WEIGHTED
};
    
#include "nezglue_x.h"

/* externs */
    
extern FILE *nes_logfile;

extern int nes_debug_mode;
extern int nes_strict_mode;

extern int nes_debug_pause;
extern int nes_debug_step;
extern int nes_debug_frame_step;
extern int nes_debug_bp;
    
typedef struct
{
    int n163_oldmode;
} NES_DEVICE_INTERFACE;
    
extern NES_DEVICE_INTERFACE nes_devif;
    
/* Debug */
typedef Uint (*RDFUNC)(Uint A);
typedef void (*SEEKFUNC)(Uint A);
typedef int (*INTFUNC)(void);
typedef float (*FLOATFUNC)(void);

enum RDREG_TAG
{
    RDREG_PC,
    RDREG_A,
    RDREG_X,
    RDREG_Y,
    RDREG_S,
    RDREG_P
};  

typedef struct
{
    RDFUNC rdmem;
    RDFUNC rdreg;
    SEEKFUNC seek;
    INTFUNC frame;
    FLOATFUNC fps;

    INTFUNC fcycle;
    INTFUNC avg;
    INTFUNC max;
    INTFUNC maxpos;

    RDFUNC rdlinear;
} nes_funcs_t;

void NESSetHandlers(nes_funcs_t *pfuncs);
void NESSetLinearHandler(RDFUNC plinear);
    
Uint NESReadLinearMemory(Uint A);
Uint NESReadMemory(Uint A);    
Uint NESReadRegister(Uint A);
void NESSeekFrame(Uint A);
int  NESGetFrame(void);
int  NESGetFramePerSeconds(void);
int  NESGetFrameCycle(void);
int NESGetAvgCycle(void);
int NESGetMaxCycle(void);
int NESGetMaxPosition(void);
    
    
void NESDebugPause(void);
void NESDebugRun(void);
void NESDebugStep(void);
void NESDebugFrameStep(void);
void NESDebugSetBP(int address);

void NESAudioRender(Int16 *bufp, Uint buflen);
void NESAudioHandlerInstall(NES_AUDIO_HANDLER *ph);
void NESAudioFrequencySet(Uint freq);
Uint NESAudioFrequencyGet(void);
void NESAudioChannelSet(Uint ch);
Uint NESAudioChannelGet(void);
void NESAudioHandlerInitialize(void);
void NESVolumeHandlerInstall(NES_VOLUME_HANDLER *ph);
void NESVolume(Uint volume);

float NESGetDeviceVolume(const char *name);
void NESSetDeviceVolume(const char *name,float volume);
void NESSetAllDeviceVolume(float volume);
	
void NESAudioFilterSet(Uint filter);

void NESAudioOpenLog(const char *filename);
void NESAudioCloseLog(void);

    
void NESAudioSetDebug(int level);
void NESAudioSetStrict(int level);

#define WRITE_LOG()  if (nes_logfile) \
	fprintf(nes_logfile,"%s:%04X:%02X\n",NES_SOUND_TAG, address, value)

#define VOLGAIN(data,vh) (Int32)(vh.volume * data)

#define VOLGAIN2(d, b, vh) \
 d[0] = d[0] + (Int32)(vh.volume * b[0]); \
 d[1] = d[1] + (Int32)(vh.volume * b[1])


#ifdef __cplusplus
}
#endif

#endif /* AUDIOSYS_H__ */
