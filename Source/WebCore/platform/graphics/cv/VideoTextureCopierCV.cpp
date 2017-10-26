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
#include "VideoTextureCopierCV.h"

#include "FourCC.h"
#include "Logging.h"
#include "TextureCacheCV.h"
#include <IOSurfaceSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

#if PLATFORM(IOS)
#include <OpenGLES/ES3/glext.h>
#endif

#include "CoreVideoSoftLink.h"

namespace WebCore {

#if USE(IOSURFACE)
enum class PixelRange {
    Unknown,
    Video,
    Full,
};

enum class TransferFunction {
    Unknown,
    kITU_R_709_2,
    kITU_R_601_4,
    kSMPTE_240M_1995,
    kDCI_P3,
    kP3_D65,
    kITU_R_2020,
};

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101300
enum {
    kCVPixelFormatType_ARGB2101010LEPacked = 'l10r',
    kCVPixelFormatType_420YpCbCr10BiPlanarVideoRange = 'x420',
    kCVPixelFormatType_422YpCbCr10BiPlanarVideoRange = 'x422',
    kCVPixelFormatType_444YpCbCr10BiPlanarVideoRange = 'x444',
    kCVPixelFormatType_420YpCbCr10BiPlanarFullRange  = 'xf20',
    kCVPixelFormatType_422YpCbCr10BiPlanarFullRange  = 'xf22',
    kCVPixelFormatType_444YpCbCr10BiPlanarFullRange  = 'xf44',
};
#endif

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
        return PixelRange::Video;
    case kCVPixelFormatType_420YpCbCr8PlanarFullRange:
    case kCVPixelFormatType_420YpCbCr8BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr8FullRange:
    case kCVPixelFormatType_ARGB2101010LEPacked:
    case kCVPixelFormatType_420YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_422YpCbCr10BiPlanarFullRange:
    case kCVPixelFormatType_444YpCbCr10BiPlanarFullRange:
        return PixelRange::Full;
    default:
        return PixelRange::Unknown;
    }
}

static TransferFunction transferFunctionFromString(CFStringRef string)
{
    if (string == kCVImageBufferYCbCrMatrix_ITU_R_709_2)
        return TransferFunction::kITU_R_709_2;
    if (string == kCVImageBufferYCbCrMatrix_ITU_R_601_4)
        return TransferFunction::kITU_R_601_4;
    if (string == kCVImageBufferYCbCrMatrix_SMPTE_240M_1995)
        return TransferFunction::kSMPTE_240M_1995;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_DCI_P3() && string == kCVImageBufferYCbCrMatrix_DCI_P3)
        return TransferFunction::kDCI_P3;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_P3_D65() && string == kCVImageBufferYCbCrMatrix_P3_D65)
        return TransferFunction::kP3_D65;
    if (canLoad_CoreVideo_kCVImageBufferYCbCrMatrix_ITU_R_2020() && string == kCVImageBufferYCbCrMatrix_ITU_R_2020)
        return TransferFunction::kITU_R_2020;
    return TransferFunction::Unknown;
}

