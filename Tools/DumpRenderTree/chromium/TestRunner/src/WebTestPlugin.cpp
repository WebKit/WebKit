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
#include "WebTestPlugin.h"

#include "WebFrame.h"
#include "WebInputEvent.h"
#include "WebKit.h"
#include "WebPluginContainer.h"
#include "WebPluginParams.h"
#include "WebTestDelegate.h"
#include "WebTouchPoint.h"
#include "platform/WebGraphicsContext3D.h"
#include "platform/WebKitPlatformSupport.h"
#include "public/WebCompositorSupport.h"
#include "public/WebExternalTextureLayer.h"
#include "public/WebExternalTextureLayerClient.h"
#include <wtf/Assertions.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/StringExtras.h>
#include <wtf/text/CString.h>

using namespace WebKit;

namespace WebTestRunner {

namespace {

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

void premultiplyAlpha(const unsigned colorIn[3], float alpha, float colorOut[4])
{
    for (int i = 0; i < 3; ++i)
        colorOut[i] = (colorIn[i] / 255.0f) * alpha;

    colorOut[3] = alpha;
}

const char* pointState(WebTouchPoint::State state)
{
    switch (state) {
    case WebTouchPoint::StateReleased:
        return "Released";
    case WebTouchPoint::StatePressed:
        return "Pressed";
    case WebTouchPoint::StateMoved:
        return "Moved";
    case WebTouchPoint::StateCancelled:
        return "Cancelled";
    default:
        return "Unknown";
    }

    ASSERT_NOT_REACHED();
    return 0;
}

void printTouchList(WebTestDelegate* delegate, const WebTouchPoint* points, int length)
{
    for (int i = 0; i < length; ++i) {
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "* %d, %d: %s\n", points[i].position.x, points[i].position.y, pointState(points[i].state));
        delegate->printMessage(buffer);
    }
}

void printEventDetails(WebTestDelegate* delegate, const WebInputEvent& event)
{
    if (WebInputEvent::isTouchEventType(event.type)) {
        const WebTouchEvent& touch = static_cast<const WebTouchEvent&>(event);
        printTouchList(delegate, touch.touches, touch.touchesLength);
        printTouchList(delegate, touch.changedTouches, touch.changedTouchesLength);
        printTouchList(delegate, touch.targetTouches, touch.targetTouchesLength);
    } else if (WebInputEvent::isMouseEventType(event.type) || event.type == WebInputEvent::MouseWheel) {
        const WebMouseEvent& mouse = static_cast<const WebMouseEvent&>(event);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "* %d, %d\n", mouse.x, mouse.y);
        delegate->printMessage(buffer);
    } else if (WebInputEvent::isGestureEventType(event.type)) {
        const WebGestureEvent& gesture = static_cast<const WebGestureEvent&>(event);
        char buffer[100];
        snprintf(buffer, sizeof(buffer), "* %d, %d\n", gesture.x, gesture.y);
        delegate->printMessage(buffer);
    }
}

WebPluginContainer::TouchEventRequestType parseTouchEventRequestType(const WebString& string)
{
    DEFINE_STATIC_LOCAL(const WebString, kPrimitiveRaw, (WebString::fromUTF8("raw")));
    DEFINE_STATIC_LOCAL(const WebString, kPrimitiveSynthetic, (WebString::fromUTF8("synthetic")));

    if (string == kPrimitiveRaw)
        return WebPluginContainer::TouchEventRequestTypeRaw;
    if (string == kPrimitiveSynthetic)
        return WebPluginContainer::TouchEventRequestTypeSynthesizedMouse;
    return WebPluginContainer::TouchEventRequestTypeNone;
}

class WebTestPluginImpl : public WebTestPlugin, public WebExternalTextureLayerClient {
public:
    WebTestPluginImpl(WebFrame*, const WebPluginParams&, WebTestDelegate*);
    virtual ~WebTestPluginImpl();

