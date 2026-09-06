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

#include <epoxy/egl.h> // NOLINT(build/include_order) -- epoxy must precede Qt OpenGL headers.
#include <wtf/SortedArrayMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/RunLoopSourcePriority.h>
#include <wtf/glib/WTFGType.h>

#include <QOpenGLContext>
#include <QCursor>
#include <QQuickWindow>
#include <QSGRendererInterface>

#include <string_view>

/**
 * WPEViewQtQuick:
 *
 */
struct _WPEViewQtQuickPrivate {
    GRefPtr<WPEBuffer> pendingBuffer;
    GRefPtr<WPEBuffer> committedBuffer;
    GRefPtr<WPEBuffer> previousCommittedBuffer;
    GRefPtr<WPEBuffer> bufferAwaitingAck;
    WPEQtView* wpeQtView;

    // Event handling
    bool isHovering;
    std::optional<QPointF> lastMousePosition;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEViewQtQuick, wpe_view_qtquick, WPE_TYPE_VIEW, WPEView)

static void wpeViewQtQuickScheduleUpdate(WPEViewQtQuickPrivate* priv)
{
    if (!priv->wpeQtView)
        return;

    priv->wpeQtView->update();
    if (auto* window = priv->wpeQtView->window())
        window->update();
}

static void wpeViewQtQuickDispose(GObject* object)
{
    wpe_view_qtquick_invalidate_rendering(WPE_VIEW_QTQUICK(object));
    G_OBJECT_CLASS(wpe_view_qtquick_parent_class)->dispose(object);
}

static gboolean wpeViewQtQuickRenderBuffer(WPEView* view, WPEBuffer* buffer, const WPERectangle*, guint, GError**)
{
    // TODO: Add support for damage rects.
    auto* priv = WPE_VIEW_QTQUICK(view)->priv;

    GRefPtr<WPEBuffer> replacedBuffer = WTF::move(priv->pendingBuffer);
    priv->pendingBuffer = buffer;

    if (replacedBuffer) {
        wpe_view_buffer_rendered(view, replacedBuffer.get());
        wpe_view_buffer_released(view, replacedBuffer.get());
    }

    wpeViewQtQuickScheduleUpdate(priv);
    return TRUE;
}

static QCursor webCursorNameToQCursor(const char* name)
{
    // https://developer.mozilla.org/en-US/docs/Web/CSS/Reference/Properties/cursor
    // https://doc.qt.io/qt-6/qcursor.html
    // Unsupported in Qt:
    // context-menu, cell, all-scroll, zoom-in, zoom-out
    // Limited:
    // vertical-text (fallback to text),
    // no-drop (fallback to forbidden),
    // all single-direction resizes (fallback to double-direction resizes)
    using namespace std::literals;
    static constexpr SortedArrayMap shapeMap { WTF::toArray<std::pair<std::string_view, Qt::CursorShape>>({
        { "alias"sv, Qt::DragLinkCursor },
        { "col-resize"sv, Qt::SplitHCursor },
        { "copy"sv, Qt::DragCopyCursor },
        { "crosshair"sv, Qt::CrossCursor },
        { "default"sv, Qt::ArrowCursor },
        { "e-resize"sv, Qt::SizeHorCursor },
        { "ew-resize"sv, Qt::SizeHorCursor },
        { "grab"sv, Qt::OpenHandCursor },
        { "grabbing"sv, Qt::ClosedHandCursor },
        { "help"sv, Qt::WhatsThisCursor },
        { "move"sv, Qt::DragMoveCursor },
        { "n-resize"sv, Qt::SizeVerCursor },
        { "ne-resize"sv, Qt::SizeBDiagCursor },
        { "nesw-resize"sv, Qt::SizeBDiagCursor },
        { "no-drop"sv, Qt::ForbiddenCursor },
        { "none"sv, Qt::BlankCursor },
        { "not-allowed"sv, Qt::ForbiddenCursor },
        { "ns-resize"sv, Qt::SizeVerCursor },
        { "nw-resize"sv, Qt::SizeFDiagCursor },
        { "nwse-resize"sv, Qt::SizeFDiagCursor },
        { "pointer"sv, Qt::PointingHandCursor },
        { "progress"sv, Qt::BusyCursor },
        { "row-resize"sv, Qt::SplitVCursor },
        { "s-resize"sv, Qt::SizeVerCursor },
        { "se-resize"sv, Qt::SizeFDiagCursor },
        { "sw-resize"sv, Qt::SizeBDiagCursor },
        { "text"sv, Qt::IBeamCursor },
        { "vertical-text"sv, Qt::IBeamCursor },
        { "w-resize"sv, Qt::SizeHorCursor },
        { "wait"sv, Qt::WaitCursor },
    }) };
    auto shape = shapeMap.get(std::string_view(name), Qt::ArrowCursor);
    return QCursor(shape);
}

static void wpeViewQtQuickSetCursorFromName(WPEView* view, const char* name)
{
    auto* priv = WPE_VIEW_QTQUICK(view)->priv;
    QCursor cursor = webCursorNameToQCursor(name);
    priv->wpeQtView->setCursor(cursor);
}

