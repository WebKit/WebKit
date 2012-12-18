/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
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

#include "LayerRenderer.h"

#include "LayerCompositingThread.h"
#include "LayerFilterRenderer.h"
#include "TextureCacheCompositingThread.h"

#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformLog.h>
#include <EGL/egl.h>
#include <limits>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#define DEBUG_LAYER_ANIMATIONS 0 // Show running animations as green.
#define DEBUG_CLIPPING 0

using BlackBerry::Platform::Graphics::GLES2Context;
using namespace std;

namespace WebCore {

static void checkGLError()
{
#ifndef NDEBUG
    if (GLenum error = glGetError())
        LOG_ERROR("GL Error: 0x%x " , error);
#endif
}

GLuint LayerRenderer::loadShader(GLenum type, const char* shaderSource)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    glShaderSource(shader, 1, &shaderSource, 0);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        char infoLog[2048];
        GLsizei length;
        glGetShaderInfoLog(shader, 2048, &length, infoLog);
        BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelCritical, "Failed to compile shader:\n%s\nlog: %s", shaderSource, infoLog);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint LayerRenderer::loadShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource)
{
    GLuint vertexShader;
    GLuint fragmentShader;
    GLuint programObject;
    GLint linked;
    vertexShader = loadShader(GL_VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader)
        return 0;
    fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        glDeleteShader(vertexShader);
        return 0;
    }
    programObject = glCreateProgram();
    if (programObject) {
        glAttachShader(programObject, vertexShader);
        glAttachShader(programObject, fragmentShader);
        glLinkProgram(programObject);
        glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
        if (!linked) {
            glDeleteProgram(programObject);
            programObject = 0;
        }
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return programObject;
}

TransformationMatrix LayerRenderer::orthoMatrix(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    float deltaX = right - left;
    float deltaY = top - bottom;
    float deltaZ = farZ - nearZ;
    TransformationMatrix ortho;
    if (!deltaX || !deltaY || !deltaZ)
        return ortho;
    ortho.setM11(2.0f / deltaX);
    ortho.setM41(-(right + left) / deltaX);
    ortho.setM22(2.0f / deltaY);
    ortho.setM42(-(top + bottom) / deltaY);
    ortho.setM33(-2.0f / deltaZ);
    ortho.setM43(-(nearZ + farZ) / deltaZ);
    return ortho;
}

static Vector<LayerCompositingThread*> rawPtrVectorFromRefPtrVector(const Vector<RefPtr<LayerCompositingThread> >& sublayers)
{
    Vector<LayerCompositingThread*> sublayerList;
    for (size_t i = 0; i < sublayers.size(); i++)
        sublayerList.append(sublayers[i].get());

    return sublayerList;
}

PassOwnPtr<LayerRenderer> LayerRenderer::create(GLES2Context* context)
{
    return adoptPtr(new LayerRenderer(context));
}

LayerRenderer::LayerRenderer(GLES2Context* context)
    : m_colorProgramObject(0)
    , m_checkerProgramObject(0)
    , m_positionLocation(0)
    , m_texCoordLocation(1)
    , m_scale(1.0)
    , m_animationTime(-numeric_limits<double>::infinity())
    , m_fbo(0)
    , m_currentLayerRendererSurface(0)
    , m_clearSurfaceOnDrawLayers(true)
    , m_context(context)
    , m_isRobustnessSupported(false)
    , m_needsCommit(false)
    , m_stencilCleared(false)
{
    if (makeContextCurrent()) {
        m_isRobustnessSupported = String(reinterpret_cast<const char*>(::glGetString(GL_EXTENSIONS))).contains("GL_EXT_robustness");
        if (m_isRobustnessSupported)
            m_glGetGraphicsResetStatusEXT = reinterpret_cast<PFNGLGETGRAPHICSRESETSTATUSEXTPROC>(eglGetProcAddress("glGetGraphicsResetStatusEXT"));
    }

    for (int i = 0; i < LayerData::NumberOfLayerProgramShaders; ++i)
        m_layerProgramObject[i] = 0;

    m_hardwareCompositing = initializeSharedGLObjects();
}

LayerRenderer::~LayerRenderer()
{
    if (m_hardwareCompositing) {
        makeContextCurrent();
        if (m_fbo)
            glDeleteFramebuffers(1, &m_fbo);
        glDeleteProgram(m_colorProgramObject);
        glDeleteProgram(m_checkerProgramObject);
        for (int i = 0; i < LayerData::NumberOfLayerProgramShaders; ++i)
            glDeleteProgram(m_layerProgramObject[i]);

        // Free up all GL textures.
        while (m_layers.begin() != m_layers.end()) {
            LayerSet::iterator iter = m_layers.begin();
            (*iter)->deleteTextures();
            (*iter)->setLayerRenderer(0);
            removeLayer(*iter);
        }

        textureCacheCompositingThread()->clear();
    }
}

void LayerRenderer::releaseLayerResources()
{
    if (m_hardwareCompositing) {
        makeContextCurrent();
        // Free up all GL textures.
        for (LayerSet::iterator iter = m_layers.begin(); iter != m_layers.end(); ++iter)
            (*iter)->deleteTextures();

        textureCacheCompositingThread()->clear();
    }
}

static inline bool compareLayerZ(const LayerCompositingThread* a, const LayerCompositingThread* b)
{
    const TransformationMatrix& transformA = a->drawTransform();
    const TransformationMatrix& transformB = b->drawTransform();

    return transformA.m43() < transformB.m43();
}

void LayerRenderer::prepareFrame(double animationTime, LayerCompositingThread* rootLayer)
{
    if (animationTime != m_animationTime) {
        m_animationTime = animationTime;

        // Aha, new frame! Reset rendering results.
        bool wasEmpty = m_lastRenderingResults.isEmpty();
        m_lastRenderingResults = LayerRenderingResults();
        m_lastRenderingResults.wasEmpty = wasEmpty;
    }

    if (!rootLayer)
        return;

    bool isContextCurrent = makeContextCurrent();
    prepareFrameRecursive(rootLayer, animationTime, isContextCurrent);
}

