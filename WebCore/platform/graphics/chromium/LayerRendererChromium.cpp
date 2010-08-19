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
#include "LayerRendererChromium.h"

#include "CanvasLayerChromium.h"
#include "GLES2Context.h"
#include "LayerChromium.h"
#include "NotImplemented.h"
#include "TransformLayerChromium.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#elif PLATFORM(CG)
#include <CoreGraphics/CGBitmapContext.h>
#endif

#include <GLES2/gl2.h>

namespace WebCore {

#ifndef NDEBUG
static WTFLogChannel LogLayerRenderer = { 0x00000000, "LayerRenderer", WTFLogChannelOn };
#endif

static void checkGLError()
{
#ifndef NDEBUG
    GLenum error = glGetError();
    if (error)
        LOG_ERROR("GL Error: %d " , error);
#endif
}

static GLuint loadShader(GLenum type, const char* shaderSource)
{
    GLuint shader = glCreateShader(type);
    if (!shader)
        return 0;
    glShaderSource(shader, 1, &shaderSource, 0);
    glCompileShader(shader);
    GLint compiled;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled) {
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

static GLuint loadShaderProgram(const char* vertexShaderSource, const char* fragmentShaderSource)
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
    if (!programObject)
        return 0;
    glAttachShader(programObject, vertexShader);
    glAttachShader(programObject, fragmentShader);
    glLinkProgram(programObject);
    glGetProgramiv(programObject, GL_LINK_STATUS, &linked);
    if (!linked) {
        glDeleteProgram(programObject);
        return 0;
    }
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return programObject;
}

bool LayerRendererChromium::createLayerShader(ShaderProgramType type, const char* vertexShaderSource, const char* fragmentShaderSource)
{
    unsigned programId = loadShaderProgram(vertexShaderSource, fragmentShaderSource);
    ASSERT(programId);

    ShaderProgram* program = &m_shaderPrograms[type];

    program->m_shaderProgramId = programId;
    program->m_samplerLocation = glGetUniformLocation(programId, "s_texture");
    program->m_matrixLocation = glGetUniformLocation(programId, "matrix");
    program->m_alphaLocation = glGetUniformLocation(programId, "alpha");

    return programId;
}


static void toGLMatrix(float* flattened, const TransformationMatrix& m)
{
    flattened[0] = m.m11();
    flattened[1] = m.m12();
    flattened[2] = m.m13();
    flattened[3] = m.m14();
    flattened[4] = m.m21();
    flattened[5] = m.m22();
    flattened[6] = m.m23();
    flattened[7] = m.m24();
    flattened[8] = m.m31();
    flattened[9] = m.m32();
    flattened[10] = m.m33();
    flattened[11] = m.m34();
    flattened[12] = m.m41();
    flattened[13] = m.m42();
    flattened[14] = m.m43();
    flattened[15] = m.m44();
}

static TransformationMatrix orthoMatrix(float left, float right, float bottom, float top, float nearZ, float farZ)
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

// Creates a GL texture object to be used for transfering the layer's bitmap into.
static GLuint createLayerTexture()
{
    GLuint textureId = 0;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);
    // Do basic linear filtering on resize.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    // NPOT textures in GL ES only work when the wrap mode is set to GL_CLAMP_TO_EDGE.
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return textureId;
}

static inline bool compareLayerZ(const LayerChromium* a, const LayerChromium* b)
{
    const TransformationMatrix& transformA = a->drawTransform();
    const TransformationMatrix& transformB = b->drawTransform();

    return transformA.m43() < transformB.m43();
}

ShaderProgram::ShaderProgram()
    : m_shaderProgramId(0)
    , m_samplerLocation(-1)
    , m_matrixLocation(-1)
    , m_alphaLocation(-1)
{
}

PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create(PassOwnPtr<GLES2Context> gles2Context)
{
    return new LayerRendererChromium(gles2Context);
}