    // WebPlugin methods:
    virtual bool initialize(WebPluginContainer*);
    virtual void destroy();
    virtual NPObject* scriptableObject() { return 0; }
    virtual bool canProcessDrag() const { return m_canProcessDrag; }
    virtual void paint(WebCanvas*, const WebRect&) { }
    virtual void updateGeometry(const WebRect& frameRect, const WebRect& clipRect, const WebVector<WebRect>& cutOutsRects, bool isVisible);
    virtual void updateFocus(bool) { }
    virtual void updateVisibility(bool) { }
    virtual bool acceptsInputEvents() { return true; }
    virtual bool handleInputEvent(const WebInputEvent&, WebCursorInfo&);
    virtual bool handleDragStatusUpdate(WebDragStatus, const WebDragData&, WebDragOperationsMask, const WebPoint& position, const WebPoint& screenPosition);
    virtual void didReceiveResponse(const WebURLResponse&) { }
    virtual void didReceiveData(const char* data, int dataLength) { }
    virtual void didFinishLoading() { }
    virtual void didFailLoading(const WebURLError&) { }
    virtual void didFinishLoadingFrameRequest(const WebURL&, void* notifyData) { }
    virtual void didFailLoadingFrameRequest(const WebURL&, void* notifyData, const WebURLError&) { }
    virtual bool isPlaceholder() { return false; }

    // WebExternalTextureLayerClient methods:
    virtual unsigned prepareTexture(WebTextureUpdater&) { return m_colorTexture; }
    virtual WebGraphicsContext3D* context() { return m_context; }

private:
    enum Primitive {
        PrimitiveNone,
        PrimitiveTriangle
    };

    struct Scene {
        Primitive primitive;
        unsigned backgroundColor[3];
        unsigned primitiveColor[3];
        float opacity;

        unsigned vbo;
        unsigned program;
        int colorLocation;
        int positionLocation;

        Scene()
            : primitive(PrimitiveNone)
            , opacity(1.0f) // Fully opaque.
            , vbo(0)
            , program(0)
            , colorLocation(-1)
            , positionLocation(-1)
        {
            backgroundColor[0] = backgroundColor[1] = backgroundColor[2] = 0;
            primitiveColor[0] = primitiveColor[1] = primitiveColor[2] = 0;
        }
    };

    // Functions for parsing plugin parameters.
    Primitive parsePrimitive(const WebString&);
    void parseColor(const WebString&, unsigned color[3]);
    float parseOpacity(const WebString&);
    bool parseBoolean(const WebString&);

    // Functions for loading and drawing scene.
    bool initScene();
    void drawScene();
    void destroyScene();
    bool initProgram();
    bool initPrimitive();
    void drawPrimitive();
    unsigned loadShader(unsigned type, const WTF::CString& source);
    unsigned loadProgram(const WTF::CString& vertexSource, const WTF::CString& fragmentSource);

    WebFrame* m_frame;
    WebTestDelegate* m_delegate;
    WebPluginContainer* m_container;

    WebRect m_rect;
    WebGraphicsContext3D* m_context;
    unsigned m_colorTexture;
    unsigned m_framebuffer;
    Scene m_scene;
    OwnPtr<WebExternalTextureLayer> m_layer;

    WebPluginContainer::TouchEventRequestType m_touchEventRequest;
    bool m_printEventDetails;
    bool m_printUserGestureStatus;
    bool m_canProcessDrag;
};

WebTestPluginImpl::WebTestPluginImpl(WebFrame* frame, const WebPluginParams& params, WebTestDelegate* delegate)
    : m_frame(frame)
    , m_delegate(delegate)
    , m_container(0)
    , m_context(0)
    , m_touchEventRequest(WebPluginContainer::TouchEventRequestTypeNone)
    , m_printEventDetails(false)
    , m_printUserGestureStatus(false)
    , m_canProcessDrag(false)
{
    static const WebString kAttributePrimitive = WebString::fromUTF8("primitive");
    static const WebString kAttributeBackgroundColor = WebString::fromUTF8("background-color");
    static const WebString kAttributePrimitiveColor = WebString::fromUTF8("primitive-color");
    static const WebString kAttributeOpacity = WebString::fromUTF8("opacity");
    static const WebString kAttributeAcceptsTouch = WebString::fromUTF8("accepts-touch");
    static const WebString kAttributePrintEventDetails = WebString::fromUTF8("print-event-details");
    static const WebString kAttributeCanProcessDrag = WebString::fromUTF8("can-process-drag");
    static const WebString kAttributePrintUserGestureStatus = WebString::fromUTF8("print-user-gesture-status");

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
        else if (attributeName == kAttributeAcceptsTouch)
            m_touchEventRequest = parseTouchEventRequestType(attributeValue);
        else if (attributeName == kAttributePrintEventDetails)
            m_printEventDetails = parseBoolean(attributeValue);
        else if (attributeName == kAttributeCanProcessDrag)
            m_canProcessDrag = parseBoolean(attributeValue);
        else if (attributeName == kAttributePrintUserGestureStatus)
            m_printUserGestureStatus = parseBoolean(attributeValue);
    }
}

