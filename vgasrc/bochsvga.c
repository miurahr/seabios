// Bochs VGA interface to extended "VBE" modes
//
// Copyright (C) 2012  Kevin O'Connor <kevin@koconnor.net>
// Copyright (C) 2011  Julian Pidancet <julian.pidancet@citrix.com>
//  Copyright (C) 2002 Jeroen Janssen
//
// This file may be distributed under the terms of the GNU LGPLv3 license.

#include "vgabios.h" // struct vbe_modeinfo
#include "vbe.h" // VBE_CAPABILITY_8BIT_DAC
#include "bochsvga.h" // bochsvga_set_mode
#include "util.h" // dprintf
#include "config.h" // CONFIG_*
#include "biosvar.h" // GET_GLOBAL
#include "stdvga.h" // VGAREG_SEQU_ADDRESS
#include "pci.h" // pci_config_readl
#include "pci_regs.h" // PCI_BASE_ADDRESS_0
#include "vbe_edid.h"

/****************************************************************
 * Mode tables
 ****************************************************************/

static struct bochsvga_mode
{
    u16 mode;
    struct vgamode_s info;
} bochsvga_modes[] VAR16 = {
    /* standard modes */
    { 0x100, { MM_PACKED, 640,  400,  8,  8, 16, SEG_GRAPH } },
    { 0x101, { MM_PACKED, 640,  480,  8,  8, 16, SEG_GRAPH } },
    { 0x102, { MM_PLANAR, 800,  600,  4,  8, 16, SEG_GRAPH } },
    { 0x103, { MM_PACKED, 800,  600,  8,  8, 16, SEG_GRAPH } },
    { 0x104, { MM_PLANAR, 1024, 768,  4,  8, 16, SEG_GRAPH } },
    { 0x105, { MM_PACKED, 1024, 768,  8,  8, 16, SEG_GRAPH } },
    { 0x106, { MM_PLANAR, 1280, 1024, 4,  8, 16, SEG_GRAPH } },
    { 0x107, { MM_PACKED, 1280, 1024, 8,  8, 16, SEG_GRAPH } },
    { 0x10D, { MM_DIRECT, 320,  200,  15, 8, 16, SEG_GRAPH } },
    { 0x10E, { MM_DIRECT, 320,  200,  16, 8, 16, SEG_GRAPH } },
    { 0x10F, { MM_DIRECT, 320,  200,  24, 8, 16, SEG_GRAPH } },
    { 0x110, { MM_DIRECT, 640,  480,  15, 8, 16, SEG_GRAPH } },
    { 0x111, { MM_DIRECT, 640,  480,  16, 8, 16, SEG_GRAPH } },
    { 0x112, { MM_DIRECT, 640,  480,  24, 8, 16, SEG_GRAPH } },
    { 0x113, { MM_DIRECT, 800,  600,  15, 8, 16, SEG_GRAPH } },
    { 0x114, { MM_DIRECT, 800,  600,  16, 8, 16, SEG_GRAPH } },
    { 0x115, { MM_DIRECT, 800,  600,  24, 8, 16, SEG_GRAPH } },
    { 0x116, { MM_DIRECT, 1024, 768,  15, 8, 16, SEG_GRAPH } },
    { 0x117, { MM_DIRECT, 1024, 768,  16, 8, 16, SEG_GRAPH } },
    { 0x118, { MM_DIRECT, 1024, 768,  24, 8, 16, SEG_GRAPH } },
    { 0x119, { MM_DIRECT, 1280, 1024, 15, 8, 16, SEG_GRAPH } },
    { 0x11A, { MM_DIRECT, 1280, 1024, 16, 8, 16, SEG_GRAPH } },
    { 0x11B, { MM_DIRECT, 1280, 1024, 24, 8, 16, SEG_GRAPH } },
    { 0x11C, { MM_PACKED, 1600, 1200, 8,  8, 16, SEG_GRAPH } },
    { 0x11D, { MM_DIRECT, 1600, 1200, 15, 8, 16, SEG_GRAPH } },
    { 0x11E, { MM_DIRECT, 1600, 1200, 16, 8, 16, SEG_GRAPH } },
    { 0x11F, { MM_DIRECT, 1600, 1200, 24, 8, 16, SEG_GRAPH } },
    /* BOCHS modes */
    { 0x140, { MM_DIRECT, 320,  200,  32, 8, 16, SEG_GRAPH } },
    { 0x141, { MM_DIRECT, 640,  400,  32, 8, 16, SEG_GRAPH } },
    { 0x142, { MM_DIRECT, 640,  480,  32, 8, 16, SEG_GRAPH } },
    { 0x143, { MM_DIRECT, 800,  600,  32, 8, 16, SEG_GRAPH } },
    { 0x144, { MM_DIRECT, 1024, 768,  32, 8, 16, SEG_GRAPH } },
    { 0x145, { MM_DIRECT, 1280, 1024, 32, 8, 16, SEG_GRAPH } },
    { 0x146, { MM_PACKED, 320,  200,  8,  8, 16, SEG_GRAPH } },
    { 0x147, { MM_DIRECT, 1600, 1200, 32, 8, 16, SEG_GRAPH } },
    { 0x148, { MM_PACKED, 1152, 864,  8,  8, 16, SEG_GRAPH } },
    { 0x149, { MM_DIRECT, 1152, 864,  15, 8, 16, SEG_GRAPH } },
    { 0x14a, { MM_DIRECT, 1152, 864,  16, 8, 16, SEG_GRAPH } },
    { 0x14b, { MM_DIRECT, 1152, 864,  24, 8, 16, SEG_GRAPH } },
    { 0x14c, { MM_DIRECT, 1152, 864,  32, 8, 16, SEG_GRAPH } },
    { 0x178, { MM_DIRECT, 1280, 800,  16, 8, 16, SEG_GRAPH } },
    { 0x179, { MM_DIRECT, 1280, 800,  24, 8, 16, SEG_GRAPH } },
    { 0x17a, { MM_DIRECT, 1280, 800,  32, 8, 16, SEG_GRAPH } },
    { 0x17b, { MM_DIRECT, 1280, 960,  16, 8, 16, SEG_GRAPH } },
    { 0x17c, { MM_DIRECT, 1280, 960,  24, 8, 16, SEG_GRAPH } },
    { 0x17d, { MM_DIRECT, 1280, 960,  32, 8, 16, SEG_GRAPH } },
    { 0x17e, { MM_DIRECT, 1440, 900,  16, 8, 16, SEG_GRAPH } },
    { 0x17f, { MM_DIRECT, 1440, 900,  24, 8, 16, SEG_GRAPH } },
    { 0x180, { MM_DIRECT, 1440, 900,  32, 8, 16, SEG_GRAPH } },
    { 0x181, { MM_DIRECT, 1400, 1050, 16, 8, 16, SEG_GRAPH } },
    { 0x182, { MM_DIRECT, 1400, 1050, 24, 8, 16, SEG_GRAPH } },
    { 0x183, { MM_DIRECT, 1400, 1050, 32, 8, 16, SEG_GRAPH } },
    { 0x184, { MM_DIRECT, 1680, 1050, 16, 8, 16, SEG_GRAPH } },
    { 0x185, { MM_DIRECT, 1680, 1050, 24, 8, 16, SEG_GRAPH } },
    { 0x186, { MM_DIRECT, 1680, 1050, 32, 8, 16, SEG_GRAPH } },
    { 0x187, { MM_DIRECT, 1920, 1200, 16, 8, 16, SEG_GRAPH } },
    { 0x188, { MM_DIRECT, 1920, 1200, 24, 8, 16, SEG_GRAPH } },
    { 0x189, { MM_DIRECT, 1920, 1200, 32, 8, 16, SEG_GRAPH } },
    { 0x18a, { MM_DIRECT, 2560, 1600, 16, 8, 16, SEG_GRAPH } },
    { 0x18b, { MM_DIRECT, 2560, 1600, 24, 8, 16, SEG_GRAPH } },
    { 0x18c, { MM_DIRECT, 2560, 1600, 32, 8, 16, SEG_GRAPH } },
};

