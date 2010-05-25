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

#include "GLES2Context.h"
#include "LayerChromium.h"
#include "NotImplemented.h"
#include "Page.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif

#include <GLES2/gl2.h>

namespace WebCore {

static WTFLogChannel LogLayerRenderer = { 0x00000000, "LayerRenderer", WTFLogChannelOn };

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


PassOwnPtr<LayerRendererChromium> LayerRendererChromium::create(Page* page)
{
    return new LayerRendererChromium(page);
}

LayerRendererChromium::LayerRendererChromium(Page* page)
    : m_rootLayer(0)
    , m_needsDisplay(false)
    , m_layerProgramObject(0)
    , m_borderProgramObject(0)
    , m_scrollProgramObject(0)
    , m_positionLocation(0)
    , m_texCoordLocation(1)
    , m_page(page)
    , m_rootLayerTextureWidth(0)
    , m_rootLayerTextureHeight(0)
{
    m_quadVboIds[Vertices] = m_quadVboIds[LayerElements] = 0;
    m_hardwareCompositing = (initGL() && initializeSharedGLObjects());
}

LayerRendererChromium::~LayerRendererChromium()
{
    if (m_hardwareCompositing) {
        makeContextCurrent();
        glDeleteBuffers(3, m_quadVboIds);
        glDeleteProgram(m_layerProgramObject);
        glDeleteProgram(m_scrollProgramObject);
        glDeleteProgram(m_borderProgramObject);
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
    m_rootLayerSkiaContext->setDrawingToImageBuffer(true);
    m_rootLayerGraphicsContext = new GraphicsContext(reinterpret_cast<PlatformGraphicsContext*>(m_rootLayerSkiaContext.get()));
#else
#error "Need to implement for your platform."
#endif

    m_rootLayerCanvasSize = size;
}

void LayerRendererChromium::drawTexturedQuad(const TransformationMatrix& matrix, float width, float height, float opacity, bool scrolling)
{
    static GLfloat glMatrix[16];

    TransformationMatrix renderMatrix = matrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    renderMatrix.multiply(m_projectionMatrix);

    toGLMatrix(&glMatrix[0], renderMatrix);

    int matrixLocation = (scrolling ? m_scrollMatrixLocation : m_matrixLocation);
    glUniformMatrix4fv(matrixLocation, 1, false, &glMatrix[0]);

    if (!scrolling)
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

    glBindTexture(GL_TEXTURE_2D, m_rootLayerTextureId);

    unsigned int visibleRectWidth = visibleRect.width();
    unsigned int visibleRectHeight = visibleRect.height();
    if (visibleRectWidth != m_rootLayerTextureWidth || visibleRectHeight != m_rootLayerTextureHeight) {
        m_rootLayerTextureWidth = visibleRect.width();
        m_rootLayerTextureHeight = visibleRect.height();

        m_projectionMatrix = orthoMatrix(0, visibleRectWidth + 0.5, visibleRectHeight + 0.5, 0, -1000, 1000);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_rootLayerTextureWidth, m_rootLayerTextureHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    }

    // The GL viewport covers the entire visible area, including the scrollbars.
    glViewport(0, 0, visibleRectWidth, visibleRectHeight);

    // The layer, scroll and debug border shaders all use the same vertex attributes
    // so we can bind them only once.
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVboIds[Vertices]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadVboIds[LayerElements]);
    GLuint offset = 0;
    glVertexAttribPointer(m_positionLocation, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(offset));
    offset += 3 * sizeof(GLfloat);
    glVertexAttribPointer(m_texCoordLocation, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(GLfloat), (GLvoid*)(offset));
    glEnableVertexAttribArray(m_positionLocation);
    glEnableVertexAttribArray(m_texCoordLocation);
    glActiveTexture(GL_TEXTURE0);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);

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
        scrolledLayerMatrix.translate3d((int)floorf(0.5 * visibleRect.width() + 0.5) - scrollDelta.x(),
            (int)floorf(0.5 * visibleRect.height() + 0.5) + scrollDelta.y(), 0);
        scrolledLayerMatrix.scale3d(1, -1, 1);

        // Switch shaders to avoid RGB swizzling.
        glUseProgram(m_scrollProgramObject);
        glUniform1i(m_scrollSamplerLocation, 0);