WebTestPluginImpl::~WebTestPluginImpl()
{
}

bool WebTestPluginImpl::initialize(WebPluginContainer* container)
{
    WebGraphicsContext3D::Attributes attrs;
    m_context = webKitPlatformSupport()->createOffscreenGraphicsContext3D(attrs);
    if (!m_context)
        return false;

    if (!m_context->makeContextCurrent())
        return false;

    if (!initScene())
        return false;

    m_layer = adoptPtr(webKitPlatformSupport()->compositorSupport()->createExternalTextureLayer(this));
    m_container = container;
    m_container->setWebLayer(m_layer->layer());
    m_container->requestTouchEventType(m_touchEventRequest);
    m_container->setWantsWheelEvents(true);
    return true;
}

void WebTestPluginImpl::destroy()
{
    if (m_container)
        m_container->setWebLayer(0);
    m_layer.clear();
    destroyScene();

    delete m_context;
    m_context = 0;

    m_container = 0;
    m_frame = 0;
}

void WebTestPluginImpl::updateGeometry(const WebRect& frameRect, const WebRect& clipRect, const WebVector<WebRect>& cutOutsRects, bool isVisible)
{
    if (clipRect == m_rect)
        return;
    m_rect = clipRect;
    if (m_rect.isEmpty())
        return;

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
    m_layer->layer()->invalidate();
}

WebTestPluginImpl::Primitive WebTestPluginImpl::parsePrimitive(const WebString& string)
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
void WebTestPluginImpl::parseColor(const WebString& string, unsigned color[3])
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

float WebTestPluginImpl::parseOpacity(const WebString& string)
{
    return static_cast<float>(atof(string.utf8().data()));
}

bool WebTestPluginImpl::parseBoolean(const WebString& string)
{
    static const WebString kPrimitiveTrue = WebString::fromUTF8("true");
    return string == kPrimitiveTrue;
}

bool WebTestPluginImpl::initScene()
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

void WebTestPluginImpl::drawScene()
{
    m_context->viewport(0, 0, m_rect.width, m_rect.height);
    m_context->clear(GL_COLOR_BUFFER_BIT);

    if (m_scene.primitive != PrimitiveNone)
        drawPrimitive();
}

void WebTestPluginImpl::destroyScene()
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

bool WebTestPluginImpl::initProgram()
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

bool WebTestPluginImpl::initPrimitive()
{
    ASSERT(m_scene.primitive == PrimitiveTriangle);

    m_scene.vbo = m_context->createBuffer();
    if (!m_scene.vbo)
        return false;

    const float vertices[] = {
        0.0f,  0.8f, 0.0f,
        -0.8f, -0.8f, 0.0f,
        0.8f, -0.8f, 0.0f };
    m_context->bindBuffer(GL_ARRAY_BUFFER, m_scene.vbo);
    m_context->bufferData(GL_ARRAY_BUFFER, sizeof(vertices), 0, GL_STATIC_DRAW);
    m_context->bufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
    return true;
}

void WebTestPluginImpl::drawPrimitive()
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

unsigned WebTestPluginImpl::loadShader(unsigned type, const CString& source)
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

unsigned WebTestPluginImpl::loadProgram(const CString& vertexSource, const CString& fragmentSource)
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

