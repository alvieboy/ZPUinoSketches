/***********************************************************
 *
 * JSW 48K implementation for ZPUino with VGA ZX interface
 *
 * (C) Alvaro Lopes 2012
 *
 * This code released under Creative Commons BY-NC-SA
 *
 * None of the additional files (ROM's and other resources) are
 * allowed for commercial use.
 *
 * Based on information found in http://mdfs.net/Software/JSW/
 * ROM extracted from http://www.worldofspectrum.org
 *
 * Original ZX Spectrum ROM used by permission:
 *   http://www.worldofspectrum.org/permits/amstrad-roms.txt
 *
 * " Amstrad have kindly given their permission for the
 *   redistribution of their copyrighted material but retain
 *   that copyright "
 *
 *
 * Based on commented disassembly as seen below:
 *
 *****
 ***** Based on: Jet Set Willy JSW48 Game Engine Source
 ***** ======================================
 ***** JSW Copyright (C) 1984 Matthew Smith & Software Projects
 *****
 ***** Commentary Copyright (C) 1985, 2004 J.G.Harston
 *****  See http://mdfs.net/Software/JSW/
 ***** The source is assembleable with ZMac
 ***** See http://mdfs.net/Software/Z80/ZMac
 *****
 ***** This was originally a commented disassembly, but it became easier to type
 ***** up as a source file and create the disassembly from it.
 *****
 **********************************************************
*/

#include <SmallFS.h>

// Define this is you use the ZPUino simulator
#define SIMULATION

#define STATIC static

#ifdef SIMULATION

#define JUMP_INVERT
#define LEFT_INVERT
#define RIGHT_INVERT
#define JUMP_PIN 25
#define LEFT_PIN 27
#define RIGHT_PIN 24

#else

// Hephaestus (megawing)

#define JUMP_PIN 36
#define JUMP_INVERT
#define LEFT_PIN 39
#define LEFT_INVERT
#define RIGHT_PIN 38
#define RIGHT_INVERT


/*#define JUMP_PIN WING_C_3
 #define JUMP_INVERT
 #define LEFT_PIN WING_C_5
 #define LEFT_INVERT
 #define RIGHT_PIN WING_C_1
 #define RIGHT_INVERT
 */
#endif

/* Some debugging functions */

void printnibble(unsigned int c)
{
	c&=0xf;
	if (c>9)
		Serial.write(c+'a'-10);
	else
		Serial.write(c+'0');
}

void printhexbyte(unsigned int c)
{
	printnibble(c>>4);
	printnibble(c);
}

void printhex(unsigned int c)
{
	printhexbyte(c>>24);
	printhexbyte(c>>16);
	printhexbyte(c>>8);
	printhexbyte(c);
}


// This converts a attrbuffer offset to a framebuffer offset

#define ATTRBUFFER_OFFSET_TO_FRAMEBUFFER_OFFSET(x)  \
	( ( (x) &0x1F) /* X offset */ + (((x)<<3) & ~0xFF) )

// Our main framebuffer

static unsigned char framebuffer[32*24*8];
static unsigned char pallete[32*24];

static unsigned int GAMEDELAY=50;


struct ROOM_s {
	unsigned char def[128];       // OK
	unsigned char name[32];       // OK
	unsigned char background[9];  // OK
	unsigned char floor[9];       // OK
	unsigned char wall[9];        // OK
	unsigned char nasty[9];       // OK
	unsigned char slope[9];       // OK
	unsigned char conveyor[9];    // OK
	unsigned char conv_dir;       // OK
	unsigned char conv_psn[2];    // OK
	unsigned char conv_num;       // OK
	unsigned char slope_dir;      // OK
	unsigned char slope_psn[2];   // OK
	unsigned char slope_num;      // OK
	unsigned char border[3];      // OK
	unsigned char object[8];
	unsigned char left;
	unsigned char right;
	unsigned char up;
	unsigned char down;
	unsigned char willisp[3]; 
	unsigned char instances[16];
} __attribute__((packed));

static struct ROOM_s ROOM;

static unsigned char guardian[66] ={
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xFF,0xFB
};

static unsigned char ROPE[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,   // Rope X offsets
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	1,1,1,1,1,1,1,1,1,1,1,1,2,2,2,2,
	2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
	2,2,1,2,2,1,1,2,1,1,2,2,3,2,3,2,
	3,3,3,3,3,3,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,   // Rope Y offsets
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,
	4,6,6,4,6,4,6,4,6,4,4,4,6,4,4,4,
	4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
	4,4,4,4,4,4,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};


static unsigned char HERE; // Current room number

// Bitmaps for triangle characters
// -------------------------------
static const unsigned char TBITMAP[] ={
	0xC0,0xF0,0xFC,0xFF,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x00,0x00,0xC0,0xF0,0xFC,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFC,0xF0,0xC0,0x00,
	0xFC,0xF0,0xC0,0x00,0x00,0x00,0x00,0x00};

const char AIR[] = "AIR";

const char MESSAGE[] =
	"+++++ Press ENTER to Start +++++"
	"  JET-SET WILLY by Matthew Smith  "
	"\177 1984 SOFTWARE PROJECTS Ltd "
	". . . . .Guide Willy to collect "
	"all the items around the house "
	"before Midnight so Maria will let "
	"you get to your bed. . . . . . ."
	"+++++ Press ENTER to Start +++++";

char ITEMS[] = "Items collected 000 Time 00:00 m";
const char GAME[] = "Game";
const char OVER[] = "Over";
char COLLECTED[] = "000";
char NOWTIME[] = " 7:00a";
char STARTTIME[] = " 7:00a";