        drawTexturedQuad(scrolledLayerMatrix, visibleRect.width(), visibleRect.height(), 1, true);

        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, contentRect.width(), contentRect.height());

        checkGLError();
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
#else
#error Must port to your platform
#endif
    }

    glClearColor(0, 0, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render the root layer using a quad that takes up the entire visible area of the window.
    glUseProgram(m_layerProgramObject);
    glUniform1i(m_samplerLocation, 0);
    TransformationMatrix layerMatrix;
    layerMatrix.translate3d(visibleRect.width() / 2, visibleRect.height() / 2, 0);
    drawTexturedQuad(layerMatrix, visibleRect.width(), visibleRect.height(), 1, false);

    // If culling is enabled then we will cull the backface.
    glCullFace(GL_BACK);
    // The orthographic projection is setup such that Y starts at zero and
    // increases going down the page so we need to adjust the winding order of
    // front facing triangles.
    glFrontFace(GL_CW);

    // The shader used to render layers returns pre-multiplied alpha colors
    // so we need to send the blending mode appropriately.
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    checkGLError();

    // FIXME: Need to prevent composited layers from drawing over the scroll
    // bars.

    // FIXME: Sublayers need to be sorted in Z to get the correct transparency effect.

    // Translate all the composited layers by the scroll position.
    TransformationMatrix matrix;
    matrix.translate3d(-m_scrollPosition.x(), -m_scrollPosition.y(), 0);
    float opacity = 1;
    const Vector<RefPtr<LayerChromium> >& sublayers = m_rootLayer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        compositeLayersRecursive(sublayers[i].get(), matrix, opacity, visibleRect);

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

    glUseProgram(m_borderProgramObject);
    TransformationMatrix renderMatrix = matrix;
    IntSize bounds = layer->bounds();
    renderMatrix.scale3d(bounds.width(), bounds.height(), 1);
    renderMatrix.multiply(m_projectionMatrix);
    toGLMatrix(&glMatrix[0], renderMatrix);
    glUniformMatrix4fv(m_borderMatrixLocation, 1, false, &glMatrix[0]);

    glUniform4f(m_borderColorLocation, borderColor.red() / 255.0,
                                       borderColor.green() / 255.0,
                                       borderColor.blue() / 255.0,
                                       1);

    glLineWidth(layer->borderWidth());

    // The indices for the line are stored in the same array as the triangle indices.
    glDrawElements(GL_LINE_LOOP, 4, GL_UNSIGNED_SHORT, (void*)(6 * sizeof(unsigned short)));
    checkGLError();

    // Switch back to the shader program used for layer contents.
    glUseProgram(m_layerProgramObject);
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

void LayerRendererChromium::compositeLayersRecursive(LayerChromium* layer, const TransformationMatrix& matrix, float opacity, const IntRect& visibleRect)
{
    static GLfloat glMatrix[16];

    // Compute the new matrix transformation that will be applied to this layer and
    // all its sublayers.
    // The basic transformation chain for the layer is (using the Matrix x Vector order):
    // M = M[p] * T[l] * T[a] * M[l] * T[-a]
    // Where M[p] is the parent matrix passed down to the function
    //       T[l] is the translation of the layer's center
    //       T[a] and T[-a] is a translation/inverse translation by the anchor point
    //       M[l] is the layer's matrix
    // Note that the final matrix used by the shader for the layer is P * M * S . This final product
    // is effectively computed in drawTexturedQuad().
    // Where: P is the projection matrix
    //        M is the layer's matrix computed above
    //        S is the scale adjustment (to scale up to the layer size)
    IntSize bounds = layer->bounds();
    FloatPoint anchorPoint = layer->anchorPoint();
    FloatPoint position = layer->position();
    float anchorX = (anchorPoint.x() - 0.5) * bounds.width();
    float anchorY = (0.5 - anchorPoint.y()) * bounds.height();

    // M = M[p]
    TransformationMatrix localMatrix = matrix;
    // M = M[p] * T[l]
    localMatrix.translate3d(position.x(), position.y(), 0);
    // M = M[p] * T[l] * T[a]
    localMatrix.translate3d(anchorX, anchorY, 0);
    // M = M[p] * T[l] * T[a] * M[l]
    localMatrix.multLeft(layer->transform());
    // M = M[p] * T[l] * T[a] * M[l] * T[-a]
    localMatrix.translate3d(-anchorX, -anchorY, 0);

    bool skipLayer = false;
    if (bounds.width() > 2048 || bounds.height() > 2048) {
        LOG(LayerRenderer, "Skipping layer with size %d %d", bounds.width(), bounds.height());
        skipLayer = true;
    }

    // Calculate the layer's opacity.
    opacity *= layer->opacity();

    bool layerVisible = isLayerVisible(layer, localMatrix, visibleRect);

    // Note that there are two types of layers:
    // 1. Layers that have their own GraphicsContext and can draw their contents on demand (layer->drawsContent() == true).
    // 2. Layers that are just containers of images/video/etc that don't own a GraphicsContext (layer->contents() == true).
    if ((layer->drawsContent() || layer->contents()) && !skipLayer && layerVisible) {
        int textureId = getTextureId(layer);
        // If no texture has been created for the layer yet then create one now.
        if (textureId == -1)
            textureId = assignTextureForLayer(layer);

        // Redraw the contents of the layer if necessary.
        if ((layer->drawsContent() || layer->contents()) && layer->contentsDirty()) {
            // Update the contents of the layer before taking a snapshot. For layers that
            // are simply containers, the following call just clears the dirty flag but doesn't
            // actually do any draws/copies.
            layer->updateTextureContents(textureId);
        }

        if (layer->doubleSided())
            glDisable(GL_CULL_FACE);
        else
            glEnable(GL_CULL_FACE);

        glBindTexture(GL_TEXTURE_2D, textureId);

        drawTexturedQuad(localMatrix, bounds.width(), bounds.height(), opacity, false);
    }

    // Draw the debug border if there is one.
    drawDebugBorder(layer, localMatrix);

    // Apply the sublayer transform.
    localMatrix.multLeft(layer->sublayerTransform());

    // The origin of the sublayers is actually the left top corner of the layer
    // instead of the center. The matrix passed down to the sublayers is therefore:
    // M[s] = M * T[-center]
    localMatrix.translate3d(-bounds.width() * 0.5, -bounds.height() * 0.5, 0);
    const Vector<RefPtr<LayerChromium> >& sublayers = layer->getSublayers();
    for (size_t i = 0; i < sublayers.size(); i++)
        compositeLayersRecursive(sublayers[i].get(), localMatrix, opacity, visibleRect);
}

bool LayerRendererChromium::makeContextCurrent()
{
    return m_gles2Context->makeCurrent();
}

bool LayerRendererChromium::initGL()
{
    m_gles2Context = GLES2Context::create(m_page);

    if (!m_gles2Context)
        return false;

    return true;
}

// Binds the given attribute name to a common location across all three programs
// used by the compositor. This allows the code to bind the attributes only once
// even when switching between programs.
void LayerRendererChromium::bindCommonAttribLocation(int location, char* attribName)
{
    glBindAttribLocation(m_layerProgramObject, location, attribName);
    glBindAttribLocation(m_borderProgramObject, location, attribName);
    glBindAttribLocation(m_scrollProgramObject, location, attribName);
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
    char fragmentShaderString[] =
        // FIXME: Re-introduce precision qualifier when we need GL ES shaders.
        "//precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "uniform float alpha;                                \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.z, texColor.y, texColor.x, texColor.w) * alpha; \n"
        "}                                                   \n";

    // Fragment shader used for rendering the scrolled root layer quad. It differs
    // from fragmentShaderString in that it doesn't swizzle the colors and doesn't
    // take an alpha value.
    char scrollFragmentShaderString[] =
        // FIXME: Re-introduce precision qualifier when we need GL ES shaders.
        "//precision mediump float;                            \n"
        "varying vec2 v_texCoord;                            \n"
        "uniform sampler2D s_texture;                        \n"
        "void main()                                         \n"
        "{                                                   \n"
        "  vec4 texColor = texture2D(s_texture, v_texCoord); \n"
        "  gl_FragColor = vec4(texColor.x, texColor.y, texColor.z, texColor.w); \n"
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
        // FIXME: Re-introduce precision qualifier when we need GL ES shaders.
        "//precision mediump float;                            \n"
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
    m_layerProgramObject = loadShaderProgram(vertexShaderString, fragmentShaderString);
    if (!m_layerProgramObject) {
        LOG_ERROR("Failed to create shader program for layers");
        return false;
    }

    m_scrollProgramObject = loadShaderProgram(vertexShaderString, scrollFragmentShaderString);
    if (!m_scrollProgramObject) {
        LOG_ERROR("Failed to create shader program for scrolling layer");
        return false;
    }

    m_borderProgramObject = loadShaderProgram(borderVertexShaderString, borderFragmentShaderString);
    if (!m_borderProgramObject) {
        LOG_ERROR("Failed to create shader program for debug borders");
        return false;
    }

    // Specify the attrib location for the position and make it the same for all three programs to
    // avoid binding re-binding the vertex attributes.
    bindCommonAttribLocation(m_positionLocation, "a_position");
    bindCommonAttribLocation(m_texCoordLocation, "a_texCoord");

    checkGLError();

    // Re-link the shaders to get the new attrib location to take effect.
    glLinkProgram(m_layerProgramObject);
    glLinkProgram(m_borderProgramObject);
    glLinkProgram(m_scrollProgramObject);

    checkGLError();

    // Get locations of uniforms for the layer content shader program.
    m_samplerLocation = glGetUniformLocation(m_layerProgramObject, "s_texture");
    m_matrixLocation = glGetUniformLocation(m_layerProgramObject, "matrix");
    m_alphaLocation = glGetUniformLocation(m_layerProgramObject, "alpha");

    m_scrollMatrixLocation = glGetUniformLocation(m_scrollProgramObject, "matrix");
    m_scrollSamplerLocation = glGetUniformLocation(m_scrollProgramObject, "s_texture");

    // Get locations of uniforms for the debug border shader program.
    m_borderMatrixLocation = glGetUniformLocation(m_borderProgramObject, "matrix");
    m_borderColorLocation = glGetUniformLocation(m_borderProgramObject, "color");

    glGenBuffers(3, m_quadVboIds);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVboIds[Vertices]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_quadVboIds[LayerElements]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Create a texture object to hold the contents of the root layer.
    m_rootLayerTextureId = createLayerTexture();
    if (m_rootLayerTextureId == -1) {
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
