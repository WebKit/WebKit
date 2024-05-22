/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEViewQtQuick.h"

#include "WPEQtView.h"

#include <epoxy/egl.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>

#include <QOffscreenSurface>
#include <QOpenGLFunctions>
#include <QQuickWindow>
#include <QSGTexture>

/**
 * WPEViewQtQuick:
 *
 */
struct _WPEViewQtQuickPrivate {
    GRefPtr<WPEBuffer> pendingBuffer;
    GRefPtr<WPEBuffer> committedBuffer;
    bool bufferUpdateRequested;
    GLuint textureId;
    GLuint textureUniform;
    GLuint program;
    QOffscreenSurface surface;
    QOpenGLContext* context;
    WPEQtView* wpeQtView;

    // Event handling
    bool isHovering;
    std::optional<QPointF> lastMousePosition;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewQtQuick, wpe_view_qtquick, WPE_TYPE_VIEW, WPEView)

static PFNGLEGLIMAGETARGETTEXTURE2DOESPROC imageTargetTexture2DOES;

static void wpeViewQtQuickDispose(GObject* object)
{
    auto* priv = WPE_VIEW_QTQUICK(object)->priv;
    if (priv->textureId) {
        if (auto* glFunctions = priv->context ? priv->context->functions() : nullptr)
            glFunctions->glDeleteTextures(1, &priv->textureId);
        priv->textureId = 0;
    }

    G_OBJECT_CLASS(wpe_view_qtquick_parent_class)->dispose(object);
}

static gboolean wpeViewQtQuickRenderBuffer(WPEView* view, WPEBuffer* buffer, GError**)
{
    auto* priv = WPE_VIEW_QTQUICK(view)->priv;
    priv->pendingBuffer = buffer;
    priv->bufferUpdateRequested = true;

    priv->wpeQtView->triggerUpdateScene();
    return TRUE;
}

static gboolean wpeViewQtQuickResize(WPEView* view, int width, int height)
{
    wpe_view_resized(view, width, height);
    return TRUE;
}

static void wpe_view_qtquick_class_init(WPEViewQtQuickClass* viewQtQuickClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewQtQuickClass);
    objectClass->dispose = wpeViewQtQuickDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewQtQuickClass);
    viewClass->render_buffer = wpeViewQtQuickRenderBuffer;
    viewClass->resize = wpeViewQtQuickResize;
}

WPEView* wpe_view_qtquick_new(WPEDisplayQtQuick* display)
{
    g_return_val_if_fail(WPE_IS_DISPLAY_QTQUICK(display), nullptr);
    return WPE_VIEW(g_object_new(WPE_TYPE_VIEW_QTQUICK, "display", display, nullptr));
}