// ====================
unsigned char TICKER=0;
unsigned char LIVES=0;
unsigned char FLASH=0;

// Current Willy state
// ===================
struct willy_state {
	unsigned int  YPOSN;
	unsigned char DIRECTION; //L85D0;
	unsigned char FALLING;
	unsigned char FRAME;
	unsigned int  POSITION;
	unsigned char JUMPING;
	unsigned char ONROPE;
};

struct willy_state wstate;
struct willy_state save_wstate;

unsigned char REMAIN=0;
unsigned char STATUS=0;
unsigned char COUNTDOWN=0;
unsigned char MTICK=0;
unsigned char MFLAGS=0;
unsigned char TELEPORT;
unsigned char TEMP;
unsigned int willy_offset=0;

int newroom=1; // Set to 1 to load a new room.

static unsigned char sprite[32]; // Willy sprite, used across the code

const unsigned char MOONLIGHT[] = {
	0x51,0x3C,0x33,0x51,0x3C,0x33,0x51,0x3C,0x33,0x51,0x3C,0x33,
	0x51,0x3C,0x33,0x51,0x3C,0x33,0x51,0x3C,0x33,0x51,0x3C,0x33,
	0x4C,0x3C,0x33,0x4C,0x3C,0x33,0x4C,0x39,0x2D,0x4C,0x39,0x2D,
	0x51,0x40,0x2D,0x51,0x3C,0x33,0x51,0x3C,0x36,0x5B,0x40,0x36,
	0x66,0x51,0x3C,0x51,0x3C,0x33,0x51,0x3C,0x33,0x28,0x3C,0x28,
	0x28,0x36,0x2D,0x51,0x36,0x2D,0x51,0x36,0x2D,0x28,0x36,0x28,
	0x28,0x3C,0x33,0x51,0x3C,0x33,0x26,0x3C,0x2D,0x4C,0x3C,0x2D,
	0x28,0x40,0x33,0x51,0x40,0x33,0x2D,0x40,0x36,0x20,0x40,0x36,
	0x3D,0x79,0x3D,0xFF	};	// Terminated with &FF

const unsigned char RICHMAN[] = {
	0x56,0x60,0x56,0x60,0x66,0x66,0x80,0x80,0x80,0x80,0x66,0x60,
	0x56,0x60,0x56,0x60,0x66,0x60,0x56,0x4C,0x48,0x4C,0x48,0x4C,
	0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x56,0x40,0x40,0x40,0x40,
	0x44,0x44,0x4C,0x4C,0x56,0x60,0x66,0x60,0x56,0x56,0x66,0x66,
	0x51,0x56,0x60,0x56,0x51,0x51,0x60,0x60,0x40,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40 };   // 64 bytes long, just loops


#define VGABASE IO_SLOT(8)
#define COLUMNS 32
#define ROWS 24

#define RWIDTH 32*8

//SmallFSFile charmap;
SmallFSFile jswrom;


unsigned char *btarget;

static inline void blit8x8(unsigned char value, void*)
{
	*btarget=value;
	btarget+=COLUMNS;
}


static inline void putChar(unsigned char value)
{
	jswrom.seek( (value*8) + 0x3C00,SEEK_SET);
	jswrom.readCallback(8, &blit8x8, NULL);
	btarget-=((8*COLUMNS)-1);
}

static inline void putString(const char *p)
{
	while (*p) {
		putChar(*p++);
	}
}

STATIC void drawpower()
{
	unsigned off = 21*COLUMNS + 20;   // Line 21, column 20
	memset( &pallete[off], 0x7, 10);
	PRMESSAGE( &framebuffer[ ATTRBUFFER_OFFSET_TO_FRAMEBUFFER_OFFSET(off) ], "Powered by", 10);
	off+=COLUMNS;
	memset( &pallete[off], 0x5, 10);
	PRMESSAGE( &framebuffer[ ATTRBUFFER_OFFSET_TO_FRAMEBUFFER_OFFSET(off) ], " ZPUino32 ", 10);
	
}

STATIC void updatepower()
{

	unsigned off = 22*COLUMNS + 20;   // Line 21, column 20
	unsigned i;
	for (i=10;i!=0;i--) {
		pallete[off] = ( pallete[off] & 0xf8 ) | // Lose INK
			(TICKER & 0x3) + 0x3;
		off++;
	}

}


STATIC void PRMATRIX(const unsigned char *source, unsigned char *dest)
{
	// Print a 8x8 matrix on screen. This loop is fully unrolled.
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source++; dest+=COLUMNS;
	*dest = *source;
}


/* Update conveyors on screen */

STATIC void UPDATECONV()
{
	if (ROOM.conv_num==0)
		return;

	unsigned v = ROOM.conveyor[1];

	if (ROOM.conv_dir==0) {
		// Left
		v<<=2;
		v|=(v>>8);
		ROOM.conveyor[1] = v;
	} else {
		v|=(v<<8);
		v>>=2;
		ROOM.conveyor[1] = v;
	}

	v = ROOM.conveyor[3];

	if (ROOM.conv_dir==0) {

		// opposite
        v|=(v<<8);
		v>>=2;
        
		ROOM.conveyor[3] = v;
	} else {
		v<<=2;
		v|=(v>>8);

		ROOM.conveyor[3] = v;
	}
}

/*
 Draw a sprite (32x32) on the screen. Eventually check for collisions
 */