static int is_bochsvga_mode(struct vgamode_s *vmode_g)
{
    return (vmode_g >= &bochsvga_modes[0].info
            && vmode_g <= &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)-1].info);
}

struct vgamode_s *bochsvga_find_mode(int mode)
{
    struct bochsvga_mode *m = bochsvga_modes;
    for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)]; m++)
        if (GET_GLOBAL(m->mode) == mode)
            return &m->info;
    return stdvga_find_mode(mode);
}

void
bochsvga_list_modes(u16 seg, u16 *dest, u16 *last)
{
    struct bochsvga_mode *m = bochsvga_modes;
    for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)] && dest<last; m++) {
        u16 mode = GET_GLOBAL(m->mode);
        if (mode == 0xffff)
            continue;
        SET_FARVAR(seg, *dest, mode);
        dest++;
    }
    stdvga_list_modes(seg, dest, last);
}


/****************************************************************
 * Helper functions
 ****************************************************************/

int
bochsvga_get_window(struct vgamode_s *vmode_g, int window)
{
    if (window != 0)
        return -1;
    return dispi_read(VBE_DISPI_INDEX_BANK);
}

int
bochsvga_set_window(struct vgamode_s *vmode_g, int window, int val)
{
    if (window != 0)
        return -1;
    dispi_write(VBE_DISPI_INDEX_BANK, val);
    if (dispi_read(VBE_DISPI_INDEX_BANK) != val)
        return -1;
    return 0;
}

