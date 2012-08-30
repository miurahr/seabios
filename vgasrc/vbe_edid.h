#ifndef __VBE_EDID_H
#define __VBE_EDID_H

#define WORDBE(x) ((((x) & 0xff) << 8 ) | (((x) >> 8) & 0xff))
#define DWORDBE(x) (((x) & 0xff) << 24) | ((((x) >> 8) & 0xff) << 16 ) | ((((x) >> 16) & 0xff) << 8 ) | ((((x) >> 24) & 0xff))

/* EDID infomation definitions */
/* Detailed Timing Descriptor */
struct edid_detailed_timing {
    u16 pixel_clock;
    u8 horizontal_addressable_low;
    u8 horizontal_blanking_low;
    u8 horizontal_high;
    u8 vertical_addressable_low;
    u8 vertical_blanking_low;
    u8 vertical_high;
    u8 horizontal_front_porch_low;
    u8 horizontal_sync_pulse_low;
    u8 vertical_low4;
    u8 horizontal_vertical_sync_hi;
    u8 horizontal_video_image_low;
    u8 vertical_video_image_low;
    u8 video_image_high;
    u8 horizontal_border;
    u8 vertical_border;
    u8 features;
} PACKED;

/* Other Monitor Descriptors */
/*
  0xFF: Monitor serial number (text)
  0xFE: Unspecified text (text)
  0xFC: Monitor name (text)
 */
struct edid_monitor_text {
    u8 header[5];
    u8 text[13];
} PACKED;

/* Monitor Range Limits Descriptor */
/*
  0xFD: Monitor range limits. 6- or 13-byte binary descriptor.
 */
struct edid_monitor_range_limit {
    u8 header[5];
    u8 vertical_rate_min;
    u8 vertical_rate_max;
    u8 horizontal_rate_min;
    u8 horizontal_rate_max;
    u8 pixel_clock_max;
    u8 extended_timing_type;
    u8 reserved;
    u8 start_frequency;
    u8 gtf_c_val;
    u16 gtf_m_val;
    u8 gtf_k_val;
    u8 gtf_j_val;
} PACKED;

/* White point Descriptor */
/*
  0xFB: Additional white point data. 2× 5-byte descriptors, padded with 0A 20 20.
  0xFA: Additional standard timing identifiers. 6× 2-byte descriptors, padded with 0A
 */
struct edid_wp_desc {
    u8 index;
    u8 least;
    u8 x;
    u8 y;
    u8 gamma;
} PACKED;

struct edid_white_point {
    u8 header[5];
    struct edid_wp_desc desc[2];
    u8 padding[3];
} PACKED;

struct edid_additional_timing {
    u8 header[5];
    u16 standard_timing[6];
    u8 padding;
} PACKED;

union edid_descriptors {
    struct edid_detailed_timing     dtd;
    struct edid_monitor_range_limit mrld;
    struct edid_monitor_text        mtxtd;
    struct edid_white_point         wpd;
    struct edid_additional_timing   astd;
} PACKED;

struct vbe_edid_info {
    u8 header[8];
    u16 vendor;
    u16 product;
    u32 serial;
    u8 week;
    u8 year;
    u8 major_version;
    u8 minor_version;
    u8 video_setup;
    u8 screen_width;
    u8 screen_height;
    u8 gamma;
    u8 feature_flag;
    u8 least_chromaticity[2];
    u8 most_chromaticity[8];
    u8 established_timing[3];
    u16 standard_timing[8];
    union edid_descriptors desc[4];
    u8 extensions;
    u8 checksum;
} PACKED;

struct prefered_mode_info {u16 x; u16 y;} PACKED;

/* EDID Standard Timing Description */

/* First byte: X resolution, divided by 8, less 31 (256–2288 pixels) */
/* bit 7-6, X:Y pixel ratio: 00=16:10; 01=4:3; 10=5:4; 11=16:9 */
/* bit 5-0, Vertical frequency, less 60 (60–123 Hz), nop 01 01 in Big Endian*/
#define VBE_EDID_STD_640x480_85Hz                        0x5931
#define VBE_EDID_STD_800x600_85Hz                        0x5945
#define VBE_EDID_STD_1024x768_85Hz                       0x5961
#define VBE_EDID_STD_1152x864_70Hz                       0x4A71
#define VBE_EDID_STD_1280x720_70Hz                       0xCA81
#define VBE_EDID_STD_1280x800_70Hz                       0x0A81
#define VBE_EDID_STD_1280x960_60Hz                       0x4081
#define VBE_EDID_STD_1280x1024_60Hz                      0x8081
#define VBE_EDID_STD_1440x900_60Hz                       0x0095
#define VBE_EDID_STD_1600x1200_60Hz                      0x40A9
#define VBE_EDID_STD_1600x900_60Hz                       0xC0A9
#define VBE_EDID_STD_1680x1050_60Hz                      0x00B3
#define VBE_EDID_STD_1920x1080_60Hz                      0xC0D1
#define VBE_EDID_STD_NOP                                 0x0101

int vesa_set_prefered_mode(u16 x, u16 y);
int vesa_get_prefered_mode(u16 seg, void *data);
int vesa_get_ddc_capabilities(u16 unit);
int vesa_read_edid(u16 unit, u16 block, u16 seg, void *data);
#endif /* vbe_edid.h */
