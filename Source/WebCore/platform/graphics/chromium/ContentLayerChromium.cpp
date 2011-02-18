/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "ContentLayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#include "LayerTexture.h"
#include "RenderLayerBacking.h"

#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#include "SkColorPriv.h"
#include "skia/ext/platform_canvas.h"
#elif PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

namespace WebCore {

ContentLayerChromium::SharedValues::SharedValues(GraphicsContext3D* context)
    : m_context(context)
    , m_contentShaderProgram(0)
    , m_shaderSamplerLocation(-1)
    , m_shaderMatrixLocation(-1)
    , m_shaderAlphaLocation(-1)
    , m_initialized(false)
{
    // Shaders for drawing the layer contents.
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "uniform mat4 matrix;         \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = matrix * a_position; \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

#if PLATFORM(SKIA)
    // Color is in RGBA order.
    char rgbaFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = texColor * alpha; \n"
        "}                                                   \n";
#endif

    // Color is in BGRA order.
    char bgraFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha; \n"
        "}                                                   \n";

#if PLATFORM(SKIA)
    // Assuming the packing is either Skia default RGBA or Chromium default BGRA.
    char* fragmentShaderString = SK_B32_SHIFT ? rgbaFragmentShaderString : bgraFragmentShaderString;
#else
    char* fragmentShaderString = bgraFragmentShaderString;
#endif
    m_contentShaderProgram = createShaderProgram(m_context, vertexShaderString, fragmentShaderString);
    if (!m_contentShaderProgram) {
        LOG_ERROR("ContentLayerChromium: Failed to create shader program");
        return;
    }

    m_shaderSamplerLocation = m_context->getUniformLocation(m_contentShaderProgram, "s_texture");
    m_shaderMatrixLocation = m_context->getUniformLocation(m_contentShaderProgram, "matrix");
    m_shaderAlphaLocation = m_context->getUniformLocation(m_contentShaderProgram, "alpha");
    ASSERT(m_shaderSamplerLocation != -1);
    ASSERT(m_shaderMatrixLocation != -1);
    ASSERT(m_shaderAlphaLocation != -1);

    m_initialized = true;
}

ContentLayerChromium::SharedValues::~SharedValues()
{
    if (m_contentShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_contentShaderProgram));
}


PassRefPtr<ContentLayerChromium> ContentLayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new ContentLayerChromium(owner));
}

ContentLayerChromium::ContentLayerChromium(GraphicsLayerChromium* owner)
    : LayerChromium(owner)
    , m_contentsTexture(0)
    , m_skipsDraw(false)
{
}

ContentLayerChromium::~ContentLayerChromium()
{
    cleanupResources();
}

void ContentLayerChromium::cleanupResources()
{
    LayerChromium::cleanupResources();
    m_contentsTexture.clear();
}

bool ContentLayerChromium::requiresClippedUpdateRect() const
{
    // To avoid allocating excessively large textures, switch into "large layer mode" if
    // one of the layer's dimensions is larger than 2000 pixels or the size of
    // surface it's rendering into. This is a temporary measure until layer tiling is implemented.
    static const int maxLayerSize = 2000;
    return (bounds().width() > max(maxLayerSize, m_targetRenderSurface->contentRect().width())
            || bounds().height() > max(maxLayerSize, m_targetRenderSurface->contentRect().height())
            || !layerRenderer()->checkTextureSize(bounds()));
}