STATIC int drawsprite(const unsigned char *source, unsigned char*dest, int check_collision)
{
	int i;
	unsigned sprite_byte;
	for (i=16;i!=0;i--) {
		sprite_byte = *source;
		if (check_collision) {
			if (sprite_byte & (*dest)) {
				return 1;
			}
			sprite_byte |= *dest;
		}
		*dest = sprite_byte;
		dest++;
		source++;
		sprite_byte=*source;
		if (check_collision) {
			if (sprite_byte & (*dest)) {
				return 1;
			}
		}
		*dest = sprite_byte;
		dest--;
		dest+=COLUMNS;
		source++;
	}
    return 0;
}

STATIC void loadscr(SmallFSFile &scr)
{
	// SCR are tricky

	unsigned i,j,z;
	for (z=0;z<3;z++) {
		unsigned char *fbptr= &framebuffer[COLUMNS*(z*8*8)];

		for (j=0;j<8;j++) {
			unsigned char *tbptr=fbptr;
			for (i=0;i<8;i++) {
				scr.read( tbptr, COLUMNS );
				tbptr+=COLUMNS*8;
			}
			fbptr+=COLUMNS;
		}
	}
	// Seek to pallete (not nedded, I think)
	scr.seek( sizeof(framebuffer), SEEK_SET);
	scr.read( &pallete[0], sizeof(pallete));

}

STATIC void opening()
{
	SmallFSFile scr;
	scr = SmallFS.open("ZPUINO");
	if (scr.valid()) {

		loadscr(scr);
		// Wait for jump key

		while (!( JUMP_INVERT digitalRead(JUMP_PIN))) {
			delay(100);
		}
	}
}


void setup()
{
	Serial.begin(115200);

	REGISTER(VGABASE,0) = (unsigned)framebuffer;
	REGISTER(VGABASE,1) = (unsigned)pallete;
	Serial.println("JSW starting");
	SmallFS.begin();

	jswrom = SmallFS.open("JSW");

	if (!jswrom.valid()) {
		while (1) {
		}
	}

	pinMode(JUMP_PIN,INPUT);
	pinMode(RIGHT_PIN,INPUT);
	pinMode(LEFT_PIN,INPUT);

	opening();
}

void generic_copy_from_flash(unsigned offset, unsigned char *target, unsigned size)
{
	jswrom.seek(offset, SEEK_SET);
	jswrom.read( target, size);
}

void loop()
{
	GAMESTART();
}

static inline void copy_attributes()
{
	generic_copy_from_flash(0x9800,pallete,sizeof(pallete));
}


STATIC void PRMESSAGE(unsigned char *dest, const char *source, unsigned size)
{
	btarget = dest;
	while(size--) {
		putChar(*source++);
	}
}

void changeBG()
{
	// TODO: implement this

}

STATIC int DRAWLIVES()
{
	unsigned l = LIVES;
	if (l==0)
		return 0;

	// TODO - use MTICK
	if ((TICKER&0x3)!=0)
		return 1;

	unsigned spritenum = TICKER>>2;
	spritenum *= 8;
	spritenum &= 0x60; // Ensure 0, 1, 2 or 3

	unsigned char *dest = &framebuffer[ 8*COLUMNS * 21 ]; // Line 21

	generic_copy_from_flash(spritenum + 0x9D00, sprite, sizeof(sprite));

	while (l--) {
		drawsprite(sprite, dest, 0);
		dest+=2;
	}
}

STATIC void ROOMBLOCK(unsigned v, unsigned char *dest)
{
	*dest = ROOM.background[ (v&0x3)*9 ];
}

STATIC void BUILDROOM()
{
	unsigned char *rb = &ROOM.def[0];
	unsigned char *attrptr = &pallete[0];
	unsigned roombyte;
	int i;

	for (i=128;i!=0;i--) {
		roombyte=*rb;
		ROOMBLOCK(roombyte>>6, attrptr++);
		ROOMBLOCK(roombyte>>4, attrptr++);
		ROOMBLOCK(roombyte>>2, attrptr++);
		ROOMBLOCK(roombyte>>0, attrptr++);
		rb++;
	}

	i=ROOM.conv_num;

	if (i!=0) {
		unsigned start = (ROOM.conv_psn[0] +( ROOM.conv_psn[1]<<8)) - 0x5E00;
		attrptr = &pallete[start];

		while (i--) {
			*attrptr++=ROOM.conveyor[0];
		}
	}

	if (ROOM.slope_num==0)
		return;

	attrptr = &pallete[ ((unsigned)ROOM.slope_psn[0]+ ((unsigned)ROOM.slope_psn[1]<<8)) - 0x5E00];

	int delta;
	if ( ROOM.slope_dir & 1 ) {
		delta = -31;
	} else {
		delta = -33;
	}

	for (i=ROOM.slope_num;i!=0;i--) {
		*attrptr = ROOM.slope[0];
		attrptr+=delta;
	}
}

STATIC void PRMATRIX_if(unsigned val, const unsigned char *src, unsigned char *dest)
{
	if (val == *src) {
		PRMATRIX(src+1,dest);
	}
}

STATIC void DRAWROOM()
{
	/* Iterate through all attributes, apply bitmap */
	unsigned int i,z;
	unsigned char *attrptr = &pallete[0];
	unsigned char *fbptr = &framebuffer[0];

	for (i=0; i<512; i++) {
		unsigned int val = *attrptr;
		unsigned char *src = &ROOM.background[0];
		// Unroll loop
		PRMATRIX_if(val,src,fbptr);
		PRMATRIX_if(val,src+9,fbptr);
		PRMATRIX_if(val,src+18,fbptr);
		PRMATRIX_if(val,src+27,fbptr);
		PRMATRIX_if(val,src+36,fbptr);
		PRMATRIX_if(val,src+45,fbptr);
		fbptr++;

		// Move to next line

		if ((i%32)==31) {
			fbptr+=(COLUMNS*7);
		}
		attrptr++;
	}
}

