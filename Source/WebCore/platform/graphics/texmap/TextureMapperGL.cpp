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
#include "TextureMapperGL.h"

#if USE(TEXTURE_MAPPER_GL)

#include "BitmapTextureGL.h"
#include "BitmapTexturePool.h"
#include "ExtensionsGL.h"
#include "FilterOperations.h"
#include "FloatQuad.h"
#include "GLContext.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "LengthFunctions.h"
#include "NotImplemented.h"
#include "TextureMapperShaderProgram.h"
#include "Timer.h"
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
    TextureMapper::PaintFlags PaintFlags { 0 };
    GLint previousProgram { 0 };
    GLint previousVAO { 0 };
    GLint targetFrameBuffer { 0 };
    bool didModifyStencil { false };
    GLint previousScissorState { 0 };
    GLint previousDepthState { 0 };
    GLint viewport[4] { 0, };
    GLint previousScissor[4] { 0, };
    RefPtr<BitmapTexture> currentSurface;
    const BitmapTextureGL::FilterInfo* filterInfo { nullptr };

private:
    class SharedGLData : public RefCounted<SharedGLData> {
    public:
        static Ref<SharedGLData> currentSharedGLData(void* platformContext)
        {
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

        HashMap<TextureMapperShaderProgram::Options, RefPtr<TextureMapperShaderProgram>> m_programs;
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

#if !USE(OPENGL_ES)
    if (GLContext::current()->version() >= 320 && m_vao)
        glDeleteVertexArrays(1, &m_vao);
#endif
}

void TextureMapperGLData::initializeStencil()
{
    if (currentSurface) {
        static_cast<BitmapTextureGL*>(currentSurface.get())->initializeStencil();
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
        [this, target, size, data] {
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
#if !USE(OPENGL_ES)
    if (GLContext::current()->version() >= 320 && !m_vao)
        glGenVertexArrays(1, &m_vao);
#endif

    return m_vao;
}

Ref<TextureMapperShaderProgram> TextureMapperGLData::getShaderProgram(TextureMapperShaderProgram::Options options)
{
    auto addResult = m_sharedGLData->m_programs.ensure(options,
        [options] { return TextureMapperShaderProgram::create(options); });
    return *addResult.iterator->value;
}

TextureMapperGL::TextureMapperGL()
    : m_contextAttributes(TextureMapperContextAttributes::get())
    , m_enableEdgeDistanceAntialiasing(false)
{
    void* platformContext = GLContext::current()->platformContext();
    ASSERT(platformContext);

    m_data = new TextureMapperGLData(platformContext);
#if USE(TEXTURE_MAPPER_GL)
    m_texturePool = makeUnique<BitmapTexturePool>(m_contextAttributes);
#endif
}

ClipStack& TextureMapperGL::clipStack()
{
    return data().currentSurface ? toBitmapTextureGL(data().currentSurface.get())->clipStack() : m_clipStack;
}

void TextureMapperGL::beginPainting(PaintFlags flags)
{
    glGetIntegerv(GL_CURRENT_PROGRAM, &data().previousProgram);
    data().previousScissorState = glIsEnabled(GL_SCISSOR_TEST);
    data().previousDepthState = glIsEnabled(GL_DEPTH_TEST);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_SCISSOR_TEST);
    data().didModifyStencil = false;
    glDepthMask(0);
    glGetIntegerv(GL_VIEWPORT, data().viewport);
    glGetIntegerv(GL_SCISSOR_BOX, data().previousScissor);
    m_clipStack.reset(IntRect(0, 0, data().viewport[2], data().viewport[3]), flags & PaintingMirrored ? ClipStack::YAxisMode::Default : ClipStack::YAxisMode::Inverted);
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &data().targetFrameBuffer);
    data().PaintFlags = flags;
    bindSurface(0);

#if !USE(OPENGL_ES)
    if (GLContext::current()->version() >= 320) {
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &data().previousVAO);
        glBindVertexArray(data().getVAO());
    }
#endif
}

void TextureMapperGL::endPainting()
{
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

#if !USE(OPENGL_ES)
    if (GLContext::current()->version() >= 320)
        glBindVertexArray(data().previousVAO);
#endif
}

