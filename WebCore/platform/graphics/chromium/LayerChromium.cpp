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

#include "LayerChromium.h"

#include "GraphicsContext3D.h"
#include "LayerRendererChromium.h"
#if PLATFORM(SKIA)
#include "NativeImageSkia.h"
#include "PlatformContextSkia.h"
#endif
#include "RenderLayerBacking.h"
#include "skia/ext/platform_canvas.h"
#include <wtf/text/WTFString.h>

namespace WebCore {

using namespace std;

const unsigned LayerChromium::s_positionAttribLocation = 0;
const unsigned LayerChromium::s_texCoordAttribLocation = 1;

static unsigned loadShader(GraphicsContext3D* context, unsigned type, const char* shaderSource)
{
    unsigned shader = context->createShader(type);
    if (!shader)
        return 0;
    String sourceString(shaderSource);
    GLC(context, context->shaderSource(shader, sourceString));
    GLC(context, context->compileShader(shader));
    int compiled;
    GLC(context, context->getShaderiv(shader, GraphicsContext3D::COMPILE_STATUS, &compiled));
    if (!compiled) {
        GLC(context, context->deleteShader(shader));
        return 0;
    }
    return shader;
}

LayerChromium::SharedValues::SharedValues(GraphicsContext3D* context)
    : m_context(context)
    , m_quadVerticesVbo(0)
    , m_quadElementsVbo(0)
    , m_maxTextureSize(0)
    , m_borderShaderProgram(0)
    , m_borderShaderMatrixLocation(-1)
    , m_borderShaderColorLocation(-1)
    , m_initialized(false)
{
    // Vertex positions and texture coordinates for the 4 corners of a 1x1 quad.
    float vertices[] = { -0.5f,  0.5f, 0.0f, 0.0f,  1.0f,
                         -0.5f, -0.5f, 0.0f, 0.0f,  0.0f,
                         0.5f, -0.5f, 0.0f, 1.0f,  0.0f,
                         0.5f,  0.5f, 0.0f, 1.0f,  1.0f };
    uint16_t indices[] = { 0, 1, 2, 0, 2, 3, // The two triangles that make up the layer quad.
                           0, 1, 2, 3}; // A line path for drawing the layer border.

    GLC(m_context, m_quadVerticesVbo = m_context->createBuffer());
    GLC(m_context, m_quadElementsVbo = m_context->createBuffer());
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, m_quadVerticesVbo));
    GLC(m_context, m_context->bufferData(GraphicsContext3D::ARRAY_BUFFER, sizeof(vertices), vertices, GraphicsContext3D::STATIC_DRAW));
    GLC(m_context, m_context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, m_quadElementsVbo));
    GLC(m_context, m_context->bufferData(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GraphicsContext3D::STATIC_DRAW));

    // Get the max texture size supported by the system.
    GLC(m_context, m_context->getIntegerv(GraphicsContext3D::MAX_TEXTURE_SIZE, &m_maxTextureSize));

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
        "  gl_FragColor = vec4(color.xyz * color.w, color.w);\n"
        "}                                                   \n";

    m_borderShaderProgram = createShaderProgram(m_context, borderVertexShaderString, borderFragmentShaderString);
    if (!m_borderShaderProgram) {
        LOG_ERROR("ContentLayerChromium: Failed to create shader program");
        return;
    }

    m_borderShaderMatrixLocation = m_context->getUniformLocation(m_borderShaderProgram, "matrix");
    m_borderShaderColorLocation = m_context->getUniformLocation(m_borderShaderProgram, "color");
    ASSERT(m_borderShaderMatrixLocation != -1);
    ASSERT(m_borderShaderColorLocation != -1);

    m_initialized = true;
}

LayerChromium::SharedValues::~SharedValues()
{
    GLC(m_context, m_context->deleteBuffer(m_quadVerticesVbo));
    GLC(m_context, m_context->deleteBuffer(m_quadElementsVbo));
    if (m_borderShaderProgram)
        GLC(m_context, m_context->deleteProgram(m_borderShaderProgram));
}


