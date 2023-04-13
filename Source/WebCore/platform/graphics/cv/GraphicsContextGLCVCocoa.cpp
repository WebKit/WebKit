/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "GraphicsContextGLCVCocoa.h"

#if ENABLE(WEBGL) && ENABLE(VIDEO) && USE(AVFOUNDATION)

#include "ANGLEHeaders.h"
#include "ANGLEUtilitiesCocoa.h"
#include "FourCC.h"
#include "GraphicsContextGLCocoa.h"
#include "Logging.h"
#include "VideoFrameCV.h"
#include <pal/spi/cf/CoreVideoSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Scope.h>
#include <wtf/StdMap.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>
#include <wtf/text/StringBuilder.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {

static constexpr auto s_yuvVertexShaderTexture2D {
    "attribute vec2 a_position;"
    "uniform vec2 u_yTextureSize;"
    "uniform vec2 u_uvTextureSize;"
    "uniform int u_flipY;"
    "uniform int u_flipX;"
    "uniform int u_swapXY;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    gl_Position = vec4(a_position, 0, 1.0);"
    "    vec2 normalizedPosition = a_position * .5 + .5;"
    "    if (u_flipY == 1)"
    "        normalizedPosition.y = 1.0 - normalizedPosition.y;"
    "    if (u_flipX == 1)"
    "        normalizedPosition.x = 1.0 - normalizedPosition.x;"
    "    if (u_swapXY == 1)"
    "        normalizedPosition.xy = normalizedPosition.yx;"
    "    v_yTextureCoordinate = normalizedPosition;"
    "    v_uvTextureCoordinate = normalizedPosition;"
    "}"_s
};

static constexpr auto s_yuvVertexShaderTextureRectangle {
    "attribute vec2 a_position;"
    "uniform vec2 u_yTextureSize;"
    "uniform vec2 u_uvTextureSize;"
    "uniform int u_flipY;"
    "uniform int u_flipX;"
    "uniform int u_swapXY;"
    "varying vec2 v_yTextureCoordinate;"
    "varying vec2 v_uvTextureCoordinate;"
    "void main()"
    "{"
    "    gl_Position = vec4(a_position, 0, 1.0);"
    "    vec2 normalizedPosition = a_position * .5 + .5;"
    "    if (u_flipY == 1)"
    "        normalizedPosition.y = 1.0 - normalizedPosition.y;"
    "    if (u_flipX == 1)"
    "        normalizedPosition.x = 1.0 - normalizedPosition.x;"
    "    if (u_swapXY == 1)"
    "        normalizedPosition.xy = normalizedPosition.yx;"
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
        : rgb { r / scale, g / scale, b / scale }
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
    static constexpr GLfloatColor black   { 0, 0, 0 };
    static constexpr GLfloatColor white   { 1, 1, 1 };
    static constexpr GLfloatColor red     { 1, 0, 0 };
    static constexpr GLfloatColor green   { 0, 1, 0 };
    static constexpr GLfloatColor blue    { 0, 0, 1 };
    static constexpr GLfloatColor cyan    { 0, 1, 1 };
    static constexpr GLfloatColor magenta { 1, 0, 1 };
    static constexpr GLfloatColor yellow  { 1, 1, 0 };
};

struct YCbCrMatrix {
    GLfloat rows[4][4];

    constexpr YCbCrMatrix(PixelRange, GLfloat cbCoefficient, GLfloat crCoefficient);