static const Vector<GLfloat> YCbCrToRGBMatrixForRangeAndTransferFunction(PixelRange range, TransferFunction transferFunction)
{
    using MapKey = std::pair<PixelRange, TransferFunction>;
    using MatrixMap = std::map<MapKey, Vector<GLfloat>>;

    static NeverDestroyed<MatrixMap> matrices;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        // Matrices are derived from the components in the ITU R.601 rev 4 specification
        // https://www.itu.int/rec/R-REC-BT.601
        matrices.get().emplace(MapKey(PixelRange::Video, TransferFunction::kITU_R_601_4), Vector<GLfloat>({
            1.164383562f,  0.0f,           1.596026786f,
            1.164383562f, -0.3917622901f, -0.8129676472f,
            1.164383562f,  2.017232143f,   0.0f,
        }));
        matrices.get().emplace(MapKey({PixelRange::Full, TransferFunction::kITU_R_601_4}), Vector<GLfloat>({
            1.000000000f,  0.0f,           1.4075196850f,
            1.000000000f, -0.3454911535f, -0.7169478464f,
            1.000000000f,  1.7789763780f,  0.0f,
        }));
        // Matrices are derived from the components in the ITU R.709 rev 2 specification
        // https://www.itu.int/rec/R-REC-BT.709-2-199510-S
        matrices.get().emplace(MapKey({PixelRange::Video, TransferFunction::kITU_R_709_2}), Vector<GLfloat>({
            1.164383562f,  0.0f,           1.792741071f,
            1.164383562f, -0.2132486143f, -0.5329093286f,
            1.164383562f,  2.112401786f,   0.0f,
        }));
        matrices.get().emplace(MapKey({PixelRange::Full, TransferFunction::kITU_R_709_2}), Vector<GLfloat>({
            1.000000000f,  0.0f,           1.5810000000f,
            1.000000000f, -0.1880617701f, -0.4699672819f,
            1.000000000f,  1.8629055118f,  0.0f,
        }));
    });

    // We should never be asked to handle a Pixel Format whose range value is unknown.
    ASSERT(range != PixelRange::Unknown);
    if (range == PixelRange::Unknown)
        range = PixelRange::Full;

    auto iterator = matrices.get().find({range, transferFunction});

    // Assume unknown transfer functions are r.601:
    if (iterator == matrices.get().end())
        iterator = matrices.get().find({range, TransferFunction::kITU_R_601_4});

    ASSERT(iterator != matrices.get().end());
    return iterator->second;
}
#endif // USE(IOSURFACE)

VideoTextureCopierCV::VideoTextureCopierCV(GraphicsContext3D& context)
    : m_context(context)
    , m_framebuffer(context.createFramebuffer())
{
}

VideoTextureCopierCV::~VideoTextureCopierCV()
{
    if (m_vertexBuffer)
        m_context->deleteProgram(m_vertexBuffer);
    if (m_program)
        m_context->deleteProgram(m_program);
    if (m_yuvVertexBuffer)
        m_context->deleteProgram(m_yuvVertexBuffer);
    if (m_yuvProgram)
        m_context->deleteProgram(m_yuvProgram);
    m_context->deleteFramebuffer(m_framebuffer);
}

#if !LOG_DISABLED
using StringMap = std::map<uint32_t, const char*, std::less<uint32_t>, FastAllocator<std::pair<const uint32_t, const char*>>>;
#define STRINGIFY_PAIR(e) e, #e
static StringMap& enumToStringMap()
{
    static NeverDestroyed<StringMap> map;
    if (map.get().empty()) {
        StringMap stringMap;
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_R8));
        map.get().emplace(STRINGIFY_PAIR(GL_R16F));
        map.get().emplace(STRINGIFY_PAIR(GL_R32F));
        map.get().emplace(STRINGIFY_PAIR(GL_R8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R8I));
        map.get().emplace(STRINGIFY_PAIR(GL_R16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R16I));
        map.get().emplace(STRINGIFY_PAIR(GL_R32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_R32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RG32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8));
        map.get().emplace(STRINGIFY_PAIR(GL_SRGB8_ALPHA8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA4));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT16));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT24));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT32F));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH24_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH32F_STENCIL8));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_LUMINANCE));
        map.get().emplace(STRINGIFY_PAIR(GL_ALPHA));
        map.get().emplace(STRINGIFY_PAIR(GL_RED));
        map.get().emplace(STRINGIFY_PAIR(GL_RG_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_STENCIL));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_6_5));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_4_4_4_4));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT_5_5_5_1));
        map.get().emplace(STRINGIFY_PAIR(GL_BYTE));
        map.get().emplace(STRINGIFY_PAIR(GL_HALF_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_SHORT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_INT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_2_10_10_10_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_24_8));
        map.get().emplace(STRINGIFY_PAIR(GL_FLOAT_32_UNSIGNED_INT_24_8_REV));