PassRefPtr<LayerChromium> LayerChromium::create(GraphicsLayerChromium* owner)
{
    return adoptRef(new LayerChromium(owner));
}

LayerChromium::LayerChromium(GraphicsLayerChromium* owner)
    : m_owner(owner)
    , m_contentsDirty(false)
    , m_superlayer(0)
    , m_anchorPoint(0.5, 0.5)
    , m_backgroundColor(0, 0, 0, 0)
    , m_borderColor(0, 0, 0, 0)
    , m_opacity(1.0)
    , m_zPosition(0.0)
    , m_anchorPointZ(0)
    , m_borderWidth(0)
    , m_clearsContext(false)
    , m_doubleSided(true)
    , m_hidden(false)
    , m_masksToBounds(false)
    , m_opaque(true)
    , m_geometryFlipped(false)
    , m_needsDisplayOnBoundsChange(false)
    , m_layerRenderer(0)
{
}

LayerChromium::~LayerChromium()
{
    // Our superlayer should be holding a reference to us so there should be no
    // way for us to be destroyed while we still have a superlayer.
    ASSERT(!superlayer());

    // Remove the superlayer reference from all sublayers.
    removeAllSublayers();
}

void LayerChromium::setLayerRenderer(LayerRendererChromium* renderer)
{
    // It's not expected that layers will ever switch renderers.
    ASSERT(!renderer || !m_layerRenderer || renderer == m_layerRenderer);

    m_layerRenderer = renderer;
}

unsigned LayerChromium::createShaderProgram(GraphicsContext3D* context, const char* vertexShaderSource, const char* fragmentShaderSource)
{
    unsigned vertexShader = loadShader(context, GraphicsContext3D::VERTEX_SHADER, vertexShaderSource);
    if (!vertexShader) {
        LOG_ERROR("Failed to create vertex shader");
        return 0;
    }

    unsigned fragmentShader = loadShader(context, GraphicsContext3D::FRAGMENT_SHADER, fragmentShaderSource);
    if (!fragmentShader) {
        GLC(context, context->deleteShader(vertexShader));
        LOG_ERROR("Failed to create fragment shader");
        return 0;
    }

    unsigned programObject = context->createProgram();
    if (!programObject) {
        LOG_ERROR("Failed to create shader program");
        return 0;
    }

    GLC(context, context->attachShader(programObject, vertexShader));
    GLC(context, context->attachShader(programObject, fragmentShader));

    // Bind the common attrib locations.
    GLC(context, context->bindAttribLocation(programObject, s_positionAttribLocation, "a_position"));
    GLC(context, context->bindAttribLocation(programObject, s_texCoordAttribLocation, "a_texCoord"));

    GLC(context, context->linkProgram(programObject));
    int linked;
    GLC(context, context->getProgramiv(programObject, GraphicsContext3D::LINK_STATUS, &linked));
    if (!linked) {
        LOG_ERROR("Failed to link shader program");
        GLC(context, context->deleteProgram(programObject));
        return 0;
    }

    GLC(context, context->deleteShader(vertexShader));
    GLC(context, context->deleteShader(fragmentShader));
    return programObject;
}

void LayerChromium::setNeedsCommit()
{
    // Call notifySyncRequired(), which in this implementation plumbs through to
    // call setRootLayerNeedsDisplay() on the WebView, which will cause LayerRendererChromium
    // to render a frame.
    if (m_owner)
        m_owner->notifySyncRequired();
}

void LayerChromium::addSublayer(PassRefPtr<LayerChromium> sublayer)
{
    insertSublayer(sublayer, numSublayers());
}

void LayerChromium::insertSublayer(PassRefPtr<LayerChromium> sublayer, size_t index)
{
    index = min(index, m_sublayers.size());
    sublayer->removeFromSuperlayer();
    sublayer->setSuperlayer(this);
    m_sublayers.insert(index, sublayer);
    setNeedsCommit();
}

void LayerChromium::removeFromSuperlayer()
{
    if (m_superlayer)
        m_superlayer->removeSublayer(this);
}

