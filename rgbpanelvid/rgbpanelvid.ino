#include "panel.h"
#include <SD.h>
#include <SPI.h>
#include "cbuffer.h"
#include <Adafruit_GFX.h> // Needed by the panel
#include <Timer.h>

using namespace ZPUino;

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

typedef unsigned int videoframe[32*64];
unsigned char tempVideoBuf[ 3*64*32 ];
int video_index;
FILE *fp_handle;

#define FPGA_PIN_SDCS WING_C_4 
#define EXPBUFSIZE 6

videoframe frames[1<<EXPBUFSIZE];
CircularBuffer<uint32_t,EXPBUFSIZE> videoBuffer;

static unsigned linearize(unsigned value)
{
    return linearTable[value&0xff];
}

bool showFrame(void *unused)
{
    if (videoBuffer.hasData()) {
        unsigned v = videoBuffer.pop();
        RGBPanel.setRawImage( frames[v] );
        RGBPanel.apply();
    }
    return 1;
}

void handleFrame(const unsigned char *buf, int index)
{
    int i;
    int p=0;
    for (i=32*64;i!=0;i--) {
        unsigned v;
        v = (linearize(*buf++) << 16); // R
        v+= linearize(*buf++) <<8;  // g
        v+= linearize(*buf++);  // b
        frames[index][ p++ ] = v;
    }
}

void setup()
{
    Serial.begin(115200);

    SPI.begin( MOSI(WING_C_3), MISO(WING_C_1), SCK(WING_C_2) );
    SPI.setClockDivider(SPI_CLOCK_DIV2);
    RGBPanel.begin();

    pinMode(FPGA_PIN_SDCS, OUTPUT);

    do {
        if (SD.begin(FPGA_PIN_SDCS))
            break;
        Serial.println("Error initializing SDcard");
        delay(2000);
    } while (1);

    fp_handle = fopen("sd:/video.raw","r");

    if (fp_handle == NULL) {
        Serial.println("Cannot open file");
        while (1) {}
    }

    RGBPanel.clear();
    // pre-fill the buffer
    videoBuffer.clear();
    for (video_index=0;video_index<(1<<EXPBUFSIZE)-1;video_index++) {
        fread(tempVideoBuf, sizeof(tempVideoBuf), 1, fp_handle);
        handleFrame(tempVideoBuf, video_index);
        videoBuffer.push(video_index);
    }
    Timers.begin();
    Timers.periodicHz( 25, &showFrame, NULL);
}

void loop()
{
    do {
        fread(tempVideoBuf, sizeof(tempVideoBuf), 1, fp_handle);
        handleFrame(tempVideoBuf,video_index);
        while (videoBuffer.isFull()) { }
        videoBuffer.push(video_index);
        video_index++;
        video_index &= (1<<EXPBUFSIZE)-1;
    } while (1);
}

RGBPanel_class RGBPanel;