LayerRendererChromium::LayerRendererChromium(PassOwnPtr<GLES2Context> gles2Context)
    : m_rootLayerTextureWidth(0)
    , m_rootLayerTextureHeight(0)
    , m_positionLocation(0)
    , m_texCoordLocation(1)
    , m_rootLayer(0)
    , m_needsDisplay(false)
    , m_scrollPosition(IntPoint(-1, -1))
    , m_currentShaderProgramType(NumShaderProgramTypes)
    , m_gles2Context(gles2Context)
{
    m_quadVboIds[Vertices] = m_quadVboIds[LayerElements] = 0;
    m_hardwareCompositing = (m_gles2Context && initializeSharedGLObjects());
}

LayerRendererChromium::~LayerRendererChromium()
{
    if (m_hardwareCompositing) {
        makeContextCurrent();
        glDeleteBuffers(3, m_quadVboIds);

        for (int i = 0; i < NumShaderProgramTypes; i++) {
            if (m_shaderPrograms[i].m_shaderProgramId)
                glDeleteProgram(m_shaderPrograms[i].m_shaderProgramId);
        }
    }

    // Free up all GL textures.
    for (TextureIdMap::iterator iter = m_textureIdMap.begin(); iter != m_textureIdMap.end(); ++iter) {
        glDeleteTextures(1, &(iter->second));
        iter->first->setLayerRenderer(0);
    }
}

// Creates a canvas and an associated graphics context that the root layer will
// render into.
void LayerRendererChromium::setRootLayerCanvasSize(const IntSize& size)
{
    if (size == m_rootLayerCanvasSize)
        return;

#if PLATFORM(SKIA)
    // Create new canvas and context. OwnPtr takes care of freeing up
    // the old ones.
    m_rootLayerCanvas = new skia::PlatformCanvas(size.width(), size.height(), false);
    m_rootLayerSkiaContext = new PlatformContextSkia(m_rootLayerCanvas.get());
#if OS(WINDOWS)
    // FIXME: why is this is a windows-only call ?
    m_rootLayerSkiaContext->setDrawingToImageBuffer(true);
#endif
    m_rootLayerGraphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_rootLayerSkiaContext.get()));
#elif PLATFORM(CG)
    // Release the previous CGBitmapContext before reallocating the backing store as a precaution.
    m_rootLayerCGContext.adoptCF(0);
    int rowBytes = 4 * size.width();
    m_rootLayerBackingStore.resize(rowBytes * size.height());
    memset(m_rootLayerBackingStore.data(), 0, m_rootLayerBackingStore.size());
    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    m_rootLayerCGContext.adoptCF(CGBitmapContextCreate(m_rootLayerBackingStore.data(),
                                                       size.width(), size.height(), 8, rowBytes,
                                                       colorSpace.get(),
                                                       kCGImageAlphaPremultipliedLast));
    m_rootLayerGraphicsContext = new GraphicsContext(m_rootLayerCGContext.get());
#else
#error "Need to implement for your platform."
#endif

    m_rootLayerCanvasSize = size;
}

void LayerRendererChromium::useShaderProgram(ShaderProgramType programType)
{
    if (programType != m_currentShaderProgramType) {
        ShaderProgram* program = &m_shaderPrograms[programType];
        glUseProgram(program->m_shaderProgramId);
        m_currentShaderProgramType = programType;

        // Set the uniform locations matching the program.
        m_samplerLocation = program->m_samplerLocation;
        m_matrixLocation = program->m_matrixLocation;
        m_alphaLocation = program->m_alphaLocation;
    }
}

void LayerRendererChromium::drawTexturedQuad(const TransformationMatrix& matrix, float width, float height, float opacity)
{
    static GLfloat glMatrix[16];

    TransformationMatrix renderMatrix = matrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    renderMatrix.multiply(m_projectionMatrix);

    toGLMatrix(&glMatrix[0], renderMatrix);

    glUniformMatrix4fv(m_matrixLocation, 1, false, &glMatrix[0]);

    if (m_alphaLocation != -1)
        glUniform1f(m_alphaLocation, opacity);

    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, 0);
}


