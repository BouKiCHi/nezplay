#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "m_nsf.h"
#include "handler.h"
#include "audiosys.h"
#include "songinfo.h"

#include "nezglue.h"

#include "nlg.h"

#include <sys/stat.h> 


typedef struct
{
    char file[NSF_FNMAX];
    char id[4];
    int  songno;
    char title[512];
    int  len;
    
    /* ignored */
    int  loop;
    int  fade;
    int  loopcnt;
    
} PLAYLIST_TAG;



NSF_STATE nsf_state;


static PLAYLIST_TAG *nsf_playlist = NULL;

static char nsf_filename[NSF_FNMAX];
static char nsf_execpath[NSF_FNMAX] = "";

static unsigned char *nsf_data = NULL;

static int nsf_size = 0;
static int nsf_volume = 0;

static int nsf_songno = 0;
static int nsf_ch = 0;
static int nsf_freq = 0;

static int nsf_playlist_len = 0;
static int nsf_playlist_pos = 0;
static int nsf_m3u = 0;

static int nsf_samprate = 22050;
static int nsf_total_samples = 0;

typedef struct
{
    unsigned char *bin;
    int size;
} BIN;


BIN stock[] = {
    {NULL,0},
    {NULL,0},
    {NULL,0}
};

#define BIN_HDR 0
#define BIN_DRV 1
#define BIN_SONG 2


#ifndef _WIN32
void _sleep(int num)
{
}

#define PATH_SEP '/'
#else
#define PATH_SEP '\\'

#endif

/****************
 Utility Methods
 *****************/

//o10
static char *tblNote[]={
    "c " , "c+" , "d " , "d+" , "e " , "f " ,
    "f+" , "g " , "g+" , "a " , "a+" , "b "
};
static int tblFreq[]={
    16744 , 17739 , 18794 , 19912 , 21096 , 22350 ,
    23679 , 25087 , 26579 , 28160 , 29834 , 31608
};

void getNote(char *dest,int freq)
{
    int oct = 10;
    
    if (freq == 0)
    {
        dest[0] = 0;
        return;
    }
    
    int i, j;
    for(i = 0; i < 12; i++)
    {
        if (freq >= (tblFreq[0] >> i))
        {
            for(j = 0; j < 12; j++)
            {
                int f1;

                if (j + 1 >= 12)
                    f1 = (tblFreq[0] >> (i-1));
                else
                    f1 = (tblFreq[j + 1] >> i);
                
                if (freq < f1)
                {
                    float diff = (float)f1 / freq;
                    
                    int f2 = (tblFreq[j] >> i);

                    float diff2 = (float)freq / f2;
                    
                    if (diff2 < diff)
                        sprintf(dest, "o%d%s", oct , tblNote[j]);
                    else
                    {
                        if (j + 1 >= 12)
                            sprintf(dest, "o%d%s", oct + 1 , tblNote[0]);
                        else
                            sprintf(dest, "o%d%s", oct , tblNote[j+1]);
                    }
                    return;
                }
            }
            sprintf(dest, "o%d%s", oct , tblNote[11]);
            return;

        }
        
        oct--;
    }

    sprintf(dest, "o%d%s", oct+1 , tblNote[0]);
}


/****************
 Filename Methods
 *****************/

void ReplaceAfterSplit(char *dest,const char *src,const char *newstr,char split)
{
    char *pos;
    
    strcpy(dest, src);
    pos = strrchr(dest, split);
    
    if (pos)
    {
        strcpy(pos + 1, newstr);
    }
}


void CopyWithExt(char *dest, const char *file, const char *newext)
{
    ReplaceAfterSplit(dest, file, newext,'.');
}

void CopyWithFilename(char *dest, const char *file, const char *newfile)
{
    ReplaceAfterSplit(dest, file, newfile, PATH_SEP);
}

/***************
 Config Methods
****************/

char *TrimString(char *dest,const char *src)
{
    int len;
        
    while (*src && isspace(*src)) src++;

    len = (int)strlen(src);
    
    while (len >= 0)
    {
        if (isspace(src[len - 1]))
            len--;
        else
            break;
    }
    
    strcpy(dest, src);
    dest[len] = 0;

    return dest;
}

void CommentOut(char *line)
{
    char *p = strchr(line,';');
    
    if (!p)
        return;
        
    *p = 0;
}

