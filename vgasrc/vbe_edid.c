// VESA VBE EDID capability for virtual display
//
// Copyright (C) 2012  Hiroshi Miura <miurahr@linux.com>
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "vbe.h" // VBE_CAPABILITY_8BIT_DAC
#include "biosvar.h" // GET_GLOBAL
#include "util.h" // dprintf
#include "config.h" // CONFIG_*
#include "vbe_edid.h"

int vesa_get_ddc_capabilities(u16 unit)
{
    if (unit != 0)
        return -1;

    return (1 << 8) | VBE_DDC1_PROTOCOL_SUPPORTED;
}

u8 most_chromaticity[8] VAR16 = {0xA6,0x55,0x48,0x9B,0x26,0x12,0x50,0x54};
unsigned char vgabios_name[] VAR16 = "Sea VGABIOS";
struct edid_detailed_timing vbe_edid_dtd_1920x1080 VAR16 = {
     WORDBE(0x3a02), 0x80, 0x90, 0x72, 0x38, 0x26,
     0x40, 0x78, 0xD0, 0x13, 0x00, 0x14, 0x2B, 0x21,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1280x1024 VAR16 = {
     WORDBE(0x2a30), 0x00, 0x98, 0x51, 0x00, 0x2A, 
     0x40, 0x30, 0x70, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1152x864 VAR16 = {
     WORDBE(0x2a30), 0x80, 0xC0, 0x41, 0x60, 0x24,
     0x30, 0x40, 0x80, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};
struct edid_detailed_timing vbe_edid_dtd_1600x1200 VAR16 = {
     WORDBE(0x3c8c), 0x40, 0x30, 0x62, 0xB0, 0x32,
     0x40, 0x40, 0xC0, 0x13, 0x00, 0x2C, 0xE1, 0x10,
     0x00, 0x00, 0x1E};

struct edid_monitor_range_limit vbe_edid_range VAR16 = {
   {0x00,0x00,0x00,0xfD,0x00},
   50,150,   /* Vertical 50Hz - 150Hz */
   31,135,   /* Horiz 31kHz - 135kHz */
   16,       /* clock 160MHz */
   0, /* no ext */
   0x0A,0x20,0x20,
   0x2020,
   0x20,0x20 /* padding */
   };

static struct prefered_mode_struct {u16 x; u16 y;} prefered_mode VAR16 = {0, 0};

int vesa_set_prefered_mode(u16 x, u16 y) {

    SET_VGA(prefered_mode.x, x);
    SET_VGA(prefered_mode.y, y);
    return 0;
}

static void get_prefered_mode(struct edid_detailed_timing *dtd, u16 *x, u16 *y) {
    u16 maxx, maxy;

    maxx = GET_GLOBAL(prefered_mode.x);
    maxy = GET_GLOBAL(prefered_mode.y);
    if (maxx == 0 && maxy == 0) {/* unknown, no-preference */
        dtd = &vbe_edid_dtd_1920x1080;
    } else if (maxx >= 1920 && (maxy >= 1080 || maxy == 0)){
        dtd = &vbe_edid_dtd_1920x1080;
    } else if (maxx >= 1600 && (maxy >= 1200 || maxy == 0)){
        dtd = &vbe_edid_dtd_1600x1200;
    } else {
        dtd = &vbe_edid_dtd_1280x1024;
    }
    *x = maxx; *y = maxy;
}

int vesa_read_edid_block0(u16 unit, u16 block, u16 seg, void *data, u8 next)
{
    struct vbe_edid_info *info = data;
    struct edid_detailed_timing *dtd = &vbe_edid_dtd_1920x1080;
    u16 max_x=1920;
    u16 max_y=1080;
    int i,j;

    memset_far(seg, info, 0, sizeof(*info));
    /* header */
    SET_FARVAR(seg, info->header[0], 0);
    for (i = 1; i < 7; i++) {
        SET_FARVAR(seg, info->header[i], 0xFF);
    }
    SET_FARVAR(seg, info->header[7], 0);
    /* Vendor/Product/Serial/Date */
    SET_FARVAR(seg, info->vendor, WORDBE(0x0421));
    SET_FARVAR(seg, info->product,WORDBE(0xABCD));
    SET_FARVAR(seg, info->serial, DWORDBE(0));
    /* date/version  */
    SET_FARVAR(seg, info->week,54);
    SET_FARVAR(seg, info->year,10); /* 2000 */
    SET_FARVAR(seg, info->major_version,1);
    SET_FARVAR(seg, info->minor_version,3); /* 1.3 */
    /* video prameters */
    SET_FARVAR(seg, info->video_setup,0x0F);
    /* Video signal interface (analogue, 0.700 : 0.300 : 1.000 V p-p,
       Video Setup: Blank Level = Black Level, Separate Sync H & V Signals are
       supported, Composite Sync Signal on Horizontal is supported, Composite 
       Sync Signal on Green Video is supported, Serration on the Vertical Sync
       is supported) */
    SET_FARVAR(seg, info->screen_width,0x21);
    SET_FARVAR(seg, info->screen_height,0x19); /* 330 mm * 250 mm */
    SET_FARVAR(seg, info->gamma,0x78); /* 2.2 */
    SET_FARVAR(seg, info->feature_flag,0x0F); /* no DMPS states, RGB, display is continuous frequency, prefered mode */
    SET_FARVAR(seg, info->least_chromaticity[0],0x78);
    SET_FARVAR(seg, info->least_chromaticity[1],0xF5);
    memcpy_far(seg, info->most_chromaticity, get_global_seg(), most_chromaticity,
               sizeof (most_chromaticity));

    SET_FARVAR(seg, info->established_timing[0], 0xFF);
    SET_FARVAR(seg, info->established_timing[1], 0xEF);
    SET_FARVAR(seg, info->established_timing[2], 0x80);
    /* 720x400@70Hz, 720x400@88Hz, 640x480@60Hz, 640x480@67Hz, 640x480@72Hz, 640x480@75Hz,
       800x600@56Hz, 800x600@60Hz, 800x600@72Hz, 800x600@75Hz, 832x624@75Hz, 1152x870@75Hz,
       not 1024x768@87Hz(I), 1024x768@60Hz, 1024x768@70Hz, 1024x768@75Hz, 1280x1024@75Hz */
    /* standard timings */
    get_prefered_mode(dtd, &max_x, &max_y);
    j=0;
    SET_FARVAR(seg, info->standard_timing[0], VBE_EDID_STD_640x480_85Hz); j++;
    SET_FARVAR(seg, info->standard_timing[1], VBE_EDID_STD_800x600_85Hz); j++;
    if (max_x < 1024 || max_y < 768) goto end_std;
    SET_FARVAR(seg, info->standard_timing[2], VBE_EDID_STD_1024x768_85Hz); j++;
    if (max_x < 1280 || max_y < 1024) goto end_std;
    SET_FARVAR(seg, info->standard_timing[3], VBE_EDID_STD_1280x720_70Hz); j++;
    SET_FARVAR(seg, info->standard_timing[4], VBE_EDID_STD_1280x960_60Hz); j++;
    SET_FARVAR(seg, info->standard_timing[5], VBE_EDID_STD_1440x900_60Hz); j++;
    if (max_x < 1600|| max_y < 1080) goto end_std;
    SET_FARVAR(seg, info->standard_timing[6], VBE_EDID_STD_1600x900_60Hz); j++;
    SET_FARVAR(seg, info->standard_timing[7], VBE_EDID_STD_1920x1080_60Hz);j++;
end_std:
    for (i = j; i < 8; i++) {
        SET_FARVAR(seg, info->standard_timing[i], VBE_EDID_STD_NOP);
    }
    /* detailed timing block */
    memcpy_far(seg, &(info->desc[0].dtd), get_global_seg(), dtd,
               sizeof (struct edid_detailed_timing));
    /* monitor range */
    memcpy_far(seg, &(info->desc[1].mrld), get_global_seg(), &vbe_edid_range,
               sizeof(vbe_edid_range));
    /* serial */
    for (i = 0; i < 5; i++) {
        SET_FARVAR(seg, info->desc[2].mtxtd.header[i], 0);
    }
    SET_FARVAR(seg, info->desc[2].mtxtd.header[3], 0xFF);
    for (i = 0; i < 10; i++) {
        SET_FARVAR(seg, info->desc[2].mtxtd.text[i], i+0x30);
    }
    SET_FARVAR(seg, info->desc[2].mtxtd.text[10], 0x0A);
    SET_FARVAR(seg, info->desc[2].mtxtd.text[11], 0x20);
    SET_FARVAR(seg, info->desc[2].mtxtd.text[12], 0x20);
    /* monitor name */
    for (i = 0; i < 5; i++) {
         SET_FARVAR(seg, info->desc[3].mtxtd.header[i], 0);
    }
    SET_FARVAR(seg, info->desc[3].mtxtd.header[3], 0xFC);
    memcpy_far(seg, info->desc[3].mtxtd.text, get_global_seg(), vgabios_name, 12);
    SET_FARVAR(seg, info->desc[3].mtxtd.text[12], 0x0A);
    /* ext */
    SET_FARVAR(seg, info->extensions, next);

    /* checksum */
    u8 sum = -checksum_far(get_global_seg(), info, sizeof(info));
    SET_FARVAR(seg, info->checksum, sum);

    return 0;
}

int vesa_read_edid(u16 unit, u16 block, u16 seg, void *data)
{
    if (unit != 0)
        return -1;

    switch (block) {
    case 0:
        return vesa_read_edid_block0(unit, block, seg, data, 0);
    default:
        return -1;
    }
}