#define DUMPROOM( dname, vname ) \
	rdump( dname, (unsigned char*)&ROOM + offsetof(ROOM_s, vname), sizeof(ROOM.vname) );

STATIC void loadroom()
{
	unsigned i;

	generic_copy_from_flash( 0xC000 + ((unsigned)HERE*256), (unsigned char*)&ROOM, 256 );

	unsigned char *gptr=&ROOM.instances[0];

	unsigned char *dptr=guardian;

	for (i=0;i<8;i++) {
		unsigned guardian_number = *gptr++;

		/*Serial.print("Read Guardian ");
		 printhexbyte(guardian_number);
		 Serial.println("");*/

		guardian_number &= 0x7F; // Max. 127 guardians
		guardian_number |= 0x1400;
		guardian_number*=8;

		// Now, guardian_number is address IN ROM. Copy parts

		generic_copy_from_flash(guardian_number,&dptr[0],2);
		/*
		 jswrom.seek(guardian_number,SEEK_SET);
		 jswrom.read( &dptr[0], 2);*/
		//
		// Copy the rest
		jswrom.read( &dptr[2], 6);

		// Set attr?
		dptr[2] = *gptr++;

		dptr+=8;
	}

	// Print room information

	memset( &framebuffer[ 16*COLUMNS*8 ], 0x0, sizeof(framebuffer)-(16*COLUMNS*8) );
	PRMESSAGE( &framebuffer[16*COLUMNS*8], (char*)ROOM.name, 32);
	PRMESSAGE( &framebuffer[19*COLUMNS*8], (char*)ITEMS, 32);
	// Update collected
	PRMESSAGE( &framebuffer[19*COLUMNS*8 + 16], (char*)COLLECTED, 3);
	drawpower();
	memcpy(&save_wstate, &wstate, sizeof(wstate)); // Save willy state
}

STATIC void GAMESTART()
{
	MTICK=FLASH=TICKER=COUNTDOWN=STATUS = 0;

	wstate.FALLING=0; // Not falling.

	LIVES=7;

	HERE = 0x21;// The bathroom

	wstate.POSITION=0x5DB4; // Position in attr buffer2
	wstate.YPOSN = 0xD0;

	COLLECTED[0]='0';
	COLLECTED[1]='0';
	COLLECTED[2]='0';

	//REMAIN=OBJECTS[0];

	int i;

	// TODO : clear object flags

	MFLAGS=0;


	unsigned deltas[9];

	wstate.POSITION=0x5DB4;
	wstate.YPOSN = 0xD0;

	copy_attributes();

	do {
		if (newroom) {
			newroom=0;
			loadroom();
		}


		i=0;
		TICKER++;
		//Serial.println("Tick");
#ifndef SIMULATION

		delay( GAMEDELAY );

		// Wait for VGA retrace
		while ((REGISTER(VGABASE,0)&1) == 0) {}
		while ((REGISTER(VGABASE,0)&1) != 0) {}
		while ((REGISTER(VGABASE,0)&1) == 0) {}
		while ((REGISTER(VGABASE,0)&1) != 0) {}
#else
		delay(GAMEDELAY);
		REGISTER(VGABASE,3) = 1; // Disable screen
#endif
		DRAWLIVES();
		bzero(framebuffer, 16*COLUMNS*8 ); // Only clear game-area
		BUILDROOM();
		DRAWROOM();
		UPDATEGUARD();
		UPDATECONV();
		MOVEMENT();

		if (wstate.YPOSN>=0xE1) {
			go_up();   // NOTE: I think there's a bug here in some scenarios
			continue;
		}

		UPDATEWILLY();
		DRAWWILLY();
		DRAWGUARD();
		CHKOBJECTS();

		updatepower();
#ifdef SIMULATION
		REGISTER(VGABASE,3) = 0; // Enable screen
#endif
	} while (1);
}

STATIC void DRAWWILLY()
{
	int A,E,DE,C;
	unsigned i;
	unsigned char *plb,*plbd;

    // willy_offset is used when we're on a stair.

	A = ((wstate.YPOSN+willy_offset)>>1)*32;
	plb = &framebuffer[A]; // plb now points to start of line

	A = wstate.DIRECTION;
	A &= 1;

	E = A<<7;

	A = wstate.FRAME;
	A&=3;
	A*=32; 
	E |= A;

	DE=E;
	DE |= 0x9D00;

    generic_copy_from_flash(DE,sprite,sizeof(sprite));

	plb += wstate.POSITION&0x1F;

	for (i=0;i<32;i+=2)  /* 32 bytes */
	{
		plbd = plb;
		*plbd = *plbd | sprite[i];
		plbd++;
		*plbd = *plbd | sprite[i+1];
		plb+=(COLUMNS); // Next line
	}
}

STATIC void DRAWROPE()
{
}
STATIC void DRAWARROW(unsigned char *loc)
{
	unsigned comp;
	if (loc[0] & 0x80) {
		loc[4]++;
		comp = 0xF4;
	} else {
		loc[4]--;
		comp = 0x2C;
	}
	if (loc[4]!=comp) {
		//?
	}
	// 82XX   -pixel line
	unsigned ypos = loc[2];
	ypos += loc[4];
	unsigned yagain=loc[2];
	yagain&=0x80;
	yagain<<=1;
	// OR 5C ?
	yagain|=0x5C; // 5C00 / 5D00
	yagain|= (ypos & 0xff);
	loc[5] = 0;

	// yagain shhould be willy attr
	if ( ( pallete[yagain-0x5C00] & 0x7) == 0x07) {
		// Willy
		loc[5]--; // ?
	}

	pallete[yagain-0x5C00] |= 0x7;

	yagain++;
}