#if PLATFORM(IOS)
        map.get().emplace(STRINGIFY_PAIR(GL_RED_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_RG8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB565));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_R11F_G11F_B10F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB9_E5));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8_SNORM));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32F));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA8I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB10_A2UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA16I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32I));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA32UI));
        map.get().emplace(STRINGIFY_PAIR(GL_RGB5_A1));
        map.get().emplace(STRINGIFY_PAIR(GL_RG));
        map.get().emplace(STRINGIFY_PAIR(GL_RGBA_INTEGER));
        map.get().emplace(STRINGIFY_PAIR(GL_DEPTH_COMPONENT));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_10F_11F_11F_REV));
        map.get().emplace(STRINGIFY_PAIR(GL_UNSIGNED_INT_5_9_9_9_REV));
#endif
    }
    return map.get();
}

#endif

bool VideoTextureCopierCV::initializeContextObjects()
{
    StringBuilder vertexShaderSource;
    vertexShaderSource.appendLiteral("attribute vec4 a_position;\n");
    vertexShaderSource.appendLiteral("uniform int u_flipY;\n");
    vertexShaderSource.appendLiteral("varying vec2 v_texturePosition;\n");
    vertexShaderSource.appendLiteral("void main() {\n");
    vertexShaderSource.appendLiteral("    v_texturePosition = vec2((a_position.x + 1.0) / 2.0, (a_position.y + 1.0) / 2.0);\n");
    vertexShaderSource.appendLiteral("    if (u_flipY == 1) {\n");
    vertexShaderSource.appendLiteral("        v_texturePosition.y = 1.0 - v_texturePosition.y;\n");
    vertexShaderSource.appendLiteral("    }\n");
    vertexShaderSource.appendLiteral("    gl_Position = a_position;\n");
    vertexShaderSource.appendLiteral("}\n");

    Platform3DObject vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    m_context->shaderSource(vertexShader, vertexShaderSource.toString());
    m_context->compileShaderDirect(vertexShader);

    GC3Dint value = 0;
    m_context->getShaderiv(vertexShader, GraphicsContext3D::COMPILE_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Vertex shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        return false;
    }

    StringBuilder fragmentShaderSource;

#if PLATFORM(IOS)
    fragmentShaderSource.appendLiteral("precision mediump float;\n");
    fragmentShaderSource.appendLiteral("uniform sampler2D u_texture;\n");
#else
    fragmentShaderSource.appendLiteral("uniform sampler2DRect u_texture;\n");
#endif
    fragmentShaderSource.appendLiteral("varying vec2 v_texturePosition;\n");
    fragmentShaderSource.appendLiteral("uniform int u_premultiply;\n");
    fragmentShaderSource.appendLiteral("uniform vec2 u_textureDimensions;\n");
    fragmentShaderSource.appendLiteral("void main() {\n");
    fragmentShaderSource.appendLiteral("    vec2 texPos = vec2(v_texturePosition.x * u_textureDimensions.x, v_texturePosition.y * u_textureDimensions.y);\n");
#if PLATFORM(IOS)
    fragmentShaderSource.appendLiteral("    vec4 color = texture2D(u_texture, texPos);\n");
#else
    fragmentShaderSource.appendLiteral("    vec4 color = texture2DRect(u_texture, texPos);\n");
#endif
    fragmentShaderSource.appendLiteral("    if (u_premultiply == 1) {\n");
    fragmentShaderSource.appendLiteral("        gl_FragColor = vec4(color.r * color.a, color.g * color.a, color.b * color.a, color.a);\n");
    fragmentShaderSource.appendLiteral("    } else {\n");
    fragmentShaderSource.appendLiteral("        gl_FragColor = color;\n");
    fragmentShaderSource.appendLiteral("    }\n");
    fragmentShaderSource.appendLiteral("}\n");

    Platform3DObject fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(fragmentShader, fragmentShaderSource.toString());
    m_context->compileShaderDirect(fragmentShader);

    m_context->getShaderiv(fragmentShader, GraphicsContext3D::COMPILE_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Fragment shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        return false;
    }

    m_program = m_context->createProgram();
    m_context->attachShader(m_program, vertexShader);
    m_context->attachShader(m_program, fragmentShader);
    m_context->linkProgram(m_program);

    m_context->getProgramiv(m_program, GraphicsContext3D::LINK_STATUS, &value);
    if (!value) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Program failed to link.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_context->deleteProgram(m_program);
        m_program = 0;
        return false;
    }

    m_textureUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_texture"));
    m_textureDimensionsUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_textureDimensions"));
    m_flipYUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_flipY"));
    m_premultiplyUniformLocation = m_context->getUniformLocation(m_program, ASCIILiteral("u_premultiply"));
    m_positionAttributeLocation = m_context->getAttribLocationDirect(m_program, ASCIILiteral("a_position"));

    m_context->detachShader(m_program, vertexShader);
    m_context->detachShader(m_program, fragmentShader);
    m_context->deleteShader(vertexShader);
    m_context->deleteShader(fragmentShader);

    LOG(WebGL, "Uniform and Attribute locations: u_texture = %d, u_textureDimensions = %d, u_flipY = %d, u_premultiply = %d, a_position = %d", m_textureUniformLocation, m_textureDimensionsUniformLocation, m_flipYUniformLocation, m_premultiplyUniformLocation, m_positionAttributeLocation);
    m_context->enableVertexAttribArray(m_positionAttributeLocation);

    m_vertexBuffer = m_context->createBuffer();
    float vertices[12] = { -1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1 };

    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexBuffer);
    m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(float) * 12, vertices, GraphicsContext3D::STATIC_DRAW);

    return true;
}

