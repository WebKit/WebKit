/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 Copyright (C) 2012 Igalia S.L.
 Copyright (C) 2012 Adobe Systems Incorporated

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TextureMapper.h"

#if USE(TEXTURE_MAPPER)

#include "BitmapTexture.h"
#include "FilterOperations.h"
#include "FloatQuad.h"
#include "FloatRoundedRect.h"
#include "GLContext.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "LengthFunctions.h"
#include "TextureMapperFlags.h"
#include "TextureMapperShaderProgram.h"
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/SetForScope.h>

#if USE(CAIRO)
#include "CairoUtilities.h"
#include "RefPtrCairo.h"
#include <cairo.h>
#include <wtf/text/CString.h>
#endif

namespace WebCore {

class TextureMapperGLData {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit TextureMapperGLData(void*);
    ~TextureMapperGLData();

    void initializeStencil();
    GLuint getStaticVBO(GLenum target, GLsizeiptr, const void* data);
    GLuint getVAO();
    Ref<TextureMapperShaderProgram> getShaderProgram(TextureMapperShaderProgram::Options);

    TransformationMatrix projectionMatrix;
    TextureMapper::FlipY flipY { TextureMapper::FlipY::No };
    GLint previousProgram { 0 };
    GLint previousVAO { 0 };
    GLint targetFrameBuffer { 0 };
    bool didModifyStencil { false };
    GLint previousScissorState { 0 };
    GLint previousDepthState { 0 };
    GLint viewport[4] { 0, };
    GLint previousScissor[4] { 0, };
    double zNear { 0 };
    double zFar { 0 };
    RefPtr<BitmapTexture> currentSurface;
    RefPtr<const FilterOperation> filterOperation;

private:
    class SharedGLData : public RefCounted<SharedGLData> {
    public:
        static Ref<SharedGLData> currentSharedGLData(void* platformContext)
        {
            ASSERT(platformContext);

            auto it = contextDataMap().find(platformContext);
            if (it != contextDataMap().end())
                return *it->value;

            Ref<SharedGLData> data = adoptRef(*new SharedGLData);
            contextDataMap().add(platformContext, data.ptr());
            return data;
        }

        ~SharedGLData()
        {
            ASSERT(std::any_of(contextDataMap().begin(), contextDataMap().end(),
                [this](auto& entry) { return entry.value == this; }));
            contextDataMap().removeIf([this](auto& entry) { return entry.value == this; });
        }

    private:
        friend class TextureMapperGLData;

        using GLContextDataMap = HashMap<void*, SharedGLData*>;
        static GLContextDataMap& contextDataMap()
        {
            static NeverDestroyed<GLContextDataMap> map;
            return map;
        }

        SharedGLData() = default;

        HashMap<unsigned, RefPtr<TextureMapperShaderProgram>> m_programs;
    };

    Ref<SharedGLData> m_sharedGLData;
    HashMap<const void*, GLuint> m_vbos;
    GLuint m_vao { 0 };
};

TextureMapperGLData::TextureMapperGLData(void* platformContext)
    : m_sharedGLData(SharedGLData::currentSharedGLData(platformContext))
{
}

TextureMapperGLData::~TextureMapperGLData()
{
    for (auto& entry : m_vbos)
        glDeleteBuffers(1, &entry.value);
}

void TextureMapperGLData::initializeStencil()
{
    if (currentSurface) {
        static_cast<BitmapTexture*>(currentSurface.get())->initializeStencil();
        return;
    }

    if (didModifyStencil)
        return;

    glClearStencil(0);
    glClear(GL_STENCIL_BUFFER_BIT);
    didModifyStencil = true;
}

GLuint TextureMapperGLData::getStaticVBO(GLenum target, GLsizeiptr size, const void* data)
{
    auto addResult = m_vbos.ensure(data,
        [target, size, data] {
            GLuint vbo = 0;
            glGenBuffers(1, &vbo);
            glBindBuffer(target, vbo);
            glBufferData(target, size, data, GL_STATIC_DRAW);
            return vbo;
        });
    return addResult.iterator->value;
}

GLuint TextureMapperGLData::getVAO()
{
    return m_vao;
}

Ref<TextureMapperShaderProgram> TextureMapperGLData::getShaderProgram(TextureMapperShaderProgram::Options options)
{
    ASSERT(!options.isEmpty());
    auto addResult = m_sharedGLData->m_programs.ensure(options.toRaw(), [options] {
        return TextureMapperShaderProgram::create(options);
    });
    return *addResult.iterator->value;
}

std::unique_ptr<TextureMapper> TextureMapper::create()
{
    return makeUnique<TextureMapper>();
}

TextureMapper::TextureMapper()
    : m_data(new TextureMapperGLData(GLContext::current()->platformContext()))
{
}

RefPtr<BitmapTexture> TextureMapper::acquireTextureFromPool(const IntSize& size, OptionSet<BitmapTexture::Flags> flags)
{
    return m_texturePool.acquireTexture(size, flags);
}

#if USE(GRAPHICS_LAYER_WC)
void TextureMapper::releaseUnusedTexturesNow()
{
    // GraphicsLayerWC runs TextureMapper in the OpenGL thread of the
    // GPU process that doesn't use RunLoop. RunLoop::Timer doesn't
    // work in the thread.
    m_texturePool.releaseUnusedTexturesTimerFired();
}
#endif

ClipStack& TextureMapper::clipStack()
{
    return data().currentSurface ? data().currentSurface->clipStack() : m_clipStack;
}

void TextureMapper::beginPainting(FlipY flipY, BitmapTexture* surface)
{
    glGetIntegerv(GL_CURRENT_PROGRAM, &data().previousProgram);
    data().previousScissorState = glIsEnabled(GL_SCISSOR_TEST);
    data().previousDepthState = glIsEnabled(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glEnable(GL_SCISSOR_TEST);
    data().didModifyStencil = false;
    glGetIntegerv(GL_VIEWPORT, data().viewport);
    glGetIntegerv(GL_SCISSOR_BOX, data().previousScissor);
    m_clipStack.reset(IntRect(0, 0, data().viewport[2], data().viewport[3]), flipY == FlipY::Yes ? ClipStack::YAxisMode::Default : ClipStack::YAxisMode::Inverted);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &data().targetFrameBuffer);
    data().flipY = flipY;
    bindSurface(surface);
}

void TextureMapper::endPainting()
{
    glBindFramebuffer(GL_FRAMEBUFFER, data().targetFrameBuffer);
    if (data().didModifyStencil) {
        glClearStencil(1);
        glClear(GL_STENCIL_BUFFER_BIT);
    }

    glUseProgram(data().previousProgram);

    glScissor(data().previousScissor[0], data().previousScissor[1], data().previousScissor[2], data().previousScissor[3]);
    if (data().previousScissorState)
        glEnable(GL_SCISSOR_TEST);
    else
        glDisable(GL_SCISSOR_TEST);

    if (data().previousDepthState)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}

void TextureMapper::drawBorder(const Color& color, float width, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix)
{
    if (clipStack().isCurrentScissorBoxEmpty())
        return;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::SolidColor);
    glUseProgram(program->programID());

