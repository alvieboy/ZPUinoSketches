#ifndef __DUALPANEL_H__
#define __DUALPANEL_H__

#include "Adafruit_GFX.h"
#include <BaseDevice.h>
#include <Arduino.h>

#define RGBBASE IO_SLOT(9)
#define WIDTH 96
#define HEIGHT 64
//#define NUMPIXELS (WIDTH*32)

extern const unsigned char linearTable[256];

using namespace ZPUino;

class DualRGBPanel_class: public Adafruit_GFX
{
public:
    BaseDevice p0,p1;
    unsigned NUMPIXELS;

    DualRGBPanel_class(): Adafruit_GFX(), p0(1), p1(2)
    {
    }
    void begin()
    {
        unsigned sizeinfo;

        if (p0.deviceBegin(0x08, 0x20)==0) {
            // Read in width and weight
            sizeinfo = p0.REG(0);
        } else {
            Serial.println("Cannot find primary RGB panel");
            return;
        }
        if (p1.deviceBegin(0x08, 0x20)==0) {
            // Read in width and weight
            if (sizeinfo != p1.REG(0)) {
                Serial.println("Invalid RGB panel configuration");
            }
        }else {
            Serial.println("Cannot find secondary RGB panel");
            return;
        }
        Serial.println("Found dual RGB panel controller");
        Serial.print("Combined panel size: ");

        Serial.print(sizeinfo>>16);
        Serial.print("x");
        Serial.print((sizeinfo &0xffff)*2);
        Serial.println(" pixels");

        NUMPIXELS=(sizeinfo>>16)*((sizeinfo &0xffff)*2);

        screen = (unsigned int*)malloc(sizeof(unsigned int)*NUMPIXELS);

        printf("Panel 0 at slot %d, instance %d, base register 0x%08x\r\n",
                   p0.getSlot(), p0.getInstance(), p0.getBaseRegister());
        printf("Panel 1 at slot %d, instance %d, base register 0x%08x\r\n",
                   p1.getSlot(), p1.getInstance(), p1.getBaseRegister());

        Adafruit_GFX::begin(sizeinfo>>16, (sizeinfo &0xffff)*2);
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

    void test()
    {
    }

    bool applyEnabled;

    void setApplyEnabled(bool e) {
        applyEnabled=e;
    }

    void apply()
    {
        int i,z;
        unsigned *fbptr=&screen[0];
        unsigned offset = 32*128;
        unsigned linecomp = 128-96;

        if (!applyEnabled)
            return;

        for (i=0;i<32;i++) {
            for (z=0;z<96;z++) {
                unsigned int v = *fbptr;
                p0.REG(offset) = v;
                v = fbptr[96*32];
                p1.REG(offset) = v;
                fbptr++;
                offset++;
            }
            /* Move to next line, hw */
            offset+=linecomp;
        }
    }

    static inline unsigned computeOverlay(const unsigned int *screen, const unsigned int *overlay)
    {
        const unsigned char *sourceptr = (const unsigned char *)overlay;
        const unsigned char *destptr = (const unsigned char *)screen;
        unsigned int v = 0;
        unsigned char *vptr = (unsigned char *)&v;

        int z;
        int alpha = sourceptr[0];
        for (z=1;z<4;z++) {
            int dst = destptr[z];
            int src = sourceptr[z];
            int fixup = (alpha*(src-dst))/256;
            dst += fixup;
            vptr[z] = dst&0xff;
        }
        return v;
    }

    void applyWithOverlay(const unsigned int *overlay)
    {

        int i,z;
        unsigned *fbptr=&screen[0];
        unsigned offset = 32*128;
        unsigned linecomp = 128-96;

        for (i=0;i<32;i++) {
            for (z=0;z<96;z++) {
                unsigned int v = computeOverlay(fbptr, overlay);
                p0.REG(offset) = v;
                v = computeOverlay(&fbptr[96*32], &overlay[96*32]);
                p1.REG(offset) = v;
                fbptr++;
                overlay++;
                offset++;
            }
            /* Move to next line, hw */
            offset+=linecomp;
        }
    }



    inline void setPixel(int x, int y, int r, int g, int b)
    {
        unsigned char c0 = linearTable[r&0xff];
        unsigned char c1 = linearTable[g&0xff];
        unsigned char c2 = linearTable[b&0xff];
        unsigned int v = c2 + (c1<<8) + (c0<<16);
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

    void setImageAlpha(const unsigned int *base)
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

    void setImageAlphaRaw(const unsigned int *base)
    {
        int i,z;
        for (i=0;i<NUMPIXELS; i++) {
            // Load and parse each of the RGB.
            unsigned char *cptr = (unsigned char *)&base[i];
            unsigned char *sptr = (unsigned char *)&screen[i];
            unsigned alpha = cptr[0];
            alpha+=1;
            alpha<<=8;

            for (z=1;z<4;z++) {
                int src = cptr[z];
                int dst = sptr[z];
                dst = dst + (alpha*(src-dst))>>8;
                sptr[z] = dst;
                /*
                 d = sA*(1-p)+dA*p;
                 d = sA + p*dA - p*sA;
                 d = sA + p(dA-sA)
                    1 => d = sA+dA-sA == dA
                    0 => d = sA
                    0.5 => d = sA + 0.5dA - 0.5sA =
                */
            }
        }
    }
    unsigned int *getScreen() {
        return screen;
    }
    unsigned int *screen;
};
extern DualRGBPanel_class RGBPanel;

#endif