bool VideoTextureCopierCV::initializeUVContextObjects()
{
    String vertexShaderSource = ASCIILiteral(
        "attribute vec2 a_position;\n"
        "uniform vec2 u_yTextureSize;\n"
        "uniform vec2 u_uvTextureSize;\n"
        "uniform int u_flipY;\n"
        "varying vec2 v_yTextureCoordinate;\n"
        "varying vec2 v_uvTextureCoordinate;\n"
        "void main() {\n"
        "   gl_Position = vec4(a_position, 0, 1.0);\n"
        "   vec2 normalizedPosition = a_position * .5 + .5;\n"
        "   if (u_flipY == 1) {\n"
        "       normalizedPosition.y = 1.0 - normalizedPosition.y;\n"
        "   }\n"
#if PLATFORM(IOS)
        "   v_yTextureCoordinate = normalizedPosition;\n"
        "   v_uvTextureCoordinate = normalizedPosition;\n"
#else
        "   v_yTextureCoordinate = normalizedPosition * u_yTextureSize;\n"
        "   v_uvTextureCoordinate = normalizedPosition * u_uvTextureSize;\n"
#endif
        "}\n"
    );

    Platform3DObject vertexShader = m_context->createShader(GraphicsContext3D::VERTEX_SHADER);
    m_context->shaderSource(vertexShader, vertexShaderSource);
    m_context->compileShaderDirect(vertexShader);

    GC3Dint status = 0;
    m_context->getShaderiv(vertexShader, GraphicsContext3D::COMPILE_STATUS, &status);
    if (!status) {
        LOG(WebGL, "VideoTextureCopierCV::initializeUVContextObjects(%p) - Vertex shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        return false;
    }

    String fragmentShaderSource = ASCIILiteral(
#if PLATFORM(IOS)
        "precision mediump float;\n"
        "#define SAMPLERTYPE sampler2D\n"
        "#define TEXTUREFUNC texture2D\n"
#else
        "#define SAMPLERTYPE sampler2DRect\n"
        "#define TEXTUREFUNC texture2DRect\n"
#endif
        "uniform SAMPLERTYPE u_yTexture;\n"
        "uniform SAMPLERTYPE u_uvTexture;\n"
        "uniform mat3 u_colorMatrix;\n"
        "varying vec2 v_yTextureCoordinate;\n"
        "varying vec2 v_uvTextureCoordinate;\n"
        "void main() {\n"
        "    vec3 yuv;\n"
        "    yuv.r = TEXTUREFUNC(u_yTexture, v_yTextureCoordinate).r;\n"
        "    yuv.gb = TEXTUREFUNC(u_uvTexture, v_uvTextureCoordinate).rg - vec2(0.5, 0.5);\n"
        "    gl_FragColor = vec4(yuv * u_colorMatrix, 1);\n"
        "}\n"
    );

    Platform3DObject fragmentShader = m_context->createShader(GraphicsContext3D::FRAGMENT_SHADER);
    m_context->shaderSource(fragmentShader, fragmentShaderSource);
    m_context->compileShaderDirect(fragmentShader);

    m_context->getShaderiv(fragmentShader, GraphicsContext3D::COMPILE_STATUS, &status);
    if (!status) {
        LOG(WebGL, "VideoTextureCopierCV::initializeUVContextObjects(%p) - Fragment shader failed to compile.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        return false;
    }

    m_yuvProgram = m_context->createProgram();
    m_context->attachShader(m_yuvProgram, vertexShader);
    m_context->attachShader(m_yuvProgram, fragmentShader);
    m_context->linkProgram(m_yuvProgram);

    m_context->getProgramiv(m_yuvProgram, GraphicsContext3D::LINK_STATUS, &status);
    if (!status) {
        LOG(WebGL, "VideoTextureCopierCV::initializeUVContextObjects(%p) - Program failed to link.", this);
        m_context->deleteShader(vertexShader);
        m_context->deleteShader(fragmentShader);
        m_context->deleteProgram(m_yuvProgram);
        m_yuvProgram = 0;
        return false;
    }

    m_yTextureUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_yTexture"));
    m_uvTextureUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_uvTexture"));
    m_colorMatrixUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_colorMatrix"));
    m_yuvFlipYUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_flipY"));
    m_yTextureSizeUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_yTextureSize"));
    m_uvTextureSizeUniformLocation = m_context->getUniformLocation(m_yuvProgram, ASCIILiteral("u_uvTextureSize"));
    m_yuvPositionAttributeLocation = m_context->getAttribLocationDirect(m_yuvProgram, ASCIILiteral("a_position"));

    m_context->detachShader(m_yuvProgram, vertexShader);
    m_context->detachShader(m_yuvProgram, fragmentShader);
    m_context->deleteShader(vertexShader);
    m_context->deleteShader(fragmentShader);

    m_yuvVertexBuffer = m_context->createBuffer();
    float vertices[12] = { -1, -1, 1, -1, 1, 1, 1, 1, -1, 1, -1, -1 };

    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_yuvVertexBuffer);
    m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW);

    return true;
}

