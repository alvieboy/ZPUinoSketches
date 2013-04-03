#ifdef __linux__

#include "./original/webplatformsid/sidtune_Jump_Out.h"
#include <stdio.h>

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;

typedef uint8_t byte;
typedef uint16_t word;

#define FLAG_N 128
#define FLAG_V 64
#define FLAG_B 16
#define FLAG_D 8
#define FLAG_I 4
#define FLAG_Z 2
#define FLAG_C 1


unsigned char memory[65535];

enum {adc, and, asl, bcc, bcs, beq, bit, bmi, bne, bpl, brk, bvc, bvs, clc,
  cld, cli, clv, cmp, cpx, cpy, dec, dex, dey, eor, inc, inx, iny, jmp,
  jsr, lda, ldx, ldy, lsr, nop, ora, pha, php, pla, plp, rol, ror, rti,
  rts, sbc, sec, sed, sei, sta, stx, sty, tax, tay, tsx, txa, txs, tya,
  xxx};

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

static uint8_t opcodes[256]= {
  brk,ora,xxx,xxx,xxx,ora,asl,xxx,php,ora,asl,xxx,xxx,ora,asl,xxx,
  bpl,ora,xxx,xxx,xxx,ora,asl,xxx,clc,ora,xxx,xxx,xxx,ora,asl,xxx,
  jsr,and,xxx,xxx,bit,and,rol,xxx,plp,and,rol,xxx,bit,and,rol,xxx,
  bmi,and,xxx,xxx,xxx,and,rol,xxx,sec,and,xxx,xxx,xxx,and,rol,xxx,
  rti,eor,xxx,xxx,xxx,eor,lsr,xxx,pha,eor,lsr,xxx,jmp,eor,lsr,xxx,
  bvc,eor,xxx,xxx,xxx,eor,lsr,xxx,cli,eor,xxx,xxx,xxx,eor,lsr,xxx,
  rts,adc,xxx,xxx,xxx,adc,ror,xxx,pla,adc,ror,xxx,jmp,adc,ror,xxx,
  bvs,adc,xxx,xxx,xxx,adc,ror,xxx,sei,adc,xxx,xxx,xxx,adc,ror,xxx,
  xxx,sta,xxx,xxx,sty,sta,stx,xxx,dey,xxx,txa,xxx,sty,sta,stx,xxx,
  bcc,sta,xxx,xxx,sty,sta,stx,xxx,tya,sta,txs,xxx,xxx,sta,xxx,xxx,
  ldy,lda,ldx,xxx,ldy,lda,ldx,xxx,tay,lda,tax,xxx,ldy,lda,ldx,xxx,
  bcs,lda,xxx,xxx,ldy,lda,ldx,xxx,clv,lda,tsx,xxx,ldy,lda,ldx,xxx,
  cpy,cmp,xxx,xxx,cpy,cmp,dec,xxx,iny,cmp,dex,xxx,cpy,cmp,dec,xxx,
  bne,cmp,xxx,xxx,xxx,cmp,dec,xxx,cld,cmp,xxx,xxx,xxx,cmp,dec,xxx,
  cpx,sbc,xxx,xxx,cpx,sbc,inc,xxx,inx,sbc,nop,xxx,cpx,sbc,inc,xxx,
  beq,sbc,xxx,xxx,xxx,sbc,inc,xxx,sed,sbc,xxx,xxx,xxx,sbc,inc,xxx
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

static int cycles;
static byte bval;
static word wval;

// ----------------------------------------------------------------- Register

static byte a,x,y,s,p;
static word pc;

// ----------------------------------------------------------- DER HARTE KERN

static byte sidregs[32];

static byte getmem(word addr)
{
	if (addr == 0xdd0d) memory[addr]=0;
	//printf("READ %04x = %04x\n", addr, memory[addr]);
	return memory[addr];
}

static void writeSIDreg(int reg, int value)
{
	sidregs[reg] = value;
}

static setmem(word addr, byte value)
{
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

static byte getaddr(int mode)
{
  word ad,ad2;  
  switch(mode)
  {
    case imp:
      cycles+=2;
      return 0;
    case imm:
      cycles+=2;
      return getmem(pc++);
    case abs:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      return getmem(ad);
    case absx:
      cycles+=4;
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+x;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad2);
    case absy:
      cycles+=4;
      ad=getmem(pc++);
      ad|=256*getmem(pc++);
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad2);
    case zp:
      cycles+=3;
      ad=getmem(pc++);
      return getmem(ad);
    case zpx:
      cycles+=4;
      ad=getmem(pc++);
      ad+=x;
      return getmem(ad&0xff);
    case zpy:
      cycles+=4;
      ad=getmem(pc++);
      ad+=y;
      return getmem(ad&0xff);
    case indx:
      cycles+=6;
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      return getmem(ad2);
    case indy:
      cycles+=5;
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      return getmem(ad);
    case acc:
      cycles+=2;
      return a;
  }  
  return 0;
}


