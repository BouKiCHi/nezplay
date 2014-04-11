/*********
 NLG.C by BouKiCHi 2013-2014
 2013-xx-xx 開始
 2014-02-11 書き込み機能を追加(NLG 1.10)
 ********/

#include <stdio.h>
#include <string.h>

#include "nlg.h"

#define NLG_VER (110)
#define NLG_BASECLK (4000000)

FILE *nlg_file;

char nlg_title[80];
int  nlg_baseclk;
int  nlg_tick;
int  nlg_length;
int  nlg_loop_ptr;
int  nlg_version;

int  nlg_ctc0;
int  nlg_ctc3;


typedef unsigned char  byte;
typedef unsigned short word;
typedef unsigned long  dword;


// 変数書き出し(WORD)
void WriteWORD(byte *p,word val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
}

// 変数書き出し(DWORD)
void WriteDWORD(byte *p,dword val)
{
    p[0] = (val & 0xff);
    p[1] = ((val>>8) & 0xff);
    p[2] = ((val>>16) & 0xff);
    p[3] = ((val>>24) & 0xff);
}

// 変数読み出し(WORD)
word ReadWORD(byte *p)
{
	return
		((word)p[0]) |
		((word)p[1])<<8;
}

// 変数読み出し(DWORD)
dword ReadDWORD(byte *p)
{
	return
		((dword)p[0]) |
		((dword)p[1])<<8 |
		((dword)p[2])<<16 |
		((dword)p[3])<<24;
}

// NLGファイルを開く
int OpenNLG(const char *file)
{
	byte hdr[0x80];
    
	nlg_file = fopen(file,"rb");
	
    if (!nlg_file)
	{
		printf("File open error:%s\n", file);
		return -1;
	}
    
	fread(hdr, 0x60, 1, nlg_file);
	
	if (memcmp(hdr,"NLG1",4) != 0)
	{
		printf("Unknown format!\n");
		fclose(nlg_file);
		return -1;
	}
	
	nlg_version = ReadWORD(hdr + 4);

	memcpy(nlg_title, hdr + 8, 64);
	nlg_title[64]=0;

	nlg_baseclk = (int)ReadDWORD(hdr + 72);
	nlg_tick = (int)ReadDWORD(hdr + 76);

	nlg_length = (int)ReadDWORD(hdr + 88);
	nlg_loop_ptr = (int)ReadDWORD(hdr + 92);
	
	fseek(nlg_file, 0x60, SEEK_SET);
    
    nlg_ctc0 = nlg_ctc3 = 0;
	
	return 0;
}


// 書き込み用NLGファイルを開く
int CreateNLG(const char *file)
{
	byte hdr[0x80];
    
	nlg_file = fopen(file, "wb");
	
    if (!nlg_file)
	{
		printf("File open error:%s\n", file);
		return -1;
	}
    
    memset(hdr, 0, 0x80);
    memcpy(hdr,"NLG1",4);
    
    WriteWORD(hdr + 4, NLG_VER); // Version
    WriteDWORD(hdr + 72, NLG_BASECLK); // BaseCLK
    WriteDWORD(hdr + 76, 0); // Tick
    WriteDWORD(hdr + 88, 0); // Length
    WriteDWORD(hdr + 92, 0); // Loop Pointer
	
    fwrite(hdr, 0x60, 1, nlg_file);

    nlg_ctc0 = nlg_ctc3 = 0;

	return 0;
}

// ファイルを閉じる
void CloseNLG(void)
{
    if (!nlg_file)
        return;

	fclose(nlg_file);

    nlg_file = NULL;
}

// コマンドの書き込み
void WriteNLG_CMD(int cmd)
{
    if (!nlg_file)
        return;
    
    fputc(cmd, nlg_file);
}

// CTC定数の書き込み
void WriteNLG_CTC(int cmd,int ctc)
{
    if (!nlg_file)
        return;

    fputc(cmd, nlg_file);
    fputc(ctc, nlg_file);
}

// データの書き込み
void WriteNLG_Data(int cmd,int addr,int data)
{
    if (!nlg_file)
        return;

    fputc(cmd, nlg_file);
    fputc(addr, nlg_file);
    fputc(data, nlg_file);
}

// データの読み出し
int ReadNLG(void)
{
	return fgetc(nlg_file);
}

// ファイルポインタの位置を取得
long TellNLG(void)
{
	return ftell(nlg_file);
}

// ファイルポインタの位置を設定
void SeekNLG(long pos)
{
	fseek(nlg_file, pos, SEEK_SET);
}

// タイトルの取得
char *GetTitleNLG(void)
{
	return nlg_title;
}

// ティックの取得
int GetTickNLG(void)
{
	return nlg_tick;
}

// CTC0値の設定
void SetCTC0_NLG(int value)
{
    nlg_ctc0 = value;
    nlg_tick = ((nlg_ctc0 * 256) * nlg_ctc3);
}

// CTC3値の設定
void SetCTC3_NLG(int value)
{
    nlg_ctc3 = value;
    nlg_tick = ((nlg_ctc0 * 256) * nlg_ctc3);
}

// 曲の長さを得る
int GetLengthNLG(void)
{
	return nlg_length;
}

// ループポインタを得る
int GetLoopPtrNLG(void)
{
	return nlg_loop_ptr;
}

// ベースクロックを得る
int GetBaseClkNLG(void)
{
	return nlg_baseclk;
}


#ifdef TEST
// テスト用

int main(int argc,char *argv[])
{
    if (argc < 2)
    {
        printf("nlgtest <file>\n");
        return -1;
    }
    
	if (OpenNLG(argv[1]) < 0)
		return -1;
	
	printf("Title:%s %d %d %dsec %d\n",
		GetTitleNLG(),
		GetBaseClkNLG(),
		GetTickNLG(),
		GetLengthNLG(),
		GetLoopPtrNLG());
		

	int tick = 0;
	int cmd = 0;
    int addr = 0;
    int data = 0;
	
	while (1)
	{
		cmd = ReadNLG();
		if (cmd == EOF)
			break;
		
		switch (cmd)
		{
			case CMD_PSG:
				addr = ReadNLG(); // addr
				data = ReadNLG(); // reg
                printf("PSG:%02X:%02X\n", addr, data);
			break;
			case CMD_OPM:
				addr = ReadNLG(); // addr
				data = ReadNLG(); // reg
                printf("OPM1:%02X:%02X\n", addr, data);
			break;
			case CMD_OPM2:
				addr = ReadNLG(); // addr
				data = ReadNLG(); // reg
                printf("OPM2:%02X:%02X\n", addr, data);
			break;
			case CMD_IRQ:
				tick += GetTickNLG();
                printf("IRQ:%d\n", GetTickNLG());
			break;
            case CMD_CTC0:
                data = ReadNLG();
                SetCTC0_NLG(data);
                printf("CTC0:%02X\n", data);
            break;
            case CMD_CTC3:
                data = ReadNLG();
                SetCTC3_NLG(data);
                printf("CTC3:%02X\n", data);
            break;
		}
	}
	
	printf("tick = %d\n", tick);
	
	CloseNLG();
	
	return 0;
}

#endif