void ContentLayerChromium::updateContentsIfDirty()
{
    RenderLayerBacking* backing = static_cast<RenderLayerBacking*>(m_owner->client());
    if (!backing || backing->paintingGoesToWindow())
        return;

    ASSERT(drawsContent());

    ASSERT(layerRenderer());

    IntRect dirtyRect;
    IntRect boundsRect(IntPoint(0, 0), bounds());
    IntPoint paintingOffset;

    // FIXME: Remove this test when tiled layers are implemented.
    if (requiresClippedUpdateRect()) {
        // A layer with 3D transforms could require an arbitrarily large number
        // of texels to be repainted, so ignore these layers until tiling is
        // implemented.
        if (!drawTransform().isIdentityOrTranslation()) {
            m_skipsDraw = true;
            return;
        }

        // Calculate the region of this layer that is currently visible.
        const IntRect clipRect = m_targetRenderSurface->contentRect();

        TransformationMatrix layerOriginTransform = drawTransform();
        layerOriginTransform.translate3d(-0.5 * bounds().width(), -0.5 * bounds().height(), 0);

        // For now we apply the large layer treatment only for layers that are either untransformed
        // or are purely translated. Their matrix is expected to be invertible.
        ASSERT(layerOriginTransform.isInvertible());

        TransformationMatrix targetToLayerMatrix = layerOriginTransform.inverse();
        IntRect visibleRectInLayerCoords = targetToLayerMatrix.mapRect(clipRect);
        visibleRectInLayerCoords.intersect(IntRect(0, 0, bounds().width(), bounds().height()));

        // For normal layers, the center of the texture corresponds with the center of the layer.
        // In large layers the center of the texture is the center of the visible region so we have
        // to keep track of the offset in order to render correctly.
        IntRect visibleRectInSurfaceCoords = layerOriginTransform.mapRect(visibleRectInLayerCoords);
        m_layerCenterInSurfaceCoords = FloatRect(visibleRectInSurfaceCoords).center();

        // If this is still too large to render, then skip the layer completely.
        if (!layerRenderer()->checkTextureSize(visibleRectInLayerCoords.size())) {
            m_skipsDraw = true;
            return;
        }

        // If we need to resize the upload buffer we have to repaint everything.
        if (m_uploadBufferSize != visibleRectInLayerCoords.size()) {
            resizeUploadBuffer(visibleRectInLayerCoords.size());
            m_dirtyRect = boundsRect;
        }
        // If the visible portion of the layer is different from the last upload.
        if (visibleRectInLayerCoords != m_visibleRectInLayerCoords)
            m_dirtyRect = boundsRect;
        m_visibleRectInLayerCoords = visibleRectInLayerCoords;

        // Calculate the portion of the dirty rectangle that is visible.  m_dirtyRect is in layer space.
        IntRect visibleDirtyRectInLayerSpace = enclosingIntRect(m_dirtyRect);
        visibleDirtyRectInLayerSpace.intersect(visibleRectInLayerCoords);

        // What the rectangles mean:
        //   dirtyRect: The region of this layer that will be updated.
        //   m_uploadUpdateRect: The region of the layer's texture that will be uploaded into.
        dirtyRect = visibleDirtyRectInLayerSpace;
        m_uploadUpdateRect = dirtyRect;
        IntSize visibleRectOffsetInLayerCoords(visibleRectInLayerCoords.x(), visibleRectInLayerCoords.y());
        paintingOffset = IntPoint(visibleRectOffsetInLayerCoords);
        m_uploadUpdateRect.move(-visibleRectOffsetInLayerCoords);
    } else {
        dirtyRect = IntRect(m_dirtyRect);
        // If the texture needs to be reallocated then we must redraw the entire
        // contents of the layer.
        if (m_uploadBufferSize != bounds()) {
            resizeUploadBuffer(bounds());
            dirtyRect = boundsRect;
        } else {
            // Clip the dirtyRect to the size of the layer to avoid drawing
            // outside the bounds of the backing texture.
            dirtyRect.intersect(boundsRect);
        }
        m_uploadUpdateRect = dirtyRect;
    }

    if (dirtyRect.isEmpty())
        return;

#if PLATFORM(SKIA)
    OwnPtr<PlatformContextSkia> skiaContext;

    skiaContext.set(new PlatformContextSkia(m_uploadPixelCanvas.get()));

    // This is needed to get text to show up correctly.
    skiaContext->setDrawingToImageBuffer(true);

    GraphicsContext graphicsContext(reinterpret_cast<PlatformGraphicsContext*>(skiaContext.get()));

#elif PLATFORM(CG)
    // FIXME: Do we need to clear the dirty rectangle in the upload buffer?
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    size_t rowBytes = m_uploadBufferSize.width() * 4;
    RetainPtr<CGContextRef> contextCG(AdoptCF, CGBitmapContextCreate(m_uploadPixelData->data(),
                                                                     m_uploadBufferSize.width(), m_uploadBufferSize.height(), 8, rowBytes,
                                                                     colorSpace.get(),
                                                                     kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Host));
    CGContextTranslateCTM(contextCG.get(), 0, m_uploadBufferSize.height());
    CGContextScaleCTM(contextCG.get(), 1, -1);

    GraphicsContext graphicsContext(contextCG.get());
#else
#error "Need to implement for your platform."
#endif

    graphicsContext.save();
    graphicsContext.translate(-paintingOffset.x(), -paintingOffset.y());
    graphicsContext.clearRect(dirtyRect);
    graphicsContext.clip(dirtyRect);

    m_owner->paintGraphicsLayerContents(graphicsContext, dirtyRect);
    graphicsContext.restore();
}

