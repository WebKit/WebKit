# Introduction

Formats (FOURCC) supported by libyuv are detailed here.

# Core Formats

There are 2 core formats supported by libyuv - I420 and ARGB.  All YUV formats can be converted to/from I420.  All RGB formats can be converted to/from ARGB.

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
      // 8 Primary YUV formats: 5 planar, 2 biplanar, 2 packed.
      FOURCC_I420 = FOURCC('I', '4', '2', '0'),
      FOURCC_I422 = FOURCC('I', '4', '2', '2'),
      FOURCC_I444 = FOURCC('I', '4', '4', '4'),
      FOURCC_I400 = FOURCC('I', '4', '0', '0'),
      FOURCC_NV21 = FOURCC('N', 'V', '2', '1'),
      FOURCC_NV12 = FOURCC('N', 'V', '1', '2'),
      FOURCC_YUY2 = FOURCC('Y', 'U', 'Y', '2'),
      FOURCC_UYVY = FOURCC('U', 'Y', 'V', 'Y'),

      // 1 Secondary YUV formats: row biplanar.
      FOURCC_M420 = FOURCC('M', '4', '2', '0'),

      // 9 Primary RGB formats: 4 32 bpp, 2 24 bpp, 3 16 bpp.
      FOURCC_ARGB = FOURCC('A', 'R', 'G', 'B'),
      FOURCC_BGRA = FOURCC('B', 'G', 'R', 'A'),
      FOURCC_ABGR = FOURCC('A', 'B', 'G', 'R'),
      FOURCC_24BG = FOURCC('2', '4', 'B', 'G'),
      FOURCC_RAW  = FOURCC('r', 'a', 'w', ' '),
      FOURCC_RGBA = FOURCC('R', 'G', 'B', 'A'),
      FOURCC_RGBP = FOURCC('R', 'G', 'B', 'P'),  // rgb565 LE.
      FOURCC_RGBO = FOURCC('R', 'G', 'B', 'O'),  // argb1555 LE.
      FOURCC_R444 = FOURCC('R', '4', '4', '4'),  // argb4444 LE.

      // 4 Secondary RGB formats: 4 Bayer Patterns.
      FOURCC_RGGB = FOURCC('R', 'G', 'G', 'B'),
      FOURCC_BGGR = FOURCC('B', 'G', 'G', 'R'),
      FOURCC_GRBG = FOURCC('G', 'R', 'B', 'G'),
      FOURCC_GBRG = FOURCC('G', 'B', 'R', 'G'),

      // 1 Primary Compressed YUV format.
      FOURCC_MJPG = FOURCC('M', 'J', 'P', 'G'),

      // 5 Auxiliary YUV variations: 3 with U and V planes are swapped, 1 Alias.
      FOURCC_YV12 = FOURCC('Y', 'V', '1', '2'),
      FOURCC_YV16 = FOURCC('Y', 'V', '1', '6'),
      FOURCC_YV24 = FOURCC('Y', 'V', '2', '4'),
      FOURCC_YU12 = FOURCC('Y', 'U', '1', '2'),  // Linux version of I420.
      FOURCC_J420 = FOURCC('J', '4', '2', '0'),
      FOURCC_J400 = FOURCC('J', '4', '0', '0'),

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

      // 1 Auxiliary compressed YUV format set aside for capturer.
      FOURCC_H264 = FOURCC('H', '2', '6', '4'),

# Planar YUV
      The following formats contains a full size Y plane followed by 1 or 2
        planes for UV: I420, I422, I444, I400, NV21, NV12, I400
      The size (subsampling) of the UV varies.
        I420, NV12 and NV21 are half width, half height
        I422, NV16 and NV61 are half width, full height
        I444, NV24 and NV42 are full width, full height
        I400 and J400 have no chroma channel.

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
