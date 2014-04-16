#include "neserr.h"
#include "handler.h"
#include "audiosys.h"
#include "m_nsf.h"
#include "nsf6502.h"
#include "songinfo.h"
#include "nsdout.h"

/* ------------ */
/*  km6502 I/F  */
/* ------------ */

#define USE_DIRECT_ZEROPAGE 0
#define USE_CALLBACK	1
#define USE_INLINEMMC	12
#define USE_USERPOINTER	0
#define External __inline static

#include "km6502/km2a03m.h"

/* ------------- */
/*  nsf6502 I/F  */
/* ------------- */

#define SHIFT_CPS 24
#define NES_BASECYCLES (21477270)
static struct {
	Uint32 cleft;	/* fixed point */
	Uint32 cps;		/* cycles per sample:fixed point */
	Uint32 cycles;
	Uint32 fcycles;
	Uint32 cpf[2];		/* cycles per frame */
	Uint32 total_cycles;
	Uint32 iframe;
	Uint8  breaked;
	Uint8  palntsc;
	Uint8  rom[0x10];		/* nosefart rom */

    Uint32 total_frames;
    int    skip_frames;
} nsf6502;


static Uint work6502_BP;		/* Break Point */
static Uint work6502_start_cycles = 0;
static Uint work6502_frame_cycles = 0;
static Uint work6502_frame_last_cycles = 0;
static Uint work6502_frame_max_cycles = 0;
static Uint work6502_frame_avg_cycles = 0;
static Uint work6502_frame_max_position = 0;


static struct K6502_Context work6502;


static int debug_count = 0;
static int debug_pc_addr = -1;

static void Disp6502regs(void)
{
	printf("PC:%04X A:%02X X:%02X Y:%02X S:%02X P:%02X\n",
		   work6502.PC,
		   work6502.A,
		   work6502.X,
		   work6502.Y,
		   work6502.S,
		   work6502.P
    );
}

static Uint NES6502Execute(Uint start_cycles, Uint total_cycles);

static void Debug6502(void)
{
	int addr = 0x0000;
	char buf[256];
	
	if (debug_count > 0)
	{
		debug_count--;
		return;
	}
	
	if (work6502.PC == debug_pc_addr)
		return;
	
	debug_pc_addr = -1;
	
	while(!feof(stdin))
	{
		printf(">");
		fgets(buf,256,stdin);
        
		if (buf[0]=='\n')
			return;
		
		if (buf[0]=='n')
		{
			debug_pc_addr = work6502.PC;
            
			if (buf[1]!='\n')
				debug_count = atoi(&buf[1]);
			
			return;
		}
        
		// display register
		if (buf[0]=='r')
		{
			Disp6502regs();
		}
        
        // step execute
		if (buf[0]=='s')
		{
			Disp6502regs();
		}
		
		// display memory
		if (buf[0]=='d')
		{
			int i,j;
			sscanf(&buf[1],"%04X",&addr);
            
			for(j=0; j < 0x10; j++)
			{
				printf("%04X:", addr);
                
				for(i = 0; i < 0x10; i++)
					printf("%02X ", NES6502Read(addr + i));
                
				addr += 0x10;
				printf("\n");
			}
		}
		
	}
}

Uint NES6502ReadRegister(Uint A)
{
    switch (A)
    {
        case RDREG_PC:
            return work6502.PC;
        case RDREG_A:
            return work6502.A;
        case RDREG_X:
            return work6502.X;
        case RDREG_Y:
            return work6502.Y;
        case RDREG_S:
            return work6502.S;
        case RDREG_P:
            return work6502.P;
    }
    return 0;
}

int NES6502GetFrames(void)
{
    return nsf6502.total_frames;
}

float NES6502GetFramePerSeconds(void)
{
    return (nsf6502.palntsc ? 50 : 59.94); // PAL or NTSC
}

// 1つ前のフレームの処理サイクル数
int NES6502GetFrameCycles(void)
{
    return work6502_frame_last_cycles;
}

