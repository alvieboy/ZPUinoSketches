#include "VGA.h"
#include "SmallFS.h"
#include "structures.h"
#include "cbuffer.h"

// AUDIO STUFF

#define YM2149BASE IO_SLOT(11)
#define YM2149REG(x) REGISTER(YM2149BASE,x)

//#define HAVE_POKEY
#define HAVE_YM

#define POKEYBASE IO_SLOT(12)
#define POKEYREG(x) REGISTER(POKEYBASE,x)

struct ymframe {
	unsigned char regval[16];
};
struct pokeyframe {
	unsigned char regval[9];
};
CircularBuffer<ymframe,2> YMaudioBuffer;
CircularBuffer<pokeyframe,2> POKEYaudioBuffer;

SmallFSFile ymaudiofile;
SmallFSFile pokeyaudiofile;

#define AUDIOPIN WING_A_15
#define AUDIOPINEXTRA WING_A_13

#define BUTTON_DOWN 26 /* WING_B_10 */
#define BUTTON_ROTATE 25 /* WING_B_9 */
#define BUTTON_LEFT 24 /* WING_B_8 */
#define BUTTON_RIGHT 27 /* WING_B_11 */



static int px = -1; /* Piece current X position */
static int py = 0; /* Piece current Y position */
static int tick = 0;
static int tickmax = 10;
unsigned score=0; /* Our score */
unsigned level=0; /* Current level */
unsigned lines=0; /* Number of lines made (all levels) */
unsigned currentlevel_lines=0; /* This level number of lines made */
unsigned lines_total=0;

unsigned char lval[4],cval[4];

/* This will hold the current game area */
unsigned char area[blocks_x][blocks_y];

/* This is used to save background image on the nextpiece area */
unsigned char nextpiecearea[piecesize_max * blocksize * piecesize_max * blocksize];

/* Current piece falling */
static struct piece currentpiece;

/* Next piece */
static struct piece nextpiece;

/* Whether the current piece is valid, or if we need to allocate a new one */
static bool currentpiecevalid=false;

/* Colors for current and next piece */
static color_type *currentcolor, *nextcolor;

static unsigned int timerTicks = 0;

const int OPERATION_CLEAR=0;
const int OPERATION_DRAW=1;

/* The score area definition */

#define SCORECHARS 6
#define SCOREX 90
#define SCOREY 40

/* The score area. We use this to save the BG image */

unsigned char scorearea[ 9 * ((8* SCORECHARS)+1) ];

typedef void (*loopfunc)(void);

enum {
	START,
	PLAY
} game_state = START;

enum event_t hasEvent()
{
	enum event_t ret = event_none;

	cval[0] = digitalRead( BUTTON_DOWN );
	cval[1] = digitalRead( BUTTON_ROTATE );
	cval[2] = digitalRead( BUTTON_LEFT );
	cval[3] = digitalRead( BUTTON_RIGHT );

	if (!lval[0] && cval[0] ) {
		ret = event_down;
	} else if (!lval[1] && cval[1]) {
		ret = event_rotate;
	} else if (!lval[2] && cval[2]) {
		ret = event_left;
	} else if (!lval[3] && cval[3]) {
		ret=  event_right;
	}
	*((unsigned*)lval)=*((unsigned*)cval);

	return ret;
}



#define DEBUG(x...)


static color_type colors[] = {
	{
		RED, RED, RED, RED, WHITE,
		RED, RED, RED, RED, WHITE,
		RED, RED, RED, RED, WHITE,
		RED, RED, RED, RED, WHITE,
		WHITE,WHITE,WHITE,WHITE,WHITE
	},
	{
		PURPLE, PURPLE, PURPLE, PURPLE, WHITE,
		PURPLE, PURPLE, PURPLE, PURPLE, WHITE,
		PURPLE, PURPLE, PURPLE, PURPLE, WHITE,
		PURPLE, PURPLE, PURPLE, PURPLE, WHITE,
		WHITE,WHITE,WHITE,WHITE,WHITE
	},
	{
		BLUE, BLUE, BLUE, BLUE, WHITE,
		BLUE, BLUE, BLUE, BLUE, WHITE,
		BLUE, BLUE, BLUE, BLUE, WHITE,
		BLUE, BLUE, BLUE, BLUE, WHITE,
		WHITE,WHITE,WHITE,WHITE,WHITE
	},
	{ 
		YELLOW, YELLOW, YELLOW, YELLOW, WHITE,
		YELLOW, YELLOW, YELLOW, YELLOW, WHITE,
		YELLOW, YELLOW, YELLOW, YELLOW, WHITE,
		YELLOW, YELLOW, YELLOW, YELLOW, WHITE,
		WHITE,WHITE,WHITE,WHITE,WHITE
	}
};