void LayerRenderer::setViewport(const IntRect& targetRect, const IntRect& clipRect, const FloatRect& visibleRect, const IntRect& layoutRect, const IntSize& contentsSize)
{
    // These parameters are used to calculate position of fixed position elements
    m_visibleRect = visibleRect;
    m_layoutRect = layoutRect;
    m_contentsSize = contentsSize;

    m_viewport = targetRect;
    m_scissorRect = clipRect;

    // The clipRect parameter uses render target coordinates, map to normalized device coordinates
    m_clipRect = clipRect;
    m_clipRect.intersect(targetRect);
    m_clipRect = FloatRect(-1 + 2 * (m_clipRect.x() - targetRect.x()) / targetRect.width(),
                           -1 + 2 * (m_clipRect.y() - targetRect.y()) / targetRect.height(),
                           2 * m_clipRect.width() / targetRect.width(),
                           2 * m_clipRect.height() / targetRect.height());

#if DEBUG_CLIPPING
    printf("LayerRenderer::setViewport() m_visibleRect=(%.2f,%.2f %.2fx%.2f), m_layoutRect=(%d,%d %dx%d), m_contentsSize=(%dx%d), m_viewport=(%d,%d %dx%d), m_scissorRect=(%d,%d %dx%d), m_clipRect=(%.2f,%.2f %.2fx%.2f)\n",
        m_visibleRect.x(), m_visibleRect.y(), m_visibleRect.width(), m_visibleRect.height(),
        m_layoutRect.x(), m_layoutRect.y(), m_layoutRect.width(), m_layoutRect.height(),
        m_contentsSize.width(), m_contentsSize.height(),
        m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height(),
        m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height(),
        m_clipRect.x(), m_clipRect.y(), m_clipRect.width(), m_clipRect.height());
    fflush(stdout);
#endif

    if (!m_hardwareCompositing)
        return;

    // Okay, we're going to do some drawing.
    if (!makeContextCurrent())
        return;

    // Get rid of any bound buffer that might affect the interpretation of our
    // glVertexAttribPointer calls.
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

    glEnableVertexAttribArray(m_positionLocation);
    glEnableVertexAttribArray(m_texCoordLocation);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_STENCIL_TEST);

    // If culling is enabled then we will cull the backface.
    glCullFace(GL_BACK);
    // The orthographic projection is setup such that Y starts at zero and
    // increases going down the page which flips the winding order of triangles.
    // The layer quads are drawn in clock-wise order so the front face is CCW.
    glFrontFace(GL_CCW);

    // Update the parameters for the checkerboard drawing.
    glUseProgram(m_checkerProgramObject);
    float bitmapScale = static_cast<float>(m_layoutRect.width()) / static_cast<float>(m_visibleRect.width());
    glUniform1f(m_checkerScaleLocation, bitmapScale);
    float scale = static_cast<float>(m_viewport.width()) / static_cast<float>(m_visibleRect.width());
    glUniform2f(m_checkerOriginLocation, m_visibleRect.x()*scale, m_visibleRect.y()*scale);
    glUniform1f(m_checkerSurfaceHeightLocation, m_context->surfaceSize().height());

    checkGLError();

    glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());

    glEnable(GL_SCISSOR_TEST);
#if DEBUG_CLIPPING
    printf("LayerRenderer::compositeLayers(): clipping to (%d,%d %dx%d)\n", m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
    fflush(stdout);
#endif
    glScissor(m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());

    if (m_clearSurfaceOnDrawLayers) {
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);
    }
    m_stencilCleared = false;
}

void LayerRenderer::compositeLayers(const TransformationMatrix& matrix, LayerCompositingThread* rootLayer)
{
    ASSERT(m_hardwareCompositing);
    if (!m_hardwareCompositing)
        return;

    if (!rootLayer)
        return;

    // Used to draw scale invariant layers. We assume uniform scale.
    // The matrix maps to normalized device coordinates, a system that maps the
    // viewport to the interval -1 to 1.
    // So it has to scale down by a factor equal to one half the viewport.
    m_scale = matrix.m11() * m_viewport.width() / 2;

    Vector<RefPtr<LayerCompositingThread> > surfaceLayers;
    const Vector<RefPtr<LayerCompositingThread> >& sublayers = rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++) {
        float opacity = 1;
        FloatRect clipRect(m_clipRect);
        updateLayersRecursive(sublayers[i].get(), matrix, surfaceLayers, opacity, clipRect);
    }

    // Decompose the dirty rect into a set of non-overlaping rectangles
    // (they need to not overlap so that the blending code doesn't draw any region twice).
    for (int i = 0; i < LayerRenderingResults::NumberOfDirtyRects; ++i) {
        BlackBerry::Platform::IntRectRegion region(BlackBerry::Platform::IntRect(m_lastRenderingResults.dirtyRect(i)));
        m_lastRenderingResults.dirtyRegion = BlackBerry::Platform::IntRectRegion::unionRegions(m_lastRenderingResults.dirtyRegion, region);
    }

    // If we won't draw anything, don't touch the OpenGL APIs.
    if (m_lastRenderingResults.isEmpty() && m_lastRenderingResults.wasEmpty)
        return;

    // Okay, we're going to do some drawing.
    if (!makeContextCurrent())
        return;

    // The shader used to render layers returns pre-multiplied alpha colors
    // so we need to send the blending mode appropriately.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    // If some layers should be drawed on temporary surfaces, we should do it first.
    drawLayersOnSurfaces(surfaceLayers);

    // Don't render the root layer, the BlackBerry port uses the BackingStore to draw the
    // root layer.
    for (size_t i = 0; i < sublayers.size(); i++) {
        int currentStencilValue = 0;
        FloatRect clipRect(m_clipRect);
        compositeLayersRecursive(sublayers[i].get(), currentStencilValue, clipRect);
    }

    // We need to make sure that all texture resource usage is finished before
    // unlocking the texture resources, so force a glFinish() in that case.
    if (m_layersLockingTextureResources.size())
        glFinish();

    m_context->swapBuffers();

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_STENCIL_TEST);

    // PR 147254, the EGL implementation crashes when the last bound texture
    // was an EGLImage, and you try to bind another texture and the pixmap
    // backing the EGLImage was deleted in between. Make this easier for the
    // driver by unbinding early (when the pixmap is hopefully still around).
    glBindTexture(GL_TEXTURE_2D, 0);

    // Turn off blending again
    glDisable(GL_BLEND);

    LayerSet::iterator iter = m_layersLockingTextureResources.begin();
    for (; iter != m_layersLockingTextureResources.end(); ++iter)
        (*iter)->releaseTextureResources();

    m_layersLockingTextureResources.clear();

    if (m_needsCommit) {
        m_needsCommit = false;
        rootLayer->scheduleCommit();
    }

    textureCacheCompositingThread()->collectGarbage();
}