bool VideoTextureCopierCV::copyImageToPlatformTexture(CVPixelBufferRef image, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    if (!m_textureCache) {
        m_textureCache = TextureCacheCV::create(m_context);
        if (!m_textureCache)
            return false;
    }

    if (auto texture = m_textureCache->textureFromImage(image, outputTarget, level, internalFormat, format, type))
        return copyVideoTextureToPlatformTexture(texture.get(), width, height, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);

#if USE(IOSURFACE)
    // FIXME: This currently only supports '420v' and '420f' pixel formats. Investigate supporting more pixel formats.
    OSType pixelFormat = CVPixelBufferGetPixelFormatType(image);
    if (pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange && pixelFormat != kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Asked to copy an unsupported pixel format ('%s').", this, FourCC(pixelFormat).toString().utf8().data());
        return false;
    }

    IOSurfaceRef surface = CVPixelBufferGetIOSurface(image);
    if (!surface)
        return false;

    auto newSurfaceSeed = IOSurfaceGetSeed(surface);
    auto newTextureSeed = m_context->textureSeed(outputTexture);
    if (flipY == m_lastFlipY
        && surface == m_lastSurface 
        && newSurfaceSeed == m_lastSurfaceSeed
        && lastTextureSeed(outputTexture) == newTextureSeed) {
        // If the texture hasn't been modified since the last time we copied to it, and the
        // image hasn't been modified since the last time it was copied, this is a no-op.
        return true;
    }

    GC3DStateSaver stateSaver(m_context.get());

    if (!m_yuvProgram) {
        if (!initializeUVContextObjects()) {
            LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to initialize OpenGL context objects.", this);
            return false;
        }
    }

    stateSaver.saveVertexAttribState(m_yuvPositionAttributeLocation);

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);

    // Allocate memory for the output texture.
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, outputTexture);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texImage2DDirect(GraphicsContext3D::TEXTURE_2D, level, internalFormat, width, height, 0, format, type, nullptr);

    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, outputTexture, level);
    GC3Denum status = m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to create framebuffer for outputTexture.", this);
        return false;
    }

    m_context->useProgram(m_yuvProgram);
    m_context->viewport(0, 0, width, height);

    // Bind and set up the textures for the video source.
    auto yPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 0);
    auto yPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 0);
    auto uvPlaneWidth = IOSurfaceGetWidthOfPlane(surface, 1);
    auto uvPlaneHeight = IOSurfaceGetHeightOfPlane(surface, 1);

