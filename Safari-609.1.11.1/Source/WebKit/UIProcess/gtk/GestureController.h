/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/FloatPoint.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventTouch GdkEventTouch;
typedef struct _GdkEventSequence GdkEventSequence;
typedef struct _GtkGesture GtkGesture;

namespace WebKit {

class GestureControllerClient {
public:
    virtual ~GestureControllerClient() = default;

    virtual void tap(GdkEventTouch*) = 0;

    virtual void startDrag(GdkEventTouch*, const WebCore::FloatPoint&) = 0;
    virtual void drag(GdkEventTouch*, const WebCore::FloatPoint&, const WebCore::FloatPoint&) = 0;
    virtual void cancelDrag() = 0;

    virtual void swipe(GdkEventTouch*, const WebCore::FloatPoint&) = 0;

    virtual void startZoom(const WebCore::IntPoint& center, double& initialScale, WebCore::IntPoint& initialPoint) = 0;
    virtual void zoom(double scale, const WebCore::IntPoint& origin) = 0;

    virtual void longPress(GdkEventTouch*) = 0;
};

class GestureController {
    WTF_MAKE_NONCOPYABLE(GestureController);
    WTF_MAKE_FAST_ALLOCATED;

public:
    GestureController(GtkWidget*, std::unique_ptr<GestureControllerClient>&&);

    bool isProcessingGestures() const;
    bool handleEvent(GdkEvent*);

    void reset()
    {
        m_dragGesture.reset();
        m_swipeGesture.reset();
        m_zoomGesture.reset();
        m_longpressGesture.reset();
    }

private:
    class Gesture {
    public:
        void reset();
        bool isActive() const;
        void handleEvent(GdkEvent*);

    protected:
        Gesture(GtkGesture*, GestureControllerClient&);

        GRefPtr<GtkGesture> m_gesture;
        GestureControllerClient& m_client;
    };

    class DragGesture final : public Gesture {
    public:
        DragGesture(GtkWidget*, GestureControllerClient&);

    private:
        // Notify that a drag started, allowing to stop kinetic deceleration.
        void startDrag(GdkEvent*);
        void handleDrag(GdkEvent*, double x, double y);
        void cancelDrag();
        void handleTap(GdkEvent*);
        void longPressFired();

        static void begin(DragGesture*, double x, double y, GtkGesture*);
        static void update(DragGesture*, double x, double y, GtkGesture*);
        static void end(DragGesture*, GdkEventSequence*, GtkGesture*);
        static void cancel(DragGesture*, GdkEventSequence*, GtkGesture*);

        WebCore::FloatPoint m_start;
        WebCore::FloatPoint m_offset;
        RunLoop::Timer<DragGesture> m_longPressTimeout;
        GRefPtr<GtkGesture> m_longPress;
        bool m_inDrag { false };
    };

    class SwipeGesture final : public Gesture {
    public:
        SwipeGesture(GtkWidget*, GestureControllerClient&);

    private:
        void startMomentumScroll(GdkEvent*, double velocityX, double velocityY);

        static void swipe(SwipeGesture*, double velocityX, double velocityY, GtkGesture*);
    };

    class ZoomGesture final : public Gesture {
    public:
        ZoomGesture(GtkWidget*, GestureControllerClient&);

    private:
        WebCore::IntPoint center() const;
        void startZoom();
        void handleZoom();

        static void begin(ZoomGesture*, GdkEventSequence*, GtkGesture*);
        static void scaleChanged(ZoomGesture*, double scale, GtkGesture*);

        double m_initialScale { 0 };
        double m_scale { 0 };
        WebCore::IntPoint m_initialPoint;
        WebCore::IntPoint m_viewPoint;
        RunLoop::Timer<ZoomGesture> m_idle;
    };

    class LongPressGesture final : public Gesture {
    public:
        LongPressGesture(GtkWidget*, GestureControllerClient&);

    private:
        void longPressed(GdkEvent*);

        static void pressed(LongPressGesture*, double x, double y, GtkGesture*);
    };

    std::unique_ptr<GestureControllerClient> m_client;
    DragGesture m_dragGesture;
    SwipeGesture m_swipeGesture;
    ZoomGesture m_zoomGesture;
    LongPressGesture m_longpressGesture;
};

} // namespace WebKit
