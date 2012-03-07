/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestWebPlugin.h"

#include "WebFrame.h"
#include "platform/WebGraphicsContext3D.h"
#include "WebKit.h"
#include "platform/WebKitPlatformSupport.h"
#include "WebPluginContainer.h"
#include "WebPluginParams.h"
#include <wtf/Assertions.h>
#include <wtf/text/CString.h>

using namespace WebKit;

// GLenum values copied from gl2.h.
#define GL_FALSE                  0
#define GL_TRUE                   1
#define GL_ONE                    1
#define GL_TRIANGLES              0x0004
#define GL_ONE_MINUS_SRC_ALPHA    0x0303
#define GL_DEPTH_TEST             0x0B71
#define GL_BLEND                  0x0BE2
#define GL_SCISSOR_TEST           0x0B90
#define GL_TEXTURE_2D             0x0DE1
#define GL_FLOAT                  0x1406
#define GL_RGBA                   0x1908
#define GL_UNSIGNED_BYTE          0x1401
#define GL_TEXTURE_MAG_FILTER     0x2800
#define GL_TEXTURE_MIN_FILTER     0x2801
#define GL_TEXTURE_WRAP_S         0x2802
#define GL_TEXTURE_WRAP_T         0x2803
#define GL_NEAREST                0x2600
#define GL_COLOR_BUFFER_BIT       0x4000
#define GL_CLAMP_TO_EDGE          0x812F
#define GL_ARRAY_BUFFER           0x8892
#define GL_STATIC_DRAW            0x88E4
#define GL_FRAGMENT_SHADER        0x8B30
#define GL_VERTEX_SHADER          0x8B31
#define GL_COMPILE_STATUS         0x8B81
#define GL_LINK_STATUS            0x8B82
#define GL_COLOR_ATTACHMENT0      0x8CE0
#define GL_FRAMEBUFFER_COMPLETE   0x8CD5
#define GL_FRAMEBUFFER            0x8D40

static void premultiplyAlpha(const unsigned colorIn[3], float alpha, float colorOut[4])
{
    for (int i = 0; i < 3; ++i)
        colorOut[i] = (colorIn[i] / 255.0f) * alpha;

    colorOut[3] = alpha;
}

TestWebPlugin::TestWebPlugin(WebKit::WebFrame* frame,
                             const WebKit::WebPluginParams& params)
    : m_frame(frame)
    , m_container(0)
    , m_context(0)
{
    static const WebString kAttributePrimitive = WebString::fromUTF8("primitive");
    static const WebString kAttributeBackgroundColor = WebString::fromUTF8("background-color");
    static const WebString kAttributePrimitiveColor = WebString::fromUTF8("primitive-color");
    static const WebString kAttributeOpacity = WebString::fromUTF8("opacity");

    ASSERT(params.attributeNames.size() == params.attributeValues.size());
    size_t size = params.attributeNames.size();
    for (size_t i = 0; i < size; ++i) {
        const WebString& attributeName = params.attributeNames[i];
        const WebString& attributeValue = params.attributeValues[i];

        if (attributeName == kAttributePrimitive)
            m_scene.primitive = parsePrimitive(attributeValue);
        else if (attributeName == kAttributeBackgroundColor)
            parseColor(attributeValue, m_scene.backgroundColor);
        else if (attributeName == kAttributePrimitiveColor)
            parseColor(attributeValue, m_scene.primitiveColor);
        else if (attributeName == kAttributeOpacity)
            m_scene.opacity = parseOpacity(attributeValue);
    }
}

TestWebPlugin::~TestWebPlugin()
{
}

const WebString& TestWebPlugin::mimeType()
{
    static const WebString kMimeType = WebString::fromUTF8("application/x-webkit-test-webplugin");
    return kMimeType;
}

bool TestWebPlugin::initialize(WebPluginContainer* container)
{
    WebGraphicsContext3D::Attributes attrs;
    m_context = webKitPlatformSupport()->createOffscreenGraphicsContext3D(attrs);
    if (!m_context)
        return false;

    if (!m_context->makeContextCurrent())
        return false;

    if (!initScene())
        return false;

    m_container = container;
    m_container->setBackingTextureId(m_colorTexture);
    return true;
}

void TestWebPlugin::destroy()
{
    destroyScene();

    delete m_context;
    m_context = 0;

    m_container = 0;
    m_frame = 0;
}

void TestWebPlugin::updateGeometry(const WebRect& frameRect,
                                   const WebRect& clipRect,
                                   const WebVector<WebRect>& cutOutsRects,
                                   bool isVisible)
{
    if (clipRect == m_rect)
        return;
    m_rect = clipRect;

    m_context->reshape(m_rect.width, m_rect.height);
    m_context->viewport(0, 0, m_rect.width, m_rect.height);

    m_context->bindTexture(GL_TEXTURE_2D, m_colorTexture);
    m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    m_context->texParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    m_context->texImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_rect.width, m_rect.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0);
    m_context->bindFramebuffer(GL_FRAMEBUFFER, m_framebuffer);
    m_context->framebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_colorTexture, 0);

    drawScene();

    m_context->flush();
    m_container->commitBackingTexture();
}

TestWebPlugin::Primitive TestWebPlugin::parsePrimitive(const WebString& string)
{
    static const WebString kPrimitiveNone = WebString::fromUTF8("none");
    static const WebString kPrimitiveTriangle = WebString::fromUTF8("triangle");

    Primitive primitive = PrimitiveNone;
    if (string == kPrimitiveNone)
        primitive = PrimitiveNone;
    else if (string == kPrimitiveTriangle)
        primitive = PrimitiveTriangle;
    else
        ASSERT_NOT_REACHED();
    return primitive;
}