#if PLATFORM(IOS)
    GC3Denum videoTextureTarget = GraphicsContext3D::TEXTURE_2D;
#else
    GC3Denum videoTextureTarget = GL_TEXTURE_RECTANGLE_ARB;
#endif
    auto uvTexture = m_context->createTexture();
    m_context->activeTexture(GraphicsContext3D::TEXTURE1);
    m_context->bindTexture(videoTextureTarget, uvTexture);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    if (!m_context->texImageIOSurface2D(videoTextureTarget, GL_RG, uvPlaneWidth, uvPlaneHeight, GL_RG, GL_UNSIGNED_BYTE, surface, 1)) {
        m_context->deleteTexture(uvTexture);
        return false;
    }

    auto yTexture = m_context->createTexture();
    m_context->activeTexture(GraphicsContext3D::TEXTURE0);
    m_context->bindTexture(videoTextureTarget, yTexture);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    if (!m_context->texImageIOSurface2D(videoTextureTarget, GL_LUMINANCE, yPlaneWidth, yPlaneHeight, GL_LUMINANCE, GL_UNSIGNED_BYTE, surface, 0)) {
        m_context->deleteTexture(yTexture);
        m_context->deleteTexture(uvTexture);
        return false;
    }

    // Configure the drawing parameters.
    m_context->uniform1i(m_yTextureUniformLocation, 0);
    m_context->uniform1i(m_uvTextureUniformLocation, 1);
    m_context->uniform1i(m_yuvFlipYUniformLocation, flipY);
    m_context->uniform2f(m_yTextureSizeUniformLocation, yPlaneWidth, yPlaneHeight);
    m_context->uniform2f(m_uvTextureSizeUniformLocation, uvPlaneWidth, uvPlaneHeight);

    auto range = pixelRangeFromPixelFormat(pixelFormat);
    auto transferFunction = transferFunctionFromString((CFStringRef)CVBufferGetAttachment(image, kCVImageBufferYCbCrMatrixKey, nil));
    auto& colorMatrix = YCbCrToRGBMatrixForRangeAndTransferFunction(range, transferFunction);
    m_context->uniformMatrix3fv(m_colorMatrixUniformLocation, 1, GL_FALSE, colorMatrix.data());

    // Do the actual drawing.
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_yuvVertexBuffer);
    m_context->enableVertexAttribArray(m_yuvPositionAttributeLocation);
    m_context->vertexAttribPointer(m_yuvPositionAttributeLocation, 2, GraphicsContext3D::FLOAT, false, 0, 0);
    m_context->drawArrays(GraphicsContext3D::TRIANGLES, 0, 6);

    // Clean-up.
    m_context->deleteTexture(yTexture);
    m_context->deleteTexture(uvTexture);
    m_context->bindTexture(videoTextureTarget, 0);

    m_lastSurface = surface;
    m_lastSurfaceSeed = newSurfaceSeed;
    m_lastTextureSeed.set(outputTexture, newTextureSeed);
    m_lastFlipY = flipY;

    return true;
