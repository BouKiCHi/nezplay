//
// sdlplay.c
//

#include <stdio.h>
#include <getopt.h>
#include <SDL.h>

#include <signal.h>
#define USE_SDL

#include "nezglue.h"

int debug = 0;

#define NEZ_VER "2014-04-29"

#define PCM_BLOCK 2048
#define PCM_BYTE_PER_SAMPLE 2
#define PCM_CH  2
#define PCM_NUM_BLOCKS 4

#define PCM_BUFFER_LEN ( PCM_BLOCK * PCM_CH * PCM_NUM_BLOCKS )

static struct pcm_struct
{
    int on;
    int write;
    int play;
    int stop;
    short buffer[ PCM_BUFFER_LEN ];
} pcm;

#define PRNDBG(xx) if (nsf_verbose) printf(xx)

#include "fade.h"

// audio_callback 再生時に呼び出される
// data : データ 
// len  : 必要なデータ長(バイト単位)
static void audio_callback( void *param , Uint8 *data , int len )
{
    
    int i;
    
    short *audio_buffer = (short *)data;

    if ( !pcm.on )
    {
        memset( data , 0 , len );
        return;
    }
    
    for( i = 0; i < len / 2; i++ )
    {
        audio_buffer[ i ] = pcm.buffer[ pcm.play++ ];
        if ( pcm.play >= PCM_BUFFER_LEN )
                pcm.play = 0;
    }
}

static int audio_sdl_init()
{

//  if ( SDL_Init( SDL_INIT_VIDEO | SDL_INIT_AUDIO ) )
    
    if ( SDL_Init( SDL_INIT_AUDIO ) )
    {
        printf("Failed to Initialize!!\n");
        return 1;
    }

    return 0;
}

static int audio_init( int freq )
{
    SDL_AudioSpec af;
    
    // SDL_SetVideoMode ( 320 , 240 , 32 , SDL_SWSURFACE );
    
    af.freq     = freq;
    af.format   = AUDIO_S16;
    af.channels = 2;
    af.samples  = 2048;
    af.callback = audio_callback;
    af.userdata = NULL;

    if (SDL_OpenAudio( &af , NULL ) < 0)
    {
        printf("Audio Error!!\n");
        return 1;
    }
    
    memset( &pcm , 0 , sizeof( pcm ) );
    
    SDL_PauseAudio( 0 );
    
    PRNDBG("Start Audio\n");
    
    return 0;
}

static void audio_free( void )
{
    PRNDBG("Close Audio\n");
    SDL_CloseAudio();
    PRNDBG("Quit SDL\n");
    SDL_Quit();
    PRNDBG("OK\n");
    
}

static int audio_poll_event( void )
{
    SDL_Event evt;
    
    while ( SDL_PollEvent( &evt ) )
    {
        switch ( evt.type ) 
        {
            case SDL_QUIT:
                return -1;
            break;
        }
    }
    
    return 0;
}

static void audio_sig_handle( int sig )
{
    pcm.stop = 1;
}

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

#ifndef DEBUG
#define INLINE inline
#else
#define INLINE
#endif


INLINE void write_dword(byte *p,dword v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
    p[2] = (v>>16) & 0xff;
    p[3] = (v>>24) & 0xff;
}

INLINE void write_word(byte *p,word v)
{
    p[0] = v & 0xff;
    p[1] = (v>>8) & 0xff;
}

// audio_write_wav_header : ヘッダを出力する
// freq : 再生周波数
// pcm_bytesize : データの長さ
static void audio_write_wav_header(FILE *fp, long freq, long pcm_bytesize)
{
    unsigned char hdr[128];
    int ch = PCM_CH;
    int bytes_per_sample = PCM_BYTE_PER_SAMPLE;
    
    if (!fp)
        return;
    
    memcpy(hdr,"RIFF", 4);
    write_dword(hdr + 4, pcm_bytesize + 44);
    memcpy(hdr + 8,"WAVEfmt ", 8);
    write_dword(hdr + 16, 16); // chunk length
    write_word(hdr + 20, 01); // pcm id
    write_word(hdr + 22, ch); // ch
    write_dword(hdr + 24, freq); // freq
    write_dword(hdr + 28, freq * ch * bytes_per_sample); // bytes per sec
    write_word(hdr + 32, ch * bytes_per_sample); // bytes per frame
    write_word(hdr + 34, bytes_per_sample * 8 ); // bits

    memcpy(hdr + 36, "data",4);
    write_dword(hdr + 40, pcm_bytesize); // pcm size
    
    fseek(fp, 0, SEEK_SET);
    fwrite(hdr, 44, 1, fp);
    
    fseek(fp, 0, SEEK_END); 
    
}