// Updates the contents of the root layer texture that fall inside the updateRect
// and re-composits all sublayers.
void LayerRendererChromium::drawLayers(const IntRect& updateRect, const IntRect& visibleRect,
                                       const IntRect& contentRect, const IntPoint& scrollPosition)
{
    ASSERT(m_hardwareCompositing);

    if (!m_rootLayer)
        return;

    // If the size of the visible area has changed then allocate a new texture
    // to store the contents of the root layer and adjust the projection matrix
    // and viewport.
    makeContextCurrent();

    checkGLError();

    glBindTexture(GL_TEXTURE_2D, m_rootLayerTextureId);

    checkGLError();

    int visibleRectWidth = visibleRect.width();
    int visibleRectHeight = visibleRect.height();
    if (visibleRectWidth != m_rootLayerTextureWidth || visibleRectHeight != m_rootLayerTextureHeight) {
        m_rootLayerTextureWidth = visibleRect.width();
        m_rootLayerTextureHeight = visibleRect.height();

        m_projectionMatrix = orthoMatrix(0, visibleRectWidth, visibleRectHeight, 0, -1000, 1000);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_rootLayerTextureWidth, m_rootLayerTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);

        checkGLError();
    }

    // The GL viewport covers the entire visible area, including the scrollbars.
    glViewport(0, 0, visibleRectWidth, visibleRectHeight);

    checkGLError();

    // The layer, scroll and debug border shaders all use the same vertex attributes
    // so we can bind them only once.
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVboIds[Vertices]);
    checkGLError();
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadVboIds[LayerElements]);
    checkGLError();
    GLuint offset = 0;
    glVertexAttribPointer(m_positionLocation, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(offset));
    checkGLError();
    offset += 3 * sizeof(GLfloat);
    glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(offset));
    checkGLError();
    glEnableVertexAttribArray(m_positionLocation);
    checkGLError();
    glEnableVertexAttribArray(m_texCoordLocation);
    checkGLError();
    glActiveTexture(GL_TEXTURE0);
    checkGLError();
    glDisable(GL_DEPTH_TEST);
    checkGLError();
    glDisable(GL_CULL_FACE);
    checkGLError();

    if (m_scrollPosition == IntPoint(-1, -1))
        m_scrollPosition = scrollPosition;

    IntPoint scrollDelta = toPoint(scrollPosition - m_scrollPosition);
    // Scroll only when the updateRect contains pixels for the newly uncovered region to avoid flashing.
    if ((scrollDelta.x() && updateRect.width() >= abs(scrollDelta.x()) && updateRect.height() >= contentRect.height())
        || (scrollDelta.y() && updateRect.height() >= abs(scrollDelta.y()) && updateRect.width() >= contentRect.width())) {
        // Scrolling works as follows: We render a quad with the current root layer contents
        // translated by the amount the page has scrolled since the last update and then read the
        // pixels of the content area (visible area excluding the scroll bars) back into the
        // root layer texture. The newly exposed area is subesquently filled as usual with
        // the contents of the updateRect.
        TransformationMatrix scrolledLayerMatrix;
#if PLATFORM(SKIA)
        float scaleFactor = 1.0f;
#elif PLATFORM(CG)
        // Because the contents of the OpenGL texture are inverted
        // vertically compared to the Skia backend, we need to move
        // the backing store in the opposite direction.
        float scaleFactor = -1.0f;
#else
#error "Need to implement for your platform."
#endif

        scrolledLayerMatrix.translate3d(0.5 * visibleRect.width() - scrollDelta.x(),
            0.5 * visibleRect.height() + scaleFactor * scrollDelta.y(), 0);
        scrolledLayerMatrix.scale3d(1, -1, 1);

        // Switch shaders to avoid RGB swizzling.
        useShaderProgram(ScrollLayerProgram);
        glUniform1i(m_shaderPrograms[ScrollLayerProgram].m_samplerLocation, 0);
        checkGLError();

        drawTexturedQuad(scrolledLayerMatrix, visibleRect.width(), visibleRect.height(), 1);
        checkGLError();

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, contentRect.width(), contentRect.height());

        checkGLError();
        m_scrollPosition = scrollPosition;
    } else if (abs(scrollDelta.y()) > contentRect.height() || abs(scrollDelta.x()) > contentRect.width()) {
        // Scrolling larger than the contentRect size does not preserve any of the pixels, so there is
        // no need to copy framebuffer pixels back into the texture.
        m_scrollPosition = scrollPosition;
    }

    // FIXME: The following check should go away when the compositor renders independently from its own thread.
    // Ignore a 1x1 update rect at (0, 0) as that's used a way to kick off a redraw for the compositor.
    if (!(!updateRect.x() && !updateRect.y() && updateRect.width() == 1 && updateRect.height() == 1)) {
        // Update the root layer texture.
        ASSERT((updateRect.x() + updateRect.width() <= m_rootLayerTextureWidth)
               && (updateRect.y() + updateRect.height() <= m_rootLayerTextureHeight));

#if PLATFORM(SKIA)
        // Get the contents of the updated rect.
        const SkBitmap bitmap = m_rootLayerCanvas->getDevice()->accessBitmap(false);
        int rootLayerWidth = bitmap.width();
        int rootLayerHeight = bitmap.height();
        ASSERT(rootLayerWidth == updateRect.width() && rootLayerHeight == updateRect.height());
        void* pixels = bitmap.getPixels();

        checkGLError();
        // Copy the contents of the updated rect to the root layer texture.
        glTexSubImage2D(GL_TEXTURE_2D, 0, updateRect.x(), updateRect.y(), updateRect.width(), updateRect.height(), GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        checkGLError();
#elif PLATFORM(CG)
        // Get the contents of the updated rect.
        ASSERT(static_cast<int>(CGBitmapContextGetWidth(m_rootLayerCGContext.get())) == updateRect.width() && static_cast<int>(CGBitmapContextGetHeight(m_rootLayerCGContext.get())) == updateRect.height());
        void* pixels = m_rootLayerBackingStore.data();

        checkGLError();
        // Copy the contents of the updated rect to the root layer texture.
        // The origin is at the lower left in Core Graphics' coordinate system. We need to correct for this here.
        glTexSubImage2D(GL_TEXTURE_2D, 0,
                        updateRect.x(), m_rootLayerTextureHeight - updateRect.y() - updateRect.height(),
                        updateRect.width(), updateRect.height(),
                        GL_RGBA, GL_UNSIGNED_BYTE, pixels);
        checkGLError();
#else
#error "Need to implement for your platform."
#endif
    }

    glClearColor(0, 0, 1, 1);
    checkGLError();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    checkGLError();

    // Render the root layer using a quad that takes up the entire visible area of the window.
    useShaderProgram(ContentLayerProgram);
    checkGLError();
    glUniform1i(m_samplerLocation, 0);
    checkGLError();
    TransformationMatrix layerMatrix;
    layerMatrix.translate3d(visibleRect.width() * 0.5f, visibleRect.height() * 0.5f, 0);
    drawTexturedQuad(layerMatrix, visibleRect.width(), visibleRect.height(), 1);
    checkGLError();

    // If culling is enabled then we will cull the backface.
    glCullFace(GL_BACK);
    checkGLError();
    // The orthographic projection is setup such that Y starts at zero and
    // increases going down the page so we need to adjust the winding order of
    // front facing triangles.
    glFrontFace(GL_CW);
    checkGLError();

    // The shader used to render layers returns pre-multiplied alpha colors
    // so we need to send the blending mode appropriately.
    glEnable(GL_BLEND);
    checkGLError();
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    checkGLError();

    // Translate all the composited layers by the scroll position.
    TransformationMatrix matrix;
    matrix.translate3d(-m_scrollPosition.x(), -m_scrollPosition.y(), 0);

    float opacity = 1;
    m_layerList.shrink(0);
    const Vector<RefPtr<LayerChromium> >& sublayers = m_rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayersRecursive(sublayers[i].get(), matrix, opacity, visibleRect);

    // Sort layers by the z coordinate of their center so that layers further
    // away get drawn first.
    std::stable_sort(m_layerList.begin(), m_layerList.end(), compareLayerZ);

    // Enable scissoring to avoid rendering composited layers over the scrollbars.
    glEnable(GL_SCISSOR_TEST);
    glScissor(0, visibleRect.height() - contentRect.height(), contentRect.width(), contentRect.height());

    for (size_t j = 0; j < m_layerList.size(); j++)
        drawLayer(m_layerList[j]);

    glDisable(GL_SCISSOR_TEST);

    glFlush();
    m_gles2Context->swapBuffers();

    m_needsDisplay = false;
}

