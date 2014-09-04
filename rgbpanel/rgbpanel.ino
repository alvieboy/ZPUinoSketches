#include "godofredo.h"
#include "wiichuck.h"
#include "mvfilter.h"
#include "panel.h"
#include "Adafruit_GFX.h"
#include "wiipositioner.h"

#include <SD.h>
#include <SigmaDelta.h>
#include <SPI.h>

#include "audio.h"

extern void ball_setup();
extern void ball_demo();
extern void ball_calibrate();

const unsigned char linearTable[256] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 5,
    5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7,
    7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 10, 10, 10,
    10, 11, 11, 11, 11, 12, 12, 12, 13, 13, 13, 14, 14, 14, 14, 15,
    15, 16, 16, 16, 17, 17, 17, 18, 18, 19, 19, 20, 20, 20, 21, 21,
    22, 22, 23, 23, 24, 25, 25, 26, 26, 27, 27, 28, 29, 29, 30, 31,
    31, 32, 33, 34, 34, 35, 36, 37, 38, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 52, 54, 55, 56, 57, 59, 60, 61, 63,
    64, 65, 67, 68, 70, 72, 73, 75, 76, 78, 80, 82, 83, 85, 87, 89,
    91, 93, 95, 97, 99, 102, 104, 106, 109, 111, 114, 116, 119, 121, 124, 127,
    129, 132, 135, 138, 141, 144, 148, 151, 154, 158, 161, 165, 168, 172, 176, 180,
    184, 188, 192, 196, 201, 205, 209, 214, 219, 224, 229, 234, 239, 244, 249, 255
};

unsigned linearize(unsigned value)
{
    return linearTable[value&0xff];
}


AudioPlayer audioPlayer;



int minx, miny, maxx, maxy;


MovingAverageFilter<unsigned,1024> xfilter,yfilter;

#define SHIFTPOS 5
#define DEADZONE 5 /* Joy dead zone */

static int xpos;
static int ypos;

static int centerx,centery; // Joy center

void setup() {
  // put your setup code here, to run once:
  int index;
  Serial.begin(115200);
  Serial1.begin(460800);
  Serial.println("Hello");

  // Setup SPI for SD card.
  SPI.begin(
            MOSI(WING_C_3), // 11
            MISO(WING_C_1), // 9
            SCK(WING_C_2) // 10
           );

  SPI.setClockDivider(SPI_CLOCK_DIV2);

  Serial1.setPins( TX(WING_B_1), RX(WING_B_0) );

  WIIChuck.begin();
  WIIChuck.init_nunchuck();
  RGBPanel.begin();
  RGBPanel.clear();

  RGBPanel.setTextColor(0x808000);
  RGBPanel.setTextWrap(false);

  WIIChuck.update();
  RGBPanel.setTextSize(1);

  WIIChuck.update();

  centerx = WIIChuck.getJoyX();
  centery = WIIChuck.getJoyY();
  ball_calibrate();

  unsigned pos=63;
  while (!WIIChuck.getCButton()) {
      //WIIChuck.update();
      RGBPanel.clear();
      RGBPanel.setCursor(pos,0);
      RGBPanel.print("Move your ");
      RGBPanel.setCursor(pos,8);
      RGBPanel.print("WII Chuck ");
      RGBPanel.apply();
      delay(5);
      if (pos>2)
          pos--;
      else
          break;
  }

  while (!WIIChuck.getCButton()) {
      WIIChuck.update();

      RGBPanel.setCursor(0,16);

      RGBPanel.fillRect(0, 16, RGBPanel.width(), 16 + 8, 0);
      RGBPanel.print("Joy ");

      RGBPanel.setTextColor(0xff00ff);
      RGBPanel.print(WIIChuck.getJoyX());

      RGBPanel.setCursor(46,16);
      RGBPanel.setTextColor(0x008080);
      RGBPanel.print(WIIChuck.getJoyY());

      RGBPanel.fillRect(0, 24, RGBPanel.width(), 24 + 8, 0);
      RGBPanel.setCursor(0,24);

      RGBPanel.print("Acc ");

      int x = WIIChuck.getAccelX();
      int y = WIIChuck.getAccelY();
      int z = WIIChuck.getAccelZ();

      RGBPanel.print(x);

      RGBPanel.apply();
  }

  xpos = (WIDTH/2)<<SHIFTPOS;
  ypos = (HEIGHT/2)<<SHIFTPOS;

}