// FIXME: This method should already exist. Use it.
// For now just parse primary colors.
void TestWebPlugin::parseColor(const WebString& string, unsigned color[3])
{
    color[0] = color[1] = color[2] = 0;
    if (string == "black")
        return;

    if (string == "red")
        color[0] = 255;
    else if (string == "green")
        color[1] = 255;
    else if (string == "blue")
        color[2] = 255;
    else
        ASSERT_NOT_REACHED();
}

float TestWebPlugin::parseOpacity(const WebString& string)
{
    return static_cast<float>(atof(string.utf8().data()));
}

bool TestWebPlugin::initScene()
{
    float color[4];
    premultiplyAlpha(m_scene.backgroundColor, m_scene.opacity, color);

    m_colorTexture = m_context->createTexture();
    m_framebuffer = m_context->createFramebuffer();

    m_context->viewport(0, 0, m_rect.width, m_rect.height);
    m_context->disable(GL_DEPTH_TEST);
    m_context->disable(GL_SCISSOR_TEST);

    m_context->clearColor(color[0], color[1], color[2], color[3]);

    m_context->enable(GL_BLEND);
    m_context->blendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);

    return m_scene.primitive != PrimitiveNone ? initProgram() && initPrimitive() : true;
}

void TestWebPlugin::drawScene()
{
    m_context->viewport(0, 0, m_rect.width, m_rect.height);
    m_context->clear(GL_COLOR_BUFFER_BIT);

    if (m_scene.primitive != PrimitiveNone)
        drawPrimitive();
}

void TestWebPlugin::destroyScene()
{
    if (m_scene.program) {
        m_context->deleteProgram(m_scene.program);
        m_scene.program = 0;
    }
    if (m_scene.vbo) {
        m_context->deleteBuffer(m_scene.vbo);
        m_scene.vbo = 0;
    }

    if (m_framebuffer) {
        m_context->deleteFramebuffer(m_framebuffer);
        m_framebuffer = 0;
    }

    if (m_colorTexture) {
        m_context->deleteTexture(m_colorTexture);
        m_colorTexture = 0;
    }
}

bool TestWebPlugin::initProgram()
{
    const CString vertexSource(
        "attribute vec4 position;  \n"
        "void main() {             \n"
        "  gl_Position = position; \n"
        "}                         \n"
    );

    const CString fragmentSource(
        "precision mediump float; \n"
        "uniform vec4 color;      \n"
        "void main() {            \n"
        "  gl_FragColor = color;  \n"
        "}                        \n"
    );

    m_scene.program = loadProgram(vertexSource, fragmentSource);
    if (!m_scene.program)
        return false;

    m_scene.colorLocation = m_context->getUniformLocation(m_scene.program, "color");
    m_scene.positionLocation = m_context->getAttribLocation(m_scene.program, "position");
    return true;
}

bool TestWebPlugin::initPrimitive()
{
    ASSERT(m_scene.primitive == PrimitiveTriangle);

    m_scene.vbo = m_context->createBuffer();
    if (!m_scene.vbo)
        return false;

    const float vertices[] = { 0.0f,  0.8f, 0.0f,
                              -0.8f, -0.8f, 0.0f,
                               0.8f, -0.8f, 0.0f };
    m_context->bindBuffer(GL_ARRAY_BUFFER, m_scene.vbo);
    m_context->bufferData(GL_ARRAY_BUFFER, sizeof(vertices), 0, GL_STATIC_DRAW);
    m_context->bufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    return true;
}

void TestWebPlugin::drawPrimitive()
{
    ASSERT(m_scene.primitive == PrimitiveTriangle);
    ASSERT(m_scene.vbo);
    ASSERT(m_scene.program);

    m_context->useProgram(m_scene.program);

    // Bind primitive color.
    float color[4];
    premultiplyAlpha(m_scene.primitiveColor, m_scene.opacity, color);
    m_context->uniform4f(m_scene.colorLocation, color[0], color[1], color[2], color[3]);

    // Bind primitive vertices.
    m_context->bindBuffer(GL_ARRAY_BUFFER, m_scene.vbo);
    m_context->enableVertexAttribArray(m_scene.positionLocation);
    m_context->vertexAttribPointer(m_scene.positionLocation, 3, GL_FLOAT, GL_FALSE, 0, 0);
    m_context->drawArrays(GL_TRIANGLES, 0, 3);
}

unsigned TestWebPlugin::loadShader(unsigned type, const CString& source)
{
    unsigned shader = m_context->createShader(type);
    if (shader) {
        m_context->shaderSource(shader, source.data());
        m_context->compileShader(shader);

        int compiled = 0;
        m_context->getShaderiv(shader, GL_COMPILE_STATUS, &compiled);
        if (!compiled) {
            m_context->deleteShader(shader);
            shader = 0;
        }
    }
    return shader;
}

unsigned TestWebPlugin::loadProgram(const CString& vertexSource,
                                    const CString& fragmentSource)
{
    unsigned vertexShader = loadShader(GL_VERTEX_SHADER, vertexSource);
    unsigned fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSource);
    unsigned program = m_context->createProgram();
    if (vertexShader && fragmentShader && program) {
        m_context->attachShader(program, vertexShader);
        m_context->attachShader(program, fragmentShader);
        m_context->linkProgram(program);

        int linked = 0;
        m_context->getProgramiv(program, GL_LINK_STATUS, &linked);
        if (!linked) {
            m_context->deleteProgram(program);
            program = 0;
        }
    }
    if (vertexShader)
        m_context->deleteShader(vertexShader);
    if (fragmentShader)
        m_context->deleteShader(fragmentShader);

    return program;
}