// Returns the id of the texture currently associated with the layer or
// -1 if the id hasn't been registered yet.
int LayerRendererChromium::getTextureId(LayerChromium* layer)
{
    TextureIdMap::iterator textureId = m_textureIdMap.find(layer);
    if (textureId != m_textureIdMap.end())
        return textureId->second;

    return -1;
}

// Allocates a new texture for the layer and registers it in the textureId map.
// FIXME: We will need to come up with a more sophisticated allocation strategy here.
int LayerRendererChromium::assignTextureForLayer(LayerChromium* layer)
{
    GLuint textureId = createLayerTexture();

    // FIXME: Check that textureId is valid
    m_textureIdMap.set(layer, textureId);

    layer->setLayerRenderer(this);

    return textureId;
}

bool LayerRendererChromium::freeLayerTexture(LayerChromium* layer)
{
    TextureIdMap::iterator textureId = m_textureIdMap.find(layer);
    if (textureId == m_textureIdMap.end())
        return false;
    // Free up the texture.
    glDeleteTextures(1, &(textureId->second));
    m_textureIdMap.remove(textureId);
    return true;
}

// Draws a debug border around the layer's bounds.
void LayerRendererChromium::drawDebugBorder(LayerChromium* layer, const TransformationMatrix& matrix)
{
    static GLfloat glMatrix[16];
    Color borderColor = layer->borderColor();
    if (!borderColor.alpha())
        return;

    useShaderProgram(DebugBorderProgram);
    TransformationMatrix renderMatrix = matrix;
    IntSize bounds = layer->bounds();
    renderMatrix.scale3d(bounds.width(), bounds.height(), 1);
    renderMatrix.multiply(m_projectionMatrix);
    toGLMatrix(&glMatrix[0], renderMatrix);
    unsigned borderMatrixLocation = m_shaderPrograms[DebugBorderProgram].m_matrixLocation;
    glUniformMatrix4fv(borderMatrixLocation, 1, false, &glMatrix[0]);

    glUniform4f(m_borderColorLocation, borderColor.red() / 255.0,
                                       borderColor.green() / 255.0,
                                       borderColor.blue() / 255.0,
                                       1);

    glLineWidth(layer->borderWidth());

    // The indices for the line are stored in the same array as the triangle indices.
    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, (void*)(6 * sizeof(unsigned short)));
    checkGLError();
}

