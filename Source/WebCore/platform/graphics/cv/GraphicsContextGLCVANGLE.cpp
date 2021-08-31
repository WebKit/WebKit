/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GraphicsContextGLCVANGLE.h"

#if ENABLE(WEBGL) && ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "ANGLEHeaders.h"
#include "FourCC.h"
#include "Logging.h"
#include <pal/spi/cf/CoreVideoSPI.h>
#include <pal/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdMap.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/StringBuilder.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

static constexpr auto s_yuvVertexShaderTexture2D {
    "attribute vec2 a_position;"
    "uniform vec2 u_yTextureSize;"
    "uniform vec2 u_uvTextureSize;"
    "uniform int u_flipY;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    gl_Position = vec4(a_position, 0, 1.0);"
    "    vec2 normalizedPosition = a_position * .5 + .5;"
    "    if (u_flipY == 1)"
    "        normalizedPosition.y = 1.0 - normalizedPosition.y;"
    "    v_yTextureCoordinate = normalizedPosition;"
    "    v_uvTextureCoordinate = normalizedPosition;"
    "}"_s
};

static constexpr auto s_yuvVertexShaderTextureRectangle {
    "attribute vec2 a_position;"
    "uniform vec2 u_yTextureSize;"
    "uniform vec2 u_uvTextureSize;"
    "uniform int u_flipY;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    gl_Position = vec4(a_position, 0, 1.0);"
    "    vec2 normalizedPosition = a_position * .5 + .5;"
    "    if (u_flipY == 1)"
    "        normalizedPosition.y = 1.0 - normalizedPosition.y;"
    "    v_yTextureCoordinate = normalizedPosition * u_yTextureSize;"
    "    v_uvTextureCoordinate = normalizedPosition * u_uvTextureSize;"
    "}"_s
};

constexpr auto s_yuvFragmentShaderTexture2D {
    "precision mediump float;"
    "uniform sampler2D u_yTexture;"
    "uniform sampler2D u_uvTexture;"
    "uniform mat4 u_colorMatrix;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    vec4 yuv;"
    "    yuv.r = texture2D(u_yTexture, v_yTextureCoordinate).r;"
    "    yuv.gb = texture2D(u_uvTexture, v_uvTextureCoordinate).rg;"
    "    yuv.a = 1.0;"
    "    gl_FragColor = yuv * u_colorMatrix;"
    "}"_s
};

static constexpr auto s_yuvFragmentShaderTextureRectangle {
    "precision mediump float;"
    "uniform sampler2DRect u_yTexture;"
    "uniform sampler2DRect u_uvTexture;"
    "uniform mat4 u_colorMatrix;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    vec4 yuv;"
    "    yuv.r = texture2DRect(u_yTexture, v_yTextureCoordinate).r;"
    "    yuv.gb = texture2DRect(u_uvTexture, v_uvTextureCoordinate).rg;"
    "    yuv.a = 1.0;"
    "    gl_FragColor = yuv * u_colorMatrix;"
    "}"_s
};

enum class PixelRange {
    Unknown,
    Video,
    Full,
};

enum class TransferFunctionCV {
    Unknown,
    kITU_R_709_2,
    kITU_R_601_4,
    kSMPTE_240M_1995,
    kDCI_P3,
    kP3_D65,
    kITU_R_2020,
};

static PixelRange pixelRangeFromPixelFormat(OSType pixelFormat)
{
    switch (pixelFormat) {
    case kCVPixelFormatType_4444AYpCbCr8:
    case kCVPixelFormatType_4444AYpCbCr16:
    case kCVPixelFormatType_422YpCbCr_4A_8BiPlanar:
    case kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange:
    case kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange:
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
    case kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange:
#endif
        return PixelRange::Video;
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr8FullRange:
    case kCVPixelFormatType_ARGB2101010LEPacked:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
    case kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange:
#endif
        return PixelRange::Full;
    default:
        return PixelRange::Unknown;
    }
}

static TransferFunctionCV transferFunctionFromString(CFStringRef string)
{
    if (!string)
        return TransferFunctionCV::Unknown;
    if (CFEqual(string, kCVImageBufferYCbCrMatrix_ITU_R_709_2))
        return TransferFunctionCV::kITU_R_709_2;
    if (CFEqual(string, kCVImageBufferYCbCrMatrix_ITU_R_601_4))
        return TransferFunctionCV::kITU_R_601_4;
    if (CFEqual(string, kCVImageBufferYCbCrMatrix_SMPTE_240M_1995))
        return TransferFunctionCV::kSMPTE_240M_1995;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_DCI_P3() && CFEqual(string, kCVImageBufferYCbCrMatrix_DCI_P3))
        return TransferFunctionCV::kDCI_P3;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_P3_D65() && CFEqual(string, kCVImageBufferYCbCrMatrix_P3_D65))
        return TransferFunctionCV::kP3_D65;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_ITU_R_2020() && CFEqual(string, kCVImageBufferYCbCrMatrix_ITU_R_2020))
        return TransferFunctionCV::kITU_R_2020;
    return TransferFunctionCV::Unknown;
}