static float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };

void LayerRenderer::compositeBuffer(const TransformationMatrix& transform, const FloatRect& contents, BlackBerry::Platform::Graphics::Buffer* buffer, bool contentsOpaque, float opacity)
{
    if (!buffer)
        return;

    FloatQuad vertices(transform.mapPoint(contents.minXMinYCorner()),
                       transform.mapPoint(contents.minXMaxYCorner()),
                       transform.mapPoint(contents.maxXMaxYCorner()),
                       transform.mapPoint(contents.maxXMinYCorner()));

    if (!vertices.boundingBox().intersects(m_clipRect))
        return;

    bool blending = !contentsOpaque || opacity < 1.0f;
    if (blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    glUseProgram(m_layerProgramObject[LayerData::LayerProgramShaderRGBA]);
    glUniform1f(m_alphaLocation[LayerData::LayerProgramShaderRGBA], opacity);

    glVertexAttribPointer(m_positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &vertices);
    glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    if (BlackBerry::Platform::Graphics::lockAndBindBufferGLTexture(buffer, GL_TEXTURE_2D)) {
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        BlackBerry::Platform::Graphics::releaseBufferGLTexture(buffer);
    }

    if (blending)
        glDisable(GL_BLEND);
}

void LayerRenderer::drawCheckerboardPattern(const TransformationMatrix& transform, const FloatRect& contents)
{
    FloatQuad vertices(transform.mapPoint(contents.minXMinYCorner()),
                       transform.mapPoint(contents.minXMaxYCorner()),
                       transform.mapPoint(contents.maxXMaxYCorner()),
                       transform.mapPoint(contents.maxXMinYCorner()));

    if (!vertices.boundingBox().intersects(m_clipRect))
        return;

    glUseProgram(m_checkerProgramObject);

    glVertexAttribPointer(m_positionLocation, 2, GL_FLOAT, GL_FALSE, 0, &vertices);
    glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
}

bool LayerRenderer::useSurface(LayerRendererSurface* surface)
{
    if (m_currentLayerRendererSurface == surface)
        return true;

    m_currentLayerRendererSurface = surface;
    if (!surface) {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(m_viewport.x(), m_viewport.y(), m_viewport.width(), m_viewport.height());
        return true;
    }

    surface->ensureTexture();

    if (!m_fbo)
        glGenFramebuffers(1, &m_fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, surface->texture()->textureId(), 0);

#ifndef NDEBUG
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        fprintf(stderr, "glCheckFramebufferStatus error %x\n", status);
        return false;
    }
#endif

    glViewport(0, 0, surface->size().width(), surface->size().height());
    return true;
}

void LayerRenderer::drawLayersOnSurfaces(const Vector<RefPtr<LayerCompositingThread> >& surfaceLayers)
{
    for (int i = surfaceLayers.size() - 1; i >= 0; i--) {
        LayerCompositingThread* layer = surfaceLayers[i].get();
        LayerRendererSurface* surface = layer->layerRendererSurface();
        if (!surface || !useSurface(surface))
            continue;

        glDisable(GL_SCISSOR_TEST);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT);

        int currentStencilValue = 0;
        FloatRect clipRect(-1, -1, 2, 2);
        compositeLayersRecursive(surfaceLayers[i].get(), currentStencilValue, clipRect);

#if ENABLE(CSS_FILTERS)
        if (!m_filterRenderer)
            m_filterRenderer = LayerFilterRenderer::create(m_positionLocation, m_texCoordLocation);
        if (layer->filterOperationsChanged()) {
            layer->setFilterOperationsChanged(false);
            layer->setFilterActions(m_filterRenderer->actionsForOperations(surface, layer->filters().operations()));
        }
        m_filterRenderer->applyActions(m_fbo, layer, layer->filterActions());
        glClearColor(0, 0, 0, 0);
#endif
    }

    // If there are layers drawed on surfaces, we need to switch to default framebuffer.
    // Otherwise, we just need to set viewport.
    if (surfaceLayers.size()) {
        useSurface(0);
        glEnable(GL_SCISSOR_TEST);
        glScissor(m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
    }
}

void LayerRenderer::addLayer(LayerCompositingThread* layer)
{
    m_layers.add(layer);
}

bool LayerRenderer::removeLayer(LayerCompositingThread* layer)
{
    LayerSet::iterator iter = m_layers.find(layer);
    if (iter == m_layers.end())
        return false;
    m_layers.remove(layer);
    return true;
}

void LayerRenderer::addLayerToReleaseTextureResourcesList(LayerCompositingThread* layer)
{
    m_layersLockingTextureResources.add(layer);
}