STATIC void DRAWGUARD()
{
	unsigned char *gptr = &guardian[0];

	do {
		unsigned i = *gptr;
		/* 	Serial.print("Guardian: ");
		 printhexbyte(*gptr);
		 Serial.println("");*/
		if (i==0xff)
			return;

		i&=7;

		if (i==0) {
			// No guardian
			//Serial.println("No guard");
			gptr+=8;
			continue;
		}
		if (i==0x3) { /* Rope */
			//Serial.println("Rope");
			DRAWROPE();
			gptr+=8;
			continue;
		}
		if (i==0x4) { /* Arrow */
			//Serial.println("Arrow");
			DRAWARROW(gptr);
			gptr+=8;
			continue;
		}
		/* Normal */
		/* Serial.print("Ypos ");
		 printhexbyte(gptr[3]);
		 Serial.println("");*/
		unsigned ypos = gptr[3]>>1;
		unsigned xpos = (unsigned)gptr[2];

		unsigned attr;

		attr = gptr[1];  // Get guardian attribute
		attr&=0xf; // Keep b3-b0
		attr+=0x38;	// Move BRIGHT up to bit 6
		attr&=0x47; // Keep INK and BRIGHT

		xpos &= 0x1F; // No overflow

		// Convert ypos.

		//Serial.print("FB at xpos ");Serial.print(xpos);Serial.print(" ypos ");Serial.println(ypos);
		xpos+=ypos*32;

		unsigned yoffset = ((xpos/32)/8)*32;

		//Serial.print("Yoff "); Serial.println(yoffset);

		unsigned char *attrptr = &pallete[ (unsigned)(gptr[2]&0x1f) + yoffset];//( ((gptr[3]>>1)<<2)&(~0x1f) ) ];




		// Merge attribute

		attr = (*attrptr & 0x38 ) | attr;


		// Two uppermost cells
		*attrptr++ = attr;
		*attrptr = attr;
		attrptr+=31; //  move to next line

		*attrptr++ =attr;
		*attrptr =attr;

		// A guardian can use 2 or 3 lines. When (ypos/8)==0, only 2 lines are used.

		if ((gptr[3]) & 0xe ) {
			//Serial.println("Guardian 3 lines");
			attrptr+=31;
			*attrptr++=attr; // Fill the 3rd line
			*attrptr=attr;
		}

		/* Draw the guardian */

		unsigned offset = (( gptr[1] & gptr[0]) | gptr[2]) & 0xE0;
		offset|=((unsigned)gptr[5]<<8);
		/*
		 Serial.print("Sprite at 0x");
		 printhex(offset);
		 Serial.println("");
		 */
		/* Seek to sprite */
		generic_copy_from_flash(offset,sprite,sizeof(sprite));

		if ( drawsprite( sprite, &framebuffer[xpos], 1 ) ) {
			//Serial.println("Kill: collision");
			kill_willy();
			return;
		}

		gptr+=8;

		if (gptr>(&guardian[0]+sizeof(guardian)))
			break;

	} while (1);
}

STATIC void UPDATEGUARD()
{
	unsigned char *gptr = &guardian[0];

	do {
		unsigned i = *gptr;
		/*
		 Serial.print("UpdateG: ");
		 printhexbyte(*gptr);

		 Serial.println("");
		 */
		if (i==0xff)
			return;

		switch (i&0x03) {
		case 0:
			/* No need */
			break;
		case 1:
			/* Horizontal */
			/*
			 Serial.print("FRAME: ");
			 Serial.println( (i>>5) & 0x3 );
			 */
			/* topmost bit depicts direction. 0->left, 1->right */
			if (i&0x80) {
				i += 0x20;
				i |= 0x80; /* ensure we're still moving right */
				// update position/frame
				*gptr=i;
				if ((i & 0x60) == 0x00) { // 1 01 000 00b
					// We overflowed.
					// Get X position
					unsigned xpos = gptr[2];
					xpos &= 0x1F;
					if (xpos==gptr[7]) { // Maximum X offset
						gptr[0] = 0x61; // Clear direction, keep type (1). 0x61 == '0 11 000 01b'
					} else {
						// Increase X position
						gptr[2] = gptr[2]+1;
					}
				}
			} else {
				// Left move

				i-=0x20;
				i&=0x7f;  // Reset high bit, if we underflowed
				*gptr = i;
				if (i>0x60) { // 0 11 000 01 b
					//  Serial.println("Underflow");
					unsigned xpos = gptr[2];
					xpos&=0x1f;
					if (xpos==gptr[6]) // Minimum X
					{
						*gptr=0x81; // Clear direction, keep type  '0 00 000 01'
					} else {
						gptr[2] = gptr[2] - 1;
					}
				} else {

				}
			}

			break;
		case 2:
			/*
			 Serial.println("Vertical");

			 Serial.print("FRAME: ");
			 Serial.println( (i>>5) & 0x3 );
			 */
			/* Vertical */
			i ^= 0x8;  // '0 00 010 00
			*gptr=i;

			//if (i&0x18 != 0x00) {  // 00001'1000b
			i+=0x20;
			*gptr=i;
			//}

			i = gptr[3] + gptr[4];
			i&=0xff; // For later comparison

			gptr[3] = i;

			if (i >= gptr[7]) {
				// JR NZ
				// Serial.println("negate");
				gptr[4] = ~gptr[4] + 1; // NEG

			} else if (i<=gptr[6]) {
				gptr[3] = gptr[6];
				gptr[4] = ~gptr[4] + 1; // NEG
			}
			break;
		default:
			break;
		}

		gptr+=8;

	} while (1);
}