struct GLfloatColor {
    union {
        struct {
            GLfloat r;
            GLfloat g;
            GLfloat b;
        } rgb;
        struct {
            GLfloat y;
            GLfloat cb;
            GLfloat cr;
        } ycbcr;
    };

    constexpr GLfloatColor(GLfloat r, GLfloat g, GLfloat b)
        : rgb { r, g, b }
    {
    }

    constexpr GLfloatColor(int r, int g, int b, GLfloat scale)
        : rgb { r / scale, g / scale, b / scale}
    {
    }

    static constexpr GLfloat abs(GLfloat value)
    {
        return value >= 0 ? value : -value;
    }

    constexpr bool isApproximatelyEqualTo(const GLfloatColor& color, GLfloat maxDelta) const
    {
        return abs(rgb.r - color.rgb.r) < abs(maxDelta)
            && abs(rgb.g - color.rgb.g) < abs(maxDelta)
            && abs(rgb.b - color.rgb.b) < abs(maxDelta);
    }
};

struct GLfloatColors {
    static constexpr GLfloatColor black   {0, 0, 0};
    static constexpr GLfloatColor white   {1, 1, 1};
    static constexpr GLfloatColor red     {1, 0, 0};
    static constexpr GLfloatColor green   {0, 1, 0};
    static constexpr GLfloatColor blue    {0, 0, 1};
    static constexpr GLfloatColor cyan    {0, 1, 1};
    static constexpr GLfloatColor magenta {1, 0, 1};
    static constexpr GLfloatColor yellow  {1, 1, 0};
};

struct YCbCrMatrix {
        GLfloat rows[4][4];

    constexpr YCbCrMatrix(PixelRange, GLfloat cbCoefficient, GLfloat crCoefficient);

    operator GCGLSpan<const GLfloat, 16>() const
    {
        return makeGCGLSpan<16>(&rows[0][0]);
    }

    constexpr GLfloatColor operator*(const GLfloatColor&) const;
};

constexpr YCbCrMatrix::YCbCrMatrix(PixelRange range, GLfloat cbCoefficient, GLfloat crCoefficient)
    : rows { }
{
    // The conversion from YCbCr -> RGB generally takes the form:
    // Y = Kr * R + Kg * G + Kb * B
    // Cb = (B - Y) / (2 * (1 - Kb))
    // Cr = (R - Y) / (2 * (1 - Kr))
    // Where the values of Kb and Kr are defined in a specification and Kg is derived from: Kr + Kg + Kb = 1
    //
    // Solving the above equations for R, B, and G derives the following:
    // R = Y + (2 * (1 - Kr)) * Cr
    // B = Y + (2 * (1 - Kb)) * Cb
    // G = Y - (2 * (1 - Kb)) * (Kb / Kg) * Cb - ((1 - Kr) * 2) * (Kr / Kg) * Cr
    //
    // When the color values are Video range, Y has a range of [16, 235] with a width of 219, and Cb & Cr have
    // a range of [16, 240] with a width of 224. When the color values are Full range, Y, Cb, and Cr all have
    // a range of [0, 255] with a width of 256.

    GLfloat cgCoefficient = 1 - cbCoefficient - crCoefficient;
    GLfloat yScalingFactor = range == PixelRange::Full ? 1.f : 255.f / 219.f;
    GLfloat cbcrScalingFactor = range == PixelRange::Full ? 1.f : 255.f / 224.f;

    rows[0][0] = yScalingFactor;
    rows[0][1] = 0;
    rows[0][2] = cbcrScalingFactor * 2 * (1 - crCoefficient);
    rows[0][3] = 0;

    rows[1][0] = yScalingFactor;
    rows[1][1] = -cbcrScalingFactor * 2 * (1 - cbCoefficient) * (cbCoefficient / cgCoefficient);
    rows[1][2] = -cbcrScalingFactor * 2 * (1 - crCoefficient) * (crCoefficient / cgCoefficient);
    rows[1][3] = 0;

    rows[2][0] = yScalingFactor;
    rows[2][1] = cbcrScalingFactor * 2 * (1 - cbCoefficient);
    rows[2][2] = 0;
    rows[2][3] = 0;

    rows[3][0] = 0;
    rows[3][1] = 0;
    rows[3][2] = 0;
    rows[3][3] = 1;

    // Configure the final column of the matrix to convert Cb and Cr to [-128, 128]
    // and, in the case of video-range, to convert Y to [16, 240]:
    for (auto rowNumber = 0; rowNumber < 3; ++rowNumber) {
        auto& row = rows[rowNumber];
        auto& x = row[0];
        auto& y = row[1];
        auto& z = row[2];
        auto& w = row[3];

        w -= (y + z) * 128 / 255;
        if (range == PixelRange::Video)
            w -= x * 16 / 255;
    }
}