void ReadKeyValue(char *key,char *value,const char *line)
{
    int len = 0;
    char *p;
    char buf[512];

    
    p = strchr(line,'=');
    
    if (!p)
        return;
    
    len = (int)(p - line);
    
    strncpy(buf, line, len);
    buf[len] = 0;
    
    TrimString(key, buf);
    
    TrimString(value, p + 1);
}

typedef void (*KeyValueFunc)(char *key, char *value);


int ReadConfig(const char *file, KeyValueFunc func)
{
    char buf[512];
    char key[128];
    char value[128];

    FILE *fp;
    
    fp = fopen(file, "rt");
    
    if (!fp)
        return -1;
        
    while(!feof(fp))
    {
        key[0] = 0;
        value[0] = 0;
                
        fgets(buf, 512, fp);
        
        CommentOut(buf);
        
        ReadKeyValue(key, value, buf);
        
        if (key[0])
        {
            func(key, value);
            // printf("Key=%s, Value=%s\n", key, value);
        }
    }
    
    fclose(fp);
    return 0;
}

/*************
 Voume Config
*************/

void VolumeKVF(char *key, char *value)
{
    float vol = 1;
    sscanf(value,"%f", &vol);
    
    NESSetDeviceVolume(key, vol);
}



/****************
 File / Memory  Methods
*****************/


char *MakePath(char *dest, const char *dir, const char *filename)
{
	const char *file_tail = strrchr(filename, PATH_SEP);
    const char *tail = strrchr(dir, PATH_SEP);
	
	if (file_tail)
		file_tail++;
	else
		file_tail = filename;
    
    if (!tail)
    {
        strcpy(dest, filename);
    }
    else
    {
        strcpy(dest, dir);
	    strcpy(dest + ( tail - dir ) + 1, file_tail);
    }
    
    return dest;
}


int LoadBinary(BIN *bin,void *src,int len)
{
    if (!bin->bin)
    {
        free(bin->bin);
        bin->bin = NULL;
    }
    
    bin->bin = malloc(len);
    bin->size = len;

    memcpy(bin->bin, src, len);

    return len;
}

int LoadDriver(void *src, int len)
{
    return  LoadBinary(&stock[BIN_DRV], src, len);
}

int LoadHeader(void *src, int len)
{
    return LoadBinary(&stock[BIN_HDR], src, len);
}

int GetNRTDRVVer(void)
{
    unsigned char v2id[] = { 0xc3,0xac,0x20 };

    // ドライバが読み込まれていない
    if (!stock[BIN_DRV].bin)
        return 0;
    
    // 2013版(BASE $1800)
    if (memcmp(stock[BIN_DRV].bin, v2id, 3) == 0)
        return 2;
    
    // 2011版(BASE $1300)
    return 1;
}

#ifdef DEBUG
#define ERROR_FP_DBG(fptr, path) if (!fptr)  printf("File open error : %s\n", path)
#else
#define ERROR_FP_DBG(fptr, path) {}
#endif

//
// read file to stock[BIN_???] with malloc
//
int LoadBinaryFromFile(BIN *bin, const char *basedir, const char *binfile)
{
    int size;
    char PATH[NSF_FNMAX];
    
    FILE *fp = NULL;

    /* 曲ファイルと同一のフォルダにあるファイルを読み込む */
    if (basedir)
    {
        MakePath(PATH, basedir, binfile);
        fp = fopen(PATH, "rb");
        ERROR_FP_DBG(fp, PATH);
    }
    
	/* 実行ディレクトリの中にあるファイルを読む */
    if (!fp && nsf_execpath[0])
    {
        MakePath(PATH, nsf_execpath, binfile);
        fp = fopen(PATH, "rb");
        ERROR_FP_DBG(fp, PATH);
    }
    
    if (!fp)
    {
        /* カレントフォルダのファイルを読む */
        fp = fopen(binfile, "rb");
        
        if (!fp)
        {
        
            printf("File open error : %s\n",binfile);
            return -1;
        }
    }

    // サイズ取得
    fseek(fp, 0, SEEK_END);
    size = (int)ftell( fp );
    fseek(fp, 0, SEEK_SET);

    if (!bin->bin)
    {
        free(bin->bin);
        bin->bin = NULL;
    }
    
    bin->bin = malloc(size);
    bin->size = size;
    
    fread(bin->bin, size, 1, fp);
    
    fclose(fp);
    
    return size;
}

int LoadSong(void *src, int len)
{
    return LoadBinary(&stock[BIN_SONG], src, len);
}