STATIC int check_and_draw_willy_attribute(unsigned char *attrptr, int skip)
{
	if (*attrptr == ROOM.nasty[0]) {
		return -1;
	}

	if (skip)
		return 0;

	if (*attrptr==ROOM.background[0]) {
		*attrptr = ROOM.background[0] | 0x7;
	}

	return 0;
}

STATIC void UPDATEWILLY()
{
	unsigned pos = wstate.POSITION;
	unsigned char *attrptr = &pallete[ pos - 0x5C00 ];

	unsigned A = ROOM.slope_dir;
	A&=1;
	A+=0x40;
	unsigned E=A;
	unsigned B=0;

	pos += E;

	willy_offset = 0;

	// 0x40 or 0x41. See if standing on a slope

	if (ROOM.slope[0] == attrptr[E] ) { // Is a slope ?

		if (wstate.FALLING == 0) {
			A = wstate.FRAME;
			A&=0x3;
			A<<=2;

			B = A;
			A=ROOM.slope_dir;
			A&=1;
			A--;
			A ^= 0xC;
			A ^= B;
			A &= 0xC;
			B=A;
			willy_offset = B;

			// This is used as offset for draw_willy


		}
	}
	/* Check all 4 attributes */
	if (check_and_draw_willy_attribute(attrptr,0)<0)
		return;

	attrptr++;

	if (check_and_draw_willy_attribute(attrptr,0)<0)
		return;

	attrptr+=31;

	if (check_and_draw_willy_attribute(attrptr,0)<0)
		return;

	attrptr++;

	if (check_and_draw_willy_attribute(attrptr,0)<0)
		return;

	attrptr+=31;

	// Now, we might occupy 3 lines....

	// Only check collisions, don't draw attribute unless we're overflowing YPOS

	// NOTE NOTE NOTE: This uses B avove, to interact with stairs: TODO

	if (check_and_draw_willy_attribute(attrptr, wstate.YPOSN&0xF!=0xF)<0)
		return;
	attrptr++;
	if (check_and_draw_willy_attribute(attrptr, wstate.YPOSN&0xF!=0xF)<0)
		return;
}

STATIC unsigned char *wally_recompute_position()
{
	unsigned A = wstate.YPOSN;
	unsigned current_line = (A>>1)/8;
	unsigned current_col = wstate.POSITION & 0x1F;
	wstate.POSITION = 0x5C00 + current_col + (current_line*32);
	return &pallete[ wstate.POSITION - 0x5C00 ];
}

STATIC void kill_willy()
{
	//Serial.println("WILLY DIED");
	newroom = 1;
	// Restore willy state
	memcpy(&wstate, &save_wstate, sizeof(wstate));
}


STATIC void L8EBC() {
	wstate.YPOSN = ( wstate.YPOSN + 0x10 ) & 0xF0;
	wally_recompute_position(); //L8E9C(); // Update position

	wstate.FALLING=2;
	wstate.DIRECTION &= 0xFD; // clear bit 1
}

STATIC void move_willy()//L8FBC()
{
	if ( (wstate.DIRECTION & 0x2) == 0)
		return;                       // don't update
	/*
	 if ( ((wstate.ONROPE-1)&0x80) == 0)
	 return;     */

	//Serial.println("Updating willy");

	if ((wstate.DIRECTION & 0x1) == 0) {

		if (wstate.FRAME==0x3) {
			move_willy_right_block();
			return;
		}
		wstate.FRAME++;
		wstate.FRAME &= 0x3;
		return;
	}

	if (wstate.FRAME==0) {
		move_willy_left_block();
		return;
	}
	wstate.FRAME--;
	wstate.FRAME &= 0x3;
}