void TextureMapperGL::drawBorder(const Color& color, float width, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix)
{
    if (clipStack().isCurrentScissorBoxEmpty())
        return;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::SolidColor);
    glUseProgram(program->programID());

    float r, g, b, a;
    Color(premultipliedARGBFromColor(color)).getRGBA(r, g, b, a);
    glUniform4f(program->colorLocation(), r, g, b, a);
    glLineWidth(width);

    draw(targetRect, modelViewMatrix, program.get(), GL_LINE_LOOP, !color.isOpaque() ? ShouldBlend : 0);
}

// FIXME: drawNumber() should save a number texture-atlas and re-use whenever possible.
void TextureMapperGL::drawNumber(int number, const Color& color, const FloatPoint& targetPoint, const TransformationMatrix& modelViewMatrix)
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
    if (color.isExtended())
        cairo_set_source_rgba(cr, color.asExtended().blue(), color.asExtended().green(), color.asExtended().red(), color.asExtended().alpha());
    else {
        float r, g, b, a;
        color.getRGBA(r, g, b, a);
        cairo_set_source_rgba(cr, b, g, r, a);
    }

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

    RefPtr<BitmapTexture> texture = acquireTextureFromPool(size);
    const unsigned char* bits = cairo_image_surface_get_data(surface);
    int stride = cairo_image_surface_get_stride(surface);
    static_cast<BitmapTextureGL*>(texture.get())->updateContents(bits, sourceRect, IntPoint::zero(), stride);
    drawTexture(*texture, targetRect, modelViewMatrix, 1.0f, AllEdges);

    cairo_surface_destroy(surface);
    cairo_destroy(cr);

#else
    UNUSED_PARAM(number);
    UNUSED_PARAM(pointSize);
    UNUSED_PARAM(targetPoint);
    UNUSED_PARAM(modelViewMatrix);
    notImplemented();
#endif
}

static TextureMapperShaderProgram::Options optionsForFilterType(FilterOperation::OperationType type, unsigned pass)
{
    switch (type) {
    case FilterOperation::GRAYSCALE:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::GrayscaleFilter;
    case FilterOperation::SEPIA:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::SepiaFilter;
    case FilterOperation::SATURATE:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::SaturateFilter;
    case FilterOperation::HUE_ROTATE:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::HueRotateFilter;
    case FilterOperation::INVERT:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::InvertFilter;
    case FilterOperation::BRIGHTNESS:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::BrightnessFilter;
    case FilterOperation::CONTRAST:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::ContrastFilter;
    case FilterOperation::OPACITY:
        return TextureMapperShaderProgram::TextureRGB | TextureMapperShaderProgram::OpacityFilter;
    case FilterOperation::BLUR:
        return TextureMapperShaderProgram::BlurFilter;
    case FilterOperation::DROP_SHADOW:
        return TextureMapperShaderProgram::AlphaBlur
            | (pass ? TextureMapperShaderProgram::ContentTexture | TextureMapperShaderProgram::SolidColor: 0);
    default:
        ASSERT_NOT_REACHED();
        return 0;
    }
}

// Create a normal distribution of 21 values between -2 and 2.
static const unsigned GaussianKernelHalfWidth = 11;
static const float GaussianKernelStep = 0.2;

static inline float gauss(float x)
{
    return exp(-(x * x) / 2.);
}

static float* gaussianKernel()
{
    static bool prepared = false;
    static float kernel[GaussianKernelHalfWidth] = {0, };

    if (prepared)
        return kernel;

    kernel[0] = gauss(0);
    float sum = kernel[0];
    for (unsigned i = 1; i < GaussianKernelHalfWidth; ++i) {
        kernel[i] = gauss(i * GaussianKernelStep);
        sum += 2 * kernel[i];
    }

    // Normalize the kernel.
    float scale = 1 / sum;
    for (unsigned i = 0; i < GaussianKernelHalfWidth; ++i)
        kernel[i] *= scale;

    prepared = true;
    return kernel;
}