// Transform normalized device coordinates to window coordinates
// as specified in the OpenGL ES 2.0 spec section 2.12.1.
IntRect LayerRenderer::toOpenGLWindowCoordinates(const FloatRect& r) const
{
    float vw2 = m_viewport.width() / 2.0;
    float vh2 = m_viewport.height() / 2.0;
    float ox = m_viewport.x() + vw2;
    float oy = m_viewport.y() + vh2;
    return enclosingIntRect(FloatRect(r.x() * vw2 + ox, r.y() * vh2 + oy, r.width() * vw2, r.height() * vh2));
}

// Transform normalized device coordinates to window coordinates as WebKit understands them.
//
// The OpenGL surface may be larger than the WebKit window, and OpenGL window coordinates
// have origin in bottom left while WebKit window coordinates origin is in top left.
// The viewport is setup to cover the upper portion of the larger OpenGL surface.
IntRect LayerRenderer::toWebKitWindowCoordinates(const FloatRect& r) const
{
    float vw2 = m_viewport.width() / 2.0;
    float vh2 = m_viewport.height() / 2.0;
    float ox = m_viewport.x() + vw2;
    float oy = m_context->surfaceSize().height() - (m_viewport.y() + vh2);
    return enclosingIntRect(FloatRect(r.x() * vw2 + ox, -(r.y()+r.height()) * vh2 + oy, r.width() * vw2, r.height() * vh2));
}

// Similar to toWebKitWindowCoordinates except that this also takes any zoom into account.
IntRect LayerRenderer::toWebKitDocumentCoordinates(const FloatRect& r) const
{
    // The zoom is the ratio between visibleRect (or layoutRect) and dstRect parameters which are passed to drawLayers
    float zoom = m_visibleRect.width() / m_viewport.width();
    // Could assert here that it doesn't matter whether we choose width or height in the above statement:
    // because both rectangles should have very similar shapes (subject only to pixel rounding error).

    IntRect result = toWebKitWindowCoordinates(r);
    result.scale(zoom);
    return result;
}

// Draws a debug border around the layer's bounds.
void LayerRenderer::drawDebugBorder(LayerCompositingThread* layer)
{
    Color borderColor = layer->borderColor();

#if DEBUG_LAYER_ANIMATIONS
    if (layer->hasRunningAnimations())
        borderColor =  Color(0x00, 0xFF, 0x00, 0xFF);
#endif

    if (!borderColor.alpha())
        return;

    glUseProgram(m_colorProgramObject);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
    glUniform4f(m_colorColorLocation, borderColor.red() / 255.0, borderColor.green() / 255.0, borderColor.blue() / 255.0, 1);

    glLineWidth(layer->borderWidth());
    glDrawArrays(GL_LINE_LOOP, 0, 4);
}

// Clears a rectangle inside the layer's bounds.
void LayerRenderer::drawHolePunchRect(LayerCompositingThread* layer)
{
    glUseProgram(m_colorProgramObject);
    glUniform4f(m_colorColorLocation, 0, 0, 0, 0);

    glBlendFunc(GL_ONE, GL_ZERO);
    FloatQuad hole = layer->getTransformedHolePunchRect();
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &hole);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    checkGLError();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    IntRect holeWC = toWebKitWindowCoordinates(hole.boundingBox());
    m_lastRenderingResults.addHolePunchRect(holeWC);
}

void LayerRenderer::prepareFrameRecursive(LayerCompositingThread* layer, double animationTime, bool isContextCurrent)
{
    // This might cause the layer to recompute some attributes.
    m_lastRenderingResults.needsAnimationFrame |= layer->updateAnimations(animationTime);

    if (isContextCurrent) {
        // Even non-visible layers need to perform their texture jobs, or they will
        // pile up and waste memory.
        if (layer->needsTexture())
            layer->updateTextureContentsIfNeeded();
        if (layer->maskLayer() && layer->maskLayer()->needsTexture())
            layer->maskLayer()->updateTextureContentsIfNeeded();
        if (layer->replicaLayer()) {
            LayerCompositingThread* replica = layer->replicaLayer();
            if (replica->needsTexture())
                replica->updateTextureContentsIfNeeded();
            if (replica->maskLayer() && replica->maskLayer()->needsTexture())
                replica->maskLayer()->updateTextureContentsIfNeeded();
        }
    }

    const Vector<RefPtr<LayerCompositingThread> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        prepareFrameRecursive(sublayers[i].get(), animationTime, isContextCurrent);
}

