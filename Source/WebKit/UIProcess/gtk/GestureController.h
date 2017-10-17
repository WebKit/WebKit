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

#ifndef GestureController_h
#define GestureController_h

#if HAVE(GTK_GESTURES)

#include <WebCore/FloatPoint.h>
#include <wtf/Noncopyable.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GRefPtr.h>

typedef union _GdkEvent GdkEvent;
typedef struct _GdkEventSequence GdkEventSequence;
typedef struct _GtkGesture GtkGesture;

namespace WebKit {
class WebPageProxy;

class GestureController {
    WTF_MAKE_NONCOPYABLE(GestureController);

public:
    GestureController(WebPageProxy&);

    bool isProcessingGestures() const;
    bool handleEvent(const GdkEvent*);

    void reset()
    {
        m_dragGesture.reset();
        m_swipeGesture.reset();
        m_zoomGesture.reset();
    }

private:
    class Gesture {
    public:
        void reset();
        bool isActive() const;
        void handleEvent(const GdkEvent*);

    protected:
        Gesture(GtkGesture*, WebPageProxy&);

        GRefPtr<GtkGesture> m_gesture;
        WebPageProxy& m_page;
    };

    class DragGesture final : public Gesture {
    public:
        DragGesture(WebPageProxy&);

    private:
        // Notify that a drag started, allowing to stop kinetic deceleration.
        void startDrag(const GdkEvent*);
        void handleDrag(const GdkEvent*, double x, double y);
        void handleTap(const GdkEvent*);
        void longPressFired();

        static void begin(DragGesture*, double x, double y, GtkGesture*);
        static void update(DragGesture*, double x, double y, GtkGesture*);
        static void end(DragGesture*, GdkEventSequence*, GtkGesture*);

        WebCore::FloatPoint m_start;
        WebCore::FloatPoint m_offset;
        RunLoop::Timer<DragGesture> m_longPressTimeout;
        GRefPtr<GtkGesture> m_longPress;
        bool m_inDrag;
    };

    class SwipeGesture final : public Gesture {
    public:
        SwipeGesture(WebPageProxy&);

    private:
        void startMomentumScroll(const GdkEvent*, double velocityX, double velocityY);

        static void swipe(SwipeGesture*, double velocityX, double velocityY, GtkGesture*);
    };

    class ZoomGesture final : public Gesture {
    public:
        ZoomGesture(WebPageProxy&);

    private:
        WebCore::IntPoint center() const;
        void handleZoom();

        static void begin(ZoomGesture*, GdkEventSequence*, GtkGesture*);
        static void scaleChanged(ZoomGesture*, double scale, GtkGesture*);

        gdouble m_initialScale;
        gdouble m_scale;
        WebCore::IntPoint m_initialPoint;
        WebCore::IntPoint m_viewPoint;
        RunLoop::Timer<ZoomGesture> m_idle;
    };

    DragGesture m_dragGesture;
    SwipeGesture m_swipeGesture;
    ZoomGesture m_zoomGesture;
};

} // namespace WebKit

#endif // HAVE(GTK_GESTURES)

#endif // GestureController_h
