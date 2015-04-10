#include "SID.h"
#include <SmallFS.h>

#undef RETROCADE // Define this if you have a retrocade hw

#define AUDIO_J1_L WING_B_1
#define AUDIO_J1_R WING_B_0
#define AUDIO_J2_L WING_B_3
#define AUDIO_J2_R WING_B_2

#define SIDBASE IO_SLOT(14)
#define SIDREG(x) REGISTER(SIDBASE,x)

unsigned short play_addr;
volatile int tick = 0;

SID sid;

// -- do not touch this below, it's a C64 emulator.

#define FLAG_N 128
#define FLAG_V 64
#define FLAG_B 16
#define FLAG_D 8
#define FLAG_I 4
#define FLAG_Z 2
#define FLAG_C 1


static unsigned char memory[65536];

typedef enum {OPadc, OPand, OPasl, OPbcc, OPbcs, OPbeq, OPbit, OPbmi, OPbne, OPbpl, OPbrk, OPbvc, OPbvs, OPclc,
  OPcld, OPcli, OPclv, OPcmp, OPcpx, OPcpy, OPdec, OPdex, OPdey, OPeor, OPinc, OPinx, OPiny, OPjmp,
  OPjsr, OPlda, OPldx, OPldy, OPlsr, OPnop, OPora, OPpha, OPphp, OPpla, OPplp, OProl, OPror, OPrti,
  OPrts, OPsbc, OPsec, OPsed, OPsei, OPsta, OPstx, OPsty, OPtax, OPtay, OPtsx, OPtxa, OPtxs, OPtya,
  OPxxx} insts;

#define imp 0
#define imm 1
#define abs 2
#define absx 3
#define absy 4
#define zp 6
#define zpx 7
#define zpy 8
#define ind 9
#define indx 10
#define indy 11
#define acc 12
#define rel 13
#define xxx 14

static uint8_t opcodes[256]= {
  OPbrk,OPora,OPxxx,OPxxx,OPxxx,OPora,OPasl,OPxxx,OPphp,OPora,OPasl,OPxxx,OPxxx,OPora,OPasl,OPxxx,
  OPbpl,OPora,OPxxx,OPxxx,OPxxx,OPora,OPasl,OPxxx,OPclc,OPora,OPxxx,OPxxx,OPxxx,OPora,OPasl,OPxxx,
  OPjsr,OPand,OPxxx,OPxxx,OPbit,OPand,OProl,OPxxx,OPplp,OPand,OProl,OPxxx,OPbit,OPand,OProl,OPxxx,
  OPbmi,OPand,OPxxx,OPxxx,OPxxx,OPand,OProl,OPxxx,OPsec,OPand,OPxxx,OPxxx,OPxxx,OPand,OProl,OPxxx,
  OPrti,OPeor,OPxxx,OPxxx,OPxxx,OPeor,OPlsr,OPxxx,OPpha,OPeor,OPlsr,OPxxx,OPjmp,OPeor,OPlsr,OPxxx,
  OPbvc,OPeor,OPxxx,OPxxx,OPxxx,OPeor,OPlsr,OPxxx,OPcli,OPeor,OPxxx,OPxxx,OPxxx,OPeor,OPlsr,OPxxx,
  OPrts,OPadc,OPxxx,OPxxx,OPxxx,OPadc,OPror,OPxxx,OPpla,OPadc,OPror,OPxxx,OPjmp,OPadc,OPror,OPxxx,
  OPbvs,OPadc,OPxxx,OPxxx,OPxxx,OPadc,OPror,OPxxx,OPsei,OPadc,OPxxx,OPxxx,OPxxx,OPadc,OPror,OPxxx,
  OPxxx,OPsta,OPxxx,OPxxx,OPsty,OPsta,OPstx,OPxxx,OPdey,OPxxx,OPtxa,OPxxx,OPsty,OPsta,OPstx,OPxxx,
  OPbcc,OPsta,OPxxx,OPxxx,OPsty,OPsta,OPstx,OPxxx,OPtya,OPsta,OPtxs,OPxxx,OPxxx,OPsta,OPxxx,OPxxx,
  OPldy,OPlda,OPldx,OPxxx,OPldy,OPlda,OPldx,OPxxx,OPtay,OPlda,OPtax,OPxxx,OPldy,OPlda,OPldx,OPxxx,
  OPbcs,OPlda,OPxxx,OPxxx,OPldy,OPlda,OPldx,OPxxx,OPclv,OPlda,OPtsx,OPxxx,OPldy,OPlda,OPldx,OPxxx,
  OPcpy,OPcmp,OPxxx,OPxxx,OPcpy,OPcmp,OPdec,OPxxx,OPiny,OPcmp,OPdex,OPxxx,OPcpy,OPcmp,OPdec,OPxxx,
  OPbne,OPcmp,OPxxx,OPxxx,OPxxx,OPcmp,OPdec,OPxxx,OPcld,OPcmp,OPxxx,OPxxx,OPxxx,OPcmp,OPdec,OPxxx,
  OPcpx,OPsbc,OPxxx,OPxxx,OPcpx,OPsbc,OPinc,OPxxx,OPinx,OPsbc,OPnop,OPxxx,OPcpx,OPsbc,OPinc,OPxxx,
  OPbeq,OPsbc,OPxxx,OPxxx,OPxxx,OPsbc,OPinc,OPxxx,OPsed,OPsbc,OPxxx,OPxxx,OPxxx,OPsbc,OPinc,OPxxx
};