    operator const GLfloat*() const
    {
        return &rows[0][0];
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

static const GLfloat* YCbCrToRGBMatrixForRangeAndTransferFunction(PixelRange range, TransferFunctionCV transferFunction)
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

    auto iterator = matrices.get().find({ range, transferFunction });

    // Assume unknown transfer functions are r.601:
    if (iterator == matrices.get().end())
        iterator = matrices.get().find({ range, TransferFunctionCV::kITU_R_601_4 });

    ASSERT(iterator != matrices.get().end());
    return iterator->second;
}

inline bool GraphicsContextGLCVCocoa::TextureContent::operator==(const TextureContent& other) const
{
    return surface == other.surface
        && surfaceSeed == other.surfaceSeed
        && level == other.level
        && internalFormat == other.internalFormat
        && format == other.format
        && type == other.type
        && unpackFlipY == other.unpackFlipY
        && orientation == other.orientation;
}

std::unique_ptr<GraphicsContextGLCVCocoa> GraphicsContextGLCVCocoa::create(GraphicsContextGLCocoa& context)
{
    std::unique_ptr<GraphicsContextGLCVCocoa> cv { new GraphicsContextGLCVCocoa(context) };
    if (!cv->m_context)
        return nullptr;
    return cv;
}

GraphicsContextGLCVCocoa::~GraphicsContextGLCVCocoa()
{
    if (!m_context || !GraphicsContextGLCocoa::makeCurrent(m_display, m_context))
        return;
    GL_DeleteBuffers(1, &m_yuvVertexBuffer);
    GL_DeleteFramebuffers(1, &m_framebuffer);
    EGL_DestroyContext(m_display, m_context);
}

GraphicsContextGLCVCocoa::GraphicsContextGLCVCocoa(GraphicsContextGLCocoa& owner)
    : m_owner(owner)
{
    // Create compatible context that shares state with owner, but one that does not
    // have robustness or WebGL compatibility.
    const EGLint contextAttributes[] = {
        EGL_CONTEXT_CLIENT_VERSION,
        owner.m_isForWebGL2 ? 3 : 2,
        EGL_CONTEXT_OPENGL_BACKWARDS_COMPATIBLE_ANGLE,
        EGL_FALSE,
        EGL_CONTEXT_CLIENT_ARRAYS_ENABLED_ANGLE,
        EGL_FALSE,
        EGL_CONTEXT_BIND_GENERATES_RESOURCE_CHROMIUM,
        EGL_FALSE,
        EGL_NONE
    };
    EGLDisplay display = owner.platformDisplay();
    EGLConfig config = owner.platformConfig();
    EGLContext context = EGL_CreateContext(display, config, owner.m_contextObj, contextAttributes);
    if (context == EGL_NO_CONTEXT)
        return;
    GraphicsContextGLCocoa::makeCurrent(display, context);

    auto contextCleanup = makeScopeExit([display, context] {
        GraphicsContextGLCocoa::makeCurrent(display, EGL_NO_CONTEXT);
        EGL_DestroyContext(display, context);
    });

    const bool useTexture2D = m_owner.drawingBufferTextureTarget() == GL_TEXTURE_2D;

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    if (!useTexture2D) {
        GL_RequestExtensionANGLE("GL_ANGLE_texture_rectangle");
        GL_RequestExtensionANGLE("GL_EXT_texture_format_BGRA8888");
        if (GL_GetError() != GL_NO_ERROR)
            return;
    }
#endif

    GLint vertexShader = GL_CreateShader(GL_VERTEX_SHADER);
    GLint fragmentShader = GL_CreateShader(GL_FRAGMENT_SHADER);
    GLuint yuvProgram = GL_CreateProgram();
    auto programCleanup = makeScopeExit([vertexShader, fragmentShader, yuvProgram] {
        GL_DeleteShader(vertexShader);
        GL_DeleteShader(fragmentShader);
        GL_DeleteProgram(yuvProgram);
    });
    // These are written so strlen might be compile-time.
    GLint vsLength = useTexture2D ? s_yuvVertexShaderTexture2D.length() : s_yuvVertexShaderTextureRectangle.length();
    GLint fsLength = useTexture2D ? s_yuvFragmentShaderTexture2D.length() : s_yuvFragmentShaderTextureRectangle.length();
    const char* vertexShaderSource = useTexture2D ? s_yuvVertexShaderTexture2D : s_yuvVertexShaderTextureRectangle;
    const char* fragmentShaderSource = useTexture2D ? s_yuvFragmentShaderTexture2D : s_yuvFragmentShaderTextureRectangle;

    GL_ShaderSource(vertexShader, 1, &vertexShaderSource, &vsLength);
    GL_ShaderSource(fragmentShader, 1, &fragmentShaderSource, &fsLength);
    GL_CompileShader(vertexShader);
    GL_CompileShader(fragmentShader);
    GL_AttachShader(yuvProgram, vertexShader);
    GL_AttachShader(yuvProgram, fragmentShader);
    GL_LinkProgram(yuvProgram);
    // Link status is checked afterwards for theoretical parallel compilation benefit.

    GLuint yuvVertexBuffer = 0;
    GL_GenBuffers(1, &yuvVertexBuffer);
    auto yuvVertexBufferCleanup = makeScopeExit([yuvVertexBuffer] {
        GL_DeleteBuffers(1, &yuvVertexBuffer);
    });
    float vertices[12] = { -1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1 };
    GL_BindBuffer(GL_ARRAY_BUFFER, yuvVertexBuffer);
    GL_BufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    GLuint framebuffer = 0;
    GL_GenFramebuffers(1, &framebuffer);
    auto framebufferCleanup = makeScopeExit([framebuffer] {
        GL_DeleteFramebuffers(1, &framebuffer);
    });

    GLint status = 0;
    GL_GetProgramivRobustANGLE(yuvProgram, GL_LINK_STATUS, 1, nullptr, &status);
    if (!status) {
        GLint vsStatus = 0;
        GL_GetShaderivRobustANGLE(vertexShader, GL_COMPILE_STATUS, 1, nullptr, &vsStatus);
        GLint fsStatus = 0;
        GL_GetShaderivRobustANGLE(fragmentShader, GL_COMPILE_STATUS, 1, nullptr, &fsStatus);
        LOG(WebGL, "GraphicsContextGLCVCocoa(%p) - YUV program failed to link: %d, %d, %d.", this, status, vsStatus, fsStatus);
        return;
    }
    contextCleanup.release();
    yuvVertexBufferCleanup.release();
    framebufferCleanup.release();
    m_display = display;
    m_context = context;
    m_config = config;
    m_yuvVertexBuffer = yuvVertexBuffer;
    m_framebuffer = framebuffer;
    m_yTextureUniformLocation = GL_GetUniformLocation(yuvProgram, "u_yTexture");
    m_uvTextureUniformLocation = GL_GetUniformLocation(yuvProgram, "u_uvTexture");
    m_colorMatrixUniformLocation = GL_GetUniformLocation(yuvProgram, "u_colorMatrix");
    m_yuvFlipYUniformLocation = GL_GetUniformLocation(yuvProgram, "u_flipY");
    m_yuvFlipXUniformLocation = GL_GetUniformLocation(yuvProgram, "u_flipX");
    m_yuvSwapXYUniformLocation = GL_GetUniformLocation(yuvProgram, "u_swapXY");
    m_yTextureSizeUniformLocation = GL_GetUniformLocation(yuvProgram, "u_yTextureSize");
    m_uvTextureSizeUniformLocation = GL_GetUniformLocation(yuvProgram, "u_uvTextureSize");
    m_yuvPositionAttributeLocation = GL_GetAttribLocation(yuvProgram, "a_position");
    // Program is deleted by the cleanup while the program binary stays in use.
    GL_UseProgram(yuvProgram);
    GL_EnableVertexAttribArray(m_yuvPositionAttributeLocation);
    GL_VertexAttribPointer(m_yuvPositionAttributeLocation, 2, GL_FLOAT, false, 0, 0);
    GL_ClearColor(0, 0, 0, 0);
    GL_BindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
}

bool GraphicsContextGLCVCocoa::copyVideoSampleToTexture(const VideoFrameCV& videoFrame, PlatformGLObject outputTexture, GLint level, GLenum internalFormat, GLenum format, GLenum type, FlipY unpackFlipY)
{
    RetainPtr<CVPixelBufferRef> convertedImage;
    auto image = videoFrame.pixelBuffer();
    // FIXME: This currently only supports '420v' and '420f' pixel formats. Investigate supporting more pixel formats.
    OSType pixelFormat = CVPixelBufferGetPixelFormatType(image);
    if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange // NOLINT
        && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange
#if HAVE(COREVIDEO_COMPRESSED_PIXEL_FORMAT_TYPES)
        && pixelFormat != kCVPixelFormatType_AGX_420YpCbCr8BiPlanarVideoRange
        && pixelFormat != kCVPixelFormatType_AGX_420YpCbCr8BiPlanarFullRange
#endif
        ) {
        convertedImage = convertPixelBuffer(image);
        if (!convertedImage) {
            LOG(WebGL, "GraphicsContextGLCVCocoa::copyVideoTextureToPlatformTexture(%p) - failed converting an image with pixel format ('%s').", this, FourCC(pixelFormat).string().data());
            return false;
        }
        image = convertedImage.get();
        pixelFormat = CVPixelBufferGetPixelFormatType(image);
        ASSERT(pixelFormat == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange);
    }
    IOSurfaceRef surface = CVPixelBufferGetIOSurface(image);
    if (!surface)
        return false;