static void setaddr(int mode, byte val)
{
  word ad,ad2;
  switch(mode)
  {
    case abs:
      cycles+=2;
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      setmem(ad,val);
      return;
    case absx:
      cycles+=3;
      ad=getmem(pc-2);
      ad|=256*getmem(pc-1);
      ad2=ad+x;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles--;
      setmem(ad2,val);
      return;
    case zp:
      cycles+=2;
      ad=getmem(pc-1);
      setmem(ad,val);
      return;
    case zpx:
      cycles+=2;
      ad=getmem(pc-1);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case acc:
      a=val;
      return;
  }
}


static void putaddr(int mode, byte val)
{
  word ad,ad2;
  switch(mode)
  {
    case abs:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      setmem(ad,val);
      return;
    case absx:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+x;
      setmem(ad2,val);
      return;
    case absy:
      cycles+=4;
      ad=getmem(pc++);
      ad|=getmem(pc++)<<8;
      ad2=ad+y;
      if ((ad2&0xff00)!=(ad&0xff00))
        cycles++;
      setmem(ad2,val);
      return;
    case zp:
      cycles+=3;
      ad=getmem(pc++);
      setmem(ad,val);
      return;
    case zpx:
      cycles+=4;
      ad=getmem(pc++);
      ad+=x;
      setmem(ad&0xff,val);
      return;
    case zpy:
      cycles+=4;
      ad=getmem(pc++);
      ad+=y;
      setmem(ad&0xff,val);
      return;
    case indx:
      cycles+=6;
      ad=getmem(pc++);
      ad+=x;
      ad2=getmem(ad&0xff);
      ad++;
      ad2|=getmem(ad&0xff)<<8;
      setmem(ad2,val);
      return;
    case indy:
      cycles+=5;
      ad=getmem(pc++);
      ad2=getmem(ad);
      ad2|=getmem((ad+1)&0xff)<<8;
      ad=ad2+y;
      setmem(ad,val);
      return;
    case acc:
      cycles+=2;
      a=val;
      return;
  }
}


static inline void setflags(int flag, int cond)
{
  // cond?p|=flag:p&=~flag;
  if (cond) p|=flag;
  else p&=~flag;
}


static inline void push(byte val)
{
  setmem(0x100+s,val);
  if (s) s--;
}

static inline byte pop()
{
  if (s<0xff) s++;
  return getmem(0x100+s);
}

static void branch(int flag)
{
  signed char dist;
  dist=(signed char)getaddr(imm);
  wval=pc+dist;
  if (flag)
  {
    cycles+=((pc&0x100)!=(wval&0x100))?2:1;
    pc=wval;
  }
}

// ----------------------------------------------------- ffentliche Routinen

void cpuReset()
{
  a=x=y=0;
  p=0;
  s=255;
  pc=getaddr(0xfffc);
}

void cpuResetTo(word npc)
{
  a=0;
  x=0;
  y=0;
  p=0;
  s=255;
  pc=npc;
}

#define OP(x...)
//do { fprintf(stderr,x); fprintf(stderr,"\n"); } while(0)