static void wpe_view_qtquick_class_init(WPEViewQtQuickClass* viewQtQuickClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(viewQtQuickClass);
    objectClass->dispose = wpeViewQtQuickDispose;

    WPEViewClass* viewClass = WPE_VIEW_CLASS(viewQtQuickClass);
    viewClass->render_buffer = wpeViewQtQuickRenderBuffer;
    viewClass->set_cursor_from_name = wpeViewQtQuickSetCursorFromName;
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
    priv->wpeQtView = wpeQtView;

    return TRUE;
}

WPEBuffer* wpe_view_qtquick_acquire_frame(WPEViewQtQuick* view, EGLImage* outImage, gboolean* didPromote, GError** error)
{
    auto* priv = view->priv;

    if (didPromote)
        *didPromote = FALSE;

    // Reuse the committed buffer while its previous frame is awaiting acknowledgement.
    auto frameBuffer = priv->pendingBuffer && !priv->bufferAwaitingAck ? priv->pendingBuffer : priv->committedBuffer;
    if (!frameBuffer)
        return nullptr;

    GUniqueOutPtr<GError> bufferError;
    auto eglImage = static_cast<EGLImage>(wpe_buffer_import_to_egl_image(frameBuffer.get(), &bufferError.outPtr()));
    if (!eglImage) {
        if (error && bufferError)
            g_propagate_error(error, bufferError.release());
        else if (error)
            g_set_error_literal(error, WPE_VIEW_ERROR, WPE_VIEW_ERROR_RENDER_FAILED, "Failed to import WPE buffer");
        return nullptr;
    }

    if (frameBuffer == priv->pendingBuffer) {
        priv->previousCommittedBuffer = WTF::move(priv->committedBuffer);
        priv->committedBuffer = WTF::move(priv->pendingBuffer);
        priv->bufferAwaitingAck = priv->committedBuffer;
        if (didPromote)
            *didPromote = TRUE;
    }

    *outImage = eglImage;

    return frameBuffer.leakRef();
}

void wpe_view_qtquick_rollback_frame(WPEViewQtQuick* view)
{
    auto* priv = view->priv;
    if (!priv->bufferAwaitingAck)
        return;

    GRefPtr<WPEBuffer> failedBuffer = WTF::move(priv->committedBuffer);
    ASSERT(!priv->pendingBuffer);

    priv->pendingBuffer = WTF::move(failedBuffer);
    priv->committedBuffer = WTF::move(priv->previousCommittedBuffer);
    priv->bufferAwaitingAck = nullptr;

    if (priv->wpeQtView)
        priv->wpeQtView->triggerUpdateScene();
}

void wpe_view_qtquick_invalidate_rendering(WPEViewQtQuick* view)
{
    auto* priv = view->priv;

    GRefPtr<WPEBuffer> committedBuffer = WTF::move(priv->bufferAwaitingAck);
    GRefPtr<WPEBuffer> previousCommittedBuffer = WTF::move(priv->previousCommittedBuffer);
    GRefPtr<WPEBuffer> pendingBuffer = WTF::move(priv->pendingBuffer);
    priv->committedBuffer = nullptr;

    if (previousCommittedBuffer)
        wpe_view_buffer_released(WPE_VIEW(view), previousCommittedBuffer.get());
    if (committedBuffer)
        wpe_view_buffer_rendered(WPE_VIEW(view), committedBuffer.get());
    if (pendingBuffer) {
        wpe_view_buffer_rendered(WPE_VIEW(view), pendingBuffer.get());
        wpe_view_buffer_released(WPE_VIEW(view), pendingBuffer.get());
    }

    priv->wpeQtView = nullptr;
}

void wpe_view_qtquick_set_frame_release_fence(WPEViewQtQuick* view, int fd)
{
    if (view->priv->committedBuffer)
        wpe_buffer_set_release_fence(view->priv->committedBuffer.get(), fd);
}

void wpe_view_qtquick_did_update_scene(WPEViewQtQuick* view)
{
    auto* priv = view->priv;
    if (!priv->bufferAwaitingAck)
        return;

    auto committedBuffer = WTF::move(priv->bufferAwaitingAck);
    auto previousCommittedBuffer = WTF::move(priv->previousCommittedBuffer);
    if (previousCommittedBuffer)
        wpe_view_buffer_released(WPE_VIEW(view), previousCommittedBuffer.get());
    if (committedBuffer)
        wpe_view_buffer_rendered(WPE_VIEW(view), committedBuffer.get());
    if (priv->pendingBuffer)
        wpeViewQtQuickScheduleUpdate(priv);
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
    double scrollX = 0;
    double scrollY = 0;
    auto hasPreciseDeltas = !numPixels.isNull();
    if (hasPreciseDeltas) {
        scrollX = numPixels.x();
        scrollY = numPixels.y();
    } else {
        // Qt gives 120 for the wheel scroll of one tick
        // In WebEventFactoryWPE.cpp, it accepts number of ticks
        // for wheel events and convert it to pixels.
        auto angleDelta = event->angleDelta().toPointF() / 120;
        scrollX = angleDelta.x();
        scrollY = angleDelta.y();
    }
    auto* wpeEvent = wpe_event_scroll_new(WPE_VIEW(view), WPE_INPUT_SOURCE_MOUSE, event->timestamp(),
        modifiersFromEvent(event), scrollX, scrollY, hasPreciseDeltas, FALSE, position.x(), position.y());
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
