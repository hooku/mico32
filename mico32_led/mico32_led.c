#include "mico32_common.h"

#include "MICO.h"
#if 1
#include "example/common/common.h"
#endif

// this code is stripped from:
// http://stackoverflow.com/questions/3018313/algorithm-to-convert-rgb-to-hsv-and-hsv-to-rgb-in-range-0-255-for-both

typedef struct RgbColor
{
    unsigned char r;
    unsigned char g;
    unsigned char b;
} RgbColor;

typedef struct HsvColor
{
    unsigned char h;
    unsigned char s;
    unsigned char v;
} HsvColor;

static RgbColor hsv_2_rgb(HsvColor hsv)
{
    RgbColor rgb;
    unsigned char region, remainder, p, q, t;

    if (hsv.s == 0)
    {
        rgb.r = hsv.v;
        rgb.g = hsv.v;
        rgb.b = hsv.v;
        return rgb;
    }

    region = hsv.h / 43;
    remainder = (hsv.h - (region * 43)) * 6; 

    p = (hsv.v * (255 - hsv.s)) >> 8;
    q = (hsv.v * (255 - ((hsv.s * remainder) >> 8))) >> 8;
    t = (hsv.v * (255 - ((hsv.s * (255 - remainder)) >> 8))) >> 8;

    switch (region)
    {
        case 0:
            rgb.r = hsv.v; rgb.g = t; rgb.b = p;
            break;
        case 1:
            rgb.r = q; rgb.g = hsv.v; rgb.b = p;
            break;
        case 2:
            rgb.r = p; rgb.g = hsv.v; rgb.b = t;
            break;
        case 3:
            rgb.r = p; rgb.g = q; rgb.b = hsv.v;
            break;
        case 4:
            rgb.r = t; rgb.g = p; rgb.b = hsv.v;
            break;
        default:
            rgb.r = hsv.v; rgb.g = p; rgb.b = q;
            break;
    }

    return rgb;
}

void play_led(char *cmd)
{
    unsigned char power;
    
    int hue, sat, val,
        r, g, b;
    
    float f_h, f_s, f_v;
    
    HsvColor cmd_hsv;
    RgbColor cmd_rgb;

    sscanf(cmd, "%d,%d,%d,%d", &power, &hue, &sat, &val);
    
    f_h = (float)(hue*0xFF)/360;        // TODO: replace with left shift
    f_s = (float)(sat*0xFF)/100;
    f_v = (float)(val*0xFF)/100;
    
    cmd_hsv.h = f_h;
    cmd_hsv.s = f_s;
    cmd_hsv.v = f_v;
    
    cmd_rgb = hsv_2_rgb(cmd_hsv);
    
    r = cmd_rgb.r*100/0xFF;
    g = cmd_rgb.g*100/0xFF;
    b = cmd_rgb.b*100/0xFF;
    
    DBG_PRINT("\r\n(%d,%d,%d) -> (%d,%d,%d)\r\n",
              hue, sat, val, r, g, b);
    
#ifdef DOG_LED
    mico_dog_led_set(r, g, b);
#else // DOG_LED
    if (power == 1)
    {
#if 0
        MicoPwmInitialize((mico_pwm_t)MICO_POWER_LED, 10000, hue);
        MicoPwmInitialize((mico_pwm_t)MICO_SYS_LED  , 10000, sat);
        MicoPwmInitialize((mico_pwm_t)MICO_RF_LED   , 10000, val);
#else 
        MicoPwmInitialize((mico_pwm_t)MICO_POWER_LED, 10000, r);
        MicoPwmInitialize((mico_pwm_t)MICO_SYS_LED  , 10000, g);
        MicoPwmInitialize((mico_pwm_t)MICO_RF_LED   , 10000, b);
#endif
        MicoPwmStart((mico_pwm_t)MICO_POWER_LED     );
        MicoPwmStart((mico_pwm_t)MICO_SYS_LED       );
        MicoPwmStart((mico_pwm_t)MICO_RF_LED        );
    }
    else
    {
        MicoPwmStop((mico_pwm_t)MICO_POWER_LED      );
        MicoPwmStop((mico_pwm_t)MICO_SYS_LED        );
        MicoPwmStop((mico_pwm_t)MICO_RF_LED         );
    }
#endif // DOG_LED

    return ;
}