#else
    return false;
#endif // USE(IOSURFACE)
}

bool VideoTextureCopierCV::copyVideoTextureToPlatformTexture(TextureType inputVideoTexture, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    if (!inputVideoTexture)
        return false;

    GLfloat lowerLeft[2] = { 0, 0 };
    GLfloat lowerRight[2] = { 0, 0 };
    GLfloat upperRight[2] = { 0, 0 };
    GLfloat upperLeft[2] = { 0, 0 };
#if PLATFORM(IOS)
    Platform3DObject videoTextureName = CVOpenGLESTextureGetName(inputVideoTexture);
    GC3Denum videoTextureTarget = CVOpenGLESTextureGetTarget(inputVideoTexture);
    CVOpenGLESTextureGetCleanTexCoords(inputVideoTexture, lowerLeft, lowerRight, upperRight, upperLeft);
#else
    Platform3DObject videoTextureName = CVOpenGLTextureGetName(inputVideoTexture);
    GC3Denum videoTextureTarget = CVOpenGLTextureGetTarget(inputVideoTexture);
    CVOpenGLTextureGetCleanTexCoords(inputVideoTexture, lowerLeft, lowerRight, upperRight, upperLeft);
#endif

    if (lowerLeft[1] < upperRight[1])
        flipY = !flipY;

    return copyVideoTextureToPlatformTexture(videoTextureName, videoTextureTarget, width, height, outputTexture, outputTarget, level, internalFormat, format, type, premultiplyAlpha, flipY);
}

bool VideoTextureCopierCV::copyVideoTextureToPlatformTexture(Platform3DObject videoTextureName, GC3Denum videoTextureTarget, size_t width, size_t height, Platform3DObject outputTexture, GC3Denum outputTarget, GC3Dint level, GC3Denum internalFormat, GC3Denum format, GC3Denum type, bool premultiplyAlpha, bool flipY)
{
    LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - internalFormat: %s, format: %s, type: %s flipY: %s, premultiplyAlpha: %s", this, enumToStringMap()[internalFormat], enumToStringMap()[format], enumToStringMap()[type], flipY ? "true" : "false", premultiplyAlpha ? "true" : "false");

    GC3DStateSaver stateSaver(m_context.get());

    if (!m_program) {
        if (!initializeContextObjects()) {
            LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to initialize OpenGL context objects.", this);
            return false;
        }
    }

    stateSaver.saveVertexAttribState(m_positionAttributeLocation);

    m_context->bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);
    
    // Allocate memory for the output texture.
    m_context->bindTexture(GraphicsContext3D::TEXTURE_2D, outputTexture);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(GraphicsContext3D::TEXTURE_2D, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texImage2DDirect(GraphicsContext3D::TEXTURE_2D, level, internalFormat, width, height, 0, format, type, nullptr);

    m_context->framebufferTexture2D(GraphicsContext3D::FRAMEBUFFER, GraphicsContext3D::COLOR_ATTACHMENT0, GraphicsContext3D::TEXTURE_2D, outputTexture, level);
    GC3Denum status = m_context->checkFramebufferStatus(GraphicsContext3D::FRAMEBUFFER);
    if (status != GraphicsContext3D::FRAMEBUFFER_COMPLETE) {
        LOG(WebGL, "VideoTextureCopierCV::copyVideoTextureToPlatformTexture(%p) - Unable to create framebuffer for outputTexture.", this);
        return false;
    }

    m_context->useProgram(m_program);
    m_context->viewport(0, 0, width, height);

    // Bind and set up the texture for the video source.
    m_context->activeTexture(GraphicsContext3D::TEXTURE0);
    m_context->bindTexture(videoTextureTarget, videoTextureName);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MAG_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_MIN_FILTER, GraphicsContext3D::LINEAR);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_S, GraphicsContext3D::CLAMP_TO_EDGE);
    m_context->texParameteri(videoTextureTarget, GraphicsContext3D::TEXTURE_WRAP_T, GraphicsContext3D::CLAMP_TO_EDGE);

    // Configure the drawing parameters.
    m_context->uniform1i(m_textureUniformLocation, 0);