STATIC void MOVEMENT()
{
	unsigned l_FALLING = wstate.FALLING;
	unsigned l_JUMPING = wstate.JUMPING;
	unsigned l_YPOSN   = wstate.YPOSN;

	if ( l_FALLING == 0x1) {

		// We are jumping

		l_YPOSN += (l_JUMPING & 0xfe) - 8;

		if (l_YPOSN >= 0xf0) {
			// We underflow below 0 ?
			// Go up
			go_up();
			return;
		}

		wstate.YPOSN = l_YPOSN;

		unsigned char *attrptr = wally_recompute_position();

		if (attrptr[0] == ROOM.wall[0] || attrptr[1] == ROOM.wall[0] ) {
			l_YPOSN += 0x10;
			l_YPOSN &= 0xF0;
			wstate.YPOSN = l_YPOSN;
			wally_recompute_position();

			wstate.FALLING=2;
			wstate.DIRECTION &= 0xFD;
			return;
		}

		l_JUMPING = l_JUMPING + 1;

		if( l_JUMPING==0x12) {
			wstate.FALLING=0x6; // We fell after jumping
			wstate.JUMPING = l_JUMPING;
			return;
		} else if (l_JUMPING!=0x10) {
			if( l_JUMPING!=0x0D) { // We are jumping downwards, and we have a block, check block
				wstate.JUMPING=l_JUMPING;
				return move_willy();
			}
		}
		wstate.JUMPING = l_JUMPING;
	}

	if ( (l_YPOSN&0xE) == 0) { // If zero, we are are to move to lower block

		// Check if we can go down.

		unsigned HL = wstate.POSITION;
		HL += 0x40; // Block under willy feet (2 lines)
		if (HL & 0x200) {
			// go down to room below (wrapped)
			go_down();
			return;
		}

		unsigned char *attrptr = &pallete[ HL - 0x5C00 ];

		if (attrptr[0]==ROOM.nasty[0] || attrptr[1]==ROOM.nasty[0]) {
			// Nasty block, die!
			//Serial.println("Kill: nasty");
			kill_willy();
			return;
		}

		// Now, see if its background
		if (attrptr[0] != ROOM.background[0] || attrptr[1] != ROOM.background[0]) {
			// Stop falling.
			// Serial.println("Steady!");
			read_willy_input();
			return;
		}
		// falling....
	}
	if (l_FALLING != 1) {
		// We were not jumping,

		wstate.DIRECTION &= 0xFD; // clear bit 1

		if (l_FALLING == 0) {
			wstate.FALLING=2;
			return;
		}

		l_FALLING++;

		if (l_FALLING==0x10) {
			l_FALLING=0x0C;
		}
		wstate.FALLING = l_FALLING;
		wstate.YPOSN = l_YPOSN+8;

		// Trying to make a sense of this.....
		wally_recompute_position();
	} else {
		move_willy();
	}
}

STATIC void read_willy_input()
{
	unsigned char *attrptr = &pallete[ wstate.POSITION - 0x5C00 ];
	static int countdown=-1; // TO allow jump+move in some situations

	if (wstate.FALLING >= 0x0C) {
		//Serial.println("Kill: felt");
		kill_willy();
		return;
	}
	wstate.FALLING=0;

	unsigned k_jump  = JUMP_INVERT digitalRead(JUMP_PIN);
	unsigned k_left  = LEFT_INVERT digitalRead(LEFT_PIN);
	unsigned k_right = RIGHT_INVERT digitalRead(RIGHT_PIN);

	// Check block under willy feet
	if (attrptr[64]==ROOM.conveyor[0] || attrptr[65]==ROOM.conveyor[0]) {
		/* Standing on a conveyor */
		if (ROOM.conv_dir) {
			k_left=1;
		} else {
			k_right=1;
		}
	}

	/* Now, for movement magic */

	// Left and right always update direction lower bit

	// Unless both set - TODO

	unsigned old_direction = wstate.DIRECTION & 1;

	if (k_left) {
		wstate.DIRECTION&=0xFE;
	}
	if (k_right) {
		wstate.DIRECTION|=1;
	}

	// Now, if we are jumping.

	if (k_jump) {
		wstate.DIRECTION &= 0xFD;

		wstate.FALLING=1;
		wstate.JUMPING=0;
		wstate.YPOSN &= 0xF0;

		if (k_left || k_right) {
			if (countdown<0)
				countdown=2;

			if (old_direction == (wstate.DIRECTION&1)) {
				// Same direction.
				// if bot both set - then jump up
				if (!(k_left & k_right)) {
					wstate.DIRECTION |= 0x2;
				}
				if (countdown>0)
					countdown--;
				if (countdown==0) {
					countdown=-1;
					goto do_move;
				}
			}
		} else {
			countdown=-1;
		}


	} else {
		countdown=-1;
		do_move:
			// Move if not both are set
			if (k_left) {
				if (!k_right) {
					wstate.DIRECTION |= 0x2;
					move_willy();
				}
			} else {
				if (k_right) {
					wstate.DIRECTION |= 0x2;
					move_willy();
				}
			}
	}
}

STATIC void go_right()
{
	HERE=ROOM.right;
	wstate.POSITION &= 0xffffffe0; // Reset our position
	newroom=1;
}


STATIC void go_left()
{
	HERE=ROOM.left;
	wstate.POSITION |= 0x1f;
	wstate.POSITION &= 0xfffffffe;  // Force into column 30
	newroom=1;
}

STATIC void go_down()
{
	HERE=ROOM.down;
	wstate.YPOSN=0;
	if (wstate.FALLING>0x0B) { // TODO: review this
		wstate.FALLING=2;
	}
	wstate.POSITION &= 0x1f;
	wstate.POSITION += 0x5C00;
	newroom=1;
}

STATIC void go_up()
{
	HERE=ROOM.up;
	wstate.POSITION&=0x1f;
	wstate.POSITION|=0x1A0;
	wstate.POSITION += 0x5C00;
	//Serial.print("Going up, room ");
	//printhexbyte(HERE);
	//Serial.println("");
	wstate.YPOSN=0xD0;   // 104, line 13
	wstate.FALLING=0;
	newroom=1;
}

STATIC int move_willy_right_block()
{
	// Move Willy rightwards
	int voff = 0;

	unsigned char *attrptr = &pallete[ (wstate.POSITION-0x5C00) ];

	unsigned yposoff=0;
	if (wstate.FALLING==0) {
		unsigned off = ROOM.slope_dir ? 34 : 64;

		if (attrptr[off] == ROOM.slope[0]) {
			// On a slope, move up or down

			voff =  ROOM.slope_dir ? -32 : 32;
			yposoff = ROOM.slope_dir ? -16:16;
		}
	}
	unsigned newpos = (wstate.POSITION - 0x5C00) + voff;

	if (((newpos+2) & 0x1f) == 0) {

		go_right();
		return 0;
	}

	// TODO: check wall on 3rd line

	// Check wall
	if ( pallete[newpos + 32 + 2] == ROOM.wall[0]) { // Is there a wall on right of willy foot ?
		return 0; // No movement
	}
	if (pallete[newpos + 2]==ROOM.wall[0]) { // Check right of willy head
		return 0; //
	}
	wstate.POSITION = newpos + 1 + 0x5C00;
	wstate.FRAME = 0;

	wstate.YPOSN+=yposoff;
}