void LayerChromium::removeSublayer(LayerChromium* sublayer)
{
    int foundIndex = indexOfSublayer(sublayer);
    if (foundIndex == -1)
        return;

    sublayer->setSuperlayer(0);
    m_sublayers.remove(foundIndex);
    setNeedsCommit();
}

void LayerChromium::replaceSublayer(LayerChromium* reference, PassRefPtr<LayerChromium> newLayer)
{
    ASSERT_ARG(reference, reference);
    ASSERT_ARG(reference, reference->superlayer() == this);

    if (reference == newLayer)
        return;

    int referenceIndex = indexOfSublayer(reference);
    if (referenceIndex == -1) {
        ASSERT_NOT_REACHED();
        return;
    }

    reference->removeFromSuperlayer();

    if (newLayer) {
        newLayer->removeFromSuperlayer();
        insertSublayer(newLayer, referenceIndex);
    }
}

int LayerChromium::indexOfSublayer(const LayerChromium* reference)
{
    for (size_t i = 0; i < m_sublayers.size(); i++) {
        if (m_sublayers[i] == reference)
            return i;
    }
    return -1;
}

void LayerChromium::setBounds(const IntSize& size)
{
    if (m_bounds == size)
        return;

    bool firstResize = !m_bounds.width() && !m_bounds.height() && size.width() && size.height();

    m_bounds = size;
    m_backingStoreSize = size;

    if (firstResize)
        setNeedsDisplay(FloatRect(0, 0, m_bounds.width(), m_bounds.height()));
    else
        setNeedsCommit();
}

void LayerChromium::setFrame(const FloatRect& rect)
{
    if (rect == m_frame)
      return;

    m_frame = rect;
    setNeedsDisplay(FloatRect(0, 0, m_bounds.width(), m_bounds.height()));
}

const LayerChromium* LayerChromium::rootLayer() const
{
    const LayerChromium* layer = this;
    for (LayerChromium* superlayer = layer->superlayer(); superlayer; layer = superlayer, superlayer = superlayer->superlayer()) { }
    return layer;
}

void LayerChromium::removeAllSublayers()
{
    while (m_sublayers.size()) {
        LayerChromium* layer = m_sublayers[0].get();
        ASSERT(layer->superlayer());
        layer->removeFromSuperlayer();
    }
}

void LayerChromium::setSublayers(const Vector<RefPtr<LayerChromium> >& sublayers)
{
    if (sublayers == m_sublayers)
        return;

    removeAllSublayers();
    size_t listSize = sublayers.size();
    for (size_t i = 0; i < listSize; i++)
        addSublayer(sublayers[i]);
}

LayerChromium* LayerChromium::superlayer() const
{
    return m_superlayer;
}

void LayerChromium::setNeedsDisplay(const FloatRect& dirtyRect)
{
    // Simply mark the contents as dirty. The actual redraw will
    // happen when it's time to do the compositing.
    m_contentsDirty = true;

    m_dirtyRect.unite(dirtyRect);
    setNeedsCommit();
}

void LayerChromium::setNeedsDisplay()
{
    m_dirtyRect.setSize(m_bounds);
    m_contentsDirty = true;
}

void LayerChromium::resetNeedsDisplay()
{
    m_dirtyRect = FloatRect();
    m_contentsDirty = false;
}

void LayerChromium::toGLMatrix(float* flattened, const TransformationMatrix& m)
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

GraphicsContext3D* LayerChromium::layerRendererContext() const
{
    ASSERT(layerRenderer());
    return layerRenderer()->context();
}

void LayerChromium::drawTexturedQuad(GraphicsContext3D* context, const TransformationMatrix& projectionMatrix, const TransformationMatrix& drawMatrix,
                                     float width, float height, float opacity,
                                     int matrixLocation, int alphaLocation)
{
    static float glMatrix[16];

    TransformationMatrix renderMatrix = drawMatrix;

    // Apply a scaling factor to size the quad from 1x1 to its intended size.
    renderMatrix.scale3d(width, height, 1);

    // Apply the projection matrix before sending the transform over to the shader.
    renderMatrix.multiply(projectionMatrix);

    toGLMatrix(&glMatrix[0], renderMatrix);

    GLC(context, context->uniformMatrix4fv(matrixLocation, false, &glMatrix[0], 1));

    if (alphaLocation != -1)
        GLC(context, context->uniform1f(alphaLocation, opacity));

    GLC(context, context->drawElements(GraphicsContext3D::TRIANGLES, 6, GraphicsContext3D::UNSIGNED_SHORT, 0));
}