// 現時点でのフレームの最大処理サイクル数
int NES6502GetFrameMaxCycles(void)
{
    return work6502_frame_max_cycles;
}

// 現時点でのフレームの平均処理サイクル数
int NES6502GetFrameAvgCycles(void)
{
    return work6502_frame_avg_cycles;
}

// フレームの最大処理サイクル数を記録したフレーム番号
int NES6502GetFrameMaxPosition(void)
{
    return work6502_frame_max_position;
}

void NES6502SetSkipFrames(Uint frames)
{
    nsf6502.skip_frames = frames;
}

static void NES6502BreakPoint(Uint A)
{
	work6502_BP = A;
}

void NES6502Irq(void)
{
	work6502.iRequest |= K6502_INT;
}

static void NES6502Nmi(void)
{
	work6502.iRequest |= K6502_NMI;
}

Uint NES6502Read(Uint A)
{
	return work6502.ReadByte[A >> USE_INLINEMMC](A);
}

Uint NES6502ReadDma(Uint A)
{
	work6502.clock++;	/* DMA cycle */
	return work6502.ReadByte[A >> USE_INLINEMMC](A);
}

void NES6502Write(Uint A, Uint V)
{
	work6502.WriteByte[A >> USE_INLINEMMC](A, V);
}

Uint NES6502GetCycles(void)
{
	return work6502.clock + work6502_start_cycles;
}

void NES6502Push(Uint V)
{
    NES6502Write(0x100 + work6502.S, V);
    work6502.S--;
}

static Uint NES6502Execute(Uint start_cycles, Uint total_cycles)
{
	work6502_start_cycles = start_cycles;

	while (work6502.clock < total_cycles)
	{
        if (nes_debug_mode)
			Debug6502();

		K6502_Exec(&work6502);
		if (work6502.PC == work6502_BP)
		{
            work6502_frame_cycles += work6502.clock;
			work6502.clock = 0;
			return 1;
		}
	}
    work6502_frame_cycles += work6502.clock;
	work6502.clock -= total_cycles;
	return 0;
}

static void NES6502Reset(void)
{
    work6502_frame_cycles = 0;
    work6502_frame_last_cycles = 0;
    work6502_frame_max_cycles = 0;
    work6502_frame_avg_cycles = 0;
    work6502_frame_max_position = 0;
    
	work6502.clock = 0;
	work6502.iRequest = K6502_INIT;
	work6502.PC = work6502_BP = 0xFFFF;
	NES6502Execute(0, work6502.clock + 1);
}

/* ----------------------- */
/*  Memory Access Handler  */
/* ----------------------- */

static NES_READ_HANDLER  *(nprh[0x10]) = { 0, };
static NES_WRITE_HANDLER *(npwh[0x10]) = { 0, };

static void NES6502ReadHandlerSet(Uint bank, READHANDLER rh)
{
	work6502.ReadByte[bank] = rh;
}

static void NES6502WriteHandlerSet(Uint bank, WRITEHANDLER wh)
{
	work6502.WriteByte[bank] = wh;
}