int LoadFileToMemory(const char *file, void *buf)
{
    int size;
    
    FILE *fp;
    fp = fopen(file, "rb");
    
    if (!fp)
    return -1;
    
    // check size
    fseek(fp, 0, SEEK_END);
    size = (int)ftell( fp );
    fseek(fp, 0, SEEK_SET);
    
    fread(buf, size, 1, fp);
    
    fclose(fp);

    return size;
}


void SaveMemory(const char *file, void *buf, int len)
{
    FILE *fp;
    fp = fopen(file, "wb");
    
    if ( !fp )
        return;
        
    fwrite(buf, len, 1, fp);
    
    fclose(fp);

    return;
}

long GetFileSize( const char *file )
{
    struct stat st;

    if (stat(file, &st) < 0)
        return -1;
    
    return (long)st.st_size;
}


int LoadNRDMemory(void)
{
        int song_size = 0;
        
        song_size = stock[BIN_SONG].size;
        
        if (song_size < 0)
            return -1;
        
        // 曲データの先頭$4000 + ファイルサイズ = メモリサイズ
        nsf_size = 0x4000 - 0xF0;
        nsf_size += song_size;
        
        if (nsf_data)
            free(nsf_data);
    
        nsf_data = malloc(nsf_size);

        // メモリ確保失敗なので終了
        if (!nsf_data)
            return -1;
            
        memset(nsf_data, 0, nsf_size);

        // Header
        if (stock[BIN_HDR].bin)
            memcpy(nsf_data,
                stock[BIN_HDR].bin, stock[BIN_HDR].size);
        
        // ドライバのベースアドレス
        int drv_base = 0x1300;
        if (GetNRTDRVVer() == 2)
            drv_base = 0x1800;
    
        // Driver
        if (stock[BIN_DRV].bin)
            memcpy(nsf_data + drv_base - 0xF0,
                stock[BIN_DRV].bin, stock[BIN_DRV].size);

        // Song
        if (stock[BIN_SONG].bin)
        {
            int ank_base = 42;
            memcpy(nsf_data + 0x4000 - 0xF0,
                stock[BIN_SONG].bin, stock[BIN_SONG].size);
            int len = (int)strlen((char *)(stock[BIN_SONG].bin + ank_base));
            int sjis_base = ank_base + len + 1;
            SONGINFO_SetTitle((char *)(stock[BIN_SONG].bin + sjis_base));
        }
        
        // メモリサイズ
        nsf_data[6] = nsf_size & 0xff;
        nsf_data[7] = (nsf_size >> 8) & 0xff;
        
        return 0;
}

int LoadNRD(const char *file)
{
    // ドライバ本体
    LoadBinaryFromFile(&stock[BIN_DRV], file, "NRTDRV.BIN");
    
	// ヘッダ
    switch(GetNRTDRVVer())
    {
        case 2:
            LoadBinaryFromFile(&stock[BIN_HDR], file, "KSSNRTV2.BIN");
        break;
        default:
            LoadBinaryFromFile(&stock[BIN_HDR], file, "KSSNRT.BIN");
        break;
    }
    
	// NRD読み出し
    LoadBinaryFromFile(&stock[BIN_SONG], file, file);
    
    LoadNRDMemory();
    
    return 0;
}


/*****************
 M3U stuff
******************/

void GetM3UInfo(const char *m3u_file, const char *m3u_line,PLAYLIST_TAG *tag)
{
    const char *start = NULL;
    char *end = NULL;
    char *p = NULL;
    int len;

    char timecode[128];
    char fnbuf[NSF_FNMAX];
    
    memset(&tag, sizeof(PLAYLIST_TAG), 0);

    if (m3u_line[0] == '#')
        return;

    start = m3u_line;
    end = strchr(start, ',');
    
    /* filename */

    if (end)
        len = (int)(end - start);
    else
        len = (int)strlen(start);

    strncpy(fnbuf, start, len);
    fnbuf[len] = 0;
    
    p = strstr(fnbuf,"::");
    
    /* ::ID */
    if (p)
    {
        strcpy(tag->id,p+2);
        p[0] = 0;
    }
    
    CopyWithFilename(tag->file, m3u_file, fnbuf);
    
    if (!end)
        return;

    /* songno */
    start = end + 1;
    end = strchr(start, ',');
    
    if (*start == 0)
        return;
    
    tag->songno = atoi(start);
    
    if (!end)
        return;
    
    /* title */
    start = end + 1;
    end = strchr(start, ',');
    
    if (*start == 0)
        return;
    
    if (end)
        len = (int)(end - start);
    else
        len = (int)strlen(start);
    
    strncpy(tag->title, start, len);
    tag->title[len] = 0;
    
    if (!end)
        return;

    /* time */
    start = end + 1;
    end = strchr(start, ',');
    
    if (*start == 0)
        return;
    
    if (end)
        len = (int)(end - start);
    else
        len = (int)strlen(start);
    
    strncpy(timecode, start, len);
    timecode[len] = 0;
    
    p = strchr(timecode,':');
    if (!p)
    {
        // sec
        tag->len = atoi(timecode);
    }
    else
    {
        char *p2 = strchr(p+1,':');
        if (!p2)
        {
            // min:sec
            tag->len = (atoi(timecode) * 60) + atoi(p+1);
        }
        else
        {
            // hour:min:sec
            tag->len = (atoi(timecode) * 3600) + (atoi(p+1)*60) + atoi(p2+1);
        }
    }
}
           