static struct piece allpieces[] = {
	{
        3,
		{
			{0,0,0,0},
			{0,1,0,0},
			{1,1,1,0},
			{0,0,0,0}
		}
	},
	{
		3,
		{
			{1,0,0,0},
			{1,0,0,0},
			{1,1,0,0},
			{0,0,0,0}
		}
	},
	{
		3,
		{
			{0,0,1,0},
			{0,0,1,0},
			{0,1,1,0},
			{0,0,0,0}
		}
	},
	{
        3,
		{
			{0,0,0,0},
			{1,1,0,0},
			{0,1,1,0},
			{0,0,0,0}
		}
	},
	{
        3,
		{
			{0,0,0,0},
			{0,1,1,0},
			{1,1,0,0},
			{0,0,0,0}
		}
	},
    {
        2,
		{
			{1,1,0,0},
			{1,1,0,0},
			{0,0,0,0},
			{0,0,0,0}
		}
	},
    {
        4,
		{
			{0,0,0,0},
			{0,0,0,0},
			{1,1,1,1},
			{0,0,0,0}
		}
	},


};

/* Random function (sorta) */
#ifndef __linux__
unsigned xrand() {
	CRC16APP=TIMERTSC;
	return CRC16ACC;
}
#else
#define xrand rand
#endif

void _zpu_interrupt()
{
	// Play
	if (YMaudioBuffer.hasData()) {
		int i;
		ymframe f = YMaudioBuffer.pop();
		for (i=0;i<16; i++) {
			YM2149REG(i) = f.regval[i];
		}
	}
#ifdef HAVE_POKEY
	if (POKEYaudioBuffer.hasData()) {
		int i;
		pokeyframe f = POKEYaudioBuffer.pop();
		for (i=0;i<9; i++) {
			POKEYREG(i) = f.regval[i];
		}
	}
#endif
	timerTicks++;

	TMR0CTL &= ~(BIT(TCTLIF));
}


struct piece *getRandomPiece()
{
	int i = xrand() % sizeof(allpieces)/sizeof(struct piece);
	DEBUG("Returning piece %i\n",i);
	return &allpieces[i];
}


color_type *getRandomColor()
{
	int i = xrand() % sizeof(colors)/sizeof(color_type);
	DEBUG("Returning piece %i\n",i);
	return &colors[i];
}

void draw_simple_block(int x, int y, color_type *color)
{
	int ay = board_y0 + y*blocksize;
	int ax = board_x0 + x*blocksize;

	VGA.writeArea(ax,ay,blocksize,blocksize,*color);
}

void area_init()
{
	int x,y;

	SmallFSFile l = SmallFS.open("level.dat");

	if (l.valid()) {

		l.read(&currentlevel_lines, sizeof(currentlevel_lines));

		Serial.print("Current level lines: ");
		Serial.println(currentlevel_lines);

		/*
			area[x][y]=currentlevel.area[x][y];
			*/
		l.read(&area,sizeof(area));
	}
}

bool can_place(int x, int y, struct piece *p)
{
	int i,j;
	for (i=0;i<p->size;i++)
		for (j=0;j<p->size;j++) {
			if (p->layout[j][i]) {
				if ( (x+i) >= blocks_x || (x+i) <0 ) {
					DEBUG("X overflow %d, %d\n",x,i);
					return false;
				}
				if ( (y+j) >= blocks_y || (x+y) <0 ) {
					DEBUG("Y overflow %d, %d\n",y,j);
					return false;
				}

				if (area[x+i][y+j]) {
					DEBUG("Collision at %d %d\n", x+i, y+j);
					return false;
				} else {
					DEBUG("Placement OK at %d %d\n", x+i, y+j);
				}
			}
		}
	return true;
}