#define EXTREADWRITE(p) \
static Uint __fastcall ExtRd##p(Uint A) \
{ \
	NES_READ_HANDLER *ph = nprh[0x##p]; \
	do \
	{ \
		if (ph->min <= A && A <= ph->max) \
		{ \
			return ph->Proc(A); \
		} \
	} while ((ph = ph->next) != 0); \
	return 0; \
} \
static void __fastcall ExtWr##p(Uint A, Uint V) \
{ \
	NES_WRITE_HANDLER *ph = npwh[0x##p]; \
	do \
	{ \
		if (ph->min <= A && A <= ph->max) \
		{ \
			ph->Proc(A, V); \
			return; \
		} \
	} while ((ph = ph->next) != 0); \
}
EXTREADWRITE(0)
EXTREADWRITE(1)
EXTREADWRITE(2)
EXTREADWRITE(3)
EXTREADWRITE(4)
EXTREADWRITE(5)
EXTREADWRITE(6)
EXTREADWRITE(7)
EXTREADWRITE(8)
EXTREADWRITE(9)
EXTREADWRITE(A)
EXTREADWRITE(B)
EXTREADWRITE(C)
EXTREADWRITE(D)
EXTREADWRITE(E)
EXTREADWRITE(F)
static READHANDLER ExtRdTbl[0x10] = {
	ExtRd0,ExtRd1,ExtRd2,ExtRd3,
	ExtRd4,ExtRd5,ExtRd6,ExtRd7,
	ExtRd8,ExtRd9,ExtRdA,ExtRdB,
	ExtRdC,ExtRdD,ExtRdE,ExtRdF,
};
static WRITEHANDLER ExtWrTbl[0x10] = {
	ExtWr0,ExtWr1,ExtWr2,ExtWr3,
	ExtWr4,ExtWr5,ExtWr6,ExtWr7,
	ExtWr8,ExtWr9,ExtWrA,ExtWrB,
	ExtWrC,ExtWrD,ExtWrE,ExtWrF,
};
static Uint __fastcall NullRead(Uint A)
{
	return 0;
}
static void __fastcall NullWrite(Uint A, Uint V)
{
}

static void InstallPageReadHandler(NES_READ_HANDLER *ph)
{
	Uint page = (ph->min >> 12) & 0xF;
	if (nprh[page])
		NES6502ReadHandlerSet(page, ExtRdTbl[page]);
	else
		NES6502ReadHandlerSet(page, ph->Proc);
	/* Add to head of list*/
	ph->next = nprh[page];
	nprh[page] = ph;
}
static void InstallPageWriteHandler(NES_WRITE_HANDLER *ph)
{
	Uint page = (ph->min >> 12) & 0xF;
	if (npwh[page])
		NES6502WriteHandlerSet(page, ExtWrTbl[page]);
	else
		NES6502WriteHandlerSet(page, ph->Proc);
	/* Add to head of list*/
	ph->next = npwh[page];
	npwh[page] = ph;
}
void NESReadHandlerInstall(NES_READ_HANDLER *ph)
{
	for (; ph->Proc; ph++) InstallPageReadHandler(ph);
}

void NESWriteHandlerInstall(NES_WRITE_HANDLER *ph)
{
	for (; ph->Proc; ph++) InstallPageWriteHandler(ph);
}

void NESMemoryHandlerInitialize(void)
{
	Uint i;
	for (i = 0; i < 0x10;  i++)
	{
		NES6502ReadHandlerSet(i, NullRead);
		NES6502WriteHandlerSet(i, NullWrite);
		nprh[i] = 0;
		npwh[i] = 0;
	}
}

/* ------------- */
/*  nsf6502   */
/* ------------- */


static void NSFRomInit(Uint A)
{
	nsf6502.rom[0] = 0x20;	/* jsr */
	nsf6502.rom[1] = A;		/* init */
	nsf6502.rom[2] = A >> 8;
	nsf6502.rom[3] = 0x4C;	/* JMP nnnn */
	nsf6502.rom[4] = 0x03;
	nsf6502.rom[5] = 0x41;
}

static void NSFRomInitStack(void)
{
	nsf6502.rom[0] = 0x40;	/* RTI */
	nsf6502.rom[1] = 0x60;  /* RTS */
	nsf6502.rom[2] = 0x00;
	nsf6502.rom[3] = 0x4C;	/* JMP nnnn */
	nsf6502.rom[4] = 0x03;
	nsf6502.rom[5] = 0x41;
}


static Uint GetWordLE(Uint8 *p)
{
	return p[0] | (p[1] << 8);
}

static void NSF6502SetPlayRoutine(int resetFlag)
{
    if (resetFlag)
    {
        work6502.A = 0x00;
        work6502.S = 0xFF;
        work6502.P = 0x26;
    }
    Uint PlayAddr = SONGINFO_GetPlayAddress();
    
    NSFRomInitStack();

    // 現在の状態
    NES6502Push((work6502.PC >> 8) & 0xff);
    NES6502Push((work6502.PC) & 0xff);
    NES6502Push(work6502.P);

    // 復帰ルーチン
    NES6502Push(((0x4100-1) >> 8) & 0xff);
    NES6502Push((0x4100-1) & 0xff);
    
    NES6502BreakPoint(0x4103);
    work6502.PC = PlayAddr; /* PLAY */
}

static void NSF6502PlaySetup(void)
{
	if (nsf6502.breaked)
	{
        if (nes_logfile)
			fprintf(nes_logfile,"6502START_FRAME\n");

		nsf6502.breaked = 0;
        NSF6502SetPlayRoutine(!nes_strict_mode);
	}
}

static void __fastcall Execute6502CheckVBlank()
{
    if (nsf6502.cycles > nsf6502.cpf[nsf6502.iframe])
	{
        nes_debug_frame_step = 0;
        
		if (nes_logfile)
			fprintf(nes_logfile,"6502VSYNC\n");
        
        nsf6502.total_frames++;
        
		nsf6502.cycles -= nsf6502.cpf[nsf6502.iframe];
		nsf6502.iframe ^= 1;
        
        if (nes_strict_mode)
            nsf6502.breaked = 1;
        
		NSF6502PlaySetup();
		if (NSD_out_mode) NSDWrite(NSD_VSYNC, nsf6502.palntsc, 0);
	}
}

static Int32 __fastcall Execute6502(void)
{
	Uint32 cycles = 0;

    if (nes_debug_pause && nsf6502.skip_frames == 0)
    {
        if (!nes_debug_frame_step)
        {
            if (nes_debug_step || nes_debug_bp >= 0)
                cycles = work6502.clock + 1;
            else
                return 0;
        }
    }
    
    do
    {
        if (!cycles)
        {
            nsf6502.cleft += nsf6502.cps;
            cycles = nsf6502.cleft >> SHIFT_CPS;
        }
        
        if (!nsf6502.breaked)
        {
            nsf6502.breaked = NES6502Execute(nsf6502.total_cycles, cycles);
            
            // 1.79MHz / 60 = 29829.55
            if (nsf6502.breaked)
            {
                if (nes_logfile)
                    fprintf(nes_logfile,"6502CYCLES %u\n",work6502_frame_cycles);
                work6502_frame_last_cycles = work6502_frame_cycles;
                work6502_frame_avg_cycles =
                    (work6502_frame_avg_cycles + work6502_frame_cycles) / 2;
                
                // MAX算出
                if (nsf6502.total_frames > 0 &&
                    work6502_frame_max_cycles < work6502_frame_cycles)
                {
                    work6502_frame_max_cycles = work6502_frame_cycles;
                    work6502_frame_max_position = nsf6502.total_frames;
                    
                }
                work6502_frame_cycles = 0;
            }
        }
        
        if (nes_debug_bp >= 0)
        {
            if (nes_debug_bp == work6502.PC)
                nes_debug_bp = -1;
        }
        
        if (!nes_debug_step)
        {
            nsf6502.cleft &= (1 << SHIFT_CPS) - 1;
        }
        nsf6502.cycles += cycles * 12;
        Execute6502CheckVBlank();
        nsf6502.total_cycles += cycles;
        
        if (nsf6502.total_frames >= nsf6502.skip_frames)
        {
            nsf6502.skip_frames = 0;
        }
    }while(nsf6502.skip_frames > 0);
    
    nes_debug_step = 0;
	return 0;
}

static NES_AUDIO_HANDLER nsf6502_audio_handler[] = {
	{ 0, Execute6502, },
	{ 0, 0, },
};

#ifdef _WIN32_OPT
extern int __stdcall MulDiv(int nNumber,int nNumerator,int nDenominator);
static Uint32 __forceinline muldiv(Uint32 m, Uint32 n, Uint32 d)
{
	return MulDiv(m,n,d);
}
#else
static Uint32 muldiv(Uint32 m, Uint32 n, Uint32 d)
{
	return ((double)m) * n / d;
}
#endif

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

static void __fastcall NSF6502Reset(void)
{
	Uint8 *nsfhead = NSFGetHeader();
	Uint freq = NESAudioFrequencyGet();
	Uint speed = 0;

	nsf6502.palntsc = nsfhead[0x7a] & 1;
	if (!nsf6502.palntsc && GetWordLE(nsfhead + 0x6E))
		speed = GetWordLE(nsfhead + 0x6E);	/* NTSC tune */
	else if (nsf6502.palntsc && GetWordLE(nsfhead + 0x78))
		speed = GetWordLE(nsfhead + 0x78);	/* PAL  tune */

	if (speed == 0)
		speed = nsf6502.palntsc ? 0x4e20 : 0x411A;

	nsf6502.cleft = 0;
	nsf6502.cps = DivFix(NES_BASECYCLES, 12 * freq, SHIFT_CPS);

	nsf6502.cycles = 0;
	if (nsf6502.palntsc)
	{
		nsf6502.cpf[0] = muldiv(speed, 4 * 341 * 313    , 0x4e20);
		nsf6502.cpf[1] = muldiv(speed, 4 * 341 * 313 - 4, 0x4e20);
	}
	else
	{
		nsf6502.cpf[0] = muldiv(speed, 4 * 341 * 262    , 0x411a);
		nsf6502.cpf[1] = muldiv(speed, 4 * 341 * 262 - 4, 0x411a);
	}
	nsf6502.iframe = 0;

	NES6502Reset();
	NES6502BreakPoint(0x4103);
	NSFRomInit(SONGINFO_GetInitAddress());
	work6502.PC = 0x4100;
	work6502.A = SONGINFO_GetSongNo() - 1;
	work6502.X = nsf6502.palntsc;
	work6502.Y = 0;
	work6502.S = 0xFF;
	work6502.P = 0x26;							/* IRZ */
	nsf6502.total_cycles = 0;
    nsf6502.total_frames = 0;
    nsf6502.skip_frames = 0;

#define LIMIT_INIT (10 * 60)	/* 10sec */
#if LIMIT_INIT
	{
		Uint sec;
		for (sec = 0; sec < LIMIT_INIT; sec++)
		{
			nsf6502.breaked = NES6502Execute(nsf6502.total_cycles, nsf6502.cpf[0]);
			if (nsf6502.breaked) break;
		}
	}
#else
	while (1)
	{
		nsf6502.breaked = NES6502Execute(nsf6502.total_cycles, ~0);
		if (nsf6502.breaked) break;
	}
#endif

	NSF6502PlaySetup();
	if (NSD_out_mode) NSDWrite(NSD_INITEND, 0, 0);
}

static NES_RESET_HANDLER nsf6502_reset_handler[] = {
	{ NES_RESET_SYS_LAST, NSF6502Reset, },
	{ 0,                  0, },
};


/* Nosefart-ROM area */
static Uint32 __fastcall ReadNosefartRom(Uint32 A)
{
	return nsf6502.rom[A & 0x000F];
}


static NES_READ_HANDLER nsf6502_read_handler[] = {
	{ 0x4100,0x410F,ReadNosefartRom, },
	{ 0     ,0     ,0, },
};

Uint NSF6502Install(void)
{
    nes_funcs_t funcs =
    {
        NES6502Read,
        NES6502ReadRegister,
        NES6502SetSkipFrames,
        NES6502GetFrames,
        NES6502GetFramePerSeconds,
        NES6502GetFrameCycles,
        NES6502GetFrameAvgCycles,
        NES6502GetFrameMaxCycles,
        NES6502GetFrameMaxPosition,
        NULL // read linear memory
    };
	NESReadHandlerInstall(nsf6502_read_handler);
	NESAudioHandlerInstall(nsf6502_audio_handler);
	NESResetHandlerInstall(nsf6502_reset_handler);
    
    NESSetHandlers(&funcs);

	return NESERR_NOERROR;
}