    auto [r, g, b, a] = premultiplied(color.toColorTypeLossy<SRGBA<float>>()).resolved();
    glUniform4f(program->colorLocation(), r, g, b, a);
    glLineWidth(width);

    draw(targetRect, modelViewMatrix, program.get(), GL_LINE_LOOP, !color.isOpaque() ? TextureMapperFlags::ShouldBlend : OptionSet<TextureMapperFlags> { });
}

// FIXME: drawNumber() should save a number texture-atlas and re-use whenever possible.
void TextureMapper::drawNumber(int number, const Color& color, const FloatPoint& targetPoint, const TransformationMatrix& modelViewMatrix)
{
    int pointSize = 8;

#if USE(CAIRO)
    CString counterString = String::number(number).ascii();
    // cairo_text_extents() requires a cairo_t, so dimensions need to be guesstimated.
    int width = counterString.length() * pointSize * 1.2;
    int height = pointSize * 1.5;

    cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
    cairo_t* cr = cairo_create(surface);

    // Since we won't swap R+B when uploading a texture, paint with the swapped R+B color.
    auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<float>>().resolved();
    cairo_set_source_rgba(cr, b, g, r, a);

    cairo_rectangle(cr, 0, 0, width, height);
    cairo_fill(cr);

    cairo_select_font_face(cr, "Monospace", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
    cairo_set_font_size(cr, pointSize);
    cairo_set_source_rgb(cr, 1, 1, 1);
    cairo_move_to(cr, 2, pointSize);
    cairo_show_text(cr, counterString.data());

    IntSize size(width, height);
    IntRect sourceRect(IntPoint::zero(), size);
    IntRect targetRect(roundedIntPoint(targetPoint), size);

    RefPtr<BitmapTexture> texture = m_texturePool.acquireTexture(size, { BitmapTexture::Flags::SupportsAlpha });
    const unsigned char* bits = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    texture->updateContents(bits, sourceRect, IntPoint::zero(), stride);
    drawTexture(*texture, targetRect, modelViewMatrix, 1.0f, AllEdgesExposed::Yes);

    cairo_surface_destroy(surface);
    cairo_destroy(cr);

#else
    UNUSED_PARAM(number);
    UNUSED_PARAM(pointSize);
    UNUSED_PARAM(targetPoint);
    UNUSED_PARAM(modelViewMatrix);
#endif
}

static TextureMapperShaderProgram::Options optionsForFilterType(FilterOperation::Type type)
{
    switch (type) {
    case FilterOperation::Type::Grayscale:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::GrayscaleFilter };
    case FilterOperation::Type::Sepia:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::SepiaFilter };
    case FilterOperation::Type::Saturate:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::SaturateFilter };
    case FilterOperation::Type::HueRotate:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::HueRotateFilter };
    case FilterOperation::Type::Invert:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::InvertFilter };
    case FilterOperation::Type::Brightness:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::BrightnessFilter };
    case FilterOperation::Type::Contrast:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::ContrastFilter };
    case FilterOperation::Type::Opacity:
        return { TextureMapperShaderProgram::TextureRGB, TextureMapperShaderProgram::OpacityFilter };
    case FilterOperation::Type::DropShadow:
    case FilterOperation::Type::Blur:
    default:
        ASSERT_NOT_REACHED();
        return { };
    }
}

static const float MinBlurRadius = 0.1;

static unsigned blurRadiusToKernelHalfSize(float radius)
{
    return ceilf(radius * 2 + 1);
}

static constexpr float kernelHalfSizeToBlurRadius(unsigned kernelHalfSize)
{
    return (kernelHalfSize - 1) / 2.f;
}

static constexpr unsigned kernelHalfSizeToSimplifiedKernelHalfSize(unsigned kernelHalfSize)
{
    return kernelHalfSize / 2 + 1;
}

// Max kernel size is 21
static constexpr unsigned GaussianKernelMaxHalfSize = 11;

static constexpr unsigned SimplifiedGaussianKernelMaxHalfSize = kernelHalfSizeToSimplifiedKernelHalfSize(GaussianKernelMaxHalfSize);

static constexpr float GaussianBlurMaxRadius = kernelHalfSizeToBlurRadius(GaussianKernelMaxHalfSize);

static inline float gauss(float x, float radius)
{
    return exp(-powf(x / radius, 2) / 2);
}

// returns kernel half size
static int computeGaussianKernel(float radius, std::array<float, SimplifiedGaussianKernelMaxHalfSize>& kernel, std::array<float, SimplifiedGaussianKernelMaxHalfSize>& offset)
{
    unsigned kernelHalfSize = blurRadiusToKernelHalfSize(radius);
    RELEASE_ASSERT(kernelHalfSize <= GaussianKernelMaxHalfSize);

    float fullKernel[GaussianKernelMaxHalfSize];

    fullKernel[0] = 1; // gauss(0, radius);
    float sum = fullKernel[0];
    for (unsigned i = 1; i < kernelHalfSize; ++i) {
        fullKernel[i] = gauss(i, radius);
        sum += 2 * fullKernel[i];
    }

    // Normalize the kernel.
    float scale = 1 / sum;
    for (unsigned i = 0; i < kernelHalfSize; ++i)
        fullKernel[i] *= scale;

    unsigned simplifiedKernelHalfSize = kernelHalfSizeToSimplifiedKernelHalfSize(kernelHalfSize);

    // Simplify the kernel by utilizing linear interpolation during texture sampling
    // full kernel                                                  simplified kernel
    // |  0  |  1  |  2  |  3  |  4  |  5  |    --- simplify -->    |  0  | 1&2 | 3&4 |  5  |
    // (kernelHalfSize = 6)
    kernel[0] = fullKernel[0];
    for (unsigned i = 1; i < simplifiedKernelHalfSize; i ++) {
        unsigned offset1 = 2 * i - 1;
        unsigned offset2 = 2 * i;

        if (offset2 >= kernelHalfSize) {
            // no pair to simplify
            kernel[i] = fullKernel[offset1];
            offset[i] = offset1;
            break;
        }

        kernel[i] = fullKernel[offset1] + fullKernel[offset2];
        offset[i] = (fullKernel[offset1] * offset1 + fullKernel[offset2] * offset2) / kernel[i];
    }

    return simplifiedKernelHalfSize;
}

