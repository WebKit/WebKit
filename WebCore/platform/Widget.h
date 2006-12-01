/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef WEBCORE_PLATFORM_WIDGET_H_
#define WEBCORE_PLATFORM_WIDGET_H_

#include <wtf/Platform.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSView;
#else
class NSView;
#endif
#endif

#if PLATFORM(WIN)
typedef struct HWND__* HWND;
#endif

#if PLATFORM(GDK)
typedef struct _GdkDrawable GdkDrawable;
#endif

#if PLATFORM(QT)
class QWidget;
#endif

namespace WebCore {

    class Cursor;
    class Font;
    class GraphicsContext;
    class IntPoint;
    class IntRect;
    class IntSize;
    class PlatformMouseEvent;
    class ScrollView;
    class WidgetClient;
    class WidgetPrivate;

    class Widget {
    public:

        enum FocusPolicy {
            NoFocus = 0,
            TabFocus = 0x1,
            ClickFocus = 0x2,
            StrongFocus = 0x3,
            WheelFocus = 0x7
        };

        Widget();
        virtual ~Widget();

        virtual IntSize sizeHint() const;
        
        virtual void setEnabled(bool);
        virtual bool isEnabled() const;

        int x() const;
        int y() const;
        int width() const;
        int height() const;
        IntSize size() const;
        void resize(int, int);
        void resize(const IntSize&);
        IntPoint pos() const;
        void move(int, int);
        void move(const IntPoint&);

        virtual void paint(GraphicsContext*, const IntRect&);
        virtual void invalidate();
        virtual void invalidateRect(const IntRect&);

        virtual void setFrameGeometry(const IntRect&);
        virtual IntRect frameGeometry() const;

        virtual int baselinePosition(int height) const; // relative to the top of the widget

        bool hasFocus() const;
        virtual void setFocus();
        virtual void clearFocus();
        virtual bool checksDescendantsForFocus() const;

        virtual FocusPolicy focusPolicy() const;

        const Font& font() const;
        virtual void setFont(const Font&);

        void setCursor(const Cursor&);
        Cursor cursor();

        virtual void show();
        virtual void hide();

        virtual void populate() {}

        GraphicsContext* lockDrawingFocus();
        void unlockDrawingFocus(GraphicsContext*);
        void enableFlushDrawing();
        void disableFlushDrawing();

        void setIsSelected(bool);

        void setClient(WidgetClient*);
        WidgetClient* client() const;

        virtual bool isFrameView() const;

        virtual void removeFromParent();

        // This method is used by plugins on all platforms to obtain a clip rect that includes clips set by WebCore,
        // e.g., in overflow:auto sections.  The clip rects coordinates are in the containing window's coordinate space.
        // This clip includes any clips that the widget itself sets up for its children.
        virtual IntRect windowClipRect() const;

#if PLATFORM(WIN)
        void setContainingWindow(HWND);
        HWND containingWindow() const;

        void setParent(ScrollView*);
        ScrollView* parent() const;

        virtual void geometryChanged() const {};

        bool capturingMouse() const;
        void setCapturingMouse(bool);
        Widget* capturingTarget();
        Widget* capturingChild();
        void setCapturingChild(Widget*);
        
        void setFocused(bool);
        Widget* focusedTarget();
        Widget* focusedChild();
        void setFocusedChild(Widget*);
        void clearFocusFromDescendants();

        IntRect convertToContainingWindow(const IntRect&) const;
        IntPoint convertToContainingWindow(const IntPoint&) const;
        IntPoint convertFromContainingWindow(const IntPoint&) const;

        virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
        virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;

        bool suppressInvalidation() const;
        void setSuppressInvalidation(bool);
#endif

#if PLATFORM(GDK)
        Widget(GdkDrawable* drawable);
        virtual void setDrawable(GdkDrawable* drawable);
        GdkDrawable* drawable() const;
#endif

#if PLATFORM(QT)
        QWidget* parentWidget() const;
        virtual void setParentWidget(QWidget*);

        QWidget* qwidget();
        void setQWidget(QWidget*);
#endif

#if PLATFORM(MAC)
        Widget(NSView* view);

        NSView* getView() const;
        NSView* getOuterView() const;
        void setView(NSView*);
        
        void sendConsumedMouseUp();
        
        static void beforeMouseDown(NSView*);
        static void afterMouseDown(NSView*);

        void addToSuperview(NSView* superview);
        void removeFromSuperview();

        static void setDeferFirstResponderChanges(bool);
#endif

    private:
        WidgetPrivate* data;
    };

} // namespace WebCore

#endif // WEBCORE_PLATFORM_WIDGET_H_