constexpr GLfloatColor YCbCrMatrix::operator*(const GLfloatColor& color) const
{
    return GLfloatColor(
        rows[0][0] * color.rgb.r + rows[0][1] * color.rgb.g + rows[0][2] * color.rgb.b + rows[0][3],
        rows[1][0] * color.rgb.r + rows[1][1] * color.rgb.g + rows[1][2] * color.rgb.b + rows[1][3],
        rows[2][0] * color.rgb.r + rows[2][1] * color.rgb.g + rows[2][2] * color.rgb.b + rows[2][3]
    );
}

static GCGLSpan<const GLfloat, 16> YCbCrToRGBMatrixForRangeAndTransferFunction(PixelRange range, TransferFunctionCV transferFunction)
{
    using MapKey = std::pair<PixelRange, TransferFunctionCV>;
    using MatrixMap = StdMap<MapKey, const YCbCrMatrix&>;

    static NeverDestroyed<MatrixMap> matrices;
    static dispatch_once_t onceToken;

    // Matrices are derived from the components in the ITU R.601 rev 4 specification
    // https://www.itu.int/rec/R-REC-BT.601
    constexpr static YCbCrMatrix r601VideoMatrix { PixelRange::Video, 0.114f, 0.299f };
    constexpr static YCbCrMatrix r601FullMatrix { PixelRange::Full, 0.114f, 0.299f };

    static_assert((r601VideoMatrix * GLfloatColor(16,  128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "r.610 video matrix does not produce black color");
    static_assert((r601VideoMatrix * GLfloatColor(235, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "r.610 video matrix does not produce white color");
    static_assert((r601VideoMatrix * GLfloatColor(81,  90,  240, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "r.610 video matrix does not produce red color");
    static_assert((r601VideoMatrix * GLfloatColor(145, 54,  34,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "r.610 video matrix does not produce green color");
    static_assert((r601VideoMatrix * GLfloatColor(41,  240, 110, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "r.610 video matrix does not produce blue color");
    static_assert((r601VideoMatrix * GLfloatColor(210, 16,  146, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "r.610 video matrix does not produce yellow color");
    static_assert((r601VideoMatrix * GLfloatColor(106, 202, 222, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "r.610 video matrix does not produce magenta color");
    static_assert((r601VideoMatrix * GLfloatColor(170, 166, 16,  255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "r.610 video matrix does not produce cyan color");

    static_assert((r601FullMatrix * GLfloatColor(0,   128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "r.610 full matrix does not produce black color");
    static_assert((r601FullMatrix * GLfloatColor(255, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "r.610 full matrix does not produce white color");
    static_assert((r601FullMatrix * GLfloatColor(76,  85,  255, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "r.610 full matrix does not produce red color");
    static_assert((r601FullMatrix * GLfloatColor(150, 44,  21,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "r.610 full matrix does not produce green color");
    static_assert((r601FullMatrix * GLfloatColor(29,  255, 107, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "r.610 full matrix does not produce blue color");
    static_assert((r601FullMatrix * GLfloatColor(226, 0,   149, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "r.610 full matrix does not produce yellow color");
    static_assert((r601FullMatrix * GLfloatColor(105, 212, 235, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "r.610 full matrix does not produce magenta color");
    static_assert((r601FullMatrix * GLfloatColor(179, 171, 1,   255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "r.610 full matrix does not produce cyan color");

    // Matrices are derived from the components in the ITU R.709 rev 2 specification
    // https://www.itu.int/rec/R-REC-BT.709-2-199510-S
    constexpr static YCbCrMatrix r709VideoMatrix { PixelRange::Video, 0.0722, 0.2126 };
    constexpr static YCbCrMatrix r709FullMatrix { PixelRange::Full, 0.0722, 0.2126 };

    static_assert((r709VideoMatrix * GLfloatColor(16,  128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "r.709 video matrix does not produce black color");
    static_assert((r709VideoMatrix * GLfloatColor(235, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "r.709 video matrix does not produce white color");
    static_assert((r709VideoMatrix * GLfloatColor(63,  102, 240, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "r.709 video matrix does not produce red color");
    static_assert((r709VideoMatrix * GLfloatColor(173, 42,  26,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "r.709 video matrix does not produce green color");
    static_assert((r709VideoMatrix * GLfloatColor(32,  240, 118, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "r.709 video matrix does not produce blue color");
    static_assert((r709VideoMatrix * GLfloatColor(219, 16,  138, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "r.709 video matrix does not produce yellow color");
    static_assert((r709VideoMatrix * GLfloatColor(78,  214, 230, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "r.709 video matrix does not produce magenta color");
    static_assert((r709VideoMatrix * GLfloatColor(188, 154, 16,  255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "r.709 video matrix does not produce cyan color");

    static_assert((r709FullMatrix * GLfloatColor(0,   128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "r.709 full matrix does not produce black color");
    static_assert((r709FullMatrix * GLfloatColor(255, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "r.709 full matrix does not produce white color");
    static_assert((r709FullMatrix * GLfloatColor(54,  99,  256, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "r.709 full matrix does not produce red color");
    static_assert((r709FullMatrix * GLfloatColor(182, 30,  12,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "r.709 full matrix does not produce green color");
    static_assert((r709FullMatrix * GLfloatColor(18,  256, 116, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "r.709 full matrix does not produce blue color");
    static_assert((r709FullMatrix * GLfloatColor(237, 1,   140, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "r.709 full matrix does not produce yellow color");
    static_assert((r709FullMatrix * GLfloatColor(73,  226, 244, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "r.709 full matrix does not produce magenta color");
    static_assert((r709FullMatrix * GLfloatColor(201, 157, 1,   255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "r.709 full matrix does not produce cyan color");

    // Matrices are derived from the components in the ITU-R BT.2020-2 specification
    // https://www.itu.int/rec/R-REC-BT.2020
    constexpr static YCbCrMatrix bt2020VideoMatrix { PixelRange::Video, 0.0593, 0.2627 };
    constexpr static YCbCrMatrix bt2020FullMatrix { PixelRange::Full, 0.0593, 0.2627 };

    static_assert((bt2020VideoMatrix * GLfloatColor(16,  128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "bt.2020 video matrix does not produce black color");
    static_assert((bt2020VideoMatrix * GLfloatColor(235, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "bt.2020 video matrix does not produce white color");
    static_assert((bt2020VideoMatrix * GLfloatColor(74,  97,  240, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "bt.2020 video matrix does not produce red color");
    static_assert((bt2020VideoMatrix * GLfloatColor(164, 47,  25,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "bt.2020 video matrix does not produce green color");
    static_assert((bt2020VideoMatrix * GLfloatColor(29,  240, 119, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "bt.2020 video matrix does not produce blue color");
    static_assert((bt2020VideoMatrix * GLfloatColor(222, 16,  137, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "bt.2020 video matrix does not produce yellow color");
    static_assert((bt2020VideoMatrix * GLfloatColor(87,  209, 231, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "bt.2020 video matrix does not produce magenta color");
    static_assert((bt2020VideoMatrix * GLfloatColor(177, 159, 16,  255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "bt.2020 video matrix does not produce cyan color");

    static_assert((bt2020FullMatrix * GLfloatColor(0,   128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "bt.2020 full matrix does not produce black color");
    static_assert((bt2020FullMatrix * GLfloatColor(255, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "bt.2020 full matrix does not produce white color");
    static_assert((bt2020FullMatrix * GLfloatColor(67,  92,  256, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "bt.2020 full matrix does not produce red color");
    static_assert((bt2020FullMatrix * GLfloatColor(173, 36,  11,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "bt.2020 full matrix does not produce green color");
    static_assert((bt2020FullMatrix * GLfloatColor(15,  256, 118, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "bt.2020 full matrix does not produce blue color");
    static_assert((bt2020FullMatrix * GLfloatColor(240, 0,   138, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "bt.2020 full matrix does not produce yellow color");
    static_assert((bt2020FullMatrix * GLfloatColor(82,  220, 245, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "bt.2020 full matrix does not produce magenta color");
    static_assert((bt2020FullMatrix * GLfloatColor(188, 164, 1,   255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "bt.2020 full matrix does not produce cyan color");

    // Matrices are derived from the components in the SMPTE 240M-1999 specification
    // http://ieeexplore.ieee.org/document/7291461/
    constexpr static YCbCrMatrix smpte240MVideoMatrix { PixelRange::Video, 0.087, 0.212 };
    constexpr static YCbCrMatrix smpte240MFullMatrix { PixelRange::Full, 0.087, 0.212 };

    static_assert((smpte240MVideoMatrix * GLfloatColor(16,  128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "SMPTE 240M video matrix does not produce black color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(235, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "SMPTE 240M video matrix does not produce white color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(62,  102, 240, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "SMPTE 240M video matrix does not produce red color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(170, 42,  28,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "SMPTE 240M video matrix does not produce green color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(35,  240, 116, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "SMPTE 240M video matrix does not produce blue color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(216, 16,  140, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "SMPTE 240M video matrix does not produce yellow color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(81,  214, 228, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "SMPTE 240M video matrix does not produce magenta color");
    static_assert((smpte240MVideoMatrix * GLfloatColor(189, 154, 16,  255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "SMPTE 240M video matrix does not produce cyan color");

    static_assert((smpte240MFullMatrix * GLfloatColor(0,   128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::black,   1.5f / 255.f), "SMPTE 240M full matrix does not produce black color");
    static_assert((smpte240MFullMatrix * GLfloatColor(255, 128, 128, 255)).isApproximatelyEqualTo(GLfloatColors::white,   1.5f / 255.f), "SMPTE 240M full matrix does not produce white color");
    static_assert((smpte240MFullMatrix * GLfloatColor(54,  98,  256, 255)).isApproximatelyEqualTo(GLfloatColors::red,     1.5f / 255.f), "SMPTE 240M full matrix does not produce red color");
    static_assert((smpte240MFullMatrix * GLfloatColor(179, 30,  15,  255)).isApproximatelyEqualTo(GLfloatColors::green,   1.5f / 255.f), "SMPTE 240M full matrix does not produce green color");
    static_assert((smpte240MFullMatrix * GLfloatColor(22,  256, 114, 255)).isApproximatelyEqualTo(GLfloatColors::blue,    1.5f / 255.f), "SMPTE 240M full matrix does not produce blue color");
    static_assert((smpte240MFullMatrix * GLfloatColor(233, 1,   142, 255)).isApproximatelyEqualTo(GLfloatColors::yellow,  1.5f / 255.f), "SMPTE 240M full matrix does not produce yellow color");
    static_assert((smpte240MFullMatrix * GLfloatColor(76,  226, 241, 255)).isApproximatelyEqualTo(GLfloatColors::magenta, 1.5f / 255.f), "SMPTE 240M full matrix does not produce magenta color");
    static_assert((smpte240MFullMatrix * GLfloatColor(201, 158, 1,   255)).isApproximatelyEqualTo(GLfloatColors::cyan,    1.5f / 255.f), "SMPTE 240M full matrix does not produce cyan color");

    dispatch_once(&onceToken, ^{
        matrices.get().emplace(MapKey(PixelRange::Video, TransferFunctionCV::kITU_R_601_4), r601VideoMatrix);
        matrices.get().emplace(MapKey(PixelRange::Full, TransferFunctionCV::kITU_R_601_4), r601FullMatrix);
        matrices.get().emplace(MapKey(PixelRange::Video, TransferFunctionCV::kITU_R_709_2), r709VideoMatrix);
        matrices.get().emplace(MapKey(PixelRange::Full, TransferFunctionCV::kITU_R_709_2), r709FullMatrix);
        matrices.get().emplace(MapKey(PixelRange::Video, TransferFunctionCV::kITU_R_2020), bt2020VideoMatrix);
        matrices.get().emplace(MapKey(PixelRange::Full, TransferFunctionCV::kITU_R_2020), bt2020FullMatrix);
        matrices.get().emplace(MapKey(PixelRange::Video, TransferFunctionCV::kSMPTE_240M_1995), smpte240MVideoMatrix);
        matrices.get().emplace(MapKey(PixelRange::Full, TransferFunctionCV::kSMPTE_240M_1995), smpte240MFullMatrix);
    });

    // We should never be asked to handle a Pixel Format whose range value is unknown.
    ASSERT(range != PixelRange::Unknown);
    if (range == PixelRange::Unknown)
        range = PixelRange::Full;

    auto iterator = matrices.get().find({range, transferFunction});

    // Assume unknown transfer functions are r.601:
    if (iterator == matrices.get().end())
        iterator = matrices.get().find({range, TransferFunctionCV::kITU_R_601_4});

    ASSERT(iterator != matrices.get().end());
    return iterator->second;
}

namespace {

// Scoped holder of a cleanup function. Calls the function at the end of the scope.
// Note: Releases the reference to the function only after the scope, not
// at the time of `reset()` call.
template <typename F>
class ScopedCleanup {
public:
    explicit ScopedCleanup(F&& function)
        : m_function(WTFMove(function))
    {
    }
    ~ScopedCleanup()
    {
        if (m_shouldCall)
            m_function();
    }
    void reset() { m_shouldCall = false; }
private:
    bool m_shouldCall = true;
    const F m_function;
};

}

GraphicsContextGLCVANGLE::GraphicsContextGLCVANGLE(GraphicsContextGLOpenGL& context)
    : m_context(GraphicsContextGLOpenGL::createShared(context))
    , m_framebuffer(m_context->createFramebuffer())
{
}

GraphicsContextGLCVANGLE::~GraphicsContextGLCVANGLE()
{
    if (m_yuvVertexBuffer)
        m_context->deleteBuffer(m_yuvVertexBuffer);
    if (m_yuvProgram)
        m_context->deleteProgram(m_yuvProgram);
    m_context->deleteFramebuffer(m_framebuffer);
}

bool GraphicsContextGLCVANGLE::initializeUVContextObjects()
{
    const bool useTexture2D = GraphicsContextGLOpenGL::drawingBufferTextureTarget() == GraphicsContextGL::TEXTURE_2D;

    PlatformGLObject vertexShader = m_context->createShader(GraphicsContextGL::VERTEX_SHADER);
    if (useTexture2D)
        m_context->shaderSource(vertexShader, s_yuvVertexShaderTexture2D);
    else
        m_context->shaderSource(vertexShader, s_yuvVertexShaderTextureRectangle);

    m_context->compileShaderDirect(vertexShader);

    GCGLint status = m_context->getShaderi(vertexShader, GraphicsContextGL::COMPILE_STATUS);
    if (!status) {
        LOG(WebGL, "GraphicsContextGLCVANGLE::initializeUVContextObjects(%p) - Vertex shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        return false;
    }

    PlatformGLObject fragmentShader = m_context->createShader(GraphicsContextGL::FRAGMENT_SHADER);
    if (useTexture2D)
        m_context->shaderSource(fragmentShader, s_yuvFragmentShaderTexture2D);
    else
        m_context->shaderSource(fragmentShader, s_yuvFragmentShaderTextureRectangle);

    m_context->compileShaderDirect(fragmentShader);

    status = m_context->getShaderi(fragmentShader, GraphicsContextGL::COMPILE_STATUS);
    if (!status) {
        LOG(WebGL, "GraphicsContextGLCVANGLE::initializeUVContextObjects(%p) - Fragment shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        return false;
    }

    m_yuvProgram = m_context->createProgram();
    m_context->attachShader(m_yuvProgram, vertexShader);
    m_context->attachShader(m_yuvProgram, fragmentShader);
    m_context->linkProgram(m_yuvProgram);

    status = m_context->getProgrami(m_yuvProgram, GraphicsContextGL::LINK_STATUS);
    if (!status) {
        LOG(WebGL, "GraphicsContextGLCVANGLE::initializeUVContextObjects(%p) - Program failed to link.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_context->deleteProgram(m_yuvProgram);
        m_yuvProgram = 0;
        return false;
    }

    m_yTextureUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_yTexture"_s);
    m_uvTextureUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_uvTexture"_s);
    m_colorMatrixUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_colorMatrix"_s);
    m_yuvFlipYUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_flipY"_s);
    m_yTextureSizeUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_yTextureSize"_s);
    m_uvTextureSizeUniformLocation = m_context->getUniformLocation(m_yuvProgram, "u_uvTextureSize"_s);
    m_yuvPositionAttributeLocation = m_context->getAttribLocationDirect(m_yuvProgram, "a_position"_s);

    m_context->detachShader(m_yuvProgram, vertexShader);
    m_context->detachShader(m_yuvProgram, fragmentShader);
    m_context->deleteShader(vertexShader);
    m_context->deleteShader(fragmentShader);

    m_yuvVertexBuffer = m_context->createBuffer();
    float vertices[12] = { -1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1 };

    m_context->bindBuffer(GraphicsContextGL::ARRAY_BUFFER, m_yuvVertexBuffer);
    m_context->bufferData(GraphicsContextGL::ARRAY_BUFFER, GCGLSpan<const GCGLvoid>(vertices, sizeof(vertices)), GraphicsContextGL::STATIC_DRAW);
    m_context->enableVertexAttribArray(m_yuvPositionAttributeLocation);
    m_context->vertexAttribPointer(m_yuvPositionAttributeLocation, 2, GraphicsContextGL::FLOAT, false, 0, 0);

    return true;
}

void* GraphicsContextGLCVANGLE::attachIOSurfaceToTexture(GCGLenum target, GCGLenum internalFormat, GCGLsizei width, GCGLsizei height, GCGLenum type, IOSurfaceRef surface, GCGLuint plane)
{
    auto display = m_context->platformDisplay();
    EGLint eglTextureTarget = 0;

    if (target == GraphicsContextGL::TEXTURE_RECTANGLE_ARB)
        eglTextureTarget = EGL_TEXTURE_RECTANGLE_ANGLE;
    else if (target == GraphicsContextGL::TEXTURE_2D)
        eglTextureTarget = EGL_TEXTURE_2D;
    else {
        LOG(WebGL, "Unknown texture target %d.", static_cast<int>(target));
        return nullptr;
    }
    if (eglTextureTarget != GraphicsContextGLOpenGL::EGLDrawingBufferTextureTarget()) {
        LOG(WebGL, "Mismatch in EGL texture target %d.", static_cast<int>(target));
        return nullptr;
    }

    const EGLint surfaceAttributes[] = {
        EGL_WIDTH, width,
        EGL_HEIGHT, height,
        EGL_IOSURFACE_PLANE_ANGLE, static_cast<EGLint>(plane),
        EGL_TEXTURE_TARGET, static_cast<EGLint>(eglTextureTarget),
        EGL_TEXTURE_INTERNAL_FORMAT_ANGLE, static_cast<EGLint>(internalFormat),
        EGL_TEXTURE_FORMAT, EGL_TEXTURE_RGBA,
        EGL_TEXTURE_TYPE_ANGLE, static_cast<EGLint>(type),
        // Only has an effect on the iOS Simulator.
        EGL_IOSURFACE_USAGE_HINT_ANGLE, EGL_IOSURFACE_READ_HINT_ANGLE,
        EGL_NONE, EGL_NONE
    };
    EGLSurface pbuffer = EGL_CreatePbufferFromClientBuffer(display, EGL_IOSURFACE_ANGLE, surface, m_context->platformConfig(), surfaceAttributes);
    if (!pbuffer)
        return nullptr;
    if (!EGL_BindTexImage(display, pbuffer, EGL_BACK_BUFFER)) {
        EGL_DestroySurface(display, pbuffer);
        return nullptr;
    }
    return pbuffer;
}

void GraphicsContextGLCVANGLE::detachIOSurfaceFromTexture(void* handle)
{
    auto display = m_context->platformDisplay();
    EGL_ReleaseTexImage(display, handle, EGL_BACK_BUFFER);
    EGL_DestroySurface(display, handle);
}

bool GraphicsContextGLCVANGLE::copyPixelBufferToTexture(CVPixelBufferRef image, PlatformGLObject outputTexture, GCGLint level, GCGLenum internalFormat, GCGLenum format, GCGLenum type, FlipY flipY)
{
    // FIXME: This currently only supports '420v' and '420f' pixel formats. Investigate supporting more pixel formats.
    OSType pixelFormat = CVPixelBufferGetPixelFormatType(image);
    if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange
        && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
        && pixelFormat != kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange
        && pixelFormat != kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange
#endif
        ) {
        LOG(WebGL, "GraphicsContextGLCVANGLE::copyVideoTextureToPlatformTexture(%p) - Asked to copy an unsupported pixel format ('%s').", this, FourCC(pixelFormat).toString().utf8().data());
        return false;
    }
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(image);
    if (!surface)
        return false;

    auto newSurfaceSeed = IOSurfaceGetSeed(surface);
    if (flipY == m_lastFlipY
        && surface == m_lastSurface 
        && newSurfaceSeed == m_lastSurfaceSeed
        && lastTextureSeed(outputTexture) == m_context->textureSeed(outputTexture)) {
        // If the texture hasn't been modified since the last time we copied to it, and the
        // image hasn't been modified since the last time it was copied, this is a no-op.
        return true;
    }

    if (!m_yuvProgram) {
        if (!initializeUVContextObjects()) {
            LOG(WebGL, "GraphicsContextGLCVANGLE::copyVideoTextureToPlatformTexture(%p) - Unable to initialize OpenGL context objects.", this);
            return false;
        }
    }
    size_t width = CVPixelBufferGetWidth(image);
    size_t height = CVPixelBufferGetHeight(image);

    m_context->bindFramebuffer(GraphicsContextGL::FRAMEBUFFER, m_framebuffer);

    // The outputTexture might contain uninitialized content on early-outs. Clear it in cases
    // autoClearTextureOnError is not reset.
    auto autoClearTextureOnError = ScopedCleanup {
        [outputTexture, level, internalFormat, format, type, context = m_context.ptr()] {
            context->bindTexture(GraphicsContextGL::TEXTURE_2D, outputTexture);
            context->texImage2DDirect(GL_TEXTURE_2D, level, internalFormat, 0, 0, 0, format, type, nullptr);
            context->bindTexture(GraphicsContextGL::TEXTURE_2D, 0);
        }
    };
    // Allocate memory for the output texture.
    m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, outputTexture);
    m_context->texParameteri(GraphicsContextGL::TEXTURE_2D, GraphicsContextGL::TEXTURE_MAG_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(GraphicsContextGL::TEXTURE_2D, GraphicsContextGL::TEXTURE_MIN_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(GraphicsContextGL::TEXTURE_2D, GraphicsContextGL::TEXTURE_WRAP_S, GraphicsContextGL::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContextGL::TEXTURE_2D, GraphicsContextGL::TEXTURE_WRAP_T, GraphicsContextGL::CLAMP_TO_EDGE);
    m_context->texImage2DDirect(GL_TEXTURE_2D, level, internalFormat, width, height, 0, format, type, nullptr);

    m_context->framebufferTexture2D(GraphicsContextGL::FRAMEBUFFER, GraphicsContextGL::COLOR_ATTACHMENT0, GraphicsContextGL::TEXTURE_2D, outputTexture, level);
    GCGLenum status = m_context->checkFramebufferStatus(GraphicsContextGL::FRAMEBUFFER);
    if (status != GraphicsContextGL::FRAMEBUFFER_COMPLETE) {
        LOG(WebGL, "GraphicsContextGLCVANGLE::copyVideoTextureToPlatformTexture(%p) - Unable to create framebuffer for outputTexture.", this);
        return false;
    }
    m_context->bindTexture(GraphicsContextGL::TEXTURE_2D, 0);

    m_context->useProgram(m_yuvProgram);
    m_context->viewport(0, 0, width, height);

    // Bind and set up the textures for the video source.
    auto yPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 0);
    auto yPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 0);
    auto uvPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 1);
    auto uvPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 1);

    GCGLenum videoTextureTarget = GraphicsContextGLOpenGL::drawingBufferTextureTarget();

    auto uvTexture = m_context->createTexture();
    m_context->activeTexture(GraphicsContextGL::TEXTURE1);
    m_context->bindTexture(videoTextureTarget, uvTexture);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_MAG_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_MIN_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_WRAP_S, GraphicsContextGL::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_WRAP_T, GraphicsContextGL::CLAMP_TO_EDGE);
    auto uvHandle = attachIOSurfaceToTexture(videoTextureTarget, GraphicsContextGL::RG, uvPlaneWidth, uvPlaneHeight, GraphicsContextGL::UNSIGNED_BYTE, surface, 1);
    if (!uvHandle) {
        m_context->deleteTexture(uvTexture);
        return false;
    }

    auto yTexture = m_context->createTexture();
    m_context->activeTexture(GraphicsContextGL::TEXTURE0);
    m_context->bindTexture(videoTextureTarget, yTexture);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_MAG_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_MIN_FILTER, GraphicsContextGL::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_WRAP_S, GraphicsContextGL::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContextGL::TEXTURE_WRAP_T, GraphicsContextGL::CLAMP_TO_EDGE);
    auto yHandle = attachIOSurfaceToTexture(videoTextureTarget, GraphicsContextGL::RED, yPlaneWidth, yPlaneHeight, GraphicsContextGL::UNSIGNED_BYTE, surface, 0);
    if (!yHandle) {
        m_context->deleteTexture(yTexture);
        m_context->deleteTexture(uvTexture);
        return false;
    }

    // Configure the drawing parameters.
    m_context->uniform1i(m_yTextureUniformLocation, 0);
    m_context->uniform1i(m_uvTextureUniformLocation, 1);
    m_context->uniform1i(m_yuvFlipYUniformLocation, flipY == FlipY::Yes ? 1 : 0);
    m_context->uniform2f(m_yTextureSizeUniformLocation, yPlaneWidth, yPlaneHeight);
    m_context->uniform2f(m_uvTextureSizeUniformLocation, uvPlaneWidth, uvPlaneHeight);

    auto range = pixelRangeFromPixelFormat(pixelFormat);
    auto transferFunction = transferFunctionFromString(dynamic_cf_cast<CFStringRef>(CVBufferGetAttachment(image, kCVImageBufferYCbCrMatrixKey, nil)));
    auto colorMatrix = YCbCrToRGBMatrixForRangeAndTransferFunction(range, transferFunction);
    m_context->uniformMatrix4fv(m_colorMatrixUniformLocation, GL_FALSE, colorMatrix);

    // Do the actual drawing.
    m_context->drawArrays(GraphicsContextGL::TRIANGLES, 0, 6);

    // Clean-up.
    m_context->deleteTexture(yTexture);
    m_context->deleteTexture(uvTexture);
    detachIOSurfaceFromTexture(yHandle);
    detachIOSurfaceFromTexture(uvHandle);

    m_lastSurface = surface;
    m_lastSurfaceSeed = newSurfaceSeed;
    m_lastTextureSeed.set(outputTexture, m_context->textureSeed(outputTexture));
    m_lastFlipY = flipY;
    autoClearTextureOnError.reset();
    return true;
}

}

#endif
