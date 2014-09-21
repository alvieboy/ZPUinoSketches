#ifndef __PANEL_H__
#define __PANEL_H__

#include "dualpanel.h"

#if 0

#include "Adafruit_GFX.h"
#include <BaseDevice.h>
#include <Arduino.h>

#define RGBBASE IO_SLOT(9)
#define WIDTH 64
#define HEIGHT 32
#define NUMPIXELS (WIDTH*32)

extern const unsigned char linearTable[256];

using namespace ZPUino;

class RGBPanel_class: public Adafruit_GFX, public BaseDevice
{
public:
    RGBPanel_class(): Adafruit_GFX(), BaseDevice(1)
    {
    }
    void begin()
    {
        if (deviceBegin(0x08, 0x20)==0) {
            // Read in width and weight
            unsigned sizeinfo = REG(0);
            Adafruit_GFX::begin(sizeinfo>>16, sizeinfo &0xffff);
        }
    }
    virtual void drawPixel(int x, int y, unsigned int v)
    {
        if (x<WIDTH && y<HEIGHT)
            screen[x+(y*WIDTH)]=v;
    }
    virtual unsigned readPixel(int x, int y)
    {
        if (x<WIDTH && y<HEIGHT)
            return screen[x+(y*WIDTH)];
        return 0;
    }

    void clear(unsigned int color=0)
    {
        int index;
        for (index=0;index<NUMPIXELS;index++) {
            screen[index] = color;
        }
    }


    void apply()
    {
        int i;
        unsigned offset = 32*64;

        for (i=0;i<NUMPIXELS;i++) {
            unsigned int v = screen[i];
            REGISTER(RGBBASE, i+offset) = v;
        }
    }
    inline void setPixel(int x, int y, int r, int g, int b)
    {
        unsigned char c0 = linearTable[r&0xff];
        unsigned char c1 = linearTable[g&0xff];
        unsigned char c2 = linearTable[b&0xff];
        unsigned int v = c0 + (c1<<8) + (c2<<16);
        screen[x+(y*WIDTH)]=v;
    }

    inline void setPixelRaw(int x, int y, int v)
    {
        screen[x+(y*WIDTH)]=v;
    }

    void setRawImage(const unsigned int *base)
    {
        int i;
        for (i=0;i<NUMPIXELS; i++) {
            // Load and parse each of the RGB.
            unsigned int v = base[i];
            // Apply fix up
            unsigned char c0 = v&0xff;
            unsigned char c1 = (v>>8)&0xff;
            unsigned char c2 = (v>>16)&0xff;
            v = c0 + (c1<<8) + (c2<<16);
            screen[i] = v;
        }
    }
    void setImage(const unsigned int *base)
    {
        int i;
        for (i=0;i<NUMPIXELS; i++) {
            // Load and parse each of the RGB.
            unsigned int v = base[i];
            // Apply fix up
            unsigned char c0 = linearTable[v&0xff];
            unsigned char c1 = linearTable[(v>>8)&0xff];
            unsigned char c2 = linearTable[(v>>16)&0xff];
            v = c0 + (c1<<8) + (c2<<16);
            screen[i] = v;
            //screen[i+(32*32)] = v;
        }
    }

    unsigned int screen[NUMPIXELS];
};
extern RGBPanel_class RGBPanel;
#endif

#endif