void ContentLayerChromium::resizeUploadBufferForImage(const IntSize& size)
{
    size_t bufferSize = size.width() * size.height() * 4;
    m_uploadPixelData = new Vector<uint8_t>(bufferSize);
#if PLATFORM(CG)
    memset(m_uploadPixelData->data(), 0, bufferSize);
#endif
    m_uploadBufferSize = size;
}
void ContentLayerChromium::resizeUploadBuffer(const IntSize& size)
{
#if PLATFORM(SKIA)
    m_uploadPixelCanvas = new skia::PlatformCanvas(size.width(), size.height(), false);
    m_uploadBufferSize = size;
#else
    resizeUploadBufferForImage(size);
#endif
}

#if PLATFORM(SKIA)
class SkBitmapConditionalAutoLockerPixels {
    WTF_MAKE_NONCOPYABLE(SkBitmapConditionalAutoLockerPixels);
public:
    SkBitmapConditionalAutoLockerPixels()
        : m_bitmap(0)
    {
    }

    ~SkBitmapConditionalAutoLockerPixels()
    {
        if (m_bitmap)
            m_bitmap->unlockPixels();
    }

    void lockPixels(const SkBitmap* bitmap)
    {
        bitmap->lockPixels();
        m_bitmap = bitmap;
    }

private:
    const SkBitmap* m_bitmap;
};
#endif