static uint8_t modes[256]= {
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 abs,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imp,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,ind,abs,abs,xxx,
 rel,indy,xxx,xxx,xxx,zpx,zpx,xxx,imp,absy,xxx,xxx,xxx,absx,absx,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpy,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx,
 imm,indx,imm,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpy,xxx,imp,absy,acc,xxx,absx,absx,absy,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpx,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx,
 imm,indx,xxx,xxx,zp,zp,zp,xxx,imp,imm,acc,xxx,abs,abs,abs,xxx,
 rel,indy,xxx,xxx,zpx,zpx,zpx,xxx,imp,absy,acc,xxx,xxx,absx,absx,xxx
};

// ----------------------------------------------- globale Faulheitsvariablen

//static int cycles;
//static uint8_t bval;
//static uint16_t wval;

// ----------------------------------------------------------------- Register

struct c64regs {
	uint8_t a,x,y,s,p;
	uint16_t pc;
};

struct c64regs cr;

// ----------------------------------------------------------- DER HARTE KERN

static uint8_t sidregs[32];

static unsigned getmem(unsigned addr)
{
	addr&=0xffff;

	if (addr == 0xdd0d) memory[addr]=0;
	//printf("READ %04x = %04x\n", addr, memory[addr]);
	if ((addr&0xfc00)==0xd400) {
		printf("SID READ %04x cycle %d\n",addr,cycles);
		abort();
	}
	return (unsigned)memory[addr];
}

static void writeSIDreg(unsigned reg, unsigned value)
{
	sidregs[reg] = value;
}

static void setmem(unsigned addr, unsigned value)
{
	addr&=0xffff;

//	printf("WRITE %04x <- %02x\n", addr, value);

	if ((addr&0xfc00)==0xd400) {
		//printf("SID WRITE %04x <- %02x cycle %d\n",addr,value,cycles);
		writeSIDreg(addr & 31, value);
	}

	if ((addr==0xdc04) || (addr==0xdc05)) {
		// printf("CIA WRITE %04x <- %02x cycle %d\n",addr,value,cycles);
	}
	memory[addr]=value;
}

#define ADDCYCLES(x)

static uint8_t getaddr(int mode, unsigned &pc)
{
  uint16_t ad,ad2;  
  switch(mode)
  {
    case imp:
      ADDCYCLES(2);
      return 0;
    case imm:
      ADDCYCLES(2);
      return getmem(pc++);
    case abs:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      return getmem(ad);
    case absx:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+x;
      if ((ad2&0xff00)!=(ad&0xff00))
        ADDCYCLES(1);
      return getmem(ad2);
    case absy:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        ADDCYCLES(1);
      return getmem(ad2);
    case zp:
      ADDCYCLES(3);
      ad=getmem(pc++);
      return getmem(ad);
    case zpx:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad+=x;
      return getmem(ad&0xff);
    case zpy:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad+=y;
      return getmem(ad&0xff);
    case indx:
      ADDCYCLES(6);
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      return getmem(ad2);
    case indy:
      ADDCYCLES(5);
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        ADDCYCLES(1);
      return getmem(ad);
    case acc:
      ADDCYCLES(2);
      return a;
  }  
  return 0;
}


