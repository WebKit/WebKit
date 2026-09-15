//
//  MTLPixelFormat.h
//  Metal
//
//  Copyright (c) 2014 Apple Inc. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Metal/MTLDefines.h>

NS_ASSUME_NONNULL_BEGIN
typedef NS_ENUM(NSUInteger, MTLPixelFormat)
{
    MTLPixelFormatInvalid = 0,

    /* Normal 8 bit formats */
    
    MTLPixelFormatA8Unorm      = 1,
    
    MTLPixelFormatR8Unorm                            = 10,
    MTLPixelFormatR8Unorm_sRGB API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 11,
    MTLPixelFormatR8Snorm      = 12,
    MTLPixelFormatR8Uint       = 13,
    MTLPixelFormatR8Sint       = 14,
    
    /* Normal 16 bit formats */

    MTLPixelFormatR16Unorm     = 20,
    MTLPixelFormatR16Snorm     = 22,
    MTLPixelFormatR16Uint      = 23,
    MTLPixelFormatR16Sint      = 24,
    MTLPixelFormatR16Float     = 25,

    MTLPixelFormatRG8Unorm                            = 30,
    MTLPixelFormatRG8Unorm_sRGB API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 31,
    MTLPixelFormatRG8Snorm                            = 32,
    MTLPixelFormatRG8Uint                             = 33,
    MTLPixelFormatRG8Sint                             = 34,

    /* Packed 16 bit formats */
    
    MTLPixelFormatB5G6R5Unorm API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 40,
    MTLPixelFormatA1BGR5Unorm API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 41,
    MTLPixelFormatABGR4Unorm  API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 42,
    MTLPixelFormatBGR5A1Unorm API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 43,


    /* Normal 32 bit formats */

    MTLPixelFormatR32Uint  = 53,
    MTLPixelFormatR32Sint  = 54,
    MTLPixelFormatR32Float = 55,

    MTLPixelFormatRG16Unorm  = 60,
    MTLPixelFormatRG16Snorm  = 62,
    MTLPixelFormatRG16Uint   = 63,
    MTLPixelFormatRG16Sint   = 64,
    MTLPixelFormatRG16Float  = 65,

    MTLPixelFormatRGBA8Unorm      = 70,
    MTLPixelFormatRGBA8Unorm_sRGB = 71,
    MTLPixelFormatRGBA8Snorm      = 72,
    MTLPixelFormatRGBA8Uint       = 73,
    MTLPixelFormatRGBA8Sint       = 74,
    
    MTLPixelFormatBGRA8Unorm      = 80,
    MTLPixelFormatBGRA8Unorm_sRGB = 81,

    /* Packed 32 bit formats */

    MTLPixelFormatRGB10A2Unorm = 90,
    MTLPixelFormatRGB10A2Uint  = 91,

    MTLPixelFormatRG11B10Float = 92,
    MTLPixelFormatRGB9E5Float = 93,

    MTLPixelFormatBGR10A2Unorm  API_AVAILABLE(macos(10.13), ios(11.0)) = 94,

    MTLPixelFormatBGR10_XR      API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(10.0)) = 554,
    MTLPixelFormatBGR10_XR_sRGB API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(10.0)) = 555,
    

    /* Normal 64 bit formats */

    MTLPixelFormatRG32Uint  = 103,
    MTLPixelFormatRG32Sint  = 104,
    MTLPixelFormatRG32Float = 105,

    MTLPixelFormatRGBA16Unorm  = 110,
    MTLPixelFormatRGBA16Snorm  = 112,
    MTLPixelFormatRGBA16Uint   = 113,
    MTLPixelFormatRGBA16Sint   = 114,
    MTLPixelFormatRGBA16Float  = 115,

    MTLPixelFormatBGRA10_XR      API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(10.0)) = 552,
    MTLPixelFormatBGRA10_XR_sRGB API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(10.0)) = 553,


    /* Normal 128 bit formats */

    MTLPixelFormatRGBA32Uint  = 123,
    MTLPixelFormatRGBA32Sint  = 124,
    MTLPixelFormatRGBA32Float = 125,

    /* Compressed formats. */

    /* S3TC/DXT */
    MTLPixelFormatBC1_RGBA              API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 130,
    MTLPixelFormatBC1_RGBA_sRGB         API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 131,
    MTLPixelFormatBC2_RGBA              API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 132,
    MTLPixelFormatBC2_RGBA_sRGB         API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 133,
    MTLPixelFormatBC3_RGBA              API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 134,
    MTLPixelFormatBC3_RGBA_sRGB         API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 135,

    /* RGTC */
    MTLPixelFormatBC4_RUnorm            API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 140,
    MTLPixelFormatBC4_RSnorm            API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 141,
    MTLPixelFormatBC5_RGUnorm           API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 142,
    MTLPixelFormatBC5_RGSnorm           API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 143,

    /* BPTC */
    MTLPixelFormatBC6H_RGBFloat         API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 150,
    MTLPixelFormatBC6H_RGBUfloat        API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 151,
    MTLPixelFormatBC7_RGBAUnorm         API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 152,
    MTLPixelFormatBC7_RGBAUnorm_sRGB    API_AVAILABLE(macos(10.11), macCatalyst(13.0), ios(16.4)) = 153,

    /* PVRTC */
    MTLPixelFormatPVRTC_RGB_2BPP        API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 160,
    MTLPixelFormatPVRTC_RGB_2BPP_sRGB   API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 161,
    MTLPixelFormatPVRTC_RGB_4BPP        API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 162,
    MTLPixelFormatPVRTC_RGB_4BPP_sRGB   API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 163,
    MTLPixelFormatPVRTC_RGBA_2BPP       API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 164,
    MTLPixelFormatPVRTC_RGBA_2BPP_sRGB  API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 165,
    MTLPixelFormatPVRTC_RGBA_4BPP       API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 166,
    MTLPixelFormatPVRTC_RGBA_4BPP_sRGB  API_DEPRECATED("Usage of ASTC/ETC2/BC formats is recommended instead.", macos(11.0,15.0), macCatalyst(14.0,18.0), ios(8.0,18.0)) = 167,

    /* ETC2 */
    MTLPixelFormatEAC_R11Unorm          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 170,
    MTLPixelFormatEAC_R11Snorm          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 172,
    MTLPixelFormatEAC_RG11Unorm         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 174,
    MTLPixelFormatEAC_RG11Snorm         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 176,
    MTLPixelFormatEAC_RGBA8             API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 178,
    MTLPixelFormatEAC_RGBA8_sRGB        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 179,

    MTLPixelFormatETC2_RGB8             API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 180,
    MTLPixelFormatETC2_RGB8_sRGB        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 181,
    MTLPixelFormatETC2_RGB8A1           API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 182,
    MTLPixelFormatETC2_RGB8A1_sRGB      API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 183,

    /* ASTC */
    MTLPixelFormatASTC_4x4_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 186,
    MTLPixelFormatASTC_5x4_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 187,
    MTLPixelFormatASTC_5x5_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 188,
    MTLPixelFormatASTC_6x5_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 189,
    MTLPixelFormatASTC_6x6_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 190,
    MTLPixelFormatASTC_8x5_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 192,
    MTLPixelFormatASTC_8x6_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 193,
    MTLPixelFormatASTC_8x8_sRGB         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 194,
    MTLPixelFormatASTC_10x5_sRGB        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 195,
    MTLPixelFormatASTC_10x6_sRGB        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 196,
    MTLPixelFormatASTC_10x8_sRGB        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 197,
    MTLPixelFormatASTC_10x10_sRGB       API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 198,
    MTLPixelFormatASTC_12x10_sRGB       API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 199,
    MTLPixelFormatASTC_12x12_sRGB       API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 200,

    MTLPixelFormatASTC_4x4_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 204,
    MTLPixelFormatASTC_5x4_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 205,
    MTLPixelFormatASTC_5x5_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 206,
    MTLPixelFormatASTC_6x5_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 207,
    MTLPixelFormatASTC_6x6_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 208,
    MTLPixelFormatASTC_8x5_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 210,
    MTLPixelFormatASTC_8x6_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 211,
    MTLPixelFormatASTC_8x8_LDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 212,
    MTLPixelFormatASTC_10x5_LDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 213,
    MTLPixelFormatASTC_10x6_LDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 214,
    MTLPixelFormatASTC_10x8_LDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 215,
    MTLPixelFormatASTC_10x10_LDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 216,
    MTLPixelFormatASTC_12x10_LDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 217,
    MTLPixelFormatASTC_12x12_LDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(8.0)) = 218,
    
    // ASTC HDR (High Dynamic Range) Formats
    MTLPixelFormatASTC_4x4_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 222,
    MTLPixelFormatASTC_5x4_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 223,
    MTLPixelFormatASTC_5x5_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 224,
    MTLPixelFormatASTC_6x5_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 225,
    MTLPixelFormatASTC_6x6_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 226,
    MTLPixelFormatASTC_8x5_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 228,
    MTLPixelFormatASTC_8x6_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 229,
    MTLPixelFormatASTC_8x8_HDR          API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 230,
    MTLPixelFormatASTC_10x5_HDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 231,
    MTLPixelFormatASTC_10x6_HDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 232,
    MTLPixelFormatASTC_10x8_HDR         API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 233,
    MTLPixelFormatASTC_10x10_HDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 234,
    MTLPixelFormatASTC_12x10_HDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 235,
    MTLPixelFormatASTC_12x12_HDR        API_AVAILABLE(macos(11.0), macCatalyst(14.0), ios(13.0), tvos(16.0)) = 236,

    /*!
     @constant MTLPixelFormatGBGR422
     @abstract A pixel format where the red and green channels are subsampled horizontally.  Two pixels are stored in 32 bits, with shared red and blue values, and unique green values.
     @discussion This format is equivalent to YUY2, YUYV, yuvs, or GL_RGB_422_APPLE/GL_UNSIGNED_SHORT_8_8_REV_APPLE.   The component order, from lowest addressed byte to highest, is Y0, Cb, Y1, Cr.  There is no implicit colorspace conversion from YUV to RGB, the shader will receive (Cr, Y, Cb, 1).  422 textures must have a width that is a multiple of 2, and can only be used for 2D non-mipmap textures.  When sampling, ClampToEdge is the only usable wrap mode.
     */
    MTLPixelFormatGBGR422 = 240,

    /*!
     @constant MTLPixelFormatBGRG422
     @abstract A pixel format where the red and green channels are subsampled horizontally.  Two pixels are stored in 32 bits, with shared red and blue values, and unique green values.
     @discussion This format is equivalent to UYVY, 2vuy, or GL_RGB_422_APPLE/GL_UNSIGNED_SHORT_8_8_APPLE. The component order, from lowest addressed byte to highest, is Cb, Y0, Cr, Y1.  There is no implicit colorspace conversion from YUV to RGB, the shader will receive (Cr, Y, Cb, 1).  422 textures must have a width that is a multiple of 2, and can only be used for 2D non-mipmap textures.  When sampling, ClampToEdge is the only usable wrap mode.
     */
    MTLPixelFormatBGRG422 = 241,

    /* Depth */

    MTLPixelFormatDepth16Unorm          API_AVAILABLE(macos(10.12), ios(13.0)) = 250,
    MTLPixelFormatDepth32Float  = 252,

    /* Stencil */

    MTLPixelFormatStencil8        = 253,

    /* Depth Stencil */
    
    MTLPixelFormatDepth24Unorm_Stencil8  API_DEPRECATED("Use MTLPixelFormatDepth32Float_Stencil8 instead", macos(10.11, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios) = 255,
    MTLPixelFormatDepth32Float_Stencil8  API_AVAILABLE(macos(10.11), ios(9.0)) = 260,

    MTLPixelFormatX32_Stencil8  API_AVAILABLE(macos(10.12), ios(10.0)) = 261,
    MTLPixelFormatX24_Stencil8  API_DEPRECATED("Use MTLPixelFormatX32_Stencil8 instead", macos(10.12, 27.0), macCatalyst(13.0, 27.0)) API_UNAVAILABLE(ios) = 262,

    MTLPixelFormatUnspecialized API_AVAILABLE(macos(26.0), ios(26.0)) = 263,
    
} API_AVAILABLE(macos(10.11), ios(8.0));

NS_ASSUME_NONNULL_END