// Returns true if any part of the layer falls within the visibleRect
bool LayerRendererChromium::isLayerVisible(LayerChromium* layer, const TransformationMatrix& matrix, const IntRect& visibleRect)
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into clip space.
    TransformationMatrix renderMatrix = matrix;
    renderMatrix.scale3d(layer->bounds().width(), layer->bounds().height(), 1);
    renderMatrix.multiply(m_projectionMatrix);

    FloatRect layerRect(-0.5, -0.5, 1, 1);
    FloatRect mappedRect = renderMatrix.mapRect(layerRect);

    // The layer is visible if it intersects any part of a rectangle whose origin
    // is at (-1, -1) and size is 2x2.
    return mappedRect.intersects(FloatRect(-1, -1, 2, 2));
}

// Updates and caches the layer transforms and opacity values that will be used
// when rendering them.
void LayerRendererChromium::updateLayersRecursive(LayerChromium* layer, const TransformationMatrix& parentMatrix, float opacity, const IntRect& visibleRect)
{
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
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();

    // Offset between anchor point and the center of the quad.
    float centerOffsetX = (0.5 - anchorPoint.x()) * bounds.width();
    float centerOffsetY = (0.5 - anchorPoint.y()) * bounds.height();

    // M = M[p]
    TransformationMatrix localMatrix = parentMatrix;
    // M = M[p] * Tr[l]
    localMatrix.translate3d(position.x(), position.y(), layer->anchorPointZ());
    // M = M[p] * Tr[l] * M[l]
    localMatrix.multLeft(layer->transform());
    // M = M[p] * Tr[l] * M[l] * Tr[c]
    localMatrix.translate3d(centerOffsetX, centerOffsetY, -layer->anchorPointZ());

    // Check if the layer falls within the visible bounds of the page.
    bool layerVisible = isLayerVisible(layer, localMatrix, visibleRect);

    bool skipLayer = false;
    if (bounds.width() > 2048 || bounds.height() > 2048) {
        if (layer->drawsContent())
            LOG(LayerRenderer, "Skipping layer with size %d %d", bounds.width(), bounds.height());
        skipLayer = true;
    }

    // Calculate the layer's opacity.
    opacity *= layer->opacity();

    layer->setDrawTransform(localMatrix);
    layer->setDrawOpacity(opacity);
    if (layerVisible && !skipLayer)
        m_layerList.append(layer);

    // Flatten to 2D if the layer doesn't preserve 3D.
    if (!layer->preserves3D()) {
        localMatrix.setM13(0);
        localMatrix.setM23(0);
        localMatrix.setM31(0);
        localMatrix.setM32(0);
        localMatrix.setM33(1);
        localMatrix.setM34(0);
        localMatrix.setM43(0);
    }

    // Apply the sublayer transform at the center of the layer.
    localMatrix.multLeft(layer->sublayerTransform());

    // The origin of the sublayers is actually the bottom left corner of the layer
    // (or top left when looking it it from the browser's pespective) instead of the center.
    // The matrix passed down to the sublayers is therefore:
    // M[s] = M * Tr[-center]
    localMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);

    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        updateLayersRecursive(sublayers[i].get(), localMatrix, opacity, visibleRect);
}