    auto orientation = videoFrame.orientation();
    TextureContent content { reinterpret_cast<intptr_t>(surface), IOSurfaceGetSeed(surface), level, internalFormat, format, type, unpackFlipY, orientation };
    auto it = m_knownContent.find(outputTexture);
    if (it != m_knownContent.end() && it->value == content) {
        // If the texture hasn't been modified since the last time we copied to it, and the
        // image hasn't been modified since the last time it was copied, this is a no-op.
        return true;
    }
    if (!m_context || !GraphicsContextGLCocoa::makeCurrent(m_display, m_context))
        return false;

    // Compute transform that undoes the `orientation`, e.g. moves the origin to top left.
    // Even number of operations (flipX, flipY, swapXY) means a rotation.
    // Odd number of operations means a rotation and a flip.
    // `flipX` switches between Left and Right. `flipY` switches between Top and Bottom.
    // `swapXY`switches LeftTop to TopLeft.
    // Goal is to get to OriginTopLeft.
    bool flipY = false; // Flip y coordinate, i.e. mirrored along the x-axis.
    bool flipX = false; // Flip x coordinate, i.e. mirrored along the y-axis.
    bool swapXY = false;
    switch (videoFrame.orientation().orientation()) {
    case ImageOrientation::Orientation::FromImage:
    case ImageOrientation::Orientation::OriginTopLeft:
        break;
    case ImageOrientation::Orientation::OriginTopRight:
        flipX = true;
        break;
    case ImageOrientation::Orientation::OriginBottomRight:
        // Rotated 180 degrees.
        flipY = true;
        flipX = true;
        break;
    case ImageOrientation::Orientation::OriginBottomLeft:
        // Mirrored along the x-axis.
        flipY = true;
        break;
    case ImageOrientation::Orientation::OriginLeftTop:
        // Mirrored along x-axis and rotated 270 degrees clock-wise.
        swapXY = true;
        break;
    case ImageOrientation::Orientation::OriginRightTop:
        // Rotated 90 degrees clock-wise.
        flipX = true;
        swapXY = true;
        break;
    case ImageOrientation::Orientation::OriginRightBottom:
        // Mirror along x-axis and rotated 90 degrees clockwise.
        flipY = true;
        flipX = true;
        swapXY = true;
        break;
    case ImageOrientation::Orientation::OriginLeftBottom:
        // Rotated 270 degrees clock-wise.
        flipY = true;
        swapXY = true;
        break;
    }
    if (unpackFlipY == FlipY::Yes)
        flipY = !flipY;