static void prepareFilterProgram(TextureMapperShaderProgram& program, const FilterOperation& operation, unsigned pass, const IntSize& size, GLuint contentTexture)
{
    glUseProgram(program.programID());

    switch (operation.type()) {
    case FilterOperation::GRAYSCALE:
    case FilterOperation::SEPIA:
    case FilterOperation::SATURATE:
    case FilterOperation::HUE_ROTATE:
        glUniform1f(program.filterAmountLocation(), static_cast<const BasicColorMatrixFilterOperation&>(operation).amount());
        break;
    case FilterOperation::INVERT:
    case FilterOperation::BRIGHTNESS:
    case FilterOperation::CONTRAST:
    case FilterOperation::OPACITY:
        glUniform1f(program.filterAmountLocation(), static_cast<const BasicComponentTransferFilterOperation&>(operation).amount());
        break;
    case FilterOperation::BLUR: {
        const BlurFilterOperation& blur = static_cast<const BlurFilterOperation&>(operation);
        FloatSize radius;

        // Blur is done in two passes, first horizontally and then vertically. The same shader is used for both.
        if (pass)
            radius.setHeight(floatValueForLength(blur.stdDeviation(), size.height()) / size.height());
        else
            radius.setWidth(floatValueForLength(blur.stdDeviation(), size.width()) / size.width());

        glUniform2f(program.blurRadiusLocation(), radius.width(), radius.height());
        glUniform1fv(program.gaussianKernelLocation(), GaussianKernelHalfWidth, gaussianKernel());
        break;
    }
    case FilterOperation::DROP_SHADOW: {
        const DropShadowFilterOperation& shadow = static_cast<const DropShadowFilterOperation&>(operation);
        glUniform1fv(program.gaussianKernelLocation(), GaussianKernelHalfWidth, gaussianKernel());
        switch (pass) {
        case 0:
            // First pass: horizontal alpha blur.
            glUniform2f(program.blurRadiusLocation(), shadow.stdDeviation() / float(size.width()), 0);
            glUniform2f(program.shadowOffsetLocation(), float(shadow.location().x()) / float(size.width()), float(shadow.location().y()) / float(size.height()));
            break;
        case 1:
            // Second pass: we need the shadow color and the content texture for compositing.
            float r, g, b, a;
            Color(premultipliedARGBFromColor(shadow.color())).getRGBA(r, g, b, a);
            glUniform4f(program.colorLocation(), r, g, b, a);
            glUniform2f(program.blurRadiusLocation(), 0, shadow.stdDeviation() / float(size.height()));
            glUniform2f(program.shadowOffsetLocation(), 0, 0);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, contentTexture);
            glUniform1i(program.contentTextureLocation(), 1);
            break;
        }
        break;
    }
    default:
        break;
    }
}

static TransformationMatrix colorSpaceMatrixForFlags(TextureMapperGL::Flags flags)
{
    // The matrix is initially the identity one, which means no color conversion.
    TransformationMatrix matrix;
    if (flags & TextureMapperGL::ShouldConvertTextureBGRAToRGBA)
        matrix.setMatrix(0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0);
    else if (flags & TextureMapperGL::ShouldConvertTextureARGBToRGBA)
        matrix.setMatrix(0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 0.0);

    return matrix;
}

void TextureMapperGL::drawTexture(const BitmapTexture& texture, const FloatRect& targetRect, const TransformationMatrix& matrix, float opacity, unsigned exposedEdges)
{
    if (!texture.isValid())
        return;

    if (clipStack().isCurrentScissorBoxEmpty())
        return;

    const BitmapTextureGL& textureGL = static_cast<const BitmapTextureGL&>(texture);
    SetForScope<const BitmapTextureGL::FilterInfo*> filterInfo(data().filterInfo, textureGL.filterInfo());

    drawTexture(textureGL.id(), textureGL.colorConvertFlags() | (textureGL.isOpaque() ? 0 : ShouldBlend), textureGL.size(), targetRect, matrix, opacity, exposedEdges);
}