bool WebTestPluginImpl::handleInputEvent(const WebInputEvent& event, WebCursorInfo& info)
{
    const char* eventName = 0;
    switch (event.type) {
    case WebInputEvent::Undefined:           eventName = "unknown"; break;

    case WebInputEvent::MouseDown:           eventName = "MouseDown"; break;
    case WebInputEvent::MouseUp:             eventName = "MouseUp"; break;
    case WebInputEvent::MouseMove:           eventName = "MouseMove"; break;
    case WebInputEvent::MouseEnter:          eventName = "MouseEnter"; break;
    case WebInputEvent::MouseLeave:          eventName = "MouseLeave"; break;
    case WebInputEvent::ContextMenu:         eventName = "ContextMenu"; break;

    case WebInputEvent::MouseWheel:          eventName = "MouseWheel"; break;

    case WebInputEvent::RawKeyDown:          eventName = "RawKeyDown"; break;
    case WebInputEvent::KeyDown:             eventName = "KeyDown"; break;
    case WebInputEvent::KeyUp:               eventName = "KeyUp"; break;
    case WebInputEvent::Char:                eventName = "Char"; break;

    case WebInputEvent::GestureScrollBegin:  eventName = "GestureScrollBegin"; break;
    case WebInputEvent::GestureScrollEnd:    eventName = "GestureScrollEnd"; break;
    case WebInputEvent::GestureScrollUpdate: eventName = "GestureScrollUpdate"; break;
    case WebInputEvent::GestureFlingStart:   eventName = "GestureFlingStart"; break;
    case WebInputEvent::GestureFlingCancel:  eventName = "GestureFlingCancel"; break;
    case WebInputEvent::GestureTap:          eventName = "GestureTap"; break;
    case WebInputEvent::GestureTapDown:      eventName = "GestureTapDown"; break;
    case WebInputEvent::GestureTapCancel:    eventName = "GestureTapCancel"; break;
    case WebInputEvent::GestureDoubleTap:    eventName = "GestureDoubleTap"; break;
    case WebInputEvent::GestureTwoFingerTap: eventName = "GestureTwoFingerTap"; break;
    case WebInputEvent::GestureLongPress:    eventName = "GestureLongPress"; break;
    case WebInputEvent::GestureLongTap:      eventName = "GestureLongTap"; break;
    case WebInputEvent::GesturePinchBegin:   eventName = "GesturePinchBegin"; break;
    case WebInputEvent::GesturePinchEnd:     eventName = "GesturePinchEnd"; break;
    case WebInputEvent::GesturePinchUpdate:  eventName = "GesturePinchUpdate"; break;

    case WebInputEvent::TouchStart:          eventName = "TouchStart"; break;
    case WebInputEvent::TouchMove:           eventName = "TouchMove"; break;
    case WebInputEvent::TouchEnd:            eventName = "TouchEnd"; break;
    case WebInputEvent::TouchCancel:         eventName = "TouchCancel"; break;
    }

    m_delegate->printMessage(std::string("Plugin received event: ") + (eventName ? eventName : "unknown") + "\n");
    if (m_printEventDetails)
        printEventDetails(m_delegate, event);
    if (m_printUserGestureStatus)
        m_delegate->printMessage(std::string("* ") + (m_frame->isProcessingUserGesture() ? "" : "not ") + "handling user gesture\n");
    return false;
}

bool WebTestPluginImpl::handleDragStatusUpdate(WebDragStatus dragStatus, const WebDragData&, WebDragOperationsMask, const WebPoint& position, const WebPoint& screenPosition)
{
    const char* dragStatusName = 0;
    switch (dragStatus) {
    case WebDragStatusEnter:
        dragStatusName = "DragEnter";
        break;
    case WebDragStatusOver:
        dragStatusName = "DragOver";
        break;
    case WebDragStatusLeave:
        dragStatusName = "DragLeave";
        break;
    case WebDragStatusDrop:
        dragStatusName = "DragDrop";
        break;
    case WebDragStatusUnknown:
        ASSERT_NOT_REACHED();
    }
    m_delegate->printMessage(std::string("Plugin received event: ") + dragStatusName + "\n");
    return false;
}

}

WebTestPlugin* WebTestPlugin::create(WebFrame* frame, const WebPluginParams& params, WebTestDelegate* delegate)
{
    return new WebTestPluginImpl(frame, params, delegate);
}

WebTestPlugin::WebTestPlugin()
{
}

WebTestPlugin::~WebTestPlugin()
{
}

const WebString& WebTestPlugin::mimeType()
{
    static const WebString kMimeType = WebString::fromUTF8("application/x-webkit-test-webplugin");
    return kMimeType;
}

}