    size_t sourceWidth = CVPixelBufferGetWidth(image);
    size_t sourceHeight = CVPixelBufferGetHeight(image);

    size_t width = swapXY ? sourceHeight : sourceWidth;
    size_t height = swapXY ? sourceWidth : sourceHeight;

    GL_Viewport(0, 0, width, height);

    // The outputTexture might contain uninitialized content on early-outs. Clear it in cases
    // autoClearTextureOnError is not reset.
    auto autoClearTextureOnError = makeScopeExit([outputTexture, level, internalFormat, format, type] {
        GL_BindTexture(GL_TEXTURE_2D, outputTexture);
        GL_TexImage2D(GL_TEXTURE_2D, level, internalFormat, 0, 0, 0, format, type, nullptr);
        GL_BindTexture(GL_TEXTURE_2D, 0);
    });
    // Allocate memory for the output texture.
    GL_BindTexture(GL_TEXTURE_2D, outputTexture);
    GL_TexImage2D(GL_TEXTURE_2D, level, internalFormat, width, height, 0, format, type, nullptr);

    GL_FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, outputTexture, level);
    GLenum status = GL_CheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        LOG(WebGL, "GraphicsContextGLCVCocoa::copyVideoTextureToPlatformTexture(%p) - Unable to create framebuffer for outputTexture.", this);
        return false;
    }
    GL_BindTexture(GL_TEXTURE_2D, 0);

    // Bind and set up the textures for the video source.
    auto yPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 0);
    auto yPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 0);
    auto uvPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 1);
    auto uvPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 1);

    GLenum videoTextureTarget = m_owner.drawingBufferTextureTarget();

    GLuint uvTexture = 0;
    GL_GenTextures(1, &uvTexture);
    auto uvTextureCleanup = makeScopeExit([uvTexture] {
        GL_DeleteTextures(1, &uvTexture);
    });
    GL_ActiveTexture(GL_TEXTURE1);
    GL_BindTexture(videoTextureTarget, uvTexture);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto uvHandle = WebCore::createPbufferAndAttachIOSurface(m_display, m_config, videoTextureTarget, EGL_IOSURFACE_READ_HINT_ANGLE, GL_RG, uvPlaneWidth, uvPlaneHeight, GL_UNSIGNED_BYTE, surface, 1);
    if (!uvHandle)
        return false;
    auto uvHandleCleanup = makeScopeExit([display = m_display, uvHandle] {
        WebCore::destroyPbufferAndDetachIOSurface(display, uvHandle);
    });

    GLuint yTexture = 0;
    GL_GenTextures(1, &yTexture);
    auto yTextureCleanup = makeScopeExit([yTexture] {
        GL_DeleteTextures(1, &yTexture);
    });
    GL_ActiveTexture(GL_TEXTURE0);
    GL_BindTexture(videoTextureTarget, yTexture);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    GL_TexParameteri(videoTextureTarget, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    auto yHandle = WebCore::createPbufferAndAttachIOSurface(m_display, m_config, videoTextureTarget, EGL_IOSURFACE_READ_HINT_ANGLE, GL_RED, yPlaneWidth, yPlaneHeight, GL_UNSIGNED_BYTE, surface, 0);
    if (!yHandle)
        return false;
    auto yHandleCleanup = makeScopeExit([display = m_display, yHandle] {
        destroyPbufferAndDetachIOSurface(display, yHandle);
    });

    // Configure the drawing parameters.
    GL_Uniform1i(m_yTextureUniformLocation, 0);
    GL_Uniform1i(m_uvTextureUniformLocation, 1);
    GL_Uniform1i(m_yuvFlipYUniformLocation, flipY ? 1 : 0);
    GL_Uniform1i(m_yuvFlipXUniformLocation, flipX ? 1 : 0);
    GL_Uniform1i(m_yuvSwapXYUniformLocation, swapXY ? 1 : 0);
    GL_Uniform2f(m_yTextureSizeUniformLocation, yPlaneWidth, yPlaneHeight);
    GL_Uniform2f(m_uvTextureSizeUniformLocation, uvPlaneWidth, uvPlaneHeight);

    auto range = pixelRangeFromPixelFormat(pixelFormat);
    auto transferFunction = transferFunctionFromString(dynamic_cf_cast<CFStringRef>(CVBufferGetAttachment(image, kCVImageBufferYCbCrMatrixKey, nil)));
    auto colorMatrix = YCbCrToRGBMatrixForRangeAndTransferFunction(range, transferFunction);
    GL_UniformMatrix4fv(m_colorMatrixUniformLocation, 1, GL_FALSE, colorMatrix);

    // Do the actual drawing.
    GL_DrawArrays(GL_TRIANGLES, 0, 6);

    m_knownContent.set(outputTexture, content);
    autoClearTextureOnError.release();
    return true;
}

void GraphicsContextGLCVCocoa::invalidateKnownTextureContent(GCGLuint texture)
{
    m_knownContent.remove(texture);
}

}

#endif