STATIC  int move_willy_left_block()
{

	int voff = 0;

	// 65 (slope=1) or 31 (slope=0):P

	unsigned yposoff=0;

	if (wstate.FALLING==0) {
		unsigned char *attrptr = &pallete[wstate.POSITION-0x5C00];
		unsigned off = ROOM.slope_dir ? 65 : 31;

		if (attrptr[off] == ROOM.slope[0]) {
			// On a slope, move up or down
			voff =  ROOM.slope_dir ? 32 : -32;
			yposoff = ROOM.slope_dir ? 16:-16;
		}
	}

	unsigned newpos = (wstate.POSITION - 0x5C00) + voff;


	if ((newpos & 0x1f)==0) {

		go_left();
		return 0;
	}

	// TODO: check wall on 3rd line


	if (pallete[newpos -1 ]==ROOM.wall[0]) { // Check left of willy head
		return 0; //
	}
	if (pallete[newpos + 32 -1 ]==ROOM.wall[0]) { // Check left of willy head
		return 0; //
	}

	wstate.POSITION = newpos - 1 + 0x5C00;
	wstate.FRAME = 0x3;
	wstate.YPOSN += yposoff;
}


// A400 / A600

// we have two buffers, one with objects themselves, other with object flags and locations

#define JSW_OBJCNT 0xad

#define NUM_OBJECTS (256-JSW_OBJCNT)     /* 83 */

unsigned char objloc[NUM_OBJECTS*2] = {
	0xf2,0xf2,0xf2,0xf2,0xf8,0xf8,0xf8,0xf8,0xeb,0xeb,
	0xeb,0xeb,0xdf,0xd2,0x4f,0x50,0x51,0xfc,0xfb,0xfa,
	0x65,0x65,0xe5,0x5b,0xf9,0xfa,0x5c,0xe2,0x59,0x59,
	0xd9,0xd9,0xf3,0x56,0xd6,0x68,0xcc,0x4c,0x4c,0xc9,
	0x47,0x6e,0x6e,0x66,0xd5,0xd5,0xd5,0xd5,0xd5,0x4e,
	0x4a,0xd5,0xd5,0x4e,0x4d,0x4d,0x6c,0x53,0xc3,0xc3,
	0xc3,0x71,0x71,0x71,0xf1,0xf1,0xf1,0x40,0x40,0x40,
	0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x40,0x42,
	0xdd,0xde,0xe1,
	// rest of XPOS/YPOS is high in the table at 0xA5A0
	0x19,0x2a,0x34,0x38,0xc9,0xb3,0x7b,0x46,0x1f,0x19,
	0x1c,0x16,0x64,0x99,0x9f,0x9f,0x9f,0x1a,0xcd,0xb6,
	0x3c,0xd0,0xa6,0xdd,0x37,0xb6,0x7a,0x22,0x43,0xa8,
	0x06,0x66,0xc7,0xc9,0x22,0xf3,0xb3,0x70,0x96,0x65,
	0xef,0x6a,0x9d,0x49,0x91,0x92,0x94,0x96,0x97,0x5a,
	0x8b,0x99,0x9b,0x7a,0xc2,0x25,0x92,0x62,0x61,0x44,
	0x67,0x9c,0xe4,0xfc,0x42,0x5c,0xa4,0x93,0x94,0x95,
	0x96,0x97,0x98,0x99,0x9a,0x9b,0x9c,0x9d,0x9e,0x16,
	0x3a,0xc8,0xb7
};




STATIC void draw_object(unsigned offset)
{
	unsigned char *fb = &framebuffer[ATTRBUFFER_OFFSET_TO_FRAMEBUFFER_OFFSET(offset)];
	pallete[offset] = ( pallete[offset] & 0xf8 ) | // Lose INK
		(TICKER & 0x3) + 0x3;
	PRMATRIX( &ROOM.object[0], fb ); // Draw it.
}

STATIC void CHKOBJECTS()
{
	unsigned char *objptr = &objloc[0];
	unsigned i;
	for (i=NUM_OBJECTS;i!=0;i--) {
		unsigned C = *objptr & 0x7F;  // Top bit is Y position bit 4.

		if (C==(HERE|0x40)) {   // Fake a non-collected bit ?

			// Yes. There's an object here

			unsigned off = (*objptr + *objptr) & 0x100; // Get bit into *512 position

			off+= objptr[NUM_OBJECTS]; // Add in X/Y extra position

			unsigned char *pltr = &pallete[off]; // Get extra x/y pos
			if ( (*pltr & 0x7)  == 0x7) {
				// Willy collecting!!!

				*objptr &= 0xBF; // Clear 0x40 bit, meaning object is collected

				// This is indeed faster than using printf or similar

				pltr = (unsigned char*)&COLLECTED[2];
				do {
					*pltr = *pltr+1;
					if (*pltr<='9') {
						break;
					}
					*pltr='0';
					pltr--;
				} while (1);

				// Update COLLECTED on screen

				PRMESSAGE( &framebuffer[19*COLUMNS*8 + 16], (char*)COLLECTED, 3);

			} else {
				draw_object(off);
			}
		}
		objptr++;
	}
}