static void prepareFilterProgram(TextureMapperShaderProgram& program, const FilterOperation& operation)
{
    glUseProgram(program.programID());

    switch (operation.type()) {
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate:
        glUniform1f(program.filterAmountLocation(), static_cast<const BasicColorMatrixFilterOperation&>(operation).amount());
        break;
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
    case FilterOperation::Type::Opacity:
        glUniform1f(program.filterAmountLocation(), static_cast<const BasicComponentTransferFilterOperation&>(operation).amount());
        break;
    case FilterOperation::Type::DropShadow:
    case FilterOperation::Type::Blur:
    default:
        break;
    }
}

static TransformationMatrix colorSpaceMatrixForFlags(OptionSet<TextureMapperFlags> flags)
{
    // The matrix is initially the identity one, which means no color conversion.
    TransformationMatrix matrix;
    if (flags.contains(TextureMapperFlags::ShouldConvertTextureBGRAToRGBA))
        matrix.setMatrix(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    else if (flags.contains(TextureMapperFlags::ShouldConvertTextureARGBToRGBA))
        matrix.setMatrix(0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);

    return matrix;
}

static void prepareRoundedRectClip(TextureMapperShaderProgram& program, const float* rects, const float* transforms, int nRects)
{
    glUseProgram(program.programID());

    glUniform1i(program.roundedRectNumberLocation(), nRects);
    glUniform4fv(program.roundedRectLocation(), 3 * nRects, rects);
    glUniformMatrix4fv(program.roundedRectInverseTransformMatrixLocation(), nRects, false, transforms);
}

void TextureMapper::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, AllEdgesExposed allEdgesExposed)
{
    if (clipStack().isCurrentScissorBoxEmpty())
        return;

    SetForScope filterOperation(data().filterOperation, texture.filterOperation());

    drawTexture(texture.id(), texture.colorConvertFlags() | (texture.isOpaque() ? OptionSet<TextureMapperFlags> { } : TextureMapperFlags::ShouldBlend), targetRect, matrix, opacity, allEdgesExposed);
}

void TextureMapper::drawTexture(GLuint texture, OptionSet<TextureMapperFlags> flags, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed allEdgesExposed)
{
    bool useAntialiasing = allEdgesExposed == AllEdgesExposed::Yes && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options;
    if (opacity < 1)
        options.add(TextureMapperShaderProgram::Opacity);
    if (useAntialiasing) {
        options.add(TextureMapperShaderProgram::Antialiasing);
        flags.add(TextureMapperFlags::ShouldAntialias);
    }
    if (m_wrapMode == WrapMode::Repeat && !GLContext::current()->glExtensions().OES_texture_npot)
        options.add(TextureMapperShaderProgram::ManualRepeat);

    auto filter = data().filterOperation;
    if (filter) {
        options.add(optionsForFilterType(filter->type()));
        if (filter->affectsOpacity())
            flags.add(TextureMapperFlags::ShouldBlend);
    } else
        options.add(TextureMapperShaderProgram::TextureRGB);

    if (useAntialiasing || opacity < 1)
        flags.add(TextureMapperFlags::ShouldBlend);

    if (clipStack().isRoundedRectClipEnabled()) {
        options.add(TextureMapperShaderProgram::RoundedRectClip);
        flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (flags.contains(TextureMapperFlags::ShouldPremultiply))
        options.add(TextureMapperShaderProgram::Premultiply);

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get());

    if (clipStack().isRoundedRectClipEnabled())
        prepareRoundedRectClip(program.get(), clipStack().roundedRectComponents(), clipStack().roundedRectInverseTransformComponents(), clipStack().roundedRectCount());

    drawTexturedQuadWithProgram(program.get(), texture, flags, targetRect, modelViewMatrix, opacity);
}

static void prepareTransformationMatrixWithFlags(TransformationMatrix& patternTransform, OptionSet<TextureMapperFlags> flags)
{
    if (flags.contains(TextureMapperFlags::ShouldRotateTexture90)) {
        patternTransform.rotate(-90);
        patternTransform.translate(-1, 0);
    }
    if (flags.contains(TextureMapperFlags::ShouldRotateTexture180)) {
        patternTransform.rotate(180);
        patternTransform.translate(-1, -1);
    }
    if (flags.contains(TextureMapperFlags::ShouldRotateTexture270)) {
        patternTransform.rotate(-270);
        patternTransform.translate(0, -1);
    }
    if (flags.contains(TextureMapperFlags::ShouldFlipTexture)) {
        patternTransform.flipY();
        patternTransform.translate(0, -1);
    }
}