gboolean wpe_view_qtquick_initialize_rendering(WPEViewQtQuick* view, WPEQtView* wpeQtView, GError** error)
{
    auto* window = wpeQtView ? wpeQtView->window() : nullptr;
    if (!window || !window->rendererInterface()) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: Cannot access renderer interface via Qt");
        return FALSE;
    }

    auto* context = static_cast<QOpenGLContext*>(window->rendererInterface()->getResource(window, QSGRendererInterface::OpenGLContextResource));
    if (!context) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to initialize rendering: Cannot retrieve OpenGL context via Qt");
        return FALSE;
    }

    auto* priv = WPE_VIEW_QTQUICK(view)->priv;
    priv->context = context;
    priv->wpeQtView = wpeQtView;

    if (!imageTargetTexture2DOES)
        imageTargetTexture2DOES = reinterpret_cast<PFNGLEGLIMAGETARGETTEXTURE2DOESPROC>(eglGetProcAddress("glEGLImageTargetTexture2DOES"));

    static const char* vertexShaderSource =
        "attribute vec2 pos;\n"
        "attribute vec2 texture;\n"
        "varying vec2 v_texture;\n"
        "void main() {\n"
        "  v_texture = texture;\n"
        "  gl_Position = vec4(pos, 0, 1);\n"
        "}\n";

    static const char* fragmentShaderSource =
        "precision mediump float;\n"
        "uniform sampler2D u_texture;\n"
        "varying vec2 v_texture;\n"
        "void main() {\n"
        "  gl_FragColor = texture2D(u_texture, v_texture);\n"
        "}\n";

    auto* glFunctions = context->functions();
    auto vertexShader = glFunctions->glCreateShader(GL_VERTEX_SHADER);
    glFunctions->glShaderSource(vertexShader, 1, &vertexShaderSource, nullptr);
    glFunctions->glCompileShader(vertexShader);

    auto fragmentShader = glFunctions->glCreateShader(GL_FRAGMENT_SHADER);
    glFunctions->glShaderSource(fragmentShader, 1, &fragmentShaderSource, nullptr);
    glFunctions->glCompileShader(fragmentShader);

    priv->program = glFunctions->glCreateProgram();
    glFunctions->glAttachShader(priv->program, vertexShader);
    glFunctions->glAttachShader(priv->program, fragmentShader);

    glFunctions->glBindAttribLocation(priv->program, 0, "pos");
    glFunctions->glBindAttribLocation(priv->program, 1, "texture");

    glFunctions->glLinkProgram(priv->program);
    priv->textureUniform = glFunctions->glGetUniformLocation(priv->program, "u_texture");

    priv->surface.setFormat(context->format());
    priv->surface.create();

    return TRUE;
}

QSGTexture* wpe_view_qtquick_render_buffer_to_texture(WPEViewQtQuick* view, QSize size, GError** error)
{
    auto* priv = WPE_VIEW_QTQUICK(view)->priv;
    priv->context->makeCurrent(&priv->surface);

    auto wrapNativeTexture = [&]() -> QSGTexture* {
        RELEASE_ASSERT(priv->wpeQtView->window());

        auto texture = QNativeInterface::QSGOpenGLTexture::fromNative(priv->textureId, priv->wpeQtView->window(), size, QQuickWindow::TextureHasAlphaChannel);
        if (UNLIKELY(!texture)) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to import QSOpenGLTexture from native OpenGL texture");
            return nullptr;
        }

        return texture;
    };

    if (UNLIKELY(!priv->pendingBuffer)) {
        if (!priv->committedBuffer) {
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render, no pending buffer available to render into, and no commitedBuffer.");
            return nullptr;
        }

        RELEASE_ASSERT(priv->textureId);
        return wrapNativeTexture();
    }

    GUniqueOutPtr<GError> bufferError;
    auto eglImage = wpe_buffer_import_to_egl_image(priv->pendingBuffer.get(), &bufferError.outPtr());
    if (UNLIKELY(!eglImage)) {
        g_set_error(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to render: %s", bufferError->message);
        return nullptr;
    }

    auto* glFunctions = priv->context->functions();
    if (UNLIKELY(!priv->textureId)) {
        glFunctions->glGenTextures(1, &priv->textureId);
        glFunctions->glBindTexture(GL_TEXTURE_2D, priv->textureId);
        glFunctions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glFunctions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glFunctions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glFunctions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glFunctions->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr); // Passed size doesn't matter.
        glFunctions->glBindTexture(GL_TEXTURE_2D, 0);
    }

    glFunctions->glClearColor(1, 0, 0, 1);
    glFunctions->glClear(GL_COLOR_BUFFER_BIT);

    glFunctions->glUseProgram(priv->program);

    glFunctions->glActiveTexture(GL_TEXTURE0);
    glFunctions->glBindTexture(GL_TEXTURE_2D, priv->textureId);
    imageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);
    glFunctions->glUniform1i(priv->textureUniform, 0);

    static const GLfloat vertices[4][2] = {
        { -1.0, 1.0 },
        { 1.0, 1.0 },
        { -1.0, -1.0 },
        { 1.0, -1.0 },
    };

    static const GLfloat texturePos[4][2] = {
        { 0, 0 },
        { 1, 0 },
        { 0, 1 },
        { 1, 1 },
    };

    glFunctions->glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, vertices);
    glFunctions->glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, texturePos);

    glFunctions->glEnableVertexAttribArray(0);
    glFunctions->glEnableVertexAttribArray(1);

    glFunctions->glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glFunctions->glDisableVertexAttribArray(0);
    glFunctions->glDisableVertexAttribArray(1);

    // Wrap in QSGOpenGLTexture for Qt Scene Graph.
    auto texture = QNativeInterface::QSGOpenGLTexture::fromNative(priv->textureId, priv->wpeQtView->window(), size, QQuickWindow::TextureHasAlphaChannel);
    if (!texture) {
        g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to import QSOpenGLTexture from native OpenGL texture");
        return nullptr;
    }

    return texture;
}