void LayerRenderer::updateLayersRecursive(LayerCompositingThread* layer, const TransformationMatrix& matrix, Vector<RefPtr<LayerCompositingThread> >& surfaceLayers, float opacity, FloatRect clipRect)
{
    // The contract for LayerCompositingThread::setLayerRenderer is it must be set if the layer has been rendered.
    // So do it now, before we render it in compositeLayersRecursive.
    layer->setLayerRenderer(this);
    if (layer->maskLayer())
        layer->maskLayer()->setLayerRenderer(this);
    if (layer->replicaLayer()) {
        LayerCompositingThread* replica = layer->replicaLayer();
        replica->setLayerRenderer(this);
        if (replica->maskLayer())
            replica->maskLayer()->setLayerRenderer(this);
    }

    // Compute the new matrix transformation that will be applied to this layer and
    // all its sublayers. It's important to remember that the layer's position
    // is the position of the layer's anchor point. Also, the coordinate system used
    // assumes that the origin is at the lower left even though the coordinates the browser
    // gives us for the layers are for the upper left corner. The Y flip happens via
    // the orthographic projection applied at render time.
    // The transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    // Where M[p] is the parent matrix passed down to the function
    //       Tr[l] is the translation matrix locating the layer's anchor point
    //       Tr[c] is the translation offset between the anchor point and the center of the layer
    //       M[l] is the layer's matrix (applied at the anchor point)
    // This transform creates a coordinate system whose origin is the center of the layer.
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    FloatSize bounds = layer->bounds();
    if (layer->sizeIsScaleInvariant())
        bounds.scale(1.0 / m_scale);
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();

    // Layer whose hasFixedContainer is true will get scrolled relative to
    // the fixed positioned parent.
    if (!layer->hasFixedContainer() && (layer->isFixedPosition() || layer->hasFixedAncestorInDOMTree())) {
        // The basic idea here is to set visible y to the value we want, and
        // layout y to the value WebCore layouted the fixed element to.
        float maximumScrollY = m_contentsSize.height() - m_visibleRect.height();
        float visibleY = max(0.0f, m_visibleRect.y());
        float layoutY = max(0.0f, min(maximumScrollY, (float)m_layoutRect.y()));

        // For stuff located on the lower half of the screen, we zoom relative to bottom.
        // This trick allows us to display fixed positioned elements aligned to top or
        // bottom correctly when panning and zooming, without actually knowing the
        // numeric values of the top and bottom CSS attributes.
        // In fact, the position is the location of the anchor, so to find the top left
        // we have to subtract the anchor times the bounds. The anchor defaults to
        // (0.5, 0.5) for most layers.
        if (position.y() - anchorPoint.y() * bounds.height() > layoutY + m_layoutRect.height() / 2) {
            visibleY = min<float>(m_contentsSize.height(), m_visibleRect.y() + m_visibleRect.height());
            layoutY = min(m_contentsSize.height(), max(0, m_layoutRect.y()) + m_layoutRect.height());
        }

        position.setY(position.y() + (visibleY - layoutY));

        float visibleX = max(0.0f, visibleRect.x());
        position.setX(position.x() + visibleX);
    }

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    // M = M[p]
    TransformationMatrix localMatrix = matrix;
    // M = M[p] * Tr[l]
    localMatrix.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // M = M[p] * Tr[l] * M[l]
    localMatrix.multiply(layer->transform());
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    localMatrix.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    // Calculate the layer's opacity.
    opacity *= layer->opacity();

#if ENABLE(CSS_FILTERS)
    bool useLayerRendererSurface = layer->maskLayer() || layer->replicaLayer() || layer->filters().size();
#else
    bool useLayerRendererSurface = layer->maskLayer() || layer->replicaLayer();
#endif
    if (!useLayerRendererSurface) {
        layer->setDrawOpacity(opacity);
        layer->clearLayerRendererSurface();
    } else {
        if (!layer->layerRendererSurface())
            layer->createLayerRendererSurface();

        LayerRendererSurface* surface = layer->layerRendererSurface();

        layer->setDrawOpacity(1.0);
        surface->setDrawOpacity(opacity);

        surface->setDrawTransform(localMatrix);
        if (layer->replicaLayer()) {
            TransformationMatrix replicaMatrix = localMatrix;
            replicaMatrix.translate3d(-0.5 * bounds.width(), -0.5 * bounds.height(), 0);
            replicaMatrix.translate3d(layer->replicaLayer()->position().x(), layer->replicaLayer()->position().y(), 0);
            replicaMatrix.multiply(layer->replicaLayer()->transform());
            replicaMatrix.translate3d(centerOffsetX, centerOffsetY, 0);
            surface->setReplicaDrawTransform(replicaMatrix);
        }

        IntRect drawRect = enclosingIntRect(FloatRect(FloatPoint(), bounds));
        surface->setContentRect(drawRect);

        TransformationMatrix projectionMatrix = orthoMatrix(drawRect.x(), drawRect.maxX(), drawRect.y(), drawRect.maxY(), -1000, 1000);
        // The origin of the new surface is the upper left corner of the layer.
        TransformationMatrix drawTransform;
        drawTransform.translate3d(0.5 * bounds.width(), 0.5 * bounds.height(), 0);
        // This layer will start using new transformation.
        localMatrix = projectionMatrix * drawTransform;

        surfaceLayers.append(layer);
    }

    layer->setDrawTransform(m_scale, localMatrix);

#if ENABLE(VIDEO)
    bool layerVisible = clipRect.intersects(layer->getDrawRect()) || layer->mediaPlayer();
#else
    bool layerVisible = clipRect.intersects(layer->getDrawRect());
#endif

    if (layer->needsTexture() && layerVisible) {
        IntRect dirtyRect = toWebKitWindowCoordinates(intersection(layer->getDrawRect(), clipRect));
        m_lastRenderingResults.addDirtyRect(dirtyRect);
    }

    if (layer->masksToBounds())
        clipRect.intersect(layer->getDrawRect());

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        // This corresponds to the depth range specified in the original orthographic projection matrix
        localMatrix.setM33(0.001);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // Apply the sublayer transform.
    localMatrix.multiply(layer->sublayerTransform());

    // The origin of the sublayers is actually the bottom left corner of the layer
    // (or top left when looking it it from the browser's pespective) instead of the center.
    // The matrix passed down to the sublayers is therefore:
    // M[s] = M * Tr[-center]
    localMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    const Vector<RefPtr<LayerCompositingThread> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayersRecursive(sublayers[i].get(), localMatrix, surfaceLayers, opacity, clipRect);
}

static bool hasRotationalComponent(const TransformationMatrix& m)
{
    return m.m12() || m.m13() || m.m23() || m.m21() || m.m31() || m.m32();
}

bool LayerRenderer::layerAlreadyOnSurface(LayerCompositingThread* layer) const
{
    return layer->layerRendererSurface() && layer->layerRendererSurface() != m_currentLayerRendererSurface;
}

static void collect3DPreservingLayers(Vector<LayerCompositingThread*>& layers)
{
    for (size_t i = 0; i < layers.size(); ++i) {
        LayerCompositingThread* layer = layers[i];
        if (!layer->preserves3D() || !layer->getSublayers().size())
            continue;

        Vector<LayerCompositingThread*> sublayers = rawPtrVectorFromRefPtrVector(layer->getSublayers());
        collect3DPreservingLayers(sublayers);
        layers.insert(i+1, sublayers);
        i += sublayers.size();
    }
}