int
bochsvga_get_linelength(struct vgamode_s *vmode_g)
{
    return dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * vga_bpp(vmode_g) / 8;
}

int
bochsvga_set_linelength(struct vgamode_s *vmode_g, int val)
{
    stdvga_set_linelength(vmode_g, val);
    int pixels = (val * 8) / vga_bpp(vmode_g);
    dispi_write(VBE_DISPI_INDEX_VIRT_WIDTH, pixels);
    return 0;
}

int
bochsvga_get_displaystart(struct vgamode_s *vmode_g)
{
    int bpp = vga_bpp(vmode_g);
    int linelength = dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * bpp / 8;
    int x = dispi_read(VBE_DISPI_INDEX_X_OFFSET);
    int y = dispi_read(VBE_DISPI_INDEX_Y_OFFSET);
    return x * bpp / 8 + linelength * y;
}

int
bochsvga_set_displaystart(struct vgamode_s *vmode_g, int val)
{
    stdvga_set_displaystart(vmode_g, val);
    int bpp = vga_bpp(vmode_g);
    int linelength = dispi_read(VBE_DISPI_INDEX_VIRT_WIDTH) * bpp / 8;
    dispi_write(VBE_DISPI_INDEX_X_OFFSET, (val % linelength) * 8 / bpp);
    dispi_write(VBE_DISPI_INDEX_Y_OFFSET, val / linelength);
    return 0;
}

int
bochsvga_get_dacformat(struct vgamode_s *vmode_g)
{
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    return (en & VBE_DISPI_8BIT_DAC) ? 8 : 6;
}

int
bochsvga_set_dacformat(struct vgamode_s *vmode_g, int val)
{
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    if (val == 6)
        en &= ~VBE_DISPI_8BIT_DAC;
    else if (val == 8)
        en |= VBE_DISPI_8BIT_DAC;
    else
        return -1;
    dispi_write(VBE_DISPI_INDEX_ENABLE, en);
    return 0;
}

int
bochsvga_size_state(int states)
{
    int size = stdvga_size_state(states);
    if (size < 0)
        return size;
    if (states & 8)
        size += (VBE_DISPI_INDEX_Y_OFFSET-VBE_DISPI_INDEX_XRES+1)*sizeof(u16);
    return size;
}