void ContentLayerChromium::updateTextureIfNeeded()
{
    uint8_t* pixels = 0;
#if PLATFORM(SKIA)
    SkBitmapConditionalAutoLockerPixels locker;
#endif
    if (!m_uploadUpdateRect.isEmpty()) {
#if PLATFORM(SKIA)
        if (m_uploadPixelCanvas) {
            const SkBitmap& bitmap = m_uploadPixelCanvas->getDevice()->accessBitmap(false);
            locker.lockPixels(&bitmap);
            // FIXME: do we need to support more image configurations?
            if (bitmap.config() == SkBitmap::kARGB_8888_Config)
                pixels = static_cast<uint8_t*>(bitmap.getPixels());
        }
#endif
        if (m_uploadPixelData)
            pixels = m_uploadPixelData->data();
    }

    if (!pixels)
        return;

    GraphicsContext3D* context = layerRendererContext();
    if (!m_contentsTexture)
        m_contentsTexture = LayerTexture::create(context, layerRenderer()->textureManager());

    // If we have to allocate a new texture we have to upload the full contents.
    if (!m_contentsTexture->isValid(m_uploadBufferSize, GraphicsContext3D::RGBA))
        m_uploadUpdateRect = IntRect(IntPoint(0, 0), m_uploadBufferSize);

    if (!m_contentsTexture->reserve(m_uploadBufferSize, GraphicsContext3D::RGBA)) {
        m_skipsDraw = true;
        return;
    }

    IntRect srcRect = IntRect(IntPoint(0, 0), m_uploadBufferSize);
    if (requiresClippedUpdateRect())
        srcRect = m_visibleRectInLayerCoords;

    const size_t destStride = m_uploadUpdateRect.width() * 4;
    const size_t srcStride = srcRect.width() * 4;

    uint8_t* uploadPixels = pixels + srcStride * m_uploadUpdateRect.x();
    Vector<uint8_t> uploadBuffer;
    if (srcStride != destStride) {
        uploadBuffer.resize(m_uploadUpdateRect.height() * destStride);
        for (int row = 0; row < m_uploadUpdateRect.height(); ++row) {
            size_t srcOffset = (m_uploadUpdateRect.y() + row) * srcStride + m_uploadUpdateRect.x() * 4;
            ASSERT(srcOffset + destStride < static_cast<size_t>(m_uploadBufferSize.width() * m_uploadBufferSize.height() * 4));
            size_t destOffset = row * destStride;
            ASSERT(destOffset  + destStride < uploadBuffer.size());
            memcpy(uploadBuffer.data() + destOffset, pixels + srcOffset, destStride);
        }
        uploadPixels = uploadBuffer.data();
    }

    m_contentsTexture->bindTexture();
    GLC(context, context->texSubImage2D(GraphicsContext3D::TEXTURE_2D, 0,
                                        m_uploadUpdateRect.x(), m_uploadUpdateRect.y(), m_uploadUpdateRect.width(), m_uploadUpdateRect.height(),
                                        GraphicsContext3D::RGBA, GraphicsContext3D::UNSIGNED_BYTE,
                                        uploadPixels));

    m_uploadUpdateRect = IntRect();
    m_dirtyRect.setSize(FloatSize());
    // Large layers always stay dirty, because they need to update when the content rect changes.
    m_contentsDirty = requiresClippedUpdateRect();
}

void ContentLayerChromium::draw()
{
    updateTextureIfNeeded();

    if (m_skipsDraw)
        return;

    ASSERT(layerRenderer());

    const ContentLayerChromium::SharedValues* sv = layerRenderer()->contentLayerSharedValues();
    ASSERT(sv && sv->initialized());
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->activeTexture(GraphicsContext3D::TEXTURE0));
    bindContentsTexture();
    layerRenderer()->useShader(sv->contentShaderProgram());
    GLC(context, context->uniform1i(sv->shaderSamplerLocation(), 0));

    if (requiresClippedUpdateRect()) {
        float m43 = drawTransform().m43();
        TransformationMatrix transform;
        transform.translate3d(m_layerCenterInSurfaceCoords.x(), m_layerCenterInSurfaceCoords.y(), m43);
        drawTexturedQuad(context, layerRenderer()->projectionMatrix(),
                         transform, m_visibleRectInLayerCoords.width(),
                         m_visibleRectInLayerCoords.height(), drawOpacity(),
                         sv->shaderMatrixLocation(), sv->shaderAlphaLocation());
    } else {
        drawTexturedQuad(context, layerRenderer()->projectionMatrix(),
                         drawTransform(), bounds().width(), bounds().height(),
                         drawOpacity(), sv->shaderMatrixLocation(),
                         sv->shaderAlphaLocation());
    }
    unreserveContentsTexture();
}

void ContentLayerChromium::unreserveContentsTexture()
{
    if (!m_skipsDraw && m_contentsTexture)
        m_contentsTexture->unreserve();
}

void ContentLayerChromium::bindContentsTexture()
{
    updateTextureIfNeeded();

    if (!m_skipsDraw && m_contentsTexture)
        m_contentsTexture->bindTexture();
}


}
#endif // USE(ACCELERATED_COMPOSITING)