// audio_loop : 再生時にループする
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop( int freq , int len )
{
    int sec;
    int last_sec;
    
    int frames;
    int total_frames;

    // len = 5;

    fade_init();
    
    last_sec = -1;
    
    sec = 
    frames =
    total_frames = 0;
    
    if ( audio_poll_event() < 0 )
    {
        SDL_PauseAudio( 1 );
        return;
    }

    
    do
    {
        // delay until next block is writable
        
        while( pcm.write >= pcm.play &&
               pcm.write <  pcm.play + ( PCM_BLOCK * PCM_CH ) )
        {
            if ( audio_poll_event() < 0 )
            {
                SDL_PauseAudio( 1 );
                return;
            }
            
            SDL_Delay( 1 );
        }
        
        // pcm.buffer + pcm.write 
        // datalen = PCM_BLOCK * PCM_CH * PCM_BYTE_PER_SAMPLE
        RenderNSF ( pcm.buffer + pcm.write , PCM_BLOCK );
                
        if ( fade_is_running () )
            fade_stereo ( pcm.buffer + pcm.write , PCM_BLOCK );

        // pcm.write += PCM_BLOCK;
        
        pcm.write += (PCM_BLOCK * PCM_CH);
        
        if ( pcm.write >= PCM_BUFFER_LEN )
                pcm.write = 0;
        
        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while ( frames >= freq )
        {
            frames -= freq;
            sec++;
        }
        
        if ( sec != last_sec )
        {
            if (! debug )
                printf("\rTime : %02d:%02d / %02d:%02d",
                sec / 60 , sec % 60 , len / 60 , len % 60 );

            /* フェーダーを起動する */
            if ( sec >= ( len - 3 ) )
            { 
                if ( ! fade_is_running () )
                    fade_start( freq , 1 );
            }
            
            last_sec = sec;
        }
        

        fflush( stdout );           


    }while( sec < len && ! pcm.stop );
    
    if ( !debug )
        printf("\n");
    
    PRNDBG("Stopping...\n");

    SDL_PauseAudio(1);

    PRNDBG("OK\n");

}


// audio_loop_file : 音声をデータ化する
// freq : 再生周波数
// len : 長さ(秒)
static void audio_loop_file(const char *file, int freq , int len )
{
    FILE *fp = NULL;

    int sec;
    int last_sec;
    
    int frames;
    int total_frames;

    short pcm_buffer[PCM_BLOCK * PCM_CH * PCM_BYTE_PER_SAMPLE];

    // len = 5;

    fade_init();
    
    last_sec = -1;
    
    sec = 
    frames =
    total_frames = 0;
    
    
    if (file)
        fp = fopen(file, "wb");
    
    if (file && fp == NULL)
    {
        printf("Can't write a PCM file!");
        return;
    }
    
    audio_write_wav_header(fp, freq, 0);
        
    do
    {
            
        // pcm.buffer + pcm.write 
        // datalen = PCM_BLOCK * PCM_CH * PCM_BYTE_PER_SAMPLE
        RenderNSF(pcm_buffer, PCM_BLOCK);
                
        if (fade_is_running())
            fade_stereo (pcm_buffer, PCM_BLOCK);
        
        if (fp)
            fwrite(pcm_buffer,PCM_BLOCK * PCM_CH * PCM_BYTE_PER_SAMPLE,1,fp);

        // pcm.write += PCM_BLOCK;
                
        frames += PCM_BLOCK;
        total_frames += PCM_BLOCK;
        
        /* 今までのフレーム数が一秒以上なら秒数カウントを進める */
        while(frames >= freq)
        {
            frames -= freq;
            sec++;
        }
        
        if (sec != last_sec)
        {
            if (! debug )
                printf("\rTime : %02d:%02d / %02d:%02d",
                sec / 60 , sec % 60 , len / 60 , len % 60 );

            /* フェーダーを起動する */
            if (sec >= ( len - 3 ))
            { 
                if ( ! fade_is_running () )
                    fade_start( freq , 1 );
            }
            
            last_sec = sec;
        }
        
        fflush( stdout );           


    }while( sec < len && !pcm.stop );
    
    audio_write_wav_header(fp, freq,
        total_frames * PCM_CH * PCM_BYTE_PER_SAMPLE);

    if (fp)
        fclose(fp);
    
    if ( !debug )
        printf("\n");
}


void usage(void)
{
    printf(
    "Usage nezplay [ options ...] <file>\n"
    "\n"
    " Options ...\n"
    " -s rate   : Set playback rate\n"
    " -n no     : Set song number\n"
    " -v vol    : Set volume\n"
    " -l n      : Set song length (n secs)\n"
    " -q dir    : Set driver's path\n"
    "\n"
    " -o file   : Generate an Wave file(PCM)\n"
    " -p        : NULL PCM mode.\n"
    "\n"
    " -z        : Set N163 mode\n"
    "\n"
    " -r file   : Record a NLG\n"
    " -b        : Record a NLG without sound\n"
    "\n"
    " -d file   : Record a debug LOG\n"
    " -x        : Set strict mode\n"
    " -w        : Set verbose mode\n"
    "\n"
    " -h        : Help (this)\n"
    "\n"
    );
}