int
bochsvga_save_state(u16 seg, void *data, int states)
{
    int ret = stdvga_save_state(seg, data, states);
    if (ret < 0)
        return ret;

    if (!(states & 8))
        return 0;

    u16 *info = (data + stdvga_size_state(states));
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    SET_FARVAR(seg, *info, en);
    info++;
    if (!(en & VBE_DISPI_ENABLED))
        return 0;
    int i;
    for (i = VBE_DISPI_INDEX_XRES; i <= VBE_DISPI_INDEX_Y_OFFSET; i++)
        if (i != VBE_DISPI_INDEX_ENABLE) {
            u16 v = dispi_read(i);
            SET_FARVAR(seg, *info, v);
            info++;
        }
    return 0;
}

int
bochsvga_restore_state(u16 seg, void *data, int states)
{
    int ret = stdvga_restore_state(seg, data, states);
    if (ret < 0)
        return ret;

    if (!(states & 8))
        return 0;

    u16 *info = (data + stdvga_size_state(states));
    u16 en = GET_FARVAR(seg, *info);
    info++;
    if (!(en & VBE_DISPI_ENABLED)) {
        dispi_write(VBE_DISPI_INDEX_ENABLE, en);
        return 0;
    }
    int i;
    for (i = VBE_DISPI_INDEX_XRES; i <= VBE_DISPI_INDEX_Y_OFFSET; i++)
        if (i == VBE_DISPI_INDEX_ENABLE) {
            dispi_write(i, en);
        } else {
            dispi_write(i, GET_FARVAR(seg, *info));
            info++;
        }
    return 0;
}


/****************************************************************
 * Mode setting
 ****************************************************************/

int
bochsvga_set_mode(struct vgamode_s *vmode_g, int flags)
{
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_DISABLED);
    if (! is_bochsvga_mode(vmode_g))
        return stdvga_set_mode(vmode_g, flags);

    u8 depth = GET_GLOBAL(vmode_g->depth);
    if (depth == 4)
        stdvga_set_mode(stdvga_find_mode(0x6a), 0);
    if (depth == 8)
        // XXX load_dac_palette(3);
        ;

    dispi_write(VBE_DISPI_INDEX_BPP, depth);
    u16 width = GET_GLOBAL(vmode_g->width);
    u16 height = GET_GLOBAL(vmode_g->height);
    dispi_write(VBE_DISPI_INDEX_XRES, width);
    dispi_write(VBE_DISPI_INDEX_YRES, height);
    dispi_write(VBE_DISPI_INDEX_BANK, 0);
    u16 bf = ((flags & MF_NOCLEARMEM ? VBE_DISPI_NOCLEARMEM : 0)
              | (flags & MF_LINEARFB ? VBE_DISPI_LFB_ENABLED : 0));
    dispi_write(VBE_DISPI_INDEX_ENABLE, VBE_DISPI_ENABLED | bf);

    /* VGA compat setup */
    u16 crtc_addr = VGAREG_VGA_CRTC_ADDRESS;
    stdvga_crtc_write(crtc_addr, 0x11, 0x00);
    stdvga_crtc_write(crtc_addr, 0x01, width / 8 - 1);
    stdvga_set_linelength(vmode_g, width);
    stdvga_crtc_write(crtc_addr, 0x12, height - 1);
    u8 v = 0;
    if ((height - 1) & 0x0100)
        v |= 0x02;
    if ((height - 1) & 0x0200)
        v |= 0x40;
    stdvga_crtc_mask(crtc_addr, 0x07, 0x42, v);

    stdvga_crtc_write(crtc_addr, 0x09, 0x00);
    stdvga_crtc_mask(crtc_addr, 0x17, 0x00, 0x03);
    stdvga_attr_mask(0x10, 0x00, 0x01);
    stdvga_grdc_write(0x06, 0x05);
    stdvga_sequ_write(0x02, 0x0f);
    if (depth >= 8) {
        stdvga_crtc_mask(crtc_addr, 0x14, 0x00, 0x40);
        stdvga_attr_mask(0x10, 0x00, 0x40);
        stdvga_sequ_mask(0x04, 0x00, 0x08);
        stdvga_grdc_mask(0x05, 0x20, 0x40);
    }

    return 0;
}



