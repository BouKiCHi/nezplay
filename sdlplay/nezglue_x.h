#ifndef __NEZGLUE_X__H
#define __NEZGLUE_X__H

#include <stdio.h>

// direct access

typedef struct
{
    char  mask;
    float freq;
    int   vol;
} S_STATE;

typedef struct
{
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
    
    unsigned char psg_reg[0x100];
    unsigned char opm1_reg[0x100];
    unsigned char opm2_reg[0x100];
    
    unsigned char  psg_adr;
    unsigned char  opm_adr[2];
    FILE *nlg_fp;
    
} NSF_STATE;

extern NSF_STATE nsf_state;


#endif
