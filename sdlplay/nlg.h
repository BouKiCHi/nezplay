#ifndef __NLG_H__
#define __NLG_H__

int  OpenNLG(const char *file);
int CreateNLG(const char *file);

void CloseNLG(void);
int  ReadNLG(void);
long  TellNLG(void);
void SeekNLG(long);

void WriteNLG_CMD(int cmd);
void WriteNLG_CTC(int cmd,int ctc);
void WriteNLG_Data(int cmd,int addr,int data);

char *GetTitleNLG(void);
int GetTickNLG(void);
int GetLengthNLG(void);
int GetLoopPtrNLG(void);
int  GetBaseClkNLG(void);

void SetCTC0_NLG(int value);
void SetCTC3_NLG(int value);


#define CMD_PSG  0x00
#define CMD_OPM  0x01
#define CMD_OPM2 0x02
#define CMD_IRQ  0x80

#define CMD_CTC0 0x81
#define CMD_CTC3 0x82


#endif
