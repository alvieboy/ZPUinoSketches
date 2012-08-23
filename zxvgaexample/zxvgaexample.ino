/***********************************************************
 *
 * ZPUino with VGA ZX interface
 *
 * (C) Alvaro Lopes 2012
 *
 * This code released under Creative Commons BY-NC-SA
 *
 * None of the additional files (ROM's and other resources) are
 * allowed for commercial use.
 *
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
*/

#include <SmallFS.h>
#include "VGAZX.h"

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

#endif

static unsigned char ball_sprite[32];

#define BALL_SIZE_BITS 6

unsigned char small_ball[6] = {
	0x30, 0x78, 0xfc, 0xdc, 0x78, 0x30
};

unsigned char block1[8] = { 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff };

void make_ball(int x, int y)
{
	memset(ball_sprite,0x0,sizeof(ball_sprite));

	int rx= x % 8;
	int ry= y % 8;

	int i,z;
	for (i=0,z=ry<<1;i<6;i++) {
		unsigned v = small_ball[i];
		ball_sprite[z++]=v>>rx;
		ball_sprite[z++]=v<<(8-rx);
	}
}

void make_blocks()
{
}


int bx=3, by=(VGA_ROWS-1)*8;
int dx=1, dy=-1;
unsigned char ftick=0;


void draw_ball(int x, int y, bool isxor)
{
	VGAZX.drawsprite(ball_sprite, VGAZX.framebuffer(x/8,(y/8)*8), false, isxor);
}

void setup()
{
	delay(1000);
	VGAZX.begin();
	VGAZX.clrscr();
	
	Serial.begin(115200);

	VGAZX.pctext("\033C\x03"
				 "Welcome!!!"
				 "\033C\x02"
				 "\033G\x02\x02"
				 "Congratulations for being a"
				 "\033G\x02\x03"
				 "proud owner of a Papilio Board"
				 "\033G\x02\x04"
				 "running "
				 "\033C\x01"
				 "ZPUino32 "
				 "\033C\x02"
				 "processor!!!"
				 "\033C\x03"
				 "\033G\x00\x06"
				 "You might notice that there's a"
				 "\033G\x00\x07"
				 "small ball jumping around. This"
				 "\033G\x00\x08"
				 "is because we are going to talk"
				 "\033G\x00\x09"
				 "a bit about games!"
				 "\033C\x05"
				 "\033G\x14\x16"
				 "Press button"
				 "\033G\x15\x17"
				 "to continue"

				);

	for (int i=8;i<16;i++) {
		VGAZX.drawblock(block1, VGAZX.framebuffer(i,14*8));
		*VGAZX.pallete(i,14) = 0xa3;
	}
	pinMode(JUMP_PIN,INPUT);
}

int lax=0, lay=0;

int getButton()
{
	return digitalRead(JUMP_PIN);
}

void flash_area(unsigned off, unsigned count)
{
    unsigned char *pallete= VGAZX.pallete();
	while (count--) {
		pallete[off] = ( pallete[off] & 0xf8 ) | // Lose INK
			(ftick & 0x3) + 0x3;
		off++;
	}
}

void flash(int x, int y, int ballx, int bally, int color=0xff)
{
    unsigned char p;
    return;
	make_ball(ballx,bally);
	draw_ball(ballx, bally,true);

	while (getButton()==0) {
		p = *VGAZX.pallete(x, y);
		*VGAZX.pallete(x, y) = color;
		delay(50);
		*VGAZX.pallete(x, y) = p;
		delay(50);
	}
	//Serial.read();
	draw_ball(ballx,bally,true);

}

int test_colliion(int x, int y)
{
	make_ball(x,y);
	return VGAZX.sprite_collides(ball_sprite,VGAZX.framebuffer(x/8,(y/8)*8));
}

void wait_up()
{
	delay(10);
    return;
}

void ball_demo()
{
	int nx, ny;
	int hcol=0, vcol=0;
	make_ball(bx,by);
	draw_ball(bx,by,true);
	//delay(10);
	wait_up();
	draw_ball(bx,by,true);

	nx=bx+dx;
	ny=by+dy;

	/* Out of bounds check */

	if (nx>(VGA_COLUMNS*8)-BALL_SIZE_BITS || nx<0) {
		dx=-1*dx;
		nx+=dx;
		nx+=dx;
	}

	if (ny>(VGA_ROWS*8)-BALL_SIZE_BITS || ny<0) {
		dy=-1*dy;
		ny+=dy;
		ny+=dy;
	}

	int col;
	/* Check collision */
	if ((col=test_colliion(nx,by)) >=0 ) {
		Serial.print("H Collision ");
		Serial.println(col);
		flash((nx/8)+(col&1),(by/8)+((col&2)>>1), nx, by);

		hcol=1;
	}
	if ((col=test_colliion(bx,ny)) >= 0) {
		Serial.print("V Collision ");
		Serial.println(col);
		flash((bx/8)+(col&1),(ny/8)+((col&2)>>1),bx, ny);

		vcol=1;
	}
	if (hcol==0 && vcol==0) {
		if ((col=test_colliion(nx,ny)) >=0 ) {
			Serial.print("EDGE Collision ");
			Serial.println(col);
			flash(1+nx/8,1+ny/8, nx, ny);

			hcol=1;
			vcol=1;
		}
	}

	if (hcol) {
		dx=-1*dx;
		nx+=dx;
		nx+=dx;
	}
	if (vcol) {
		dy=-1*dy;
		ny+=dy;
		ny+=dy;
	}

	bx=nx;
	by=ny;
}