void TextureMapperGL::drawTexture(GLuint texture, Flags flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges)
{
    bool useRect = flags & ShouldUseARBTextureRect;
    bool useAntialiasing = m_enableEdgeDistanceAntialiasing
        && exposedEdges == AllEdges
        && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::TextureRGB;
    if (useRect)
        options |= TextureMapperShaderProgram::Rect;
    if (opacity < 1)
        options |= TextureMapperShaderProgram::Opacity;
    if (useAntialiasing) {
        options |= TextureMapperShaderProgram::Antialiasing;
        flags |= ShouldAntialias;
    }
    if (wrapMode() == RepeatWrap && !m_contextAttributes.supportsNPOTTextures)
        options |= TextureMapperShaderProgram::ManualRepeat;

    RefPtr<FilterOperation> filter = data().filterInfo ? data().filterInfo->filter: 0;
    GLuint filterContentTextureID = 0;

    if (filter) {
        if (data().filterInfo->contentTexture)
            filterContentTextureID = toBitmapTextureGL(data().filterInfo->contentTexture.get())->id();
        options |= optionsForFilterType(filter->type(), data().filterInfo->pass);
        if (filter->affectsOpacity())
            flags |= ShouldBlend;
    }

    if (useAntialiasing || opacity < 1)
        flags |= ShouldBlend;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get(), data().filterInfo->pass, textureSize, filterContentTextureID);

    drawTexturedQuadWithProgram(program.get(), texture, flags, textureSize, targetRect, modelViewMatrix, opacity);
}

static void prepareTransformationMatrixWithFlags(TransformationMatrix& patternTransform, TextureMapperGL::Flags flags, const IntSize& size)
{
    if (flags & TextureMapperGL::ShouldRotateTexture90) {
        patternTransform.rotate(-90);
        patternTransform.translate(-1, 0);
    }
    if (flags & TextureMapperGL::ShouldRotateTexture180) {
        patternTransform.rotate(180);
        patternTransform.translate(-1, -1);
    }
    if (flags & TextureMapperGL::ShouldRotateTexture270) {
        patternTransform.rotate(-270);
        patternTransform.translate(0, -1);
    }
    if (flags & TextureMapperGL::ShouldFlipTexture)
        patternTransform.flipY();
    if (flags & TextureMapperGL::ShouldUseARBTextureRect)
        patternTransform.scaleNonUniform(size.width(), size.height());
    if (flags & TextureMapperGL::ShouldFlipTexture)
        patternTransform.translate(0, -1);
}

void TextureMapperGL::drawTexturePlanarYUV(const std::array<GLuint, 3>& textures, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges)
{
    bool useRect = flags & ShouldUseARBTextureRect;
    bool useAntialiasing = m_enableEdgeDistanceAntialiasing
        && exposedEdges == AllEdges
        && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::TextureYUV;
    if (useRect)
        options |= TextureMapperShaderProgram::Rect;
    if (opacity < 1)
        options |= TextureMapperShaderProgram::Opacity;
    if (useAntialiasing) {
        options |= TextureMapperShaderProgram::Antialiasing;
        flags |= ShouldAntialias;
    }
    if (wrapMode() == RepeatWrap && !m_contextAttributes.supportsNPOTTextures)
        options |= TextureMapperShaderProgram::ManualRepeat;

    RefPtr<FilterOperation> filter = data().filterInfo ? data().filterInfo->filter: 0;
    GLuint filterContentTextureID = 0;

    if (filter) {
        if (data().filterInfo->contentTexture)
            filterContentTextureID = toBitmapTextureGL(data().filterInfo->contentTexture.get())->id();
        options |= optionsForFilterType(filter->type(), data().filterInfo->pass);
        if (filter->affectsOpacity())
            flags |= ShouldBlend;
    }

    if (useAntialiasing || opacity < 1)
        flags |= ShouldBlend;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get(), data().filterInfo->pass, textureSize, filterContentTextureID);

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { textures[0], program->samplerYLocation() },
        { textures[1], program->samplerULocation() },
        { textures[2], program->samplerVLocation() }
    };

    glUseProgram(program->programID());
    glUniformMatrix3fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, textureSize, targetRect, modelViewMatrix, opacity);
}

