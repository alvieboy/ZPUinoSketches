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
#include "fixedpoint.h"

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

extern "C" const unsigned int costable[64]; /* Cosine table from 0 to PI/2 */

static unsigned char ball_sprite[32];

#define BALL_SIZE_BITS 6

unsigned char small_ball[6] = {
	0x30, 0x78, 0xfc, 0xdc, 0x78, 0x30
};

#define BLOCK_HEIGHT 20

#define PLAYER_SIZE_PIX 24

unsigned char screenblocks[VGA_COLUMNS*BLOCK_HEIGHT];

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

int player_x = ((VGA_COLUMNS*8)/2) - PLAYER_SIZE_PIX/2;
int player_moved=0;

void draw_player()
{
	int x;
	if (player_moved) {
        memset( VGAZX.framebuffer(0,4+23*8),0,VGA_COLUMNS*4);
	}
	for (x=player_x;x<player_x+PLAYER_SIZE_PIX;x++) {
		VGAZX.putPixel(x,4 + 23*8);
		VGAZX.putPixel(x,5 + 23*8);
		VGAZX.putPixel(x,6 + 23*8);
                VGAZX.putPixel(x,7 + 23*8);
	}
}


int bx=3, by=(VGA_ROWS-1)*(8*16);
int dx=8, dy=-16;
unsigned char ftick=0;


void draw_ball(int x, int y, bool isxor)
{
	VGAZX.drawsprite(ball_sprite, VGAZX.framebuffer(x/8,(y/8)*8), false, isxor);
}

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

int test_collision(int x, int y)
{
	make_ball(x,y);
	return VGAZX.sprite_collides(ball_sprite,VGAZX.framebuffer(x/8,(y/8)*8));
}

void wait_up()
{
	delay(5);
    return;
}

void new_screen()
{

}
/*
 Block definition, in bits

 T T C C C I I I

 T - 2 bits, block type
 C - 3 bits, color index
 I - 3 bits, block ID
 */

static unsigned char blockcolors[8] = {
	0x17,0x21,0x31,0x02,0x13,0x17,0x17,0x17
};
static unsigned char emptyblock[8] = {0};

unsigned char blockdefs[8][8] = {
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff },
	{ 0xff, 0xaa, 0x55, 0xaa, 0x55, 0xaa, 0x55, 0xff }
};

unsigned char tempblock[8];

#define BLOCK_TYPE_MASK 0xC0
#define BLOCK_ID_MASK 0x07
#define BLOCK_COLOR_SHIFT 3
#define BLOCK_COLOR_MASK 0x7

#define BLOCK_LEFT   0x00
#define BLOCK_RIGHT  0x40
#define BLOCK_CENTER 0x80
#define BLOCK_SOLID  0xC0

fp32_16_16 speed;
int angle; /* 0 to 63 for 0 to pi/2 */


void draw_block_screen()
{
	unsigned p;
	unsigned bpixid;
	unsigned i;

	for (p=0;p<sizeof(screenblocks);p++) {
		if (screenblocks[p]==BLOCK_SOLID)
			continue; /* No block */

		/* Get block pixmap */
		bpixid = screenblocks[p] & BLOCK_ID_MASK;

		/* Copy block into temporary area, and manipulate for Left/Right block */

		for (i=0;i<8;i++) {
			tempblock[i] = blockdefs[bpixid][i];
			switch ( screenblocks[p] & BLOCK_TYPE_MASK ) {
			case BLOCK_LEFT:
				tempblock[i] |= 0x80;
				break;
			case BLOCK_RIGHT:
				tempblock[i] |= 0x1;
                break;
			}
		}

		/* draw */

		VGAZX.drawblock(tempblock, VGAZX.framebuffer(p%VGA_COLUMNS,(p/VGA_COLUMNS)*8));
		VGAZX.pallete()[p] = blockcolors[ (screenblocks[p] >> BLOCK_COLOR_SHIFT) & BLOCK_COLOR_MASK];

	}
}

void clear_block(int x, int y)
{
	/* Remove block */
	unsigned boffset = x + (y * VGA_COLUMNS);

	Serial.print("Clearing block at ");
	Serial.print(x);
        Serial.print(",");
	Serial.print(y);
	Serial.print(" type ");
        Serial.println(screenblocks[boffset] & BLOCK_TYPE_MASK);

	VGAZX.pallete()[boffset] = 0x7;
	screenblocks[boffset] = BLOCK_SOLID;
	/* Clear pixmap */
	VGAZX.drawblock(emptyblock, VGAZX.framebuffer(x,y*8));

}

void collision(int x, int y)
{
	unsigned boffset = x + (y * VGA_COLUMNS);
	unsigned sx, soffset,type;

	Serial.print("Collision at ");
        Serial.print(x);
        Serial.print(",");
        Serial.println(y);

	if (y==23)
            return;

	if ((screenblocks[boffset] & BLOCK_TYPE_MASK) != BLOCK_SOLID) {
		sx = x;
		soffset = boffset;
		unsigned savetype = screenblocks[boffset] & BLOCK_TYPE_MASK;

		do {
			type = screenblocks[soffset] & BLOCK_TYPE_MASK;
			clear_block(sx,y);
			if (type==BLOCK_RIGHT)
				break;
                        soffset++, sx++;
		} while (1);

		sx = x;
		soffset = boffset;
		screenblocks[soffset] = savetype;

		do {
			type = screenblocks[soffset] & BLOCK_TYPE_MASK;
			clear_block(sx,y);
			if (type==BLOCK_LEFT)
				break;
                        soffset--, sx--;
		} while (1);
	}
}