void wpe_view_qtquick_did_update_scene(WPEViewQtQuick* view)
{
    auto* priv = view->priv;
    if (!priv->bufferUpdateRequested)
        return;

    priv->bufferUpdateRequested = false;
    if (priv->committedBuffer)
        wpe_view_buffer_released(WPE_VIEW(view), priv->committedBuffer.get());
    priv->committedBuffer = WTFMove(priv->pendingBuffer);
    wpe_view_buffer_rendered(WPE_VIEW(view), priv->committedBuffer.get());
}

// Event handling
static inline guint buttonFromEvent(QMouseEvent* event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        return WPE_BUTTON_PRIMARY;
    case Qt::MiddleButton:
        return WPE_BUTTON_MIDDLE;
    case Qt::RightButton:
        return WPE_BUTTON_SECONDARY;
    default:
        break;
    }

    return 0;
}

template<typename QtEvent>
static inline uint32_t mouseModifiersFromEvent(QtEvent* event)
{
    switch (event->button()) {
    case Qt::LeftButton:
        return WPE_MODIFIER_POINTER_BUTTON1;
    case Qt::RightButton:
        return WPE_MODIFIER_POINTER_BUTTON2;
    default:
        break;
    }

    return 0;
}

template<typename QtEvent>
static inline uint32_t keyboardModifiersFromEvent(QtEvent* event)
{
    uint32_t result = 0;

    // No support for WPE_MODIFIER_KEYBOARD_CAPS_LOCK. Qt doesn't expose CapsLock.
    auto modifiers = event->modifiers();
    if (modifiers.testFlag(Qt::ShiftModifier))
        result |= WPE_MODIFIER_KEYBOARD_SHIFT;
    if (modifiers.testFlag(Qt::ControlModifier))
        result |= WPE_MODIFIER_KEYBOARD_CONTROL;
    if (modifiers.testFlag(Qt::AltModifier))
        result |= WPE_MODIFIER_KEYBOARD_ALT;
    if (modifiers.testFlag(Qt::MetaModifier))
        result |= WPE_MODIFIER_KEYBOARD_META;

    return result;
}

template<typename QtEvent>
static inline WPEModifiers modifiersFromEvent(QtEvent* event)
{
    return static_cast<WPEModifiers>(mouseModifiersFromEvent(event) | keyboardModifiersFromEvent(event));
}