void do_place(int x, int y, struct piece *p)
{
	int i,j;
	for (i=0;i<p->size;i++)
		for (j=0;j<p->size;j++) {
			if (p->layout[j][i]) {
				DEBUG("Marking %d %d\n", x+i,y+j);
				area[x+i][y+j] = 1;
			}
		}
}


void draw_piece(int x, int y, struct piece *p, color_type *color, int operation)
{
	int i,j,ax,ay;
	ay = board_y0 + y*blocksize;

	VGA.setColor(BLACK);

	for (i=0;i<p->size;i++) {
		ax= board_x0 + x*blocksize;

		for (j=0;j<p->size;j++) {
			if (p->layout[i][j]) {
				if (operation==OPERATION_CLEAR)
					VGA.drawRect(ax,ay,blocksize,blocksize);
				else
					VGA.writeArea(ax,ay,blocksize,blocksize,*color);
			}
			ax+=blocksize;
		}
		ay+=blocksize;
	}
}

char *sprintint(char *dest, int max, unsigned v)
{
	dest+=max;
	*dest--='\0';

	do {
		*dest = (v%10)+'0';
		dest--, max--;
		if (max==0)
			return dest;
		v=v/10;
	} while (v!=0);
	return dest+1;
}

void update_score()
{
	char tscore[SCORECHARS+1];
	char *ts = tscore;
	int x = SCOREX;

	int i;
	for (i=0;i<6;i++)
		tscore[i]=' ';

    tscore[6]='\0';

	VGA.writeArea( SCOREX, SCOREY, (8*SCORECHARS)+1, 8+1, scorearea);

	sprintint(tscore, sizeof(tscore)-1, score);

	while (*ts==' ') {
		ts++;
		x+=8;
	}
	VGA.setColor(BLACK);
	VGA.printtext(x,SCOREY,ts,true);

	VGA.setColor(YELLOW);
	VGA.printtext(x+1,SCOREY+1,ts,true);
}

void score_init()
{
	/* Save the score area */
	VGA.readArea( SCOREX, SCOREY, (8*SCORECHARS)+1, 8+1, scorearea);
}

void score_draw()
{
	VGA.setColor(BLACK);
	VGA.printtext(100, 30, "Score", true);
	VGA.setColor(YELLOW);
	VGA.printtext(101, 31, "Score", true);

	update_score();
}

void board_draw()
{
	int x,y;
	VGA.setColor(BLACK);
	VGA.drawRect(board_x0,board_y0,board_width,board_height);

	for (x=0;x<blocks_x;x++)
		for (y=0;y<blocks_y;y++) {
			if (area[x][y]) {
				draw_simple_block(x,y,&colors[ (area[x][y])-1 ]);
			}
		}
}

void clear_area()
{
	memset(&area,sizeof(area),0);
}

void blitImage(const char *name)
{
	VGA.blitStreamInit(0, 0, VGA.getHSize());

	SmallFSFile entry = SmallFS.open(name);
	if (entry.valid()) {
		entry.readCallback( entry.size(), &VGA.blitStream, (void*)&VGA );
	}
}

void entryImage(const char *name)
{
	blitImage(name);
	// Wait for upper key
	event_t ev;
	do {
		ev = hasEvent();
	} while (ev==event_none);

}