void randomize_color_area()
{
    	volatile unsigned char *memory=(volatile unsigned char*)0x1010;

	int d = 5*VGA_COLUMNS;
	for (;d<20*VGA_COLUMNS;d++) {
		VGAZX.pallete()[d] = memory[TIMERTSC & 0xfff];
	}
}
void stage5_init()
{
	SmallFSFile f = SmallFS.open("scr1");
	VGAZX.loadscr(f);
}
void stage6_init()
{
	SmallFSFile f = SmallFS.open("scr2");
	VGAZX.loadscr(f);
}

void stage7_init()
{
	SmallFSFile f = SmallFS.open("scr3");
	VGAZX.loadscr(f);
}
void stage8_init()
{
	SmallFSFile f = SmallFS.open("scr4");
	VGAZX.loadscr(f);
}

void stage4_init()
{
	int i;
	for (i=0;i<((VGA_ROWS*8)/2)-1;i+=2) {
		VGAZX.drawLine(0,0,(VGA_COLUMNS*8)-1,i);
	}

	for (i=((VGA_COLUMNS*8)/2)-2;i>=0;i-=2) {
		VGAZX.drawLine(0,0,i,(VGA_ROWS*8)-1);
	}

	VGAZX.pctext("\033G\x08\x09"
				 "\033C\x04"
				 "You can address"
				 "\033G\x09\x0A"
				 "all pixels on the"
				 "\033G\x0a\x0b"
				 "screen, like this"
				 "\033G\x0b\x0c"
				 "Moire pattern"
				 "\033G\x0c\x0d"
				 "shows."
				 "\033G\x0d\x0f"
				 "\033C\x01"
				 "Let's see some"
				 "\033G\x0e\x10"
				 "popular games now!"
				);

	bx=200, by=(VGA_ROWS-1)*8;
	dx=1, dy=-1;

}

void stage3_init()
{
	volatile unsigned char *memory=(volatile unsigned char*)0x1010;

	VGAZX.pctext("But you can get some other nice"
				 "\033G\x00\x01"
				 "effects, with small CPU effort !"
				 "\033G\x00\x02"
				 "________________________________");
    randomize_color_area();
}

void stage2_init()
{

	VGAZX.pctext("This 8x8 color block can cause"
				 "\033G\x00\x01"
				 "some odd appearance on pixels,"
				 "\033G\x00\x02"
                 "since you cannot directly map a"
				 "\033G\x00\x03"
				 "pixel color..."
				 "\033G\x00\x04"
				 "________________________________");
    randomize_color_area();
}


void stage1_init()
{
	VGAZX.pctext("\033C\x07"
				 "First of all, let's take a look"
				 "\033G\x00\x01"
				 "at this graphical adaptor we are"
				 "\033G\x00\x02"
				 "Using. It is based in original"
				 "\033G\x00\x03"
				 "ZX Spectrum graphics adaptor."
				 "\033G\x00\x04"
				 "The screen is 256x192 pixel wide"
				 "\033G\x00\x05"
				 "and color can be mapped to any"
				 "\033G\x00\x06"
				 "8x8 block, with 3 bits for the"
				 "\033G\x00\x07"
				 "background and foreground color."
				 "\033G\x00\x08"
				 "\033C\x01" "J"
				 "\033C\x02" "u"
				 "\033C\x03" "s"
				 "\033C\x04" "t"
				 "\033C\x05" "L"
				 "\033C\x06" "i"
				 "\033C\x07" "k"
				 "\033C\x08" "e"
				 "\033C\x10" "T"
				 "\033C\x18" "h"
				 "\033C\x20" "i"
				 "\033C\x28" "s"
				 "\033C\x30" "!"
                 "\033C\x07" "___________________"

				 "\033G\x14\x16"
				 "Press button"
				 "\033G\x15\x17"
				 "to continue"

				);
}

int stage=0;

void switch_to_stage(int newstage)
{

	VGAZX.clrscr();

	bx=3, by=(VGA_ROWS-1)*8;
	dx=1, dy=-1;

	switch (newstage) {
	case 1:
		stage1_init();
        break;
	case 2:
		stage2_init();
        break;
	case 3:
		stage3_init();
        break;
	case 4:
		stage4_init();
        break;
	case 5:
		stage5_init();
        break;
	case 6:
		stage6_init();
        break;
	case 7:
		stage7_init();
        break;
	case 8:
		stage8_init();
        break;
	}
}

void loop()
{
	switch (stage) {
	case 3:
		if (ftick&2)
			randomize_color_area();
	case 0:
	case 2:
	case 4:
		ball_demo();
		break;
	case 1:
		ball_demo();
        flash_area(3*VGA_COLUMNS,11);
		break;
	}

	if (getButton()==1) {
		while (getButton()==1);
		stage++;
        switch_to_stage(stage);
	}
	ftick++;
}