void TextureMapperGL::drawTextureSemiPlanarYUV(const std::array<GLuint, 2>& textures, bool uvReversed, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges)
{
    bool useRect = flags & ShouldUseARBTextureRect;
    bool useAntialiasing = m_enableEdgeDistanceAntialiasing
        && exposedEdges == AllEdges
        && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = uvReversed ?
        TextureMapperShaderProgram::TextureNV21 : TextureMapperShaderProgram::TextureNV12;
    if (useRect)
        options |= TextureMapperShaderProgram::Rect;
    if (opacity < 1)
        options |= TextureMapperShaderProgram::Opacity;
    if (useAntialiasing) {
        options |= TextureMapperShaderProgram::Antialiasing;
        flags |= ShouldAntialias;
    }
    if (wrapMode() == RepeatWrap && !m_contextAttributes.supportsNPOTTextures)
        options |= TextureMapperShaderProgram::ManualRepeat;

    RefPtr<FilterOperation> filter = data().filterInfo ? data().filterInfo->filter: 0;
    GLuint filterContentTextureID = 0;

    if (filter) {
        if (data().filterInfo->contentTexture)
            filterContentTextureID = toBitmapTextureGL(data().filterInfo->contentTexture.get())->id();
        options |= optionsForFilterType(filter->type(), data().filterInfo->pass);
        if (filter->affectsOpacity())
            flags |= ShouldBlend;
    }

    if (useAntialiasing || opacity < 1)
        flags |= ShouldBlend;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get(), data().filterInfo->pass, textureSize, filterContentTextureID);

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { textures[0], program->samplerYLocation() },
        { textures[1], program->samplerULocation() }
    };

    glUseProgram(program->programID());
    glUniformMatrix3fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, textureSize, targetRect, modelViewMatrix, opacity);
}

void TextureMapperGL::drawTexturePackedYUV(GLuint texture, const std::array<GLfloat, 9>& yuvToRgbMatrix, Flags flags, const IntSize& textureSize, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity, unsigned exposedEdges)
{
    bool useRect = flags & ShouldUseARBTextureRect;
    bool useAntialiasing = m_enableEdgeDistanceAntialiasing
        && exposedEdges == AllEdges
        && !modelViewMatrix.mapQuad(targetRect).isRectilinear();

    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::TexturePackedYUV;
    if (useRect)
        options |= TextureMapperShaderProgram::Rect;
    if (opacity < 1)
        options |= TextureMapperShaderProgram::Opacity;
    if (useAntialiasing) {
        options |= TextureMapperShaderProgram::Antialiasing;
        flags |= ShouldAntialias;
    }
    if (wrapMode() == RepeatWrap && !m_contextAttributes.supportsNPOTTextures)
        options |= TextureMapperShaderProgram::ManualRepeat;

    RefPtr<FilterOperation> filter = data().filterInfo ? data().filterInfo->filter: 0;
    GLuint filterContentTextureID = 0;

    if (filter) {
        if (data().filterInfo->contentTexture)
            filterContentTextureID = toBitmapTextureGL(data().filterInfo->contentTexture.get())->id();
        options |= optionsForFilterType(filter->type(), data().filterInfo->pass);
        if (filter->affectsOpacity())
            flags |= ShouldBlend;
    }

    if (useAntialiasing || opacity < 1)
        flags |= ShouldBlend;

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    if (filter)
        prepareFilterProgram(program.get(), *filter.get(), data().filterInfo->pass, textureSize, filterContentTextureID);

    Vector<std::pair<GLuint, GLuint> > texturesAndSamplers = {
        { texture, program->samplerLocation() }
    };

    glUseProgram(program->programID());
    glUniformMatrix3fv(program->yuvToRgbLocation(), 1, GL_FALSE, static_cast<const GLfloat *>(&yuvToRgbMatrix[0]));
    drawTexturedQuadWithProgram(program.get(), texturesAndSamplers, flags, textureSize, targetRect, modelViewMatrix, opacity);
}

