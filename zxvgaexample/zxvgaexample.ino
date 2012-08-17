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

void flash(int x, int y, int ballx, int bally, int color=0xff)
{
    unsigned char p;
    return;
	make_ball(ballx,bally);
	draw_ball(ballx, bally,true);

	while (digitalRead(JUMP_PIN)==0) {
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

void loop()
{
	int nx, ny, nay,nax;
	int hcol=0, vcol=0;
	make_ball(bx,by);
	draw_ball(bx,by,true);
	//delay(10);
	wait_up();
	draw_ball(bx,by,true);

	nx=bx+dx;
	ny=by+dy;

	if (dx==1) {
		/* Moving right */
		nax=nx+BALL_SIZE_BITS-1;
	} else {
		nax=nx-(BALL_SIZE_BITS+1);
	}

	if (dy==1) {
		/* Moving right */
		nay=ny+BALL_SIZE_BITS-1;
	} else {
		nay=ny-(BALL_SIZE_BITS+1);
	}

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