inline int deg2index(int deg)
{
	// 90 -> 63
	return (deg*63)/90;
}

inline int index2deg(int index)
{
	// 63 -> 90
	return (index*90)/63;
}

void ball_demo()
{
	int nx, ny;
	int hcol=0, vcol=0;

	unsigned rbx = bx>>4;
	unsigned rby = by>>4;

	make_ball(rbx,rby);
	draw_ball(rbx,rby,true);
        draw_player();
	wait_up();
	draw_ball(rbx,rby,true);

	make_speed(&dx,&dy);

	nx=bx+dx;
	ny=by+dy;

	/* Out of bounds check */

	if ((nx>>4)>(VGA_COLUMNS*8)-BALL_SIZE_BITS || nx<0) {
		dx=-1*dx;
		nx+=dx;
		nx+=dx;

		angle = deg2index(180) - angle;
		if (angle > deg2index(180)) {
			angle= angle - deg2index(360);
		}
	}

	if ((ny>>4)>(VGA_ROWS*8)-BALL_SIZE_BITS || ny<0) {
		dy=-1*dy;
		ny+=dy;
		ny+=dy;

		angle = -1*angle;
		if (angle < (-1*deg2index(180)))
			angle= angle + deg2index(360);
	}

	int col;
	/* Check collision */
	if ((col=test_collision(nx/16,by/16)) >=0 ) {

		collision((nx/(16*8))+(col&1),(by/(16*8))+((col&2)>>1));
		hcol=1;
	}
	if ((col=test_collision(bx/16,ny/16)) >= 0) {
		collision((bx/(16*8))+(col&1),(ny/(16*8))+((col&2)>>1));
		vcol=1;
	}
	if (hcol==0 && vcol==0) {
		if ((col=test_collision(nx/16,ny/16)) >=0 ) {
			collision(1+nx/(16*8),1+ny/(16*8));
			hcol=1;
			vcol=1;
		}
	}

	if (hcol) {
		dx=-1*dx;
		nx+=dx;
		nx+=dx;

		angle = deg2index(180) - angle;
		if (angle > deg2index(180)) {
			angle= angle - deg2index(360);
		}
	}
	if (vcol) {
		dy=-1*dy;
		ny+=dy;
		ny+=dy;

		angle = -1*angle;
		if (angle < (-1*deg2index(180)))
			angle= angle + deg2index(360);

	}

	bx=nx;
	by=ny;

	if (bx <0 || bx>VGA_COLUMNS*8*16) {
		bx = 23;
        by = 23;
	}
	if (by > VGA_ROWS*8*16) {
		bx = 23;
        by = 23;
	}
/*	Serial.print("BX ");
    Serial.print(bx);
	Serial.print("BY ");
    Serial.println(by);*/
}

void randomize_color_area()
{
	volatile unsigned char *memory=(volatile unsigned char*)0x1010;

	int d = 5*VGA_COLUMNS;
	for (;d<20*VGA_COLUMNS;d++) {
		VGAZX.pallete()[d] = memory[TIMERTSC & 0xfff];
	}
}

void make_speed(int *sx, int *sy)
{

	fp32_16_16 speedx, speedy;
	fp32_16_16 cosi=1.0, sini=1.0;
	int la=angle;


	if (la<0) {
		la=-la;
		sini = fp32_16_16(0xffff0000,1); // This is -1.0
	}

	if (la>63) {
		la=127-la;
        cosi = fp32_16_16(0xffff0000,1); // This is -1.0
	}

	fp32_16_16 cos( costable[la] ,1);
	fp32_16_16 sin( costable[63-la] ,1);

	cos*=cosi;
	sin*=sini;

	speedx = (speed * cos);
	speedy = (speed * sin);

	*sx = speedx.asInt();
	*sy = -1 * speedy.asInt(); // Vertical is inverted.
}

void update_player()
{
	player_moved=0;

	if (digitalRead(LEFT_PIN)) {
		if (player_x < (VGA_COLUMNS*8)-PLAYER_SIZE_PIX) {
			player_x ++;
                        player_moved=1;
                }

	}
	if (digitalRead(RIGHT_PIN)) {
		if (player_x!=0) {
			player_x --;
                        player_moved=1;
		}
	}

}

void setup()
{
#ifdef SIMULATION
	delay(1000);
#endif
	VGAZX.begin();
	VGAZX.clrscr();
	Serial.begin(115200);
	pinMode(JUMP_PIN,INPUT);
	pinMode(LEFT_PIN,INPUT);
        pinMode(RIGHT_PIN,INPUT);
	SmallFS.begin();
	SmallFSFile map = SmallFS.open("map");
	if (map.valid()) {
		map.read(screenblocks,sizeof(screenblocks));
	}

	speed = 16.0;
	angle = 32;

	draw_block_screen();
}

void loop()
{
	ball_demo();
	ftick++;
	update_player();
}