int LoadM3U(const char *file)
{
    FILE *fp = NULL;

    int i;
    char buf[1024];
    
    fp = fopen(file, "r");
    if (!fp)
        return -1;
    
    if (nsf_playlist != NULL)
    {
        free(nsf_playlist);
        nsf_playlist = NULL;
    }

    nsf_playlist_len = 0;
    
    // count list items
    while(!feof(fp))
    {
        buf[0] = 0;
        fgets(buf, 1024, fp);
        if (buf[0] && buf[0] != '#')
            nsf_playlist_len++;
    }
    
    fseek(fp,0,SEEK_SET);
    
    nsf_playlist = malloc(sizeof(PLAYLIST_TAG) * nsf_playlist_len);
    if (!nsf_playlist)
    {
        fclose(fp);
        return -1;
    }
    
    i=0;
    while(!feof(fp))
    {
        buf[0] = 0;
        fgets(buf, 1024, fp);
        if (buf[0] && buf[0] != '#')
        {
            GetM3UInfo(file, buf, &nsf_playlist[i++]);
        }
    }
    
    fclose(fp); 
    
    nsf_m3u = 1;
    return 0;

}

int LoadFileFromList(void)
{
    int result = 0;
    char *listfile = NULL;
    
    if (!nsf_m3u)
        return 0;
    
    listfile = nsf_playlist[nsf_playlist_pos].file;
    
    if (!listfile[0])
        return -1;
    
    if (strcmp(nsf_filename, listfile) != 0)
    {
        strcpy( nsf_filename, listfile );
                
        result = LoadFileNSF( nsf_filename );
    }
    
    return result;
}



int LoadFileNSF(const char *file)
{
    FILE *fp = NULL;
    char *ext = NULL;
    
    ext = strrchr(file, '.');

    if (ext && strcasecmp(ext + 1, "m3u") == 0)
    {
        int result = LoadM3U( file );
        
        if (nsf_m3u)
        {
            nsf_playlist_pos = 0;
        }
        return result;
        
    }
    
    if (ext && strcasecmp(ext + 1, "nrd") == 0)
    {
        return LoadNRD( file );
    }
    
    fp = fopen(file, "rb");
    if (!fp)
        return -1;
    
    nsf_size = (int)GetFileSize(file);

    if ( nsf_size < 0 || nsf_size > NSF_MAXSIZE )
        goto ERROR_END;
    
    if (nsf_data)
        free(nsf_data);
    
    nsf_data = malloc( nsf_size );
    
    if ( ! nsf_data )
        goto ERROR_END;
    
    fread( nsf_data , nsf_size , 1 , fp );
    
    if (fp)
        fclose( fp );

    return 0;


ERROR_END:
    if (fp)
        fclose( fp );
        
    return -1;
}


/*******************
  Bridge Methods
********************/

int LoadNSFBody(void);


void CreateNLG_NSF(const char *filename)
{
    CreateNLG(filename);
}

void CloseNLG_NSF(void)
{
    CloseNLG();
}



void OpenLogNSF(const char *filename)
{
    NESAudioOpenLog(filename);
}

void CloseLogNSF(void)
{
    NESAudioCloseLog();
}

void SetSongNSF(int song)
{
    SONGINFO_SetSongNo( song );
}

void SetFreqNSF(int freq)
{
    nsf_samprate = freq;
    NESAudioFrequencySet( freq );
}

void SetChannelNSF(int ch)
{
    NESAudioChannelSet( ch );
}