int bochsvga_get_ddc_capabilities(u16 unit)
{
    if (unit != 0)
        return -1;

    return (1 << 8) | VBE_DDC1_PROTOCOL_SUPPORTED;
}

u8 most_chromaticity[8] VAR16 = {0xA6,0x55,0x48,0x9B,0x26,0x12,0x50,0x54};
unsigned char vgabios_name[] VAR16 = "SeaBIOS VGAB";

int bochsvga_read_edid(u16 unit, u16 block, u16 seg, void *data)
{
    struct vbe_edid_info  *info = data;
    int i;

    if (unit != 0 || block != 0)
        return -1;

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
    SET_FARVAR(seg, info->feature_flag,0x0D); /* no DMPS states, RGB, display is continuous frequency */
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
    SET_FARVAR(seg, info->standard_timing[0], VBE_EDID_STD_640x480_85Hz);
    SET_FARVAR(seg, info->standard_timing[1], VBE_EDID_STD_800x600_85Hz);
    SET_FARVAR(seg, info->standard_timing[2], VBE_EDID_STD_1024x768_85Hz);
    SET_FARVAR(seg, info->standard_timing[3], VBE_EDID_STD_1280x720_70Hz);
    SET_FARVAR(seg, info->standard_timing[4], VBE_EDID_STD_1280x960_60Hz);
    SET_FARVAR(seg, info->standard_timing[5], VBE_EDID_STD_1440x900_60Hz);
    SET_FARVAR(seg, info->standard_timing[6], VBE_EDID_STD_1600x1200_60Hz);
    SET_FARVAR(seg, info->standard_timing[7], VBE_EDID_STD_1680x1050_60Hz);
    /* detailed timing blocks */
    int dtd_block=0; /* VBE_EDID_DTD_1152x864 */
        SET_FARVAR(seg, info->desc[dtd_block].dtd.pixel_clock,                 WORDBE(0x302a));
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_addressable_low,  0x80);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_blanking_low,     0xC0);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_high,             0x41);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_addressable_low,    0x60);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_blanking_low,       0x24);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_high,               0x30);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_front_porch_low,  0x40);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_sync_pulse_low,   0x80);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_low4,               0x13);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_vertical_sync_hi, 0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_video_image_low,  0x2C);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_video_image_low,    0xE1);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.video_image_high,            0x10);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_border,           0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_border,             0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.features,                    0x1E);
    dtd_block = 1; /* VBE_EDID_DTD_1280x1024 */
        SET_FARVAR(seg, info->desc[dtd_block].dtd.pixel_clock,                 WORDBE(0x302a));
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_addressable_low,  0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_blanking_low,     0x98);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_high,             0x51);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_addressable_low,    0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_blanking_low,       0x2A);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_high,               0x40);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_front_porch_low,  0x30);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_sync_pulse_low,   0x70);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_low4,               0x13);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_vertical_sync_hi, 0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_video_image_low,  0x2C);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_video_image_low,    0xE1);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.video_image_high,            0x10);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.horizontal_border,           0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.vertical_border,             0x00);
        SET_FARVAR(seg, info->desc[dtd_block].dtd.features,                    0x1E);
    /* serial */
    dtd_block = 2;
        for (i = 0; i < 5; i++) {
            SET_FARVAR(seg, info->desc[dtd_block].mtxtd.header[i], 0);
        }
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.header[3], 0xFF);
        for (i = 0; i < 10; i++) {
            SET_FARVAR(seg, info->desc[dtd_block].mtxtd.text[i], i+0x30);
        }
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.text[10], 0x0A);
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.text[11], 0x20);
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.text[12], 0x20);
    /* monitor name */
    dtd_block = 3;
        for (i = 0; i < 5; i++) {
             SET_FARVAR(seg, info->desc[dtd_block].mtxtd.header[i], 0);
        }
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.header[3], 0xFC);
        memcpy_far(seg, info->desc[dtd_block].mtxtd.text, get_global_seg(), vgabios_name, 12);
        SET_FARVAR(seg, info->desc[dtd_block].mtxtd.text[12], 0x0A);
    /* ext */
    SET_FARVAR(seg, info->extensions, 0);

    /* checksum */
    u8 sum = -checksum_far(get_global_seg(), info, sizeof(info));
    SET_FARVAR(seg, info->checksum, sum);

    return 0;
}