unsigned random()
{
    return TIMERTSC;
}

static const int colors[8] = {
    0x300000,
    0x003000,
    0x000030,
    0x003030,
    0x300030,
    0x303000,
    0x300000,
    0x003000
};

static const int colors_lighter[8] = {
    0xA00000,
    0x00A000,
    0x0000A0,
    0x00A0A0,
    0xA000A0,
    0xA0A000,
    0xA00000,
    0x00A000
};





template<unsigned int W, unsigned int H>
class Blocks {
    static const int BlockWidth = 8;
    static const int BlockHeight = 3;

    static const int WB = W / BlockWidth;
    static const int HB = 3;
    static const int NC = 8; // Colors

    Positioner_class<WIDTH-5, 0> positioner;

public:
    Blocks(Adafruit_GFX *base): D(base)
    {
        positioner.set((WIDTH/2)-3, 0);
    }

    void start()
    {
        D->fillRect(0,0,W,H,0);
        initBlocks();
        drawBlocks();
        RGBPanel.apply();
    }
    void go()
    {
        while(1) {
            positioner.update();
            D->fillRect( 0, HEIGHT-2, WIDTH, HEIGHT, 0);
            // Place

            D->fillRect( positioner.getX(), HEIGHT-2, 5, 2, 0x808080);

            RGBPanel.apply();
            delay(1);
        }
    }
    void initBlocks()
    {
        int x,y;
        for (x=0;x<WB;x++)
            for (y=0;y<HB;y++) {
                blocks[x][y] = random()%NC;
            }
    }
    void drawBlocks()
    {
        D->fillRect(0,0,W,HB*2,0x0);
        int x,y;
        for (x=0;x<WB;x++)
            for (y=0;y<HB;y++) {
                int colorval = blocks[x][y];
                if (colorval>=0) {
                    D->fillRect(x*BlockWidth,
                                y*BlockHeight,
                                BlockWidth-1,
                                BlockHeight-1,
                                colors_lighter[ colorval ]);
                    // Lighten color a bit now

                    unsigned color = colors_lighter[colorval];
#if 1
                    D->drawFastHLine(x*BlockWidth,
                                     ((y+1)*BlockHeight)-1,
                                     BlockWidth,
                                     color);

                    D->drawFastVLine(((x+1)*BlockWidth)-1,
                                     y*BlockHeight,
                                     BlockHeight-1,color);
#endif
                }
            }
    }

    Adafruit_GFX *D;
    int blocks[WB][HB];
};

static Blocks<64,32> mBlocks(&RGBPanel);


// Accelerometer test
extern unsigned int hsvtable[256];