void SetVolumeNSF(int vol)
{
    nsf_volume = vol;
    NESVolume( nsf_volume );    
}

void SetDeviceVolumeNSF(const char *name, float vol)
{
    NESSetDeviceVolume(name, vol);
}

void SetAllDeviceVolumeNSF(float vol)
{
    NESSetAllDeviceVolume(vol);
}

int GetChannelNSF(void)
{
    return SONGINFO_GetChannel();
}

int GetExtDeviceNSF(void)
{
    return SONGINFO_GetExtendDevice();
}

// 表示用文字列
struct
{
    char *name;
    int mask;
} extdevs[] =
{
    {"VRC6",EXT_VRC6},
    {"VRC7",EXT_VRC7},
    {"FDS" ,EXT_FDS},
    {"MMC5",EXT_MMC5},
    {"N163",EXT_N106},
    {"FME7",EXT_FME7},
    {"J86" ,EXT_J86},
    {NULL,0}
};

void GetExtDeviceStringNSF(char *dest)
{
    int ext = SONGINFO_GetExtendDevice();
    dest[0] = 0;

    int i = 0;
    if (SONGINFO_GetNSFflag() != SONGINFO_NSF)
        return;
    
    strcpy(dest,"2A03");
    
    for(i = 0; extdevs[i].name; i++)
    {
        if (ext & extdevs[i].mask)
        {
            if (dest[0])
                strcat(dest,"/");
            strcat(dest,extdevs[i].name);
        }    
    }
}

void ResetNSF(void)
{
    nsf_total_samples = 0;
    NESReset();
    
    NESVolume( nsf_volume );
}

void RenderNSF(void *bufp, unsigned samples)
{
    nsf_total_samples += samples;
    NESAudioRender( bufp , samples );
}

void SeekNSF(unsigned frames)
{
    int current = NESGetFrame();
    
    // reset if the frame is passed
    if (frames < current)
        ResetNSF();
    
    NESSeekFrame(frames);
}

int GetFramesNSF(void)
{
    return NESGetFrame();
}

int GetFrameCyclesNSF(void)
{
    return NESGetFrameCycle();
}

int GetAvgCyclesNSF(void)
{
    return NESGetAvgCycle();
}

int GetMaxCyclesNSF(void)
{
    return NESGetMaxCycle();
}

int GetMaxCyclesPositionNSF(void)
{
    return NESGetMaxPosition();
}



float GetFramePerSecondsNSF(void)
{
    return NESGetFramePerSeconds();
}

void N163SetOldMode(int flag)
{
    nes_devif.n163_oldmode = flag;
}

// 厳密なエミュレーションモード
void SetStrictModeNSF(int flag)
{
    NESAudioSetStrict(flag);
}


/*
  Debug functions
*/

unsigned ReadMemoryNSF(unsigned address)
{
    return NESReadMemory(address);
}


unsigned ReadLinearNSF(unsigned address)
{
    return NESReadLinearMemory(address);
}

unsigned ReadRegisterNSF(unsigned address)
{
    return NESReadRegister(address);
}

void DebugPauseNSF(void)
{
    NESDebugPause();
}

void DebugRunNSF(void)
{
    NESDebugRun();
}

void DebugStepNSF(void)
{
    NESDebugStep();
}

void DebugStepOverNSF(void)
{
    unsigned pc = ReadRegisterNSF(REG_PC);
    unsigned char op = ReadMemoryNSF(pc);
    if (op == 0x20) // JSR
    {
        NESDebugSetBP(pc + len6502(op));
    }
    else
    {
        NESDebugStep();
    }
}


void DebugFrameStepNSF(void)
{
    NESDebugFrameStep();
}

void DebugSetBPNSF(int address)
{
    NESDebugSetBP(address);
}

void TerminateNSF(void)
{
    NESTerminate();
}

void FreeBufferNSF(void)
{
    if (nsf_data)
    {
        free( nsf_data );
        nsf_data = NULL;
    }   
}

void GetTitleNSF(char *title)
{
    title[0] = 0;
    
    if (nsf_m3u)
    {
        strcpy(title, nsf_playlist[nsf_playlist_pos].title);
    }
    else
        strcpy(title,SONGINFO_GetTitle());
    
}

// プレイリストの長さ
int GetTotalSongsPLNSF(void)
{
    if (!nsf_m3u)
        return -1;
    
    return nsf_playlist_len;
}