void LayerRenderer::compositeLayersRecursive(LayerCompositingThread* layer, int stencilValue, FloatRect clipRect)
{
    FloatRect rect;
    if (layerAlreadyOnSurface(layer))
        rect = layer->layerRendererSurface()->drawRect();
    else
        rect = layer->getDrawRect();

#if ENABLE(VIDEO)
    bool layerVisible = clipRect.intersects(rect) || layer->mediaPlayer();
#else
    bool layerVisible = clipRect.intersects(rect);
#endif

    layer->setVisible(layerVisible);

    // Note that there are two types of layers:
    // 1. Layers that have their own GraphicsContext and can draw their contents on demand (layer->drawsContent() == true).
    // 2. Layers that are just containers of images/video/etc that don't own a GraphicsContext (layer->contents() == true).

    if ((layer->needsTexture() || layer->layerRendererSurface()) && layerVisible) {
        updateScissorIfNeeded(clipRect);

        if (stencilValue) {
            glStencilFunc(GL_EQUAL, stencilValue, 0xff);
            glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
        }

        if (layer->doubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        if (layer->hasVisibleHolePunchRect())
            drawHolePunchRect(layer);

        // Draw the surface onto another surface or screen.
        bool drawSurface = layerAlreadyOnSurface(layer);
        // The texture format for the surface is RGBA.
        LayerData::LayerProgramShader shader = drawSurface ? LayerData::LayerProgramShaderRGBA : layer->layerProgramShader();

        if (!drawSurface) {
            glUseProgram(m_layerProgramObject[shader]);
            glUniform1f(m_alphaLocation[shader], layer->drawOpacity());
            layer->drawTextures(m_scale, m_positionLocation, m_texCoordLocation, m_visibleRect);
        } else {
            // Draw the reflection if it exists.
            if (layer->replicaLayer()) {
                // If this layer and its reflection both have mask, we need another temporary surface.
                // Since this use case should be rare, currently it's not handled and the mask for
                // the reflection is applied only when this layer has no mask.
                LayerCompositingThread* mask = layer->maskLayer();
                if (!mask && layer->replicaLayer())
                    mask = layer->replicaLayer()->maskLayer();

                glUseProgram(mask ? m_layerMaskProgramObject[shader] : m_layerProgramObject[shader]);
                glUniform1f(mask ? m_maskAlphaLocation[shader] : m_alphaLocation[shader], layer->layerRendererSurface()->drawOpacity());
                layer->drawSurface(layer->layerRendererSurface()->replicaDrawTransform(), mask, m_positionLocation, m_texCoordLocation);
            }

            glUseProgram(layer->maskLayer() ? m_layerMaskProgramObject[shader] : m_layerProgramObject[shader]);
            glUniform1f(layer->maskLayer() ? m_maskAlphaLocation[shader] : m_alphaLocation[shader], layer->layerRendererSurface()->drawOpacity());
            layer->drawSurface(layer->layerRendererSurface()->drawTransform(), layer->maskLayer(), m_positionLocation, m_texCoordLocation);
        }

        if (layer->hasMissingTextures()) {
            glDisable(GL_BLEND);
            glUseProgram(m_checkerProgramObject);
            layer->drawMissingTextures(m_scale, m_positionLocation, m_texCoordLocation, m_visibleRect);
            glEnable(GL_BLEND);
            glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
        }
    }

    // Draw the debug border if there is one.
    drawDebugBorder(layer);

    // The texture for the LayerRendererSurface can be released after the surface was drawed on another surface.
    if (layerAlreadyOnSurface(layer)) {
        layer->layerRendererSurface()->releaseTexture();
        return;
    }

    // If we need to mask to bounds but the transformation has a rotational component
    // to it, scissoring is not enough and we need to use the stencil buffer for clipping.
    bool stencilClip = layer->masksToBounds() && hasRotationalComponent(layer->drawTransform());

    if (stencilClip) {
        if (!m_stencilCleared) {
            glClearStencil(0);
            glClear(GL_STENCIL_BUFFER_BIT);
            m_stencilCleared = true;
        }

        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, stencilValue, 0xff);
        glStencilOp(GL_KEEP, GL_INCR, GL_INCR);

        updateScissorIfNeeded(clipRect);
        glUseProgram(m_colorProgramObject);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    }

    if (layer->masksToBounds())
        clipRect.intersect(layer->getDrawRect());

    // Here, we need to sort the whole subtree of layers with preserve-3d. It
    // affects all children, and the children of any children with preserve-3d,
    // and so on.
    Vector<LayerCompositingThread*> sublayers = rawPtrVectorFromRefPtrVector(layer->getSublayers());

    bool preserves3D = layer->preserves3D();
    bool superlayerPreserves3D = layer->superlayer() && layer->superlayer()->preserves3D();

    // Collect and render all sublayers with preserves-3D.
    // If the superlayer preserves 3D, we've already collected and rendered its
    // children, so bail.
    if (preserves3D && !superlayerPreserves3D) {
        collect3DPreservingLayers(sublayers);
        std::stable_sort(sublayers.begin(), sublayers.end(), compareLayerZ);
    }

    int newStencilValue = stencilClip ? stencilValue+1 : stencilValue;
    for (size_t i = 0; i < sublayers.size(); i++) {
        LayerCompositingThread* sublayer = sublayers[i];
        // The root of the 3d-preserving subtree has collected all
        // 3d-preserving layers and their children and will render them all in
        // the right order.
        if (preserves3D && superlayerPreserves3D)
            continue;

         compositeLayersRecursive(sublayer, newStencilValue, clipRect);
    }

    if (stencilClip) {
        glStencilFunc(GL_LEQUAL, stencilValue, 0xff);
        glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);

        updateScissorIfNeeded(clipRect);
        glUseProgram(m_colorProgramObject);
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, &layer->getTransformedBounds());
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        if (!stencilValue)
            glDisable(GL_STENCIL_TEST);
    }
}