void TextureMapper::drawTexturePlanarYUV(const std::array<GLuint, 3>& textures, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags> flags, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, std::optional<GLuint> alphaPlane, AllEdgesExposed allEdgesExposed)
{
    bool useAntialiasing = allEdgesExposed == AllEdgesExposed::Yes && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = alphaPlane ? TextureMapperShaderProgram::TextureYUVA : TextureMapperShaderProgram::TextureYUV;
    if (opacity < 1)
        options.add(TextureMapperShaderProgram::Opacity);
    if (useAntialiasing) {
        options.add(TextureMapperShaderProgram::Antialiasing);
        flags.add(TextureMapperFlags::ShouldAntialias);
    }
    if (m_wrapMode == WrapMode::Repeat && !GLContext::current()->glExtensions().OES_texture_npot)
        options.add(TextureMapperShaderProgram::ManualRepeat);

    auto filter = data().filterOperation;
    if (filter) {
        options.add(optionsForFilterType(filter->type()));
        if (filter->affectsOpacity())
            flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (useAntialiasing || opacity < 1)
        flags.add(TextureMapperFlags::ShouldBlend);

    if (clipStack().isRoundedRectClipEnabled()) {
        options.add(TextureMapperShaderProgram::RoundedRectClip);
        flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (flags.contains(TextureMapperFlags::ShouldPremultiply))
        options.add(TextureMapperShaderProgram::Premultiply);

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get());

    if (clipStack().isRoundedRectClipEnabled())
        prepareRoundedRectClip(program.get(), clipStack().roundedRectComponents(), clipStack().roundedRectInverseTransformComponents(), clipStack().roundedRectCount());

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { textures[0], program->samplerYLocation() },
        { textures[1], program->samplerULocation() },
        { textures[2], program->samplerVLocation() }
    };

    if (alphaPlane)
        texturesAndSamplers.append({ *alphaPlane, program->samplerALocation() });

    glUseProgram(program->programID());
    glUniformMatrix4fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, targetRect, modelViewMatrix, opacity);
}

void TextureMapper::drawTextureSemiPlanarYUV(const std::array<GLuint, 2>& textures, bool uvReversed, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags> flags, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed allEdgesExposed)
{
    bool useAntialiasing = allEdgesExposed == AllEdgesExposed::Yes && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = uvReversed ?
        TextureMapperShaderProgram::TextureNV21 : TextureMapperShaderProgram::TextureNV12;
    if (opacity < 1)
        options.add(TextureMapperShaderProgram::Opacity);
    if (useAntialiasing) {
        options.add(TextureMapperShaderProgram::Antialiasing);
        flags.add(TextureMapperFlags::ShouldAntialias);
    }
    if (m_wrapMode == WrapMode::Repeat && !GLContext::current()->glExtensions().OES_texture_npot)
        options.add(TextureMapperShaderProgram::ManualRepeat);

    auto filter = data().filterOperation;
    if (filter) {
        options.add(optionsForFilterType(filter->type()));
        if (filter->affectsOpacity())
            flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (useAntialiasing || opacity < 1)
        flags.add(TextureMapperFlags::ShouldBlend);

    if (clipStack().isRoundedRectClipEnabled()) {
        options.add(TextureMapperShaderProgram::RoundedRectClip);
        flags.add(TextureMapperFlags::ShouldBlend);
    }

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get());

    if (clipStack().isRoundedRectClipEnabled())
        prepareRoundedRectClip(program.get(), clipStack().roundedRectComponents(), clipStack().roundedRectInverseTransformComponents(), clipStack().roundedRectCount());

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { textures[0], program->samplerYLocation() },
        { textures[1], program->samplerULocation() }
    };

    glUseProgram(program->programID());
    glUniformMatrix4fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, targetRect, modelViewMatrix, opacity);
}

void TextureMapper::drawTexturePackedYUV(GLuint texture, const std::array<GLfloat, 16>& yuvToRgbMatrix, OptionSet<TextureMapperFlags> flags, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, AllEdgesExposed allEdgesExposed)
{
    bool useAntialiasing = allEdgesExposed == AllEdgesExposed::Yes && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::TexturePackedYUV;
    if (opacity < 1)
        options.add(TextureMapperShaderProgram::Opacity);
    if (useAntialiasing) {
        options.add(TextureMapperShaderProgram::Antialiasing);
        flags.add(TextureMapperFlags::ShouldAntialias);
    }
    if (m_wrapMode == WrapMode::Repeat && !GLContext::current()->glExtensions().OES_texture_npot)
        options.add(TextureMapperShaderProgram::ManualRepeat);

    auto filter = data().filterOperation;
    if (filter) {
        options.add(optionsForFilterType(filter->type()));
        if (filter->affectsOpacity())
            flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (useAntialiasing || opacity < 1)
        flags.add(TextureMapperFlags::ShouldBlend);

    if (clipStack().isRoundedRectClipEnabled()) {
        options.add(TextureMapperShaderProgram::RoundedRectClip);
        flags.add(TextureMapperFlags::ShouldBlend);
    }

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get());

    if (clipStack().isRoundedRectClipEnabled())
        prepareRoundedRectClip(program.get(), clipStack().roundedRectComponents(), clipStack().roundedRectInverseTransformComponents(), clipStack().roundedRectCount());

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { texture, program->samplerLocation() }
    };

    glUseProgram(program->programID());
    glUniformMatrix4fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, targetRect, modelViewMatrix, opacity);
}

void TextureMapper::drawSolidColor(const FloatRect& rect, const TransformationMatrix& matrix, const Color& color, bool isBlendingAllowed)
{
    OptionSet<TextureMapperFlags> flags;
    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::SolidColor;
    if (!matrix.mapQuad(rect).isRectilinear()) {
        options.add(TextureMapperShaderProgram::Antialiasing);
        flags.add(TextureMapperFlags::ShouldAntialias);
        if (isBlendingAllowed)
            flags.add(TextureMapperFlags::ShouldBlend);
    }

    if (clipStack().isRoundedRectClipEnabled()) {
        options.add(TextureMapperShaderProgram::RoundedRectClip);
        flags.add(TextureMapperFlags::ShouldBlend);
    }

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);
    glUseProgram(program->programID());

    if (clipStack().isRoundedRectClipEnabled())
        prepareRoundedRectClip(program.get(), clipStack().roundedRectComponents(), clipStack().roundedRectInverseTransformComponents(), clipStack().roundedRectCount());

    auto [r, g, b, a] = premultiplied(color.toColorTypeLossy<SRGBA<float>>()).resolved();
    glUniform4f(program->colorLocation(), r, g, b, a);
    if (a < 1 && isBlendingAllowed)
        flags.add(TextureMapperFlags::ShouldBlend);

    draw(rect, matrix, program.get(), GL_TRIANGLE_FAN, flags);
}