void TextureMapperGL::drawSolidColor(const FloatRect& rect, const TransformationMatrix& matrix, const Color& color, bool isBlendingAllowed)
{
    Flags flags = 0;
    TextureMapperShaderProgram::Options options = TextureMapperShaderProgram::SolidColor;
    if (!matrix.mapQuad(rect).isRectilinear()) {
        options |= TextureMapperShaderProgram::Antialiasing;
        flags |= ShouldAntialias | (isBlendingAllowed ? ShouldBlend : 0);
    }

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);
    glUseProgram(program->programID());

    float r, g, b, a;
    Color(premultipliedARGBFromColor(color)).getRGBA(r, g, b, a);
    glUniform4f(program->colorLocation(), r, g, b, a);
    if (a < 1 && isBlendingAllowed)
        flags |= ShouldBlend;

    draw(rect, matrix, program.get(), GL_TRIANGLE_FAN, flags);
}

void TextureMapperGL::clearColor(const Color& color)
{
    glClearColor(color.red() / 255.0f, color.green() / 255.0f, color.blue() / 255.0f, color.alpha() / 255.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

void TextureMapperGL::drawEdgeTriangles(TextureMapperShaderProgram& program)
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

void TextureMapperGL::drawUnitRect(TextureMapperShaderProgram& program, GLenum drawingMode)
{
    static const GLfloat unitRect[] = { 0, 0, 1, 0, 1, 1, 0, 1 };
    GLuint vbo = data().getStaticVBO(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, unitRect);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(program.vertexLocation(), 2, GL_FLOAT, false, 0, 0);
    glDrawArrays(drawingMode, 0, 4);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void TextureMapperGL::draw(const FloatRect& rect, const TransformationMatrix& modelViewMatrix, TextureMapperShaderProgram& program, GLenum drawingMode, Flags flags)
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
        if (flags & ShouldBlend) {
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
            glEnable(GL_BLEND);
        } else
            glDisable(GL_BLEND);
    }

    if (flags & ShouldAntialias)
        drawEdgeTriangles(program);
    else
        drawUnitRect(program, drawingMode);

    glDisableVertexAttribArray(program.vertexLocation());
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_BLEND);
}

void TextureMapperGL::drawTexturedQuadWithProgram(TextureMapperShaderProgram& program, const Vector<std::pair<GLuint, GLuint> >& texturesAndSamplers, Flags flags, const IntSize& size, const FloatRect& rect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    glUseProgram(program.programID());

    bool repeatWrap = wrapMode() == RepeatWrap && m_contextAttributes.supportsNPOTTextures;
    GLenum target;
    if (flags & ShouldUseExternalOESTextureRect)
        target = GLenum(GL_TEXTURE_EXTERNAL_OES);
    else
        target = flags & ShouldUseARBTextureRect ? GLenum(GL_TEXTURE_RECTANGLE_ARB) : GLenum(GL_TEXTURE_2D);

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
    prepareTransformationMatrixWithFlags(patternTransform, flags, size);

    program.setMatrix(program.textureSpaceMatrixLocation(), patternTransform);
    program.setMatrix(program.textureColorSpaceMatrixLocation(), colorSpaceMatrixForFlags(flags));
    glUniform1f(program.opacityLocation(), opacity);

    if (opacity < 1)
        flags |= ShouldBlend;

    draw(rect, modelViewMatrix, program, GL_TRIANGLE_FAN, flags);

    if (repeatWrap) {
        for (auto& textureAndSampler : texturesAndSamplers) {
            glBindTexture(target, textureAndSampler.first);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        }
    }
}

void TextureMapperGL::drawTexturedQuadWithProgram(TextureMapperShaderProgram& program, uint32_t texture, Flags flags, const IntSize& size, const FloatRect& rect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    drawTexturedQuadWithProgram(program, { { texture, program.samplerLocation() } }, flags, size, rect, modelViewMatrix, opacity);
}