void LayerRenderer::updateScissorIfNeeded(const FloatRect& clipRect)
{
#if DEBUG_CLIPPING
    printf("LayerRenderer::updateScissorIfNeeded(): clipRect=(%.2f,%.2f %.2fx%.2f)\n", clipRect.x(), clipRect.y(), clipRect.width(), clipRect.height());
    fflush(stdout);
#endif
    IntRect clipRectWC = toOpenGLWindowCoordinates(clipRect);
    if (m_scissorRect == clipRectWC)
        return;

    m_scissorRect = clipRectWC;
#if DEBUG_CLIPPING
    printf("LayerRenderer::updateScissorIfNeeded(): clipping to (%d,%d %dx%d)\n", m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
    fflush(stdout);
#endif
    glScissor(m_scissorRect.x(), m_scissorRect.y(), m_scissorRect.width(), m_scissorRect.height());
}

bool LayerRenderer::makeContextCurrent()
{
    bool ret = m_context->makeCurrent();
    if (ret && m_isRobustnessSupported) {
        if (m_glGetGraphicsResetStatusEXT() != GL_NO_ERROR) {
            BlackBerry::Platform::logAlways(BlackBerry::Platform::LogLevelCritical, "Robust OpenGL context has been reset. Aborting.");
            CRASH();
        }
    }
    return ret;
}

// Binds the given attribute name to a common location across all programs
// used by the compositor. This allows the code to bind the attributes only once
// even when switching between programs.
void LayerRenderer::bindCommonAttribLocation(int location, const char* attribName)
{
    for (int i = 0; i < LayerData::NumberOfLayerProgramShaders; ++i) {
        glBindAttribLocation(m_layerProgramObject[i], location, attribName);
        glBindAttribLocation(m_layerMaskProgramObject[i], location, attribName);
    }

    glBindAttribLocation(m_colorProgramObject, location, attribName);
    glBindAttribLocation(m_checkerProgramObject, location, attribName);
}

bool LayerRenderer::initializeSharedGLObjects()
{
    // Shaders for drawing the layer contents.
    char vertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "attribute vec2 a_texCoord;   \n"
        "varying vec2 v_texCoord;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_Position = a_position;  \n"
        "  v_texCoord = a_texCoord;   \n"
        "}                            \n";

    char fragmentShaderStringRGBA[] =
        "varying mediump vec2 v_texCoord;                           \n"
        "uniform lowp sampler2D s_texture;                          \n"
        "uniform lowp float alpha;                                  \n"
        "void main()                                                \n"
        "{                                                          \n"
        "  gl_FragColor = texture2D(s_texture, v_texCoord) * alpha; \n"
        "}                                                          \n";

    char fragmentShaderStringBGRA[] =
        "varying mediump vec2 v_texCoord;                                \n"
        "uniform lowp sampler2D s_texture;                               \n"
        "uniform lowp float alpha;                                       \n"
        "void main()                                                     \n"
        "{                                                               \n"
        "  gl_FragColor = texture2D(s_texture, v_texCoord).bgra * alpha; \n"
        "}                                                               \n";

    char fragmentShaderStringMaskRGBA[] =
        "varying mediump vec2 v_texCoord;                           \n"
        "uniform lowp sampler2D s_texture;                          \n"
        "uniform lowp sampler2D s_mask;                             \n"
        "uniform lowp float alpha;                                  \n"
        "void main()                                                \n"
        "{                                                          \n"
        "  lowp vec4 texColor = texture2D(s_texture, v_texCoord);   \n"
        "  lowp vec4 maskColor = texture2D(s_mask, v_texCoord);     \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w;           \n"
        "}                                                          \n";

    char fragmentShaderStringMaskBGRA[] =
        "varying mediump vec2 v_texCoord;                                \n"
        "uniform lowp sampler2D s_texture;                               \n"
        "uniform lowp sampler2D s_mask;                                  \n"
        "uniform lowp float alpha;                                       \n"
        "void main()                                                     \n"
        "{                                                               \n"
        "  lowp vec4 texColor = texture2D(s_texture, v_texCoord).bgra;             \n"
        "  lowp vec4 maskColor = texture2D(s_mask, v_texCoord).bgra;          \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha * maskColor.w;         \n"
        "}                                                               \n";

    // Shaders for drawing the debug borders around the layers.
    char colorVertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "void main()                  \n"
        "{                            \n"
        "   gl_Position = a_position; \n"
        "}                            \n";

    char colorFragmentShaderString[] =
        "uniform lowp vec4 color;     \n"
        "void main()                  \n"
        "{                            \n"
        "  gl_FragColor = color;      \n"
        "}                            \n";

    // FIXME: get screen size, get light/dark color, use
    // string manipulation methods to insert those constants into
    // the shader source.
    static Color lightColor(0xfb, 0xfd, 0xff);
    // checkerboardColorDark()
    static Color darkColor(0xe8, 0xee, 0xf7);
    int checkerSize = 20;
    String tmp(
        "uniform mediump float scale;                  \n"
        "uniform mediump vec2 origin;                  \n"
        "uniform mediump float surfaceHeight;          \n"
        "void main()                                   \n"
        "{                                             \n"
        "  const mediump float grid = GRID;            \n"
        "  const lowp vec4 lightColor = LIGHT_COLOR;   \n"
        "  const lowp vec4 darkColor = DARK_COLOR;     \n"
        "  mediump float tmp = grid * scale;           \n"
        "  gl_FragColor = mod(floor((gl_FragCoord.x + origin.x) / tmp) + floor((surfaceHeight - gl_FragCoord.y + origin.y) / tmp), 2.0) > 0.99 \n"
        "    ? lightColor : darkColor;                 \n"
        "}                                             \n");

    // Let WTF::String be our preprocessor
    tmp.replace("LIGHT_COLOR", String::format("vec4(%.4f, %.4f, %.4f, 1.0)", lightColor.red() / 255.0, lightColor.green() / 255.0, lightColor.blue() / 255.0));
    tmp.replace("DARK_COLOR", String::format("vec4(%.4f, %.4f, %.4f, 1.0)", darkColor.red() / 255.0, darkColor.green() / 255.0, darkColor.blue() / 255.0));
    tmp.replace("GRID", String::format("%.3f", (float)checkerSize));
    CString checkerFragmentShaderString = tmp.latin1();

    if (!makeContextCurrent())
        return false;

    m_layerProgramObject[LayerData::LayerProgramShaderRGBA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringRGBA);
    if (!m_layerProgramObject[LayerData::LayerProgramShaderRGBA])
        LOG_ERROR("Failed to create shader program for RGBA layers");

    m_layerProgramObject[LayerData::LayerProgramShaderBGRA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringBGRA);
    if (!m_layerProgramObject[LayerData::LayerProgramShaderBGRA])
        LOG_ERROR("Failed to create shader program for BGRA layers");

    m_layerMaskProgramObject[LayerData::LayerProgramShaderRGBA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringMaskRGBA);
    if (!m_layerMaskProgramObject[LayerData::LayerProgramShaderRGBA])
        LOG_ERROR("Failed to create shader mask program for RGBA layers");

    m_layerMaskProgramObject[LayerData::LayerProgramShaderBGRA] =
        loadShaderProgram(vertexShaderString, fragmentShaderStringMaskBGRA);
    if (!m_layerMaskProgramObject[LayerData::LayerProgramShaderBGRA])
        LOG_ERROR("Failed to create shader mask program for BGRA layers");

    m_colorProgramObject = loadShaderProgram(colorVertexShaderString, colorFragmentShaderString);
    if (!m_colorProgramObject)
        LOG_ERROR("Failed to create shader program for debug borders");

    m_checkerProgramObject = loadShaderProgram(colorVertexShaderString, checkerFragmentShaderString.data());
    if (!m_checkerProgramObject)
        LOG_ERROR("Failed to create shader program for checkerboard pattern");

    // Specify the attrib location for the position and make it the same for all programs to
    // avoid binding re-binding the vertex attributes.
    bindCommonAttribLocation(m_positionLocation, "a_position");
    bindCommonAttribLocation(m_texCoordLocation, "a_texCoord");

    checkGLError();

    // Re-link the shaders to get the new attrib location to take effect.
    for (int i = 0; i < LayerData::NumberOfLayerProgramShaders; ++i) {
        glLinkProgram(m_layerProgramObject[i]);
        glLinkProgram(m_layerMaskProgramObject[i]);
    }

    glLinkProgram(m_colorProgramObject);
    glLinkProgram(m_checkerProgramObject);

    checkGLError();

    // Get locations of uniforms for the layer content shader program.
    for (int i = 0; i < LayerData::NumberOfLayerProgramShaders; ++i) {
        m_samplerLocation[i] = glGetUniformLocation(m_layerProgramObject[i], "s_texture");
        m_alphaLocation[i] = glGetUniformLocation(m_layerProgramObject[i], "alpha");
        glUseProgram(m_layerProgramObject[i]);
        glUniform1i(m_samplerLocation[i], 0);
        m_maskSamplerLocation[i] = glGetUniformLocation(m_layerMaskProgramObject[i], "s_texture");
        m_maskSamplerLocationMask[i] = glGetUniformLocation(m_layerMaskProgramObject[i], "s_mask");
        m_maskAlphaLocation[i] = glGetUniformLocation(m_layerMaskProgramObject[i], "alpha");
        glUseProgram(m_layerMaskProgramObject[i]);
        glUniform1i(m_maskSamplerLocation[i], 0);
        glUniform1i(m_maskSamplerLocationMask[i], 1);
    }

    // Get locations of uniforms for the debug border shader program.
    m_colorColorLocation = glGetUniformLocation(m_colorProgramObject, "color");

    // Get locations of uniforms for the checkerboard shader program.
    m_checkerScaleLocation = glGetUniformLocation(m_checkerProgramObject, "scale");
    m_checkerOriginLocation = glGetUniformLocation(m_checkerProgramObject, "origin");
    m_checkerSurfaceHeightLocation = glGetUniformLocation(m_checkerProgramObject, "surfaceHeight");

    return true;
}

IntRect LayerRenderingResults::holePunchRect(unsigned index) const
{
    if (index >= m_holePunchRects.size())
        return IntRect();

    return m_holePunchRects.at(index);
}

void LayerRenderingResults::addHolePunchRect(const IntRect& rect)
{
#if DEBUG_CLIPPING
    printf("LayerRenderingResults::addHolePunchRect (%d,%d %dx%d)\n", rect.x(), rect.y(), rect.width(), rect.height());
    fflush(stdout);
#endif
    if (!rect.isEmpty())
        m_holePunchRects.append(rect);
}

void LayerRenderingResults::addDirtyRect(const IntRect& rect)
{
    IntRect dirtyUnion[NumberOfDirtyRects];
    int smallestIncrease = INT_MAX;
    int modifiedRect = 0;
    for (int i = 0; i < NumberOfDirtyRects; ++i) {
        dirtyUnion[i] = m_dirtyRects[i];
        dirtyUnion[i].unite(rect);
        int increase = dirtyUnion[i].width()*dirtyUnion[i].height() - m_dirtyRects[i].width()*m_dirtyRects[i].height();
        if (increase < smallestIncrease) {
            smallestIncrease = increase;
            modifiedRect = i;
        }
    }

    m_dirtyRects[modifiedRect] = dirtyUnion[modifiedRect];
}

bool LayerRenderingResults::isEmpty() const
{
    for (int i = 0; i < NumberOfDirtyRects; ++i) {
        if (!m_dirtyRects[i].isEmpty())
            return false;
    }
    return true;
}

} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