static void setaddr(int mode, unsigned val, unsigned &pc)
{
  unsigned ad,ad2;
  switch(mode)
  {
    case abs:
      ADDCYCLES(2);
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      setmem(ad,val);
      return;
    case absx:
      ADDCYCLES(3);
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      ad2=(ad+x)&0xffff;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles--;
      setmem(ad2,val);
      return;
    case zp:
      ADDCYCLES(2);
      ad=getmem(pc-1);
      setmem(ad,val);
      return;
    case zpx:
      ADDCYCLES(2);
      ad=getmem(pc-1);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case acc:
      a=val;
      return;
  }
}


static void putaddr(int mode, unsigned val)
{
  uint16_t ad,ad2;
  switch(mode)
  {
    case abs:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      setmem(ad,val);
      return;
    case absx:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+x;
      setmem(ad2,val);
      return;
    case absy:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        ADDCYCLES(1);
      setmem(ad2,val);
      return;
    case zp:
      ADDCYCLES(3);
      ad=getmem(pc++);
      setmem(ad,val);
      return;
    case zpx:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case zpy:
      ADDCYCLES(4);
      ad=getmem(pc++);
      ad+=y;
      setmem(ad&0xff,val);
      return;
    case indx:
      ADDCYCLES(6);
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      setmem(ad2,val);
      return;
    case indy:
      ADDCYCLES(5);
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      setmem(ad,val);
      return;
    case acc:
      ADDCYCLES(2);
      a=val;
      return;
  }
}


static inline void setflags(int flag, int cond)
{
  if (cond) p|=flag;
  else p&=~flag;
}


static inline void push(unsigned val, unsigned &s)
{
  setmem(0x100+s,val);
  if (s) s--;
}

static inline unsigned pop(unsigned &s)
{
  if (s<0xff) s++;
  return getmem(0x100+s);
}

static void branch(int flag, unsigned &pc, unsigned &imm)
{
	signed dist;
	unsigned wval;
	dist=(signed)getaddr(imm);
	wval=pc+dist;
	if (flag)
	{
		ADDCYCLES(((pc&0x100)!=(wval&0x100))?2:1);
		pc=wval;
	}
}

// ----------------------------------------------------- ffentliche Routinen

static void cpuReset(c64regs &cr)
{
  cr.a=cr.x=cr.y=0;
  cr.p=0;
  cr.s=255;
  cr.pc=getaddr(0xfffc);
}

static void cpuResetTo(c64regs &cr, unsigned npc)
{
  cr.a=0;
  cr.x=0;
  cr.y=0;
  cr.p=0;
  cr.s=255;
  cr.pc=npc;
}

#define OP(x...)
//do { fprintf(stderr,x); fprintf(stderr,"\n"); } while(0)

