# Introduction

Formats (FOURCC) supported by libyuv are detailed here.

# Core Formats

There are 2 core formats supported by libyuv - I420 and ARGB.
  All YUV formats can be converted to/from I420.
  All RGB formats can be converted to/from ARGB.

Filtering functions such as scaling and planar functions work on I420 and/or ARGB.

# OSX Core Media Pixel Formats

This is how OSX formats map to libyuv

    enum {
      kCMPixelFormat_32ARGB          = 32,      FOURCC_BGRA
      kCMPixelFormat_32BGRA          = 'BGRA',  FOURCC_ARGB
      kCMPixelFormat_24RGB           = 24,      FOURCC_RAW
      kCMPixelFormat_16BE555         = 16,      Not supported.
      kCMPixelFormat_16BE565         = 'B565',  Not supported.
      kCMPixelFormat_16LE555         = 'L555',  FOURCC_RGBO
      kCMPixelFormat_16LE565         = 'L565',  FOURCC_RGBP
      kCMPixelFormat_16LE5551        = '5551',  FOURCC_RGBO
      kCMPixelFormat_422YpCbCr8      = '2vuy',  FOURCC_UYVY
      kCMPixelFormat_422YpCbCr8_yuvs = 'yuvs',  FOURCC_YUY2
      kCMPixelFormat_444YpCbCr8      = 'v308',  FOURCC_I444 ?
      kCMPixelFormat_4444YpCbCrA8    = 'v408',  Not supported.
      kCMPixelFormat_422YpCbCr16     = 'v216',  Not supported.
      kCMPixelFormat_422YpCbCr10     = 'v210',  FOURCC_V210 previously.  Removed now.
      kCMPixelFormat_444YpCbCr10     = 'v410',  Not supported.
      kCMPixelFormat_8IndexedGray_WhiteIsZero = 0x00000028,  Not supported.
    };


# FOURCC (Four Charactacter Code) List

The following is extracted from video_common.h as a complete list of formats supported by libyuv.
    enum FourCC {
      // 10 Primary YUV formats: 5 planar, 2 biplanar, 2 packed.
      FOURCC_I420 = FOURCC('I', '4', '2', '0'),
      FOURCC_I422 = FOURCC('I', '4', '2', '2'),
      FOURCC_I444 = FOURCC('I', '4', '4', '4'),
      FOURCC_I400 = FOURCC('I', '4', '0', '0'),
      FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
      FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
      FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
      FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),
      FOURCC_H010 = FOURCC('H', '0', '1', '0'),  // unofficial fourcc. 10 bit lsb
      FOURCC_U010 = FOURCC('U', '0', '1', '0'),  // bt.2020, unofficial fourcc.
                                                 // 10 bit lsb

      // 1 Secondary YUV format: row biplanar.
      FOURCC_M420 = FOURCC('M', '4', '2', '0'),  // deprecated.

      // 13 Primary RGB formats: 4 32 bpp, 2 24 bpp, 3 16 bpp, 1 10 bpc, 2 64 bpp
      FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
      FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
      FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
      FOURCC_AR30 = FOURCC('A', 'R', '3', '0'),  // 10 bit per channel. 2101010.
      FOURCC_AB30 = FOURCC('A', 'B', '3', '0'),  // ABGR version of 10 bit
      FOURCC_AR64 = FOURCC('A', 'R', '6', '4'),  // 16 bit per channel.
      FOURCC_AB64 = FOURCC('A', 'B', '6', '4'),  // ABGR version of 16 bit
      FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
      FOURCC_RAW = FOURCC('r', 'a', 'w', ' '),
      FOURCC_RGBA = FOURCC('R', 'G', 'B', 'A'),
      FOURCC_RGBP = FOURCC('R', 'G', 'B', 'P'),  // rgb565 LE.
      FOURCC_RGBO = FOURCC('R', 'G', 'B', 'O'),  // argb1555 LE.
      FOURCC_R444 = FOURCC('R', '4', '4', '4'),  // argb4444 LE.

      // 1 Primary Compressed YUV format.
      FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),

      // 11 Auxiliary YUV variations: 3 with U and V planes are swapped, 1 Alias.
      FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
      FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
      FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
      FOURCC_YU12 = FOURCC('Y', 'U', '1', '2'),  // Linux version of I420.
      FOURCC_J420 = FOURCC('J', '4', '2', '0'),
      FOURCC_J400 = FOURCC('J', '4', '0', '0'),  // unofficial fourcc
      FOURCC_H420 = FOURCC('H', '4', '2', '0'),  // unofficial fourcc
      FOURCC_H422 = FOURCC('H', '4', '2', '2'),  // unofficial fourcc
      FOURCC_U420 = FOURCC('U', '4', '2', '0'),  // bt.2020, unofficial fourcc
      FOURCC_U422 = FOURCC('U', '4', '2', '2'),  // bt.2020, unofficial fourcc
      FOURCC_U444 = FOURCC('U', '4', '4', '4'),  // bt.2020, unofficial fourcc

      // 14 Auxiliary aliases.  CanonicalFourCC() maps these to canonical fourcc.
      FOURCC_IYUV = FOURCC('I', 'Y', 'U', 'V'),  // Alias for I420.
      FOURCC_YU16 = FOURCC('Y', 'U', '1', '6'),  // Alias for I422.
      FOURCC_YU24 = FOURCC('Y', 'U', '2', '4'),  // Alias for I444.
      FOURCC_YUYV = FOURCC('Y', 'U', 'Y', 'V'),  // Alias for YUY2.
      FOURCC_YUVS = FOURCC('y', 'u', 'v', 's'),  // Alias for YUY2 on Mac.
      FOURCC_HDYC = FOURCC('H', 'D', 'Y', 'C'),  // Alias for UYVY.
      FOURCC_2VUY = FOURCC('2', 'v', 'u', 'y'),  // Alias for UYVY on Mac.
      FOURCC_JPEG = FOURCC('J', 'P', 'E', 'G'),  // Alias for MJPG.
      FOURCC_DMB1 = FOURCC('d', 'm', 'b', '1'),  // Alias for MJPG on Mac.
      FOURCC_BA81 = FOURCC('B', 'A', '8', '1'),  // Alias for BGGR.
      FOURCC_RGB3 = FOURCC('R', 'G', 'B', '3'),  // Alias for RAW.
      FOURCC_BGR3 = FOURCC('B', 'G', 'R', '3'),  // Alias for 24BG.
      FOURCC_CM32 = FOURCC(0, 0, 0, 32),  // Alias for BGRA kCMPixelFormat_32ARGB
      FOURCC_CM24 = FOURCC(0, 0, 0, 24),  // Alias for RAW kCMPixelFormat_24RGB
      FOURCC_L555 = FOURCC('L', '5', '5', '5'),  // Alias for RGBO.
      FOURCC_L565 = FOURCC('L', '5', '6', '5'),  // Alias for RGBP.
      FOURCC_5551 = FOURCC('5', '5', '5', '1'),  // Alias for RGBO.