void TextureMapper::clearColor(const Color& color)
{
    auto [r, g, b, a] = color.toColorTypeLossy<SRGBA<float>>().resolved();
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void TextureMapper::drawEdgeTriangles(TextureMapperShaderProgram& program)
{
    const GLfloat left = 0;
    const GLfloat top = 0;
    const GLfloat right = 1;
    const GLfloat bottom = 1;
    const GLfloat center = 0.5;

// Each 4d triangle consists of a center point and two edge points, where the zw coordinates
// of each vertex equals the nearest point to the vertex on the edge.
#define SIDE_TRIANGLE_DATA(x1, y1, x2, y2) \
    x1, y1, x1, y1, \
    x2, y2, x2, y2, \
    center, center, (x1 + x2) / 2, (y1 + y2) / 2

    static const GLfloat unitRectSideTriangles[] = {
        SIDE_TRIANGLE_DATA(left, top, right, top),
        SIDE_TRIANGLE_DATA(left, top, left, bottom),
        SIDE_TRIANGLE_DATA(right, top, right, bottom),
        SIDE_TRIANGLE_DATA(left, bottom, right, bottom)
    };
#undef SIDE_TRIANGLE_DATA

    GLuint vbo = data().getStaticVBO(GL_ARRAY_BUFFER, sizeof(GCGLfloat) * 48, unitRectSideTriangles);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(program.vertexLocation(), 4, GL_FLOAT, false, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, 12);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TextureMapper::drawUnitRect(TextureMapperShaderProgram& program, GLenum drawingMode)
{
    static const GLfloat unitRect[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLuint vbo = data().getStaticVBO(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, unitRect);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(program.vertexLocation(), 2, GL_FLOAT, false, 0, 0);
    glDrawArrays(drawingMode, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TextureMapper::draw(const FloatRect& rect, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram& program, GLenum drawingMode, OptionSet<TextureMapperFlags> flags)
{
    TransformationMatrix matrix(modelViewMatrix);
    matrix.multiply(TransformationMatrix::rectToRect(FloatRect(0, 0, 1, 1), rect));

    glEnableVertexAttribArray(program.vertexLocation());
    program.setMatrix(program.modelViewMatrixLocation(), matrix);
    program.setMatrix(program.projectionMatrixLocation(), data().projectionMatrix);

    if (isInMaskMode()) {
        glBlendFunc(GL_ZERO, GL_SRC_ALPHA);
        glEnable(GL_BLEND);
    } else {
        if (flags.contains(TextureMapperFlags::ShouldBlend)) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
        } else
            glDisable(GL_BLEND);
    }

    if (flags.contains(TextureMapperFlags::ShouldAntialias))
        drawEdgeTriangles(program);
    else
        drawUnitRect(program, drawingMode);

    glDisableVertexAttribArray(program.vertexLocation());
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
}

void TextureMapper::drawTexturedQuadWithProgram(TextureMapperShaderProgram& program, const Vector<std::pair<GLuint, GLuint> >& texturesAndSamplers, OptionSet<TextureMapperFlags> flags, const FloatRect& rect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    glUseProgram(program.programID());

    bool repeatWrap = m_wrapMode == WrapMode::Repeat && GLContext::current()->glExtensions().OES_texture_npot;
    GLenum target = GLenum(GL_TEXTURE_2D);
    if (flags.contains(TextureMapperFlags::ShouldUseExternalOESTextureRect))
        target = GLenum(GL_TEXTURE_EXTERNAL_OES);

    for (unsigned i = 0; i < texturesAndSamplers.size(); ++i) {
        auto& textureAndSampler = texturesAndSamplers[i];

        glActiveTexture(GL_TEXTURE0 + i);
        glBindTexture(target, textureAndSampler.first);
        glUniform1i(textureAndSampler.second, i);

        if (repeatWrap) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    }

    TransformationMatrix patternTransform = this->patternTransform();
    prepareTransformationMatrixWithFlags(patternTransform, flags);

    program.setMatrix(program.textureSpaceMatrixLocation(), patternTransform);
    program.setMatrix(program.textureColorSpaceMatrixLocation(), colorSpaceMatrixForFlags(flags));
    glUniform1f(program.opacityLocation(), opacity);

    if (opacity < 1)
        flags.add(TextureMapperFlags::ShouldBlend);

    draw(rect, modelViewMatrix, program, GL_TRIANGLE_FAN, flags);

    if (repeatWrap) {
        for (auto& textureAndSampler : texturesAndSamplers) {
            glBindTexture(target, textureAndSampler.first);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

void TextureMapper::drawTexturedQuadWithProgram(TextureMapperShaderProgram& program, uint32_t texture, OptionSet<TextureMapperFlags> flags, const FloatRect& rect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    drawTexturedQuadWithProgram(program, { { texture, program.samplerLocation() } }, flags, rect, modelViewMatrix, opacity);
}

void TextureMapper::drawTextureCopy(const BitmapTexture& sourceTexture, const FloatRect& sourceRect, const FloatRect& targetRect)
{
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram({ TextureMapperShaderProgram::TextureCopy });
    const auto& textureSize = sourceTexture.size();

    glUseProgram(program->programID());

    auto textureCopyMatrix = TransformationMatrix::identity;

    textureCopyMatrix.scale3d(
        double(sourceRect.width()) / textureSize.width(),
        double(sourceRect.height()) / textureSize.height(),
        1
    ).translate3d(
        double(sourceRect.x()) / textureSize.width(),
        double(sourceRect.y()) / textureSize.height(),
        0
    );

    program->setMatrix(program->textureSpaceMatrixLocation(), textureCopyMatrix);

    glUniform2f(program->texelSizeLocation(), 1.f / textureSize.width(), 1.f / textureSize.height());

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture.id());
    glUniform1i(program->samplerLocation(), 0);

    draw(targetRect, TransformationMatrix(), program.get(), GL_TRIANGLE_FAN, { });
}

void TextureMapper::drawBlurred(const BitmapTexture& sourceTexture, const FloatRect& rect, float radius, Direction direction, bool alphaBlur)
{
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram({
        alphaBlur ? TextureMapperShaderProgram::AlphaBlur : TextureMapperShaderProgram::BlurFilter,
    });

    const auto& textureSize = sourceTexture.size();

    glUseProgram(program->programID());

    glUniform2f(program->texelSizeLocation(), 1.f / textureSize.width(), 1.f / textureSize.height());

    auto directionVector = direction == Direction::X ? FloatPoint(1, 0) : FloatPoint(0, 1);
    glUniform2f(program->blurDirectionLocation(), directionVector.x(), directionVector.y());

    // Zero-filled arrays for GLES<300
    std::array<float, SimplifiedGaussianKernelMaxHalfSize> kernel = { };
    std::array<float, SimplifiedGaussianKernelMaxHalfSize> offset = { };
    int simplifiedKernelHalfSize = computeGaussianKernel(radius, kernel, offset);
    glUniform1fv(program->gaussianKernelLocation(), SimplifiedGaussianKernelMaxHalfSize, kernel.data());
    glUniform1fv(program->gaussianKernelOffsetLocation(), SimplifiedGaussianKernelMaxHalfSize, offset.data());
    glUniform1i(program->gaussianKernelHalfSizeLocation(), simplifiedKernelHalfSize);

    auto textureBlurMatrix = TransformationMatrix::identity;

    textureBlurMatrix.scale3d(
        double(rect.width()) / textureSize.width(),
        double(rect.height()) / textureSize.height(),
        1
    ).translate3d(
        double(rect.x()) / textureSize.width(),
        double(rect.y()) / textureSize.height(),
        0
    );

    program->setMatrix(program->textureSpaceMatrixLocation(), textureBlurMatrix);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sourceTexture.id());
    glUniform1i(program->samplerLocation(), 0);

    draw(rect, TransformationMatrix(), program.get(), GL_TRIANGLE_FAN, { });
}

RefPtr<BitmapTexture> TextureMapper::applyBlurFilter(RefPtr<BitmapTexture>& sourceTexture, const BlurFilterOperation& blurFilter)
{
    const auto& textureSize = sourceTexture->size();
    float radiusX = floatValueForLength(blurFilter.stdDeviation(), textureSize.width());
    float radiusY = floatValueForLength(blurFilter.stdDeviation(), textureSize.height());

    if (radiusX < MinBlurRadius && radiusY < MinBlurRadius)
        return sourceTexture;

    RefPtr<BitmapTexture> resultTexture = m_texturePool.acquireTexture(textureSize, { BitmapTexture::Flags::SupportsAlpha });
    IntSize currentSize = textureSize;
    IntSize targetSize = currentSize;
    Vector<Direction> blurDirections;

    if (radiusX >= MinBlurRadius) {
        blurDirections.append(Direction::X);

        float scaleX = GaussianBlurMaxRadius / radiusX;
        if (scaleX < 1) {
            targetSize.setWidth(std::max(floorf(textureSize.width() * scaleX), 1.f));
            scaleX = float(targetSize.width()) / textureSize.width();
            radiusX = std::min(GaussianBlurMaxRadius, radiusX * scaleX);
        }
    }

    if (radiusY >= MinBlurRadius) {
        blurDirections.append(Direction::Y);

        float scaleY = GaussianBlurMaxRadius / radiusY;
        if (scaleY < 1) {
            targetSize.setHeight(std::max(floorf(textureSize.height() * scaleY), 1.f));
            scaleY = float(targetSize.height()) / textureSize.height();
            radiusY = std::min(GaussianBlurMaxRadius, radiusY * scaleY);
        }
    }

    // Shrink the texture content if the blur radius is too large
    while (currentSize.width() > targetSize.width() || currentSize.height() > targetSize.height()) {
        IntSize nextSize(
            std::max((currentSize.width() + 1) / 2, targetSize.width()),
            std::max((currentSize.height() + 1) / 2, targetSize.height())
        );

        FloatRect sourceRect(IntPoint::zero(), currentSize);
        FloatRect targetRect(IntPoint::zero(), nextSize);

        bindSurface(resultTexture.get());

        drawTextureCopy(*sourceTexture, sourceRect, targetRect);

        currentSize = nextSize;

        std::swap(resultTexture, sourceTexture);
    }

    // Apply blur
    for (auto direction : blurDirections) {
        bindSurface(resultTexture.get());

        FloatRect rect(FloatPoint::zero(), currentSize);

        float radius = direction == Direction::X ? radiusX : radiusY;

        drawBlurred(*sourceTexture, rect, radius, direction);

        std::swap(resultTexture, sourceTexture);
    }

    // Expand the texture if needed
    if (currentSize != textureSize) {
        bindSurface(resultTexture.get());

        FloatRect sourceRect(IntPoint::zero(), currentSize);
        FloatRect targetRect(IntPoint::zero(), textureSize);

        drawTextureCopy(*sourceTexture, sourceRect, targetRect);
    } else
        std::swap(resultTexture, sourceTexture);

    return resultTexture;
}

RefPtr<BitmapTexture> TextureMapper::applyDropShadowFilter(RefPtr<BitmapTexture>& sourceTexture, const DropShadowFilterOperation& dropShadowFilter)
{
    const auto& textureSize = sourceTexture->size();
    RefPtr<BitmapTexture> resultTexture = m_texturePool.acquireTexture(textureSize, { BitmapTexture::Flags::SupportsAlpha });
    RefPtr<BitmapTexture> contentTexture = m_texturePool.acquireTexture(textureSize, { BitmapTexture::Flags::SupportsAlpha });
    IntSize currentSize = textureSize;
    IntSize targetSize = currentSize;
    float radius = float(dropShadowFilter.stdDeviation());
    bool shouldBlur = radius >= MinBlurRadius;

    if (shouldBlur) {
        float scale = GaussianBlurMaxRadius / radius;

        if (scale < 1) {
            targetSize = IntSize(
                std::max(textureSize.width() * scale, 1.f),
                std::max(textureSize.height() * scale, 1.f)
            );
            scale = float(targetSize.width()) / textureSize.width();
            radius = std::min(GaussianBlurMaxRadius, radius * scale);
        }
    }

    { // Move the texture by shadow offset, and shrink the texture if needed
        IntSize nextSize(
            std::max((currentSize.width() + 1) / 2, targetSize.width()),
            std::max((currentSize.height() + 1) / 2, targetSize.height())
        );

        FloatPoint targetPoint = dropShadowFilter.location();

        if (shouldBlur) {
            float scaleX = float(nextSize.width()) / currentSize.width();
            float scaleY = float(nextSize.height()) / currentSize.height();

            targetPoint.scale(scaleX, scaleY);
        }

        FloatRect sourceRect(FloatPoint::zero(), currentSize);
        FloatRect targetRect(targetPoint, nextSize);

        bindSurface(resultTexture.get());

        drawTextureCopy(*sourceTexture, sourceRect, targetRect);

        currentSize = nextSize;

        std::swap(sourceTexture, contentTexture);

        std::swap(resultTexture, sourceTexture);
    }

    // Shrink texture content if blur radius is too large
    while (currentSize.width() > targetSize.width() || currentSize.height() > targetSize.height()) {
        IntSize nextSize(
            std::max((currentSize.width() + 1) / 2, targetSize.width()),
            std::max((currentSize.height() + 1) / 2, targetSize.height())
        );

        FloatRect sourceRect(FloatPoint::zero(), currentSize);
        FloatRect targetRect(FloatPoint::zero(), nextSize);

        bindSurface(resultTexture.get());

        drawTextureCopy(*sourceTexture, sourceRect, targetRect);

        currentSize = nextSize;

        std::swap(resultTexture, sourceTexture);
    }

    if (shouldBlur) {
        // Apply blur
        for (auto direction : { Direction::X, Direction::Y }) {
            bindSurface(resultTexture.get());

            FloatRect rect(FloatPoint::zero(), currentSize);

            drawBlurred(*sourceTexture, rect, radius, direction, true);

            std::swap(resultTexture, sourceTexture);
        }
    }

    // Expand the texture if needed
    if (currentSize != textureSize) {
        bindSurface(resultTexture.get());

        FloatRect sourceRect(FloatPoint::zero(), currentSize);
        FloatRect targetRect(FloatPoint::zero(), textureSize);

        drawTextureCopy(*sourceTexture, sourceRect, targetRect);

        std::swap(resultTexture, sourceTexture);
    }

    { // Convert the blurred image to a shadow and draw the original content over the shadow
        bindSurface(resultTexture.get());

        Ref<TextureMapperShaderProgram> program = data().getShaderProgram({
            TextureMapperShaderProgram::AlphaToShadow,
            TextureMapperShaderProgram::ContentTexture
        });

        glUseProgram(program->programID());

        auto [r, g, b, a] = premultiplied(dropShadowFilter.color().toColorTypeLossy<SRGBA<float>>()).resolved();
        glUniform4f(program->colorLocation(), r, g, b, a);

        glUniform2f(program->texelSizeLocation(), 1.f / textureSize.width(), 1.f / textureSize.height());

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, contentTexture->id());
        glUniform1i(program->contentTextureLocation(), 1);

        FloatRect targetRect(FloatPoint::zero(), textureSize);
        drawTexturedQuadWithProgram(program.get(), sourceTexture->id(), { }, targetRect, TransformationMatrix(), 1);
    }

    return resultTexture;
}

RefPtr<BitmapTexture> TextureMapper::applySinglePassFilter(RefPtr<BitmapTexture>& sourceTexture, const RefPtr<const FilterOperation>& filter, bool shouldDefer)
{
    if (shouldDefer) {
        sourceTexture->setFilterOperation(filter.copyRef());
        return sourceTexture;
    }

    RefPtr<BitmapTexture> resultTexture = m_texturePool.acquireTexture(sourceTexture->size(), { BitmapTexture::Flags::SupportsAlpha });

    bindSurface(resultTexture.get());

    // For standard filters, we always draw the whole texture without transformations.
    TextureMapperShaderProgram::Options options = optionsForFilterType(filter->type());
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    prepareFilterProgram(program.get(), *filter);
    FloatRect targetRect(FloatPoint::zero(), sourceTexture->size());
    drawTexturedQuadWithProgram(program.get(), sourceTexture->id(), { }, targetRect, TransformationMatrix(), 1);

    return resultTexture;
}

RefPtr<BitmapTexture> TextureMapper::applyFilters(RefPtr<BitmapTexture>& sourceTexture, const FilterOperations& filters, bool defersLastPass)
{
    if (filters.isEmpty())
        return sourceTexture;

    RefPtr<BitmapTexture> previousSurface = currentSurface();
    RefPtr<BitmapTexture> surface = sourceTexture;

    auto lastFilterIndex = filters.operations().size() - 1;
    size_t i = 0;
    for (const auto& filter : filters.operations()) {
        bool lastFilter = lastFilterIndex == i;
        surface = applyFilter(surface, filter, defersLastPass && lastFilter);
        ++i;
    }

    bindSurface(previousSurface.get());
    return surface;
}

RefPtr<BitmapTexture> TextureMapper::applyFilter(RefPtr<BitmapTexture>& sourceTexture, const RefPtr<const FilterOperation>& filter, bool defersLastPass)
{
    switch (filter->type()) {
    case FilterOperation::Type::Grayscale:
    case FilterOperation::Type::Sepia:
    case FilterOperation::Type::Saturate:
    case FilterOperation::Type::HueRotate:
    case FilterOperation::Type::Invert:
    case FilterOperation::Type::Brightness:
    case FilterOperation::Type::Contrast:
    case FilterOperation::Type::Opacity:
        return applySinglePassFilter(sourceTexture, filter, defersLastPass);
    case FilterOperation::Type::Blur:
        return applyBlurFilter(sourceTexture, static_cast<const BlurFilterOperation&>(*filter));
    case FilterOperation::Type::DropShadow:
        return applyDropShadowFilter(sourceTexture, static_cast<const DropShadowFilterOperation&>(*filter));
    default:
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return nullptr;
}

static inline TransformationMatrix createProjectionMatrix(const IntSize& size, bool flipY, double zNear, double zFar)
{
    const double nearValue = std::min(zNear + 1, 9999999.0);
    const double farValue = std::max(zFar - 1, -99999.0);
    return TransformationMatrix(2.0 / size.width(), 0, 0, 0,
        0, (flipY ? 2.0 : -2.0) / size.height(), 0, 0,
        0, 0, 2.0 / (farValue - nearValue), 0,
        -1, flipY ? -1 : 1, -(farValue + nearValue) / (farValue - nearValue), 1);
}

TextureMapper::~TextureMapper()
{
    delete m_data;
}

void TextureMapper::bindDefaultSurface()
{
    glBindFramebuffer(GL_FRAMEBUFFER, data().targetFrameBuffer);
    auto& viewport = data().viewport;
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    glDisable(GL_DEPTH_TEST);
    m_clipStack.apply();
    data().currentSurface = nullptr;
    updateProjectionMatrix();
}

void TextureMapper::bindSurface(BitmapTexture *surface)
{
    if (!surface) {
        bindDefaultSurface();
        return;
    }

    surface->bindAsSurface();
    data().currentSurface = surface;
    updateProjectionMatrix();
}

BitmapTexture* TextureMapper::currentSurface()
{
    return data().currentSurface.get();
}

bool TextureMapper::beginScissorClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    // 3D transforms are currently not supported in scissor clipping
    // resulting in cropped surfaces when z>0.
    if (!modelViewMatrix.isAffine())
        return false;

    FloatQuad quad = modelViewMatrix.projectQuad(targetRect);
    IntRect rect = quad.enclosingBoundingBox();

    // Only use scissors on rectilinear clips.
    if (!quad.isRectilinear() || rect.isEmpty())
        return false;

    clipStack().intersect(rect);
    clipStack().applyIfNeeded();
    return true;
}

bool TextureMapper::beginRoundedRectClip(const TransformationMatrix& modelViewMatrix, const FloatRoundedRect& targetRect)
{
    // This is implemented by telling the fragment shader to check whether each pixel is inside the rounded rectangle
    // before painting it.
    //
    // Inside the shader, the math to check whether a point is inside the rounded rectangle requires the rectangle to
    // be aligned to the X and Y axis, which is not guaranteed if the transformation matrix includes rotations. In order
    // to avoid this, instead of applying the transformation to the rounded rectangle, we calculate the inverse
    // of the transformation and apply it to the pixels before checking whether they are inside the rounded rectangle.
    // This works fine as long as the transformation matrix is invertible.
    //
    // There is a limit to the number of rounded rectangle clippings that can be done, that happens because the GLSL
    // arrays must have a predefined size. The limit is defined inside ClipStack, and that's why we need to call
    // clipStack().isRoundedRectClipAllowed() before trying to add a new clip.

    if (!targetRect.isRounded() || !targetRect.isRenderable() || targetRect.isEmpty() || !modelViewMatrix.isInvertible() || !clipStack().isRoundedRectClipAllowed())
        return false;

    FloatQuad quad = modelViewMatrix.projectQuad(targetRect.rect());
    IntRect rect = quad.enclosingBoundingBox();

    clipStack().addRoundedRect(targetRect, modelViewMatrix.inverse().value());
    clipStack().intersect(rect);
    clipStack().applyIfNeeded();

    return true;
}

void TextureMapper::beginClip(const TransformationMatrix& modelViewMatrix, const FloatRoundedRect& targetRect)
{
    clipStack().push();
    if (beginRoundedRectClip(modelViewMatrix, targetRect))
        return;

    if (beginScissorClip(modelViewMatrix, targetRect.rect()))
        return;

    data().initializeStencil();

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::SolidColor);

    glUseProgram(program->programID());
    glEnableVertexAttribArray(program->vertexLocation());
    const GLfloat unitRect[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLuint vbo = data().getStaticVBO(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, unitRect);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(program->vertexLocation(), 2, GL_FLOAT, false, 0, 0);

    TransformationMatrix matrix(modelViewMatrix);
    matrix.multiply(TransformationMatrix::rectToRect(FloatRect(0, 0, 1, 1), targetRect.rect()));

    static const TransformationMatrix fullProjectionMatrix = TransformationMatrix::rectToRect(FloatRect(0, 0, 1, 1), FloatRect(-1, -1, 2, 2));

    int stencilIndex = clipStack().getStencilIndex();

    glEnable(GL_STENCIL_TEST);

    // Make sure we don't do any actual drawing.
    glStencilFunc(GL_NEVER, stencilIndex, stencilIndex);

    // Operate only on the stencilIndex and above.
    glStencilMask(0xff & ~(stencilIndex - 1));

    // First clear the entire buffer at the current index.
    program->setMatrix(program->projectionMatrixLocation(), fullProjectionMatrix);
    program->setMatrix(program->modelViewMatrixLocation(), TransformationMatrix());
    glStencilOp(GL_ZERO, GL_ZERO, GL_ZERO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Now apply the current index to the new quad.
    glStencilOp(GL_REPLACE, GL_REPLACE, GL_REPLACE);
    program->setMatrix(program->projectionMatrixLocation(), data().projectionMatrix);
    program->setMatrix(program->modelViewMatrixLocation(), matrix);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    // Clear the state.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glDisableVertexAttribArray(program->vertexLocation());
    glStencilMask(0);

    // Increase stencilIndex and apply stencil testing.
    clipStack().setStencilIndex(stencilIndex * 2);
    clipStack().applyIfNeeded();
}

void TextureMapper::endClip()
{
    clipStack().pop();
    clipStack().applyIfNeeded();
}

IntRect TextureMapper::clipBounds()
{
    return clipStack().current().scissorBox;
}

void TextureMapper::setDepthRange(double zNear, double zFar)
{
    data().zNear = zNear;
    data().zFar = zFar;
    updateProjectionMatrix();
}

void TextureMapper::updateProjectionMatrix()
{
    bool flipY;
    IntSize size;
    if (data().currentSurface) {
        size = data().currentSurface->size();
        flipY = true;
    } else {
        size = IntSize(data().viewport[2], data().viewport[3]);
        flipY = data().flipY == FlipY::Yes;
    }
    data().projectionMatrix = createProjectionMatrix(size, flipY, data().zNear, data().zFar);
}

void TextureMapper::drawTextureExternalOES(GLuint texture, OptionSet<TextureMapperFlags> flags, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    flags.add(TextureMapperFlags::ShouldUseExternalOESTextureRect);
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::Option::TextureExternalOES);
    drawTexturedQuadWithProgram(program.get(), { { texture, program->externalOESTextureLocation() } }, flags, targetRect, modelViewMatrix, opacity);
}

} // namespace WebCore

#endif // USE(TEXTURE_MAPPER)