void TextureMapperGL::drawFiltered(const BitmapTexture& sampler, const BitmapTexture* contentTexture, const FilterOperation& filter, int pass)
{
    // For standard filters, we always draw the whole texture without transformations.
    TextureMapperShaderProgram::Options options = optionsForFilterType(filter.type(), pass);
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(options);

    prepareFilterProgram(program.get(), filter, pass, sampler.contentSize(), contentTexture ? static_cast<const BitmapTextureGL*>(contentTexture)->id() : 0);
    FloatRect targetRect(IntPoint::zero(), sampler.contentSize());
    drawTexturedQuadWithProgram(program.get(), static_cast<const BitmapTextureGL&>(sampler).id(), 0, IntSize(1, 1), targetRect, TransformationMatrix(), 1);
}

static inline TransformationMatrix createProjectionMatrix(const IntSize& size, bool mirrored)
{
    const float nearValue = 9999999;
    const float farValue = -99999;

    return TransformationMatrix(2.0 / float(size.width()), 0, 0, 0,
                                0, (mirrored ? 2.0 : -2.0) / float(size.height()), 0, 0,
                                0, 0, -2.f / (farValue - nearValue), 0,
                                -1, mirrored ? -1 : 1, -(farValue + nearValue) / (farValue - nearValue), 1);
}

TextureMapperGL::~TextureMapperGL()
{
    delete m_data;
}

void TextureMapperGL::bindDefaultSurface()
{
    glBindFramebuffer(GL_FRAMEBUFFER, data().targetFrameBuffer);
    auto& viewport = data().viewport;
    data().projectionMatrix = createProjectionMatrix(IntSize(viewport[2], viewport[3]), data().PaintFlags & PaintingMirrored);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
    m_clipStack.apply();
    data().currentSurface = nullptr;
}

void TextureMapperGL::bindSurface(BitmapTexture *surface)
{
    if (!surface) {
        bindDefaultSurface();
        return;
    }

    static_cast<BitmapTextureGL*>(surface)->bindAsSurface();
    data().projectionMatrix = createProjectionMatrix(surface->size(), true /* mirrored */);
    data().currentSurface = surface;
}

BitmapTexture* TextureMapperGL::currentSurface()
{
    return data().currentSurface.get();
}

bool TextureMapperGL::beginScissorClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
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

void TextureMapperGL::beginClip(const TransformationMatrix& modelViewMatrix, const FloatRect& targetRect)
{
    clipStack().push();
    if (beginScissorClip(modelViewMatrix, targetRect))
        return;

    data().initializeStencil();

    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::SolidColor);

    glUseProgram(program->programID());
    glEnableVertexAttribArray(program->vertexLocation());
    const GLfloat unitRect[] = {0, 0, 1, 0, 1, 1, 0, 1};
    GLuint vbo = data().getStaticVBO(GL_ARRAY_BUFFER, sizeof(GLfloat) * 8, unitRect);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glVertexAttribPointer(program->vertexLocation(), 2, GL_FLOAT, false, 0, 0);

    TransformationMatrix matrix(modelViewMatrix);
    matrix.multiply(TransformationMatrix::rectToRect(FloatRect(0, 0, 1, 1), targetRect));

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

void TextureMapperGL::endClip()
{
    clipStack().pop();
    clipStack().applyIfNeeded();
}

IntRect TextureMapperGL::clipBounds()
{
    return clipStack().current().scissorBox;
}

Ref<BitmapTexture> TextureMapperGL::createTexture(GLint internalFormat)
{
    return BitmapTextureGL::create(m_contextAttributes, internalFormat);
}

std::unique_ptr<TextureMapper> TextureMapper::platformCreateAccelerated()
{
    return makeUnique<TextureMapperGL>();
}

void TextureMapperGL::drawTextureExternalOES(GLuint texture, Flags flags, const IntSize& size, const FloatRect& targetRect, const TransformationMatrix& modelViewMatrix, float opacity)
{
    Ref<TextureMapperShaderProgram> program = data().getShaderProgram(TextureMapperShaderProgram::Option::TextureExternalOES);
    drawTexturedQuadWithProgram(program.get(), { { texture, program->externalOESTextureLocation() } },
        flags | TextureMapperGL::ShouldUseExternalOESTextureRect, size, targetRect, modelViewMatrix, opacity);
}

};

#endif // USE(TEXTURE_MAPPER_GL)
