#ifndef __NEZGLUE_X__H
#define __NEZGLUE_X__H

#include <stdio.h>
#include "nlg.h"

#ifdef __ANDROID__

void output_log(const char *format, ...);

#define OUTPUT_LOG(...) output_log(__VA_ARGS__)

#else

#define OUTPUT_LOG(...) {}

#endif

#define MASKSET(x) XMEMSET(x, 1, sizeof(x))
#define ZEROSET(x) XMEMSET(x, 0, sizeof(x))

// direct access

typedef struct
{
    char  mask;
    float freq;
    int   vol;
} S_STATE;

#define MODE_UNKNOWN 0
#define MODE_NSF 1
#define MODE_KSS 2
#define MODE_NRT 3


typedef struct
{
    int mode;
    
    int use_ch;
    
    int freq[32];
    int vol[32];
    int pan[32];

    int mute[32];

    char apu_mask[8];
    char fds_mask[8];
    char fme7_mask[8];
    char mmc5_mask[8];
    char n106_mask[8];
    char vrc6_mask[8];
    char vrc7_mask[12];
    
    S_STATE apu_state[8];
    S_STATE fds_state[8];
    S_STATE fme7_state[8];
    S_STATE mmc5_state[8];
    S_STATE n106_state[8];
    S_STATE vrc6_state[8];
    S_STATE vrc7_state[12];    

    char opm_mask[8];
    char opm2_mask[8];
    char psg_mask[8];
    
    unsigned char psg_reg[0x100];
    unsigned char opm1_reg[0x100];
    unsigned char opm2_reg[0x100];
    
    unsigned char  psg_adr;
    unsigned char  opm_adr[2];
    NLGCTX *nlgctx;
} NSF_STATE;

extern NSF_STATE nsf_state;

/* Debug */
enum REG_TAG
{
    REG_PC,
    REG_A,
    REG_X,
    REG_Y,
    REG_S,
    REG_P,
    
    REG_Z80_PC,
    REG_Z80_AF,
    REG_Z80_BC,
    REG_Z80_DE,
    REG_Z80_HL,
    REG_Z80_SP
};


#endif