// プレイリストのタイトル
void GetTitlePLNSF(char *title, int pos)
{
    title[0] = 0;
    
    if (!nsf_m3u)
        return;
    
    if (pos < 0 || pos >= nsf_playlist_len)
        return;
    
    if (nsf_m3u)
    {
        strcpy(title, nsf_playlist[pos].title);
    }
}

// プレイリストの曲長
// -1 : 失敗 , >=0 : 長さ
int GetLengthPLNSF(int pos)
{
    if (!nsf_m3u)
        return -1;
    
    if (pos < 0 || pos >= nsf_playlist_len)
        return -1;
    
    if (nsf_m3u)
    {
        return nsf_playlist[pos].len;
    }
    return -1;
}


int GetSongLengthNSF(void)
{
    if (nsf_m3u)
    {
        return nsf_playlist[nsf_playlist_pos].len;
    }
    return 0;
}

void SetSongNoNSF(int no)
{
    if (nsf_m3u)
    {
        if (no < 0)
            no = 0;
        if (no >= nsf_playlist_len)
            no = nsf_playlist_len - 1;
        
        nsf_playlist_pos = no;
    }
    else
        nsf_songno = no;
}

int GetSongNoNSF(void)
{
    if (nsf_m3u)
    {
        return nsf_playlist_pos;
    }
    return nsf_songno;
}

// Load Current Song
int LoadSongNSF(void)
{
    int result = 0;
    int songno = nsf_songno;
    
    if (nsf_m3u)
    {
        result = LoadNSFBody();
        if (result)
            return result;
        songno = nsf_playlist[nsf_playlist_pos].songno;
    }

    SetSongNSF( songno );
    ResetNSF();
    
    return result;
}


int NextSongNSF( void )
{
    if (nsf_m3u)
    {
        nsf_playlist_pos++;
        if (nsf_playlist_pos >= nsf_playlist_len)
            nsf_playlist_pos = 0;
    }
    else nsf_songno++;
    
    return LoadSongNSF();
}

int PrevSongNSF( void )
{
    if (nsf_m3u)
    {
        nsf_playlist_pos--;
        if (nsf_playlist_pos < 0 )
            nsf_playlist_pos = nsf_playlist_len - 1;
    }
    else nsf_songno--;
    
    return LoadSongNSF();
}

int GetTotalSecNSF( void )
{
    if ( nsf_samprate )
        return nsf_total_samples / nsf_samprate;
    else
        return 0;
}


/*************
 Load Methods
 *************/


int LoadNSFBody(void)
{
    if (nsf_m3u)
    {
        if (LoadFileFromList())
        {
            FreeBufferNSF();
            return -1;
        }
    }
    
    if (!nsf_data)
        return -1;
    
    if ( NSFLoad ( nsf_data , nsf_size ) )
    {
        FreeBufferNSF();
        return -1;
    }
    
    SetFreqNSF( nsf_freq );
    SetChannelNSF( nsf_ch );
    
    
    if ( nsf_songno >= 0 )
        SetSongNSF( nsf_songno );
    
    ResetNSF();
    
    // SetVolumeNSF( vol );
    
    nsf_songno = SONGINFO_GetStartSongNo();
    
    return 0;
}

int LoadNSFData( int freq , int ch , int vol , int songno )
{
    // LoadVRC7Tone();
    
    nsf_freq = freq;
    nsf_ch = ch;
    nsf_volume = vol;
    nsf_songno = songno;
    
    return LoadNSFBody();
}

// 実行パスをセットする
void SetNSFExecPath(const char *path)
{
	strcpy(nsf_execpath, path);
}

int LoadNSF(const char *file, int freq, int ch, int vol, int songno)
{
    char vol_file[NSF_FNMAX];

    SONGINFO_SetTitle("");
    
    strcpy( nsf_filename, file );
    
    nsf_m3u = 0;
    
    if ( LoadFileNSF ( nsf_filename ) < 0 )
    {
        return -1;
    }
    int result = LoadNSFData( freq, ch, vol, songno );
    
    // read volume define
    CopyWithFilename(vol_file, file, "nezplug.vol");
    ReadConfig(vol_file, VolumeKVF);
    CopyWithExt(vol_file, file, "vol");
    ReadConfig(vol_file, VolumeKVF);
    
    return result;

}

int LoadSongInMemory(int freq ,int ch ,int vol, int songno)
{
    if (LoadNRDMemory())
        return -1;

    return LoadNSFData(freq, ch, vol, songno);
}