void setup()
{

	pinMode(AUDIOPIN,OUTPUT);
	digitalWrite(AUDIOPIN,HIGH);
	outputPinForFunction(AUDIOPIN, 14);
	pinModePPS(AUDIOPIN, HIGH);

#ifdef AUDIOPINEXTRA
	pinMode(AUDIOPINEXTRA,OUTPUT);
	outputPinForFunction(AUDIOPINEXTRA, 14);
	pinModePPS(AUDIOPINEXTRA, HIGH);
#endif

	VGA.clear();

	Serial.begin(115200);
	Serial.println("Starting");

	if (SmallFS.begin()<0) {
		Serial.println("No SmalLFS found, aborting.");
		while(1);
	}


	entryImage("eimage.dat");
	entryImage("eimage2.dat");


	blitImage("image.dat");
	ymaudiofile = SmallFS.open("audio.dat");
	pokeyaudiofile = SmallFS.open("sapdump.bin");


    // Init next piece Area

	VGA.readArea( board_x0 + (blocksize * (blocks_x+2)),
				 board_y0,
				 piecesize_max * blocksize,
				 piecesize_max * blocksize,
				 nextpiecearea
				);


	// Area init uses SmallFS also


	nextpiece=*getRandomPiece();
	nextcolor=getRandomColor();

	score_init();

	pre_game_start();

	INTRMASK = BIT(INTRLINE_TIMER0); // Enable Timer0 interrupt

	INTRCTL=1;

	// Start timer, 50Hz (prescaler 64)
	TMR0CMP = (CLK_FREQ/(50U*64))-1;
	TMR0CNT = 0x0;
	TMR0CTL = BIT(TCTLENA)|BIT(TCTLCCM)|BIT(TCTLDIR)|BIT(TCTLCP2)|BIT(TCTLCP0)|BIT(TCTLIEN);
}


void rotatepiece()
{
	struct piece rotated;

	int i,j;
	rotated.size = currentpiece.size;

	for (i=0;i<currentpiece.size;i++)
		for (j=0;j<currentpiece.size;j++) {
			rotated.layout[i][j] = currentpiece.layout[j][currentpiece.size-i-1];
		}

	if (can_place(px,py,&rotated)) {
		draw_piece( px, py, &currentpiece,  currentcolor, OPERATION_CLEAR);
		currentpiece=rotated;
		draw_piece( px, py, &currentpiece,  currentcolor, OPERATION_DRAW);
	}
}

int did_make_line()
{
	int x,y;
	for (y=blocks_y; y>=0;y--) {
		int count = 0;

		for (x=0;x<blocks_x;x++) {
			if (area[x][y])
				count++;
		}
		if (count==blocks_x) {
            return y;
		}
	}
	return -1;
}

void waitTick()
{
#ifdef __linux__
	sync();
	usleep(100000);
#endif
	timerTicks = 0;
	while ( timerTicks < 5 ) {
		audiofill();
	}
}

void special_effects(int y)
{
	// Thingie will disappear.

	unsigned offsety = board_y0 + (y * blocksize);

	int i;
	for (i=0;i<4;i++) {
		VGA.setColor(YELLOW);
		VGA.drawRect(board_x0,offsety, blocksize*blocks_x, blocksize);
		waitTick();
		VGA.setColor(RED);
		VGA.drawRect(board_x0,offsety, blocksize*blocks_x, blocksize);
		waitTick();
	}
	VGA.setColor(GREEN);

}

void line_done(int y)
{
	int x,py;

	DEBUG("Line done at %d\n",y);
	special_effects(y);
	// Shift down area
	for (py=y;py>=0;py--) {
		for (x=0;x<blocks_x;x++) {
			area[x][py] = py>0 ? area[x][py-1] : 0; // Clear
		}
	}

	// And shift screen

	if (y>0) {
		VGA.moveArea( board_x0,
					 board_y0,
					 blocks_x*blocksize,
					 y * blocksize,
					 board_x0,
					 board_y0 + blocksize
					);
	}

}

void checklines()
{
	int y;
	int s = 30;
	while ((y=did_make_line())!=-1) {
		score+=s;
		s=s*2;
		line_done(y);
	}
}

void draw_next_piece()
{
	VGA.setColor(BLACK);
	unsigned xpos = board_x0 + (blocksize * (blocks_x+2));
	unsigned ypos = board_y0;


	VGA.writeArea( xpos,
				  ypos,
				  piecesize_max * blocksize,
				  piecesize_max * blocksize,
                  nextpiecearea
				 );
    VGA.setColor(GREEN);

    draw_piece( blocks_x + 2, 0, &nextpiece, nextcolor, OPERATION_DRAW);
}