#define NLG_NORMAL 1
#define NLG_SAMEPATH 2

int audio_main(int argc, char *argv[])
{
    char nlg_path[NSF_FNMAX];
    
    char *drvpath = NULL;
    char *nlgfile = NULL;
    char *pcmfile = NULL;
    char *logfile = NULL;
    int opt;
    
    int rate   = 44100;
    int vol    = -1;
    int len    = 360;
    int songno = -1;
    
    int nlg_log = 0;
    int nosound = 0;
    
    int n163mode = 0;
    int strictmode = 0;

#ifdef _WIN32   
    freopen( "CON", "wt", stdout );
    freopen( "CON", "wt", stderr );
#endif

    audio_sdl_init();
    
    
    signal( SIGINT , audio_sig_handle );
    
    printf(
        "NEZPLAY on SDL Version %s\n"
        "build at %s\n"
        "Ctrl-C to stop\n"
           , NEZ_VER, __DATE__);

	SetNSFExecPath(argv[0]);
	
    
    if (argc < 2)
    {
        usage();
        return 0;
    }
    
    debug = 0;
    
    while ((opt = getopt(argc, argv, "q:s:n:v:l:d:o:r:btxhpzw")) != -1)
    {
        switch (opt) 
        {
            case 'q':
                drvpath = optarg;
                break;
            case 'b':
                nlg_log = NLG_SAMEPATH;
                nlgfile = NULL;
                nosound = 1;
                break;
            case 'r':
                nlg_log = NLG_NORMAL;
                nlgfile = optarg;
                break;
            case 'p':
                nosound = 1;
                break;
            case 'z':
                n163mode = 1;
            break;
            case 's':
                rate = atoi(optarg);
                break;
            case 'n':
                songno = atoi(optarg);
                break;
            case 'v':
                vol = atoi(optarg);
                break;
            case 'd':
                logfile = optarg;
                break;
            case 'o':
                pcmfile = optarg;
                break;
            case 'l':
                len = atoi(optarg);
                break;
            case 'x':
                strictmode = 1;
                break;
            case 'w':
                nsf_verbose = 1;
                break;
            case 't':
                // NESAudioSetDebug(1);
                debug = 1;
                break;
            case 'h':
            default:
                usage();
                return 1;
        }
    }
    
    if (audio_init(rate))
    {
        printf("Failed to initialize audio hardware\n");
        return 1;
    }
    
    pcm.on = 1;
    
    if (!n163mode)
        N163SetOldMode(1);

    SetStrictModeNSF(strictmode);
    
    if (drvpath)
    {
        SetDriverPathNSF(drvpath);
    }
    else
    {
        PathFromEnvNSF();
    }
    
    if (logfile)
        OpenLogNSF(logfile);
    
    // ファイルの数だけ処理
    for(;optind < argc; optind++)
    {
        if (nlg_log)
        {
            if (nlg_log == NLG_SAMEPATH)
            {
                strcpy(nlg_path, argv[optind]);
                char *p = strrchr(nlg_path, '.');
                
                if (p)
                    strcpy(p,".NLG");
                else
                    strcat(nlg_path,".NLG");
                
                nlgfile = nlg_path;
            }

            printf("CreateNLG:%s\n",nlgfile);
            CreateNLG_NSF(nlgfile);
        }
        

        
        if (LoadNSF(argv[optind], rate, 2, vol, songno))
        {
            printf("File open error : %s\n", argv[optind]);
            CloseLogNSF();
            CloseNLG_NSF();
            
            return 0;
        }

        if (!debug)
            printf("Freq = %d, SongNo = %d\n", rate, songno);
        
        if (vol >= 0)
        {
            SetVolumeNSF(vol);
        }
        
        if (nosound || pcmfile)
            audio_loop_file(pcmfile, rate, len);
        else
            audio_loop(rate , len);
        
        CloseNLG_NSF();
    }
    
    CloseLogNSF();
    
    audio_free();
    
    PRNDBG("terminateNSF\n");
    
    TerminateNSF();
    FreeBufferNSF();

    PRNDBG("done\n");

    return 0;
}

// disable SDLmain for win32 console app
#ifdef _WIN32
#undef main
#endif

int main(int argc, char *argv[])
{
	int ret = audio_main(argc, argv);

#ifdef DEBUG
	getch();
#endif

	return ret;
}