/****************************************************************
 * Init
 ****************************************************************/

int
bochsvga_init(void)
{
    int ret = stdvga_init();
    if (ret)
        return ret;

    /* Sanity checks */
    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID0);
    if (dispi_read(VBE_DISPI_INDEX_ID) != VBE_DISPI_ID0) {
        dprintf(1, "No VBE DISPI interface detected\n");
        return -1;
    }

    dispi_write(VBE_DISPI_INDEX_ID, VBE_DISPI_ID5);

    if (GET_GLOBAL(HaveRunInit))
        return 0;

    u32 lfb_addr = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
    int bdf = GET_GLOBAL(VgaBDF);
    if (CONFIG_VGA_PCI && bdf >= 0) {
        int barid = 0;
        u32 bar = pci_config_readl(bdf, PCI_BASE_ADDRESS_0);
        if ((bar & PCI_BASE_ADDRESS_SPACE) != PCI_BASE_ADDRESS_SPACE_MEMORY) {
            barid = 1;
            bar = pci_config_readl(bdf, PCI_BASE_ADDRESS_1);
        }
        lfb_addr = bar & PCI_BASE_ADDRESS_MEM_MASK;
        dprintf(1, "VBE DISPI: bdf %02x:%02x.%x, bar %d\n", pci_bdf_to_bus(bdf)
                , pci_bdf_to_dev(bdf), pci_bdf_to_fn(bdf), barid);
    }

    SET_VGA(VBE_framebuffer, lfb_addr);
    u32 totalmem = dispi_read(VBE_DISPI_INDEX_VIDEO_MEMORY_64K) * 64 * 1024;
    SET_VGA(VBE_total_memory, totalmem);
    SET_VGA(VBE_capabilities, VBE_CAPABILITY_8BIT_DAC);

    dprintf(1, "VBE DISPI: lfb_addr=%x, size %d MB\n",
            lfb_addr, totalmem >> 20);

    // Validate modes
    u16 en = dispi_read(VBE_DISPI_INDEX_ENABLE);
    dispi_write(VBE_DISPI_INDEX_ENABLE, en | VBE_DISPI_GETCAPS);
    u16 max_xres = dispi_read(VBE_DISPI_INDEX_XRES);
    u16 max_bpp = dispi_read(VBE_DISPI_INDEX_BPP);
    dispi_write(VBE_DISPI_INDEX_ENABLE, en);
    struct bochsvga_mode *m = bochsvga_modes;
    for (; m < &bochsvga_modes[ARRAY_SIZE(bochsvga_modes)]; m++) {
        u16 width = GET_GLOBAL(m->info.width);
        u16 height = GET_GLOBAL(m->info.height);
        u8 depth = GET_GLOBAL(m->info.depth);
        u32 mem = (height * DIV_ROUND_UP(width * vga_bpp(&m->info), 8)
                   * 4 / stdvga_bpp_factor(&m->info));

        if (width > max_xres || depth > max_bpp || mem > totalmem) {
            dprintf(1, "Removing mode %x\n", GET_GLOBAL(m->mode));
            SET_VGA(m->mode, 0xffff);
        }
    }

    return 0;
}