int cpuParse()
{
	byte opc;
	int cmd, addr, c;
	cycles=0;

	//#ifdef TRACE
	//  cpuStatus();
	//  if (opcodes[getmem(pc)]==xxx) getch();
	//#endif

	opc=getmem(pc++);
	cmd=opcodes[opc];
	addr=modes[opc];
//	fprintf(stderr,"$%04x [%02x]: ", pc-1, opc);
	switch (cmd)
	{
	case adc:
		OP("adc");
		wval=(word)a+getaddr(addr)+((p&FLAG_C)?1:0);
		setflags(FLAG_C, wval&0x100);
		a=(byte)wval;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		setflags(FLAG_V, (!!(p&FLAG_C)) ^ (!!(p&FLAG_N)));
		break;
	case and:
		OP("and");
		bval=getaddr(addr);
		a&=bval;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		break;
	case asl:
		OP("asl");
		wval=getaddr(addr);
		wval<<=1;
		setaddr(addr,(byte)wval);
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,wval&0x100);
		break;
	case bcc:
		OP("bcc");
		branch(!(p&FLAG_C));
		break;
	case bcs:
		OP("bcs");
		branch(p&FLAG_C);
		break;
	case bne:
		OP("bne");
		branch(!(p&FLAG_Z));
		break;
	case beq:
		OP("beq");
		branch(p&FLAG_Z);
		break;
	case bpl:
		OP("bpl");
		branch(!(p&FLAG_N));
		break;
	case bmi:
		OP("bmi");
		branch(p&FLAG_N);
		break;
	case bvc:
		OP("bvc");
		branch(!(p&FLAG_V));
		break;
	case bvs:
		OP("bvs");
		branch(p&FLAG_V);
		break;
	case bit:
		OP("bit");
		bval=getaddr(addr);
		setflags(FLAG_Z,!(a&bval));
		setflags(FLAG_N,bval&0x80);
		setflags(FLAG_V,bval&0x40);
		break;
	case brk:
		OP("brk");
		push(pc&0xff);
		push(pc>>8);
		push(p);
		setflags(FLAG_B,1);
		pc=getmem(0xfffe);
		cycles+=7;
		break;
	case clc:
		OP("clc");
		cycles+=2;
		setflags(FLAG_C,0);
		break;
	case cld:
		OP("cld");
		cycles+=2;
		setflags(FLAG_D,0);
		break;
	case cli:
		OP("cli");
		cycles+=2;
		setflags(FLAG_I,0);
		break;
	case clv:
		OP("clv");
		cycles+=2;
		setflags(FLAG_V,0);
		break;
	case cmp:
		OP("cmp");
		bval=getaddr(addr);
		wval=(word)a-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case cpx:
		OP("cpx");
		bval=getaddr(addr);
		wval=(word)x-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case cpy:
		OP("cpy");
		bval=getaddr(addr);
		wval=(word)y-bval;
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,a>=bval);
		break;
	case dec:
		OP("dec");
		bval=getaddr(addr);
		bval--;
		setaddr(addr,bval);
		setflags(FLAG_Z,!bval);
		setflags(FLAG_N,bval&0x80);
		break;
	case dex:
		OP("dex");
		cycles+=2;
		x--;
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case dey:
		OP("dey");
		cycles+=2;
		y--;
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case eor:
		OP("eor");
		bval=getaddr(addr);
		a^=bval;
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case inc:
		OP("inc");
		bval=getaddr(addr);
		bval++;
		setaddr(addr,bval);
		setflags(FLAG_Z,!bval);
		setflags(FLAG_N,bval&0x80);
		break;
	case inx:
		OP("inx");
		cycles+=2;
		x++;
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case iny:
		OP("iny");
		cycles+=2;
		y++;
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case jmp:
		cycles+=3;
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
			cycles+=2;
			break;
		}
		break;
	case jsr:
		OP("jsr");
		cycles+=6;
		push((pc+2));
		push((pc+2)>>8);
		wval=getmem(pc++);
		wval|=256*getmem(pc++);
		pc=wval;
		break;
	case lda:
		OP("lda");
		a=getaddr(addr);
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case ldx:
		OP("ldx");
		x=getaddr(addr);
		setflags(FLAG_Z,!x);
		setflags(FLAG_N,x&0x80);
		break;
	case ldy:
		OP("ldy");
		y=getaddr(addr);
		setflags(FLAG_Z,!y);
		setflags(FLAG_N,y&0x80);
		break;
	case lsr:
		OP("lsr");
		//bval=wval=getaddr(addr);
		bval=getaddr(addr); wval=(byte)bval;
		wval>>=1;
		setaddr(addr,(byte)wval);
		setflags(FLAG_Z,!wval);
		setflags(FLAG_N,wval&0x80);
		setflags(FLAG_C,bval&1);
		break;
	case nop:
		OP("nop");
		cycles+=2;
		break;
	case ora:
		OP("ora");
		bval=getaddr(addr);
		a|=bval;
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		break;
	case pha:
		OP("pha");
		push(a);
		cycles+=3;
		break;
	case php:
		OP("php");
		push(p);
		cycles+=3;
		break;
	case pla:
		OP("pla");
		a=pop();
		setflags(FLAG_Z,!a);
		setflags(FLAG_N,a&0x80);
		cycles+=4;
		break;
	case plp:
		OP("plp");
		p=pop();
		cycles+=4;
		break;
	case rol:
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
	case ror:
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
	case rti:
		OP("rti");
		// NEU, rti wie rts, au�r das alle register wieder vom stack kommen
		//p=pop();
		p=pop();
		y=pop();
		x=pop();
		a=pop();
		// in_nmi = false;
		//write_console("NMI EXIT!");
	case rts:
		OP("rts");
		wval=256*pop();
		wval|=pop();
		pc=wval;
		cycles+=6;
		break;
	case sbc:
		OP("sbc");
		bval=getaddr(addr)^0xff;
		wval=(word)a+bval+((p&FLAG_C)?1:0);
		setflags(FLAG_C, wval&0x100);
		a=(byte)wval;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a>127);
		setflags(FLAG_V, (!!(p&FLAG_C)) ^ (!!(p&FLAG_N)));
		break;
	case sec:
		OP("sec");
		cycles+=2;
		setflags(FLAG_C,1);
		break;
	case sed:
		OP("sec");
		cycles+=2;
		setflags(FLAG_D,1);
		break;
	case sei:
		OP("sei");
		cycles+=2;
		setflags(FLAG_I,1);
		break;
	case sta:
		OP("sta");
		putaddr(addr,a);
		break;
	case stx:
		OP("stx");
		putaddr(addr,x);
		break;
	case sty:
		OP("sty");
		putaddr(addr,y);
		break;
	case tax:
		OP("tax");
		cycles+=2;
		x=a;
		setflags(FLAG_Z, !x);
		setflags(FLAG_N, x&0x80);
		break;
	case tay:
		OP("tay");
		cycles+=2;
		y=a;
		setflags(FLAG_Z, !y);
		setflags(FLAG_N, y&0x80);
		break;
	case tsx:
		OP("tsx");
		cycles+=2;
		x=s;
		setflags(FLAG_Z, !x);
		setflags(FLAG_N, x&0x80);
		break;
	case txa:
		OP("txa");
		cycles+=2;
		a=x;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		break;
	case txs:
		OP("txs");
		cycles+=2;
		s=x;
		break;
	case tya:
		OP("tya");
		cycles+=2;
		a=y;
		setflags(FLAG_Z, !a);
		setflags(FLAG_N, a&0x80);
		break;
	}

	return cycles;
}