void loop() {
    static int mode = 0;
    static unsigned frame = 0;

#if 0
    for (int x=0;x<64;x++)
        for (int y=0;y<32;y++) {
            setPixel(x, y, x<<1, x<<1, x<<1);
        }
    setPixel(0,1,255,255,0);
    apply();
#endif

#if 0
    set(image1);
    apply();
    delay(200);
    set(image2);
    apply();
    delay(200);
#endif
    static int r=0,g=0,b=0;

    WIIChuck.update();
    printf("Chuck: %d %d buttons %d %d\n",
           WIIChuck.getJoyX(),
           WIIChuck.getJoyY(),
           WIIChuck.getZButton(),
           WIIChuck.getCButton());

    switch (WIIChuck.getButtons()) {
    case BUTTON_C:
        RGBPanel.setImage(godofredo_image);
        break;
    case BUTTON_ZC:
        r = linearize(WIIChuck.getJoyX());
        g = linearize(WIIChuck.getJoyY());
        RGBPanel.clear( (r<<16) + (g<<8) + b);
        break;
    default:
        RGBPanel.clear( 0x0 );
        {
#if 1
            int jx = (int)WIIChuck.getJoyX() - centerx;
            int jy = centery - (int)WIIChuck.getJoyY();

            if (jx>DEADZONE || jx<-DEADZONE) {
                xpos+=jx;
            }

            if (jy>DEADZONE|| jy<-DEADZONE) {
                ypos+=jy;
            }

            if (xpos<0)
                xpos=0;

            if (ypos<0)
                ypos=0;

            if (xpos>=WIDTH<<SHIFTPOS)
                xpos =(WIDTH<<SHIFTPOS)-1;

            if (ypos>=HEIGHT<<SHIFTPOS)
                ypos =(HEIGHT<<SHIFTPOS)-1;
#endif
            RGBPanel.drawPixel(xpos>>SHIFTPOS,
                               ypos>>SHIFTPOS, 0xdeadbe);
            printf("X %d Y %d\n", xpos,ypos);
            printf("jX %d jY %d\n", jx,jy);
        }
        break;
    case BUTTON_Z:
#if 0
        if ((frame & 1) ==0) {
            RGBPanel.clear( 0x0 );
            {
                int x = WIIChuck.getAccelX();
                x =xfilter.push(x);
                int y = WIIChuck.getAccelY();
                y = yfilter.push(y);
                x-=512;
                x/=(128/WIDTH);
                y-=512;
                y/=(128/HEIGHT);
                if (x<0) x=0;
                if (y<0) y=0;
                if (x>=WIDTH) x=WIDTH-1;
                if (y>=HEIGHT) y=HEIGHT-1;

                RGBPanel.drawPixel(x,y, 0x803080);
            }
        }
#else
        ball_demo();
#if 1
        audioPlayer.begin();
        audioPlayer.open();
        audioPlayer.play();
#endif

        //videoLoop();
        //mBlocks.start();
        //mBlocks.go();
#endif
        break;
    }
    RGBPanel.apply();
    frame++;

}

#define HDLC_frameFlag 0x7E
#define HDLC_escapeFlag 0x7D
#define HDLC_escapeXOR 0x20

int syncSeen;
int unescaping;
int bufferpos;
unsigned char buffer[8192];

inline int inbyte()
{
    while (!Serial1.available());
    return Serial1.read();
}

void processFrame(unsigned char *buf,int size)
{
    int x,y;
    printf("Frame, size %d\r\n",size);
    if (size==(32*64*3)) {
        for (x=0;x<RGBPanel.width(); x++) {
            for (y=0;y<RGBPanel.height();y++) {
                unsigned v=(linearize((unsigned)*buf++))<<16;
                v+=(linearize((unsigned)*buf++))<<8;
                v+=(linearize((unsigned)*buf++));
                RGBPanel.drawPixel(x,y,v);
            }
        }
        RGBPanel.apply();
    }
}

void videoLoop() {

    syncSeen = 0;
    unescaping = 0;
    while (1) {
        int i;
        i = inbyte();
        if (syncSeen) {
            if (i==HDLC_frameFlag) {
                if (bufferpos>0) {
                    syncSeen=0;
                    processFrame(buffer, bufferpos);
                }
            } else if (i==HDLC_escapeFlag) {
                unescaping=1;
            } else if (bufferpos<sizeof(buffer)) {
                if (unescaping) {
                    unescaping=0;
                    i^=HDLC_escapeXOR;
                }
                buffer[bufferpos++]=i;
            } else {
                syncSeen=0;
            }
        } else {
            if (i==HDLC_frameFlag) {
                bufferpos=0;
                syncSeen=1;
                unescaping=0;
            }
        }
    }
}

RGBPanel_class RGBPanel;