// Mouse events
void wpe_view_dispatch_mouse_press_event(WPEViewQtQuick *view, QMouseEvent *event)
{
    auto position = event->position().toPoint();
    auto button = buttonFromEvent(event);

    auto pressCount = wpe_view_compute_press_count(WPE_VIEW(view), position.x(), position.y(), button, event->timestamp());
    auto* wpeEvent = wpe_event_pointer_button_new(WPE_EVENT_POINTER_DOWN, WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), button, position.x(), position.y(), pressCount);
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void wpe_view_dispatch_mouse_move_event(WPEViewQtQuick *view, QMouseEvent *event)
{
    auto position = event->position().toPoint();
    auto delta = view->priv->lastMousePosition ? position - view->priv->lastMousePosition.value() : QPointF();
    view->priv->lastMousePosition = position;

    auto* wpeEvent = wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), position.x(), position.y(), delta.x(), delta.y());
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void wpe_view_dispatch_mouse_release_event(WPEViewQtQuick *view, QMouseEvent *event)
{
    auto position = event->position().toPoint();
    auto* wpeEvent = wpe_event_pointer_button_new(WPE_EVENT_POINTER_UP, WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), buttonFromEvent(event), position.x(), position.y(), 0 /* pressCount */);
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

// Wheel events
void wpe_view_dispatch_wheel_event(WPEViewQtQuick *view, QWheelEvent *event)
{
    auto position = event->position().toPoint();
    auto numPixels = event->pixelDelta();

    auto* wpeEvent = wpe_event_scroll_new(WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), numPixels.x(), numPixels.y(), FALSE, FALSE, position.x(), position.y());
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

// Hover events
void wpe_view_dispatch_hover_enter_event(WPEViewQtQuick *view, QHoverEvent *)
{
    view->priv->isHovering = true;
}

void wpe_view_dispatch_hover_move_event(WPEViewQtQuick *view, QHoverEvent *event)
{
    if (!view->priv->isHovering)
        return;

    auto position = event->position().toPoint();
    auto delta = view->priv->lastMousePosition ? position - view->priv->lastMousePosition.value() : QPointF();
    view->priv->lastMousePosition = position;

    auto* wpeEvent = wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), position.x(), position.y(), delta.x(), delta.y());
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void wpe_view_dispatch_hover_leave_event(WPEViewQtQuick *view, QHoverEvent *)
{
    view->priv->isHovering = false;
}

// Keyboard events
void wpe_view_dispatch_key_press_event(WPEViewQtQuick *view, QKeyEvent *event)
{
    auto modifiers = static_cast<WPEModifiers>(keyboardModifiersFromEvent(event));
    auto* wpeEvent = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_DOWN, WPE_VIEW(view), WPE_INPUT_SOURCE_KEYBOARD, event->timestamp(),
        modifiers, event->key(), event->nativeVirtualKey());
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void wpe_view_dispatch_key_release_event(WPEViewQtQuick *view, QKeyEvent *event)
{
    auto modifiers = static_cast<WPEModifiers>(keyboardModifiersFromEvent(event));
    auto* wpeEvent = wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, WPE_VIEW(view), WPE_INPUT_SOURCE_KEYBOARD, event->timestamp(),
        modifiers, event->key(), event->nativeVirtualKey());
    wpe_view_event(WPE_VIEW(view), wpeEvent);
    wpe_event_unref(wpeEvent);
}

// Touch events
void wpe_view_dispatch_touch_event(WPEViewQtQuick *view, QTouchEvent *event)
{
    WPEEventType eventType = WPE_EVENT_NONE;
    switch (event->type()) {
    case QEvent::TouchBegin:
        eventType = WPE_EVENT_TOUCH_DOWN;
        break;
    case QEvent::TouchUpdate:
        eventType = WPE_EVENT_TOUCH_MOVE;
        break;
    case QEvent::TouchEnd:
        eventType = WPE_EVENT_TOUCH_UP;
        break;
    default:
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }

    auto modifiers = static_cast<WPEModifiers>(keyboardModifiersFromEvent(event));
    for (auto& point : event->points()) {
        auto position = point.position();
        auto* wpeEvent = wpe_event_touch_new(eventType, WPE_VIEW(view), WPE_INPUT_SOURCE_TOUCHPAD, event->timestamp(),
            modifiers, point.id(), position.x(), position.y());
        wpe_view_event(WPE_VIEW(view), wpeEvent);
        wpe_event_unref(wpeEvent);
    }
}