int cpuJSR(word npc, byte na)
{
  int ccl;
  
  a=na;
  x=0;
  y=0;
  p=0;
  s=255;
  pc=npc;
  push(0);
  push(0);
  ccl=0;    
  while (pc)
  {
    ccl+=cpuParse();
  }  
  return ccl;
}

unsigned short LoadSIDFromMemory(const void *pSidData, unsigned short *load_addr,
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
	printf("R: ");
	for (i=0;i<23;i++) {
		printf("%02x ",sidregs[i]);
	}
    printf("\n");
}

#define SIDTUNE sidtune_Jump_Out

int main(int argc,char **argv)
{
	unsigned short load_addr,init_addr,play_addr;
	unsigned char subsongs,startsong,speed;
	unsigned char *b;

	FILE *f = fopen(argv[1],"r");
	if (f==NULL)
		return;
	fseek(f,0,SEEK_END);
	long size = ftell(f);
	rewind(f);
	b=malloc(size);

	fread(b, size, 1, f);
	fclose(f);

	LoadSIDFromMemory(//  b,
					  sidtune_Jump_Out,
					  &load_addr,
					  &init_addr,
					  &play_addr,
					  &subsongs,
					  &startsong,
					  &speed,
					  sizeof(sidtune_Jump_Out));
					  //size);

	printf("Load address: %04x\n",load_addr);
    printf("Play address: %04x\n",play_addr);

	cpuReset();
    memset(sidregs,0,sizeof(sidregs));

	cpuJSR(init_addr, 0);

	while (1) {
		int clocks = cpuJSR(play_addr,0);
		//printf("Ticck? %d\n",clocks);
		dumpregs();
	}
}
#endif