# Planar YUV
      The following formats contains a full size Y plane followed by 1 or 2
        planes for UV: I420, I422, I444, I400, NV21, NV12, I400
      The size (subsampling) of the UV varies.
        I420, NV12 and NV21 are half width, half height
        I422, NV16 and NV61 are half width, full height
        I444, NV24 and NV42 are full width, full height
        I400 and J400 have no chroma channel.

# Color space
      The YUV formats start with a letter to specify the color space. e.g. I420
        I = BT.601 limited range
        J = BT.601 full range     (J = JPEG that uses this)
        H = BT.709 limited range  (H for HD)
        F = BT.709 full range     (F for Full range)
        U = BT.2020 limited range (U for UHD)
        V = BT.2020 full range
        For YUV to RGB conversions, a matrix can be passed.  See also convert_argh.h

# HDR formats
      Planar formats with 10 or 12 bits use the following fourcc:
        I010, I012, P010, P012 are half width, half height
        I210, I212, P210, P212 are half width, full height
        I410, I412, P410, P412 are full width, full height
      where
        I is the color space (see above) and 3 planes: Y, U and V.
        P is a biplanar format, similar to NV12 but 16 bits, with the valid bits in the high bits.  There is a Y plane and a UV plane.
        0, 2 or 4 is the last digit of subsampling: 4:2:0, 4:2:2, or 4:4:4
        10 or 12 is the bits per channel.  The bits are in the low bits of a 16 bit channel.

# The ARGB FOURCC

There are 4 ARGB layouts - ARGB, BGRA, ABGR and RGBA.  ARGB is most common by far, used for screen formats, and windows webcam drivers.

The fourcc describes the order of channels in a ***register***.

A fourcc provided by capturer, can be thought of string, e.g. "ARGB".

On little endian machines, as an int, this would have 'A' in the lowest byte.  The FOURCC macro reverses the order:

    #define FOURCC(a, b, c, d) (((uint32)(a)) | ((uint32)(b) << 8) | ((uint32)(c) << 16) | ((uint32)(d) << 24))

So the "ARGB" string, read as an uint32, is

    FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B')

If you were to read ARGB pixels as uint32's, the alpha would be in the high byte, and the blue in the lowest byte.  In memory, these are stored little endian, so 'B' is first, then 'G', 'R' and 'A' last.

When calling conversion functions, the names match the FOURCC, so in this case it would be I420ToARGB().

All formats can be converted to/from ARGB.

Most 'planar_functions' work on ARGB (e.g. ARGBBlend).

Some are channel order agnostic (e.g. ARGBScale).

Some functions are symmetric (e.g. ARGBToBGRA is the same as BGRAToARGB, so its a macro).

ARGBBlend expects preattenuated ARGB. The R,G,B are premultiplied by alpha.  Other functions don't care.

# RGB24 and RAW

There are 2 RGB layouts - RGB24 (aka 24BG) and RAW

RGB24 is B,G,R in memory
RAW is R,G,B in memory

# AR30 and XR30

AR30 is 2 10 10 10 ARGB stored in little endian order.
The 2 bit alpha has 4 values.  Here are the comparable 8 bit alpha values.
0 - 0.    00000000b = 0x00 = 0
1 - 33%.  01010101b = 0x55 = 85
2 - 66%.  10101010b = 0xaa = 170
3 - 100%. 11111111b = 0xff = 255
The 10 bit RGB values range from 0 to 1023.
XR30 is the same as AR30 but with no alpha channel.

# AB64 and AR64

AB64 is similar to ABGR, with 16 bit (2 bytes) per channel. Each channel stores an unsigned short.
In memory R is the lowest and A is the highest.
Each channel has value ranges from 0 to 65535.
AR64 is similar to ARGB.

# NV12 and NV21

NV12 is a biplanar format with a full sized Y plane followed by a single
chroma plane with weaved U and V values.
NV21 is the same but with weaved V and U values.
The 12 in NV12 refers to 12 bits per pixel.  NV12 has a half width and half
height chroma channel, and therefore is a 420 subsampling.
NV16 is 16 bits per pixel, with half width and full height.  aka 422.
NV24 is 24 bits per pixel with full sized chroma channel. aka 444.
Most NV12 functions allow the destination Y pointer to be NULL.

# YUY2 and UYVY

YUY2 is a packed YUV format with half width, full height.

YUY2 is YUYV in memory
UYVY is UYVY in memory