void LayerChromium::drawDebugBorder()
{
    static float glMatrix[16];
    if (!borderColor().alpha())
        return;

    ASSERT(layerRenderer());
    const SharedValues* sv = layerRenderer()->layerSharedValues();
    ASSERT(sv && sv->initialized());
    layerRenderer()->useShader(sv->borderShaderProgram());
    TransformationMatrix renderMatrix = drawTransform();
    renderMatrix.scale3d(bounds().width(), bounds().height(), 1);
    renderMatrix.multiply(layerRenderer()->projectionMatrix());
    toGLMatrix(&glMatrix[0], renderMatrix);
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->uniformMatrix4fv(sv->borderShaderMatrixLocation(), false, &glMatrix[0], 1));

    GLC(context, context->uniform4f(sv->borderShaderColorLocation(), borderColor().red() / 255.0, borderColor().green() / 255.0, borderColor().blue() / 255.0, 1));

    GLC(context, context->lineWidth(borderWidth()));

    // The indices for the line are stored in the same array as the triangle indices.
    GLC(context, context->drawElements(GraphicsContext3D::LINE_LOOP, 4, GraphicsContext3D::UNSIGNED_SHORT, 6 * sizeof(unsigned short)));
}

const FloatRect LayerChromium::getDrawRect() const
{
    // Form the matrix used by the shader to map the corners of the layer's
    // bounds into the view space.
    TransformationMatrix renderMatrix = drawTransform();
    renderMatrix.scale3d(bounds().width(), bounds().height(), 1);

    FloatRect layerRect(-0.5, -0.5, 1, 1);
    FloatRect mappedRect = renderMatrix.mapRect(layerRect);
    return mappedRect;
}

// Draws the layer with a single colored shader. This method is used to do
// quick draws into the stencil buffer.
void LayerChromium::drawAsMask()
{
    ASSERT(layerRenderer());
    const SharedValues* sv = layerRenderer()->layerSharedValues();
    ASSERT(sv && sv->initialized());
    layerRenderer()->useShader(sv->borderShaderProgram());

    // We reuse the border shader here as all we need a single colored shader pass.
    // The color specified here is only for debug puproses as typically when we call this
    // method, writes to the color channels are disabled.
    GraphicsContext3D* context = layerRendererContext();
    GLC(context, context->uniform4f(sv->borderShaderColorLocation(), 0, 1 , 0, 0.7));

    drawTexturedQuad(context, layerRenderer()->projectionMatrix(), drawTransform(),
                     bounds().width(), bounds().height(), drawOpacity(),
                     sv->borderShaderMatrixLocation(), -1);
}

// static
void LayerChromium::prepareForDraw(const SharedValues* sv)
{
    GraphicsContext3D* context = sv->context();
    GLC(context, context->bindBuffer(GraphicsContext3D::ARRAY_BUFFER, sv->quadVerticesVbo()));
    GLC(context, context->bindBuffer(GraphicsContext3D::ELEMENT_ARRAY_BUFFER, sv->quadElementsVbo()));
    unsigned offset = 0;
    GLC(context, context->vertexAttribPointer(s_positionAttribLocation, 3, GraphicsContext3D::FLOAT, false, 5 * sizeof(float), offset));
    offset += 3 * sizeof(float);
    GLC(context, context->vertexAttribPointer(s_texCoordAttribLocation, 2, GraphicsContext3D::FLOAT, false, 5 * sizeof(float), offset));
    GLC(context, context->enableVertexAttribArray(s_positionAttribLocation));
    GLC(context, context->enableVertexAttribArray(s_texCoordAttribLocation));
}

}
#endif // USE(ACCELERATED_COMPOSITING)