void processEvent( enum event_t ev )
{
	int nextx,nexty;
	bool checkcollision;

	checkcollision=false;

	nextx = px;
	nexty = py;

	if (ev==event_rotate) {
		rotatepiece();
	}
	if (ev==event_left) {
		nextx=px-1;
		DEBUG("left\n");
	}
	if (ev==event_right) {
		nextx=px+1;
		DEBUG("right\n");
	}
	if (ev==event_down || tick==tickmax) {
		nexty=py+1;
		DEBUG("down\n");
		checkcollision=true;
	}

	if (can_place(nextx,nexty, &currentpiece)) {
		draw_piece( px, py, &currentpiece,  currentcolor, OPERATION_CLEAR);
		py=nexty;
		px=nextx;
		draw_piece( px, py, &currentpiece,  currentcolor, OPERATION_DRAW);
	} else {
		if (checkcollision) {
			DEBUG("Piece is no longer\n");
			score+=7;
			do_place(px,py, &currentpiece);
			checklines();
			update_score();
			currentpiecevalid=false;
			py=0;
		}
	}

}

void end_of_game()
{
	DEBUG("End of game\n");
	game_state = START;
	pre_game_start();
}

void pre_game_start()
{
	clear_area();
	board_draw();
	VGA.setColor(WHITE);

/*	const int board_x0 = 40;
	const int board_width = blocks_x * blocksize;
  */
	VGA.printtext(board_x0 + (board_width/2) - ((5*8)/2), 30, "Press");
	VGA.printtext(board_x0 + (board_width/2) - ((6*8)/2), 38, "rotate");
	VGA.printtext(board_x0 + (board_width/2) - ((2*8)/2), 46, "to");
	VGA.printtext(board_x0 + (board_width/2) - ((5*8)/2), 54, "start");
}

void pre_game_play()
{
	currentpiecevalid=false;

	area_init();
	board_draw();
	
	score=0;
	lines=0;
	lines_total=0;

	score_draw();
}

void game_start()
{
	enum event_t ev = hasEvent();
	if (ev==event_rotate) {
		game_state = PLAY;
		pre_game_play();
	}
}

void audiofill()
{
	// Check audio
	int r;

#ifdef HAVE_YM
	ymframe f;
	while (!YMaudioBuffer.isFull()) {
		r = ymaudiofile.read(&f.regval[0], 16);
		if (r==0) {
			ymaudiofile.seek(0,SEEK_SET);
			ymaudiofile.read(&f.regval[0], 16);
		}
		YMaudioBuffer.push(f);
	}
#endif


#ifdef HAVE_POKEY
	pokeyframe p;
	while (!POKEYaudioBuffer.isFull()) {
		r = pokeyaudiofile.read(&p.regval[0], 9);
		if (r==0) {
			pokeyaudiofile.seek(0,SEEK_SET);
			pokeyaudiofile.read(&p.regval[0], 9);
		}
		POKEYaudioBuffer.push(p);
		}
#endif

}

void game_play()
{
	if (!currentpiecevalid) {
		px=5;py=0;
		currentpiece=nextpiece;
		currentcolor=nextcolor;

		nextpiece=*getRandomPiece();
		currentpiecevalid=true;
		nextcolor=getRandomColor();

		if (!can_place(px,py,&currentpiece)) {
			// End of game
			end_of_game();
			return;
		}
		draw_piece( px, py, &currentpiece,  currentcolor, OPERATION_DRAW);
		draw_next_piece();
	}

	enum event_t ev = hasEvent();
	tick++;

	if (tick==tickmax) {
		tick=0;
		processEvent(event_down);
	}

	if (ev!=event_none) {
		processEvent(ev);
	}

	delay(20);
}

static loopfunc loop_functions[] =
{
	&game_start,
	&game_play
};


void loop()
{
	audiofill();
	loop_functions[game_state]();
}