static int cpuParse(const c64regs &cr_in)
{
	unsigned opc;
    unsigned wval,bval;
	int cmd, addr, c;
	c64regs cr(cr_in);

	//cycles=0;
	RESETCYCLES();

	//#ifdef TRACE
	//  cpuStatus();
	//  if (opcodes[getmem(pc)]==xxx) getch();
	//#endif

	opc=getmem(cr.pc++);
	cmd=opcodes[opc];
	addr=modes[opc];
//	fprintf(stderr,"$%04x [%02x]: ", pc-1, opc);
	switch (cmd)
	{
	case OPadc:
		OP("adc");
		wval=(uint16_t)a+getaddr(addr)+((cr.p&FLAG_C)?1:0);
		setflags(FLAG_C, wval&0x100);
		cr.a=(uint8_t)wval;
		setflags(FLAG_Z, !cr.a);
		setflags(FLAG_N, cr.a&0x80);
		setflags(FLAG_V, (!!(cr.p&FLAG_C)) ^ (!!(cr.p&FLAG_N)));
		break;
	case OPand:
		OP("and");
		bval=getaddr(addr);
		cr.a&=bval;
		setflags(FLAG_Z, !cr.a);
		setflags(FLAG_N, cr.a&0x80);
		break;
	case OPasl:
		OP("asl");
		wval=getaddr(addr);
		wval<<=1;
		setaddr(addr,wval&0xff);
		setflags(FLAG_Z,!cr.wval);
		setflags(FLAG_N,cr.wval&0x80);
		setflags(FLAG_C,cr.wval&0x100);
		break;
	case OPbcc:
		OP("bcc");
		branch(!(cr.p&FLAG_C));
		break;
	case OPbcs:
		OP("bcs");
		branch(cr.p&FLAG_C);
		break;
	case OPbne:
		OP("bne");
		branch(!(cr.p&FLAG_Z));
		break;
	case OPbeq:
		OP("beq");
		branch(cr.p&FLAG_Z);
		break;
	case OPbpl:
		OP("bpl");
		branch(!(cr.p&FLAG_N));
		break;
	case OPbmi:
		OP("bmi");
		branch(cr.p&FLAG_N);
		break;
	case OPbvc:
		OP("bvc");
		branch(!(cr.p&FLAG_V));
		break;
	case OPbvs:
		OP("bvs");
		branch(cr.p&FLAG_V);
		break;
	case OPbit:
		OP("bit");
		bval=getaddr(addr);
		setflags(FLAG_Z,!(a&bval));
		setflags(FLAG_N,bval&0x80);
		setflags(FLAG_V,bval&0x40);
		break;
	case OPbrk:
		OP("brk");
		push(pc&0xff);
		push(pc>>8);
		push(p);
		setflags(FLAG_B,1);
		pc=getmem(0xfffe);
		ADDCYCLES(7;
		break;
	case OPclc:
		OP("clc");
		ADDCYCLES(2;
		setflags(FLAG_C,0);
		break;
	case OPcld:
		OP("cld");
		ADDCYCLES(2;
		setflags(FLAG_D,0);
		break;
	case OPcli:
		OP("cli");
		ADDCYCLES(2;
		setflags(FLAG_I,0);
		break;
	case OPclv:
		OP("clv");
		ADDCYCLES(2;
		setflags(FLAG_V,0);
		break;
	case OPcmp:
		OP("cmp");
		bval=getaddr(addr);
		wval=(uint16_t)a-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case OPcpx:
		OP("cpx");
		bval=getaddr(addr);
		wval=(uint16_t)x-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case OPcpy:
		OP("cpy");
		bval=getaddr(addr);
		wval=(uint16_t)y-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case OPdec:
		OP("dec");
		bval=getaddr(addr);
		bval--;
		setaddr(addr,bval);
		setflags(FLAG_Z,!bval);
		setflags(FLAG_N,bval&0x80);
		break;
	case OPdex:
		OP("dex");
		ADDCYCLES(2;
		x--;
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case OPdey:
		OP("dey");
		ADDCYCLES(2;
		y--;
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case OPeor:
		OP("eor");
		bval=getaddr(addr);
		a^=bval;
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case OPinc:
		OP("inc");
		bval=getaddr(addr);
		bval++;
		setaddr(addr,bval);
		setflags(FLAG_Z,!bval);
		setflags(FLAG_N,bval&0x80);
		break;
	case OPinx:
		OP("inx");
		ADDCYCLES(2;
		x++;
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case OPiny:
		OP("iny");
		ADDCYCLES(2;
		y++;
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case OPjmp:
		ADDCYCLES(3;
		wval=getmem(pc++);
		wval|=256*getmem(pc++);
		switch (addr)
		{
		case abs:
			OP("jmp $%04x",wval);
			pc=wval;
			break;
		case ind:
			pc=getmem(wval);
			pc|=256*getmem(wval+1);
			OP("jmpr $%04x",pc);
			ADDCYCLES(2;
			break;
		}
		break;
	case OPjsr:
		OP("jsr");
		ADDCYCLES(6;
		push((pc+2));
		push((pc+2)>>8);
		wval=getmem(pc++);
		wval|=256*getmem(pc++);
		pc=wval;
		break;
	case OPlda:
		OP("lda");
		a=getaddr(addr);
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case OPldx:
		OP("ldx");
		x=getaddr(addr);
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case OPldy:
		OP("ldy");
		y=getaddr(addr);
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case OPlsr:
		OP("lsr");
		//bval=wval=getaddr(addr);
		bval=getaddr(addr); wval=(uint8_t)bval;
		wval>>=1;
		setaddr(addr,(uint8_t)wval);
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,bval&1);
		break;
	case OPnop:
		OP("nop");
		ADDCYCLES(2;
		break;
	case OPora:
		OP("ora");
		bval=getaddr(addr);
		a|=bval;
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case OPpha:
		OP("pha");
		push(a);
		ADDCYCLES(3;
		break;
	case OPphp:
		OP("php");
		push(p);
		ADDCYCLES(3;
		break;
	case OPpla:
		OP("pla");
		a=pop();
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		ADDCYCLES(4;
		break;
	case OPplp:
		OP("plp");
		p=pop();
		ADDCYCLES(4;
		break;
	case OProl:
		OP("rol");
		bval=getaddr(addr);
		c=!!(p&FLAG_C);
		setflags(FLAG_C,bval&0x80);
		bval<<=1;
		bval|=c;
		setaddr(addr,bval);
		setflags(FLAG_N,bval&0x80);
		setflags(FLAG_Z,!bval);
		break;
	case OPror:
		OP("ror");
		bval=getaddr(addr);
		c=!!(p&FLAG_C);
		setflags(FLAG_C,bval&1);
		bval>>=1;
		bval|=128*c;
		setaddr(addr,bval);
		setflags(FLAG_N,bval&0x80);
		setflags(FLAG_Z,!bval);
		break;
	case OPrti:
		OP("rti");
		// NEU, rti wie rts, au�r das alle register wieder vom stack kommen
		//p=pop();
		p=pop();
		y=pop();
		x=pop();
		a=pop();
		// in_nmi = false;
		//write_console("NMI EXIT!");
	case OPrts:
		OP("rts");
		wval=256*pop();
		wval|=pop();
		pc=wval;
		ADDCYCLES(6;
		break;
	case OPsbc:
		OP("sbc");
		bval=getaddr(addr)^0xff;
		wval=(uint16_t)a+bval+((p&FLAG_C)?1:0);
		setflags(FLAG_C, wval&0x100);
		a=(uint8_t)wval;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a>127);
		setflags(FLAG_V, (!!(p&FLAG_C)) ^ (!!(p&FLAG_N)));
		break;
	case OPsec:
		OP("sec");
		ADDCYCLES(2;
		setflags(FLAG_C,1);
		break;
	case OPsed:
		OP("sec");
		ADDCYCLES(2;
		setflags(FLAG_D,1);
		break;
	case OPsei:
		OP("sei");
		ADDCYCLES(2;
		setflags(FLAG_I,1);
		break;
	case OPsta:
		OP("sta");
		putaddr(addr,a);
		break;
	case OPstx:
		OP("stx");
		putaddr(addr,x);
		break;
	case OPsty:
		OP("sty");
		putaddr(addr,y);
		break;
	case OPtax:
		OP("tax");
		ADDCYCLES(2;
		x=a;
		setflags(FLAG_Z, !x);
		setflags(FLAG_N, x&0x80);
		break;
	case OPtay:
		OP("tay");
		ADDCYCLES(2;
		y=a;
		setflags(FLAG_Z, !y);
		setflags(FLAG_N, y&0x80);
		break;
	case OPtsx:
		OP("tsx");
		ADDCYCLES(2;
		x=s;
		setflags(FLAG_Z, !x);
		setflags(FLAG_N, x&0x80);
		break;
	case OPtxa:
		OP("txa");
		ADDCYCLES(2;
		a=x;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		break;
	case OPtxs:
		OP("txs");
		ADDCYCLES(2;
		s=x;
		break;
	case OPtya:
		OP("tya");
		ADDCYCLES(2;
		a=y;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		break;
	}

	return cycles;
}

int cpuJSR(c64regs &cr, unsigned npc, unsigned na)
{
  int ccl;
  
  cr.a=na;
  cr.x=0;
  cr.y=0;
  cr.p=0;
  cr.s=255;
  cr.pc=npc;
  push(cr,0);
  push(cr,0);
  ccl=0;
  while (cr.pc)
  {
    ccl+=cpuParse(cr);
  }  
  return ccl;
}

static unsigned short LoadSIDFromMemory(const void *pSidData, unsigned short *load_addr,
								 unsigned short *init_addr, unsigned short *play_addr,
								 unsigned char *subsongs, unsigned char *startsong,
								 unsigned char *speed, unsigned short size)
{
	unsigned char *pData;
    unsigned char data_file_offset;

    pData = (unsigned char*)pSidData;
    data_file_offset = pData[7];

    *load_addr = pData[8]<<8;
    *load_addr|= pData[9];

    *init_addr = pData[10]<<8;
    *init_addr|= pData[11];

    *play_addr = pData[12]<<8;
    *play_addr|= pData[13];

    *subsongs = pData[0xf]-1;
    *startsong = pData[0x11]-1;

    *load_addr = pData[data_file_offset];
    *load_addr|= pData[data_file_offset+1]<<8;
        
    *speed = pData[0x15];
    
    memset(memory, 0, sizeof(memory));
    memcpy(&memory[*load_addr], &pData[data_file_offset+2], size-(data_file_offset+2));
    
    if (*play_addr == 0)
    {
        cpuJSR(*init_addr, 0);
        *play_addr = (memory[0x0315]<<8)+memory[0x0314];
    }

    return *load_addr;
}

static void dumpregs()
{
	int i;
	Serial.print("R: ");
	for (i=0;i<25;i++) {
		Serial.print(sidregs[i]);
		Serial.print(" ");
	}
    Serial.println("");
}

void _zpu_interrupt() {
	tick=1;
	TMR0CTL &= ~(BIT(TCTLIF));
}
static unsigned short init_addr;
void setup()
{
	unsigned short load_addr;
	unsigned char subsongs,startsong,speed;
	SmallFSFile file;

	Serial.begin(115200);
	Serial.println("Starting");
	if (SmallFS.begin()==0) {
		file = SmallFS.open("music.sid");
		if (file.valid()) {
			unsigned size = file.size();
            unsigned char *buf = (unsigned char*)malloc(size);
			/* Allocate */
			file.read(buf,size);

			LoadSIDFromMemory(buf,
							  &load_addr,
							  &init_addr,
							  &play_addr,
							  &subsongs,
							  &startsong,
							  &speed,
							  size);

		}
	}
	else {
		Serial.println("Cannot load tune!");
		while (1) {
		};
	}

	cpuReset();
	memset(sidregs,0,sizeof(sidregs));

#ifdef RETROCADE
	//Move the audio output to the appropriate pins on the Papilio Hardware
	pinMode(AUDIO_J1_L,OUTPUT);
	digitalWrite(AUDIO_J1_L,HIGH);
	//outputPinForFunction(AUDIO_J1_L, IOPIN_SIGMADELTA0);
	outputPinForFunction(AUDIO_J1_L, 8);
	pinModePPS(AUDIO_J1_L, HIGH);

	pinMode(AUDIO_J1_R,OUTPUT);
	digitalWrite(AUDIO_J1_R,HIGH);
	outputPinForFunction(AUDIO_J1_R, 8);
	//outputPinForFunction(AUDIO_J1_R, IOPIN_SIGMADELTA1);
	pinModePPS(AUDIO_J1_R, HIGH);

	pinMode(AUDIO_J2_L,OUTPUT);
	digitalWrite(AUDIO_J2_L,HIGH);
	outputPinForFunction(AUDIO_J2_L, 8);
	pinModePPS(AUDIO_J2_L, HIGH);

	pinMode(AUDIO_J2_R,OUTPUT);
	digitalWrite(AUDIO_J2_R,HIGH);
	outputPinForFunction(AUDIO_J2_R, 8);
	pinModePPS(AUDIO_J2_R, HIGH);
#else

	pinMode(WING_C_0,OUTPUT);
	digitalWrite(WING_C_0,HIGH);
	outputPinForFunction(WING_C_0, 8);
	pinModePPS(WING_C_0, HIGH);

	pinMode(WING_C_1,OUTPUT);
	digitalWrite(WING_C_1,HIGH);
	outputPinForFunction(WING_C_1, 8);
	pinModePPS(WING_C_1, HIGH);

#endif

	cpuJSR(init_addr, 0);
	// Prescale 64 (101b), generate 50Hz tick

	TMR0CMP = (CLK_FREQ/(50*64))-1;
	TMR0CNT = 0x0;
	TMR0CTL = BIT(TCTLENA)|BIT(TCTLCCM)|BIT(TCTLDIR)|BIT(TCTLCP2)|BIT(TCTLCP0)|BIT(TCTLIEN);
	INTRMASK = BIT(INTRLINE_TIMER0); // Enable Timer0 interrupt
	INTRCTL=1;
}

void loop()
{
	while (1) {
		int clocks = cpuJSR(play_addr,0);
		//printf("Ticck? %d\n",clocks);
		tick=0;
		while (!tick);
		/* Write SID regs here */
		//dumpregs();
		int i;
		for (i=0;i<25;i++) {
			sid.writeData(i, sidregs[i]);
		}
		if (Serial.available()) {
			int r=Serial.read();
			if (r=='e') {
				Serial.println("Enable");
				sid.writeData(25,1);
			}
			if (r=='d') {
				Serial.println("Disable");
				sid.writeData(25,0);
			}
			if (r=='r') {
				// Restart
				Serial.println("Restart");
				cpuReset();
				memset(sidregs,0,sizeof(sidregs));
				cpuJSR(init_addr, 0);
			}
		}
	}
}