void LayerRendererChromium::drawLayer(LayerChromium* layer)
{
    const TransformationMatrix& localMatrix = layer->drawTransform();
    IntSize bounds = layer->bounds();

    if (layer->drawsContent()) {
        int textureId;
        if (layer->ownsTexture())
            textureId = layer->textureId();
        else {
            textureId = getTextureId(layer);
            // If no texture has been created for the layer yet then create one now.
            if (textureId == -1)
                textureId = assignTextureForLayer(layer);
        }

        // Redraw the contents of the layer if necessary.
        if (layer->contentsDirty()) {
            // Update the backing texture contents for any dirty portion of the layer.
            layer->updateTextureContents(textureId);
            m_gles2Context->makeCurrent();
        }

        if (layer->doubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        glBindTexture(GL_TEXTURE_2D, textureId);
        useShaderProgram(static_cast<ShaderProgramType>(layer->shaderProgramId()));
        drawTexturedQuad(localMatrix, bounds.width(), bounds.height(), layer->drawOpacity());
    }

    // Draw the debug border if there is one.
    drawDebugBorder(layer, localMatrix);
}

bool LayerRendererChromium::makeContextCurrent()
{
    return m_gles2Context->makeCurrent();
}

void LayerRendererChromium::bindCommonAttribLocations(ShaderProgramType program)
{
    unsigned programId = m_shaderPrograms[program].m_shaderProgramId;
    glBindAttribLocation(programId, m_positionLocation, "a_position");
    glBindAttribLocation(programId, m_texCoordLocation, "a_texCoord");

    // Re-link the program for the new attribute locations to take effect.
    glLinkProgram(programId);
    checkGLError();
}

bool LayerRendererChromium::initializeSharedGLObjects()
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
    // Note differences between Skia and Core Graphics versions:
    //  - Skia uses BGRA and origin is upper left
    //  - Core Graphics uses RGBA and origin is lower left
    char fragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
#if PLATFORM(SKIA)
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha; \n"
#elif PLATFORM(CG)
        "  vec4 texColor = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
#else
#error "Need to implement for your platform."
#endif
        "}                                                   \n";

    // Fragment shader used for rendering the scrolled root layer quad. It differs
    // from fragmentShaderString in that it doesn't swizzle the colors and doesn't
    // take an alpha value.
    char scrollFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w); \n"
        "}                                                   \n";

    // Canvas layers need to be flipped vertically and their colors shouldn't be
    // swizzled.
    char canvasFragmentShaderString[] =
        "precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, vec2(v_texCoord.x, 1.0 - v_texCoord.y)); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w) * alpha; \n"
        "}                                                   \n";

    // Shaders for drawing the debug borders around the layers.
    char borderVertexShaderString[] =
        "attribute vec4 a_position;   \n"
        "uniform mat4 matrix;         \n"
        "void main()                  \n"
        "{                            \n"
        "   gl_Position = matrix * a_position; \n"
        "}                            \n";
    char borderFragmentShaderString[] =
        "precision mediump float;                            \n"
        "uniform vec4 color;                                 \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  gl_FragColor = color;                             \n"
        "}                                                   \n";

    GLfloat vertices[] = { -0.5f,  0.5f, 0.0f, // Position 0
                            0.0f,  1.0f, // TexCoord 0 
                           -0.5f, -0.5f, 0.0f, // Position 1
                            0.0f,  0.0f, // TexCoord 1
                            0.5f, -0.5f, 0.0f, // Position 2
                            1.0f,  0.0f, // TexCoord 2
                            0.5f,  0.5f, 0.0f, // Position 3
                            1.0f,  1.0f // TexCoord 3
                         };
    GLushort indices[] = { 0, 1, 2, 0, 2, 3, // The two triangles that make up the layer quad.
                           0, 1, 2, 3}; // A line path for drawing the layer border.

    makeContextCurrent();

    if (!createLayerShader(ContentLayerProgram, vertexShaderString, fragmentShaderString)) {
        LOG_ERROR("Failed to create shader program for content layers");
        return false;
    }
    LayerChromium::setShaderProgramId(ContentLayerProgram);

    if (!createLayerShader(CanvasLayerProgram, vertexShaderString, canvasFragmentShaderString)) {
        LOG_ERROR("Failed to create shader program for Canvas layers");
        return false;
    }
    CanvasLayerChromium::setShaderProgramId(CanvasLayerProgram);

    if (!createLayerShader(ScrollLayerProgram, vertexShaderString, scrollFragmentShaderString)) {
        LOG_ERROR("Failed to create shader program for scrolling layer");
        return false;
    }

    if (!createLayerShader(DebugBorderProgram, borderVertexShaderString, borderFragmentShaderString)) {
        LOG_ERROR("Failed to create shader program for debug borders");
        return false;
    }

    // Specify the attrib location for the position and texCoord and make it the same for all programs to
    // avoid binding re-binding the vertex attributes.
    bindCommonAttribLocations(ContentLayerProgram);
    bindCommonAttribLocations(CanvasLayerProgram);
    bindCommonAttribLocations(DebugBorderProgram);
    bindCommonAttribLocations(ScrollLayerProgram);

    // Get the location of the color uniform for the debug border shader program.
    m_borderColorLocation = glGetUniformLocation(m_shaderPrograms[DebugBorderProgram].m_shaderProgramId, "color");

    glGenBuffers(3, m_quadVboIds);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVboIds[Vertices]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadVboIds[LayerElements]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Create a texture object to hold the contents of the root layer.
    m_rootLayerTextureId = createLayerTexture();
    if (!m_rootLayerTextureId) {
        LOG_ERROR("Failed to create texture for root layer");
        return false;
    }
    // Turn off filtering for the root layer to avoid blurring from the repeated
    // writes and reads to the framebuffer that happen while scrolling.
    glBindTexture(GL_TEXTURE_2D, m_rootLayerTextureId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}
} // namespace WebCore

#endif // USE(ACCELERATED_COMPOSITING)