#if PLATFORM(IOS)
    m_context->uniform2f(m_textureDimensionsUniformLocation, 1, 1);
#else
    m_context->uniform2f(m_textureDimensionsUniformLocation, width, height);
#endif

    m_context->uniform1i(m_flipYUniformLocation, flipY);
    m_context->uniform1i(m_premultiplyUniformLocation, premultiplyAlpha);

    // Do the actual drawing.
    m_context->enableVertexAttribArray(m_positionAttributeLocation);
    m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_vertexBuffer);
    m_context->vertexAttribPointer(m_positionAttributeLocation, 2, GraphicsContext3D::FLOAT, false, 0, 0);
    m_context->drawArrays(GraphicsContext3D::TRIANGLES, 0, 6);

    // Clean-up.
    m_context->bindTexture(videoTextureTarget, 0);
    m_context->bindTexture(outputTarget, outputTexture);

    return true;
}

VideoTextureCopierCV::GC3DStateSaver::GC3DStateSaver(GraphicsContext3D& context)
    : m_context(context)
{
    m_context.getIntegerv(GraphicsContext3D::TEXTURE_BINDING_2D, &m_texture);
    m_context.getIntegerv(GraphicsContext3D::FRAMEBUFFER_BINDING, &m_framebuffer);
    m_context.getIntegerv(GraphicsContext3D::CURRENT_PROGRAM, &m_program);
    m_context.getIntegerv(GraphicsContext3D::ARRAY_BUFFER_BINDING, &m_arrayBuffer);
    m_context.getIntegerv(GraphicsContext3D::VIEWPORT, m_viewport);

}

VideoTextureCopierCV::GC3DStateSaver::~GC3DStateSaver()
{
    if (m_vertexAttribEnabled)
        m_context.enableVertexAttribArray(m_vertexAttribIndex);
    else
        m_context.disableVertexAttribArray(m_vertexAttribIndex);

    m_context.bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_arrayBuffer);
    m_context.vertexAttribPointer(m_vertexAttribIndex, m_vertexAttribSize, m_vertexAttribType, m_vertexAttribNormalized, m_vertexAttribStride, m_vertexAttribPointer);

    m_context.bindTexture(GraphicsContext3D::TEXTURE_BINDING_2D, m_texture);
    m_context.bindFramebuffer(GraphicsContext3D::FRAMEBUFFER, m_framebuffer);
    m_context.useProgram(m_program);
    m_context.viewport(m_viewport[0], m_viewport[1], m_viewport[2], m_viewport[3]);
}

void VideoTextureCopierCV::GC3DStateSaver::saveVertexAttribState(GC3Duint index)
{
    m_vertexAttribIndex = index;
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_ENABLED, &m_vertexAttribEnabled);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_SIZE, &m_vertexAttribSize);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_TYPE, &m_vertexAttribType);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_NORMALIZED, &m_vertexAttribNormalized);
    m_context.getVertexAttribiv(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_STRIDE, &m_vertexAttribStride);
    m_vertexAttribPointer = m_context.getVertexAttribOffset(index, GraphicsContext3D::VERTEX_ATTRIB_ARRAY_POINTER);
}

}
