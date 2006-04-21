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

namespace WebCore {
    class Font;
}

#if __APPLE__
#ifdef __OBJC__
@class NSView;
#else
class NSView;
#endif
#endif

#if WIN32
typedef struct HWND__ *HWND;
typedef struct HINSTANCE__ *HINSTANCE;
#endif

namespace WebCore {

    class Cursor;
    class GraphicsContext;
    class IntPoint;
    class IntRect;
    class IntSize;
    class WidgetClient;
    class WidgetPrivate;

    enum HorizontalAlignment { AlignLeft, AlignRight, AlignHCenter };

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
        
        void setActiveWindow();

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

        virtual IntRect frameGeometry() const;
        virtual void setFrameGeometry(const IntRect&);

        virtual int baselinePosition(int height) const; // relative to the top of the widget

        virtual IntPoint mapFromGlobal(const IntPoint&) const;

        bool hasFocus() const;
        void setFocus();
        void clearFocus();
        virtual bool checksDescendantsForFocus() const;

        virtual FocusPolicy focusPolicy() const;

        const Font& font() const;
        virtual void setFont(const Font&);

        void setCursor(const Cursor&);
        Cursor cursor();

        void show();
        void hide();

        virtual void populate() {}

        GraphicsContext* lockDrawingFocus();
        void unlockDrawingFocus(GraphicsContext*);
        void enableFlushDrawing();
        void disableFlushDrawing();

        void setIsSelected(bool);

        void setClient(WidgetClient*);
        WidgetClient* client() const;

        virtual bool isFrameView() const;

#if WIN32
        Widget(HWND);
        HWND windowHandle() const;
        void setWindowHandle(HWND);
        // The global DLL or application instance used for all WebCore windows.
        static HINSTANCE instanceHandle;
#endif

#if __APPLE__
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

}

using WebCore::IntPoint; // FIXME: remove when we move everything into the WebCore namespace
using WebCore::IntRect; // FIXME: remove when we move everything into the WebCore namespace
using WebCore::IntSize; // FIXME: remove when we move everything into the WebCore namespace
using WebCore::Widget; // FIXME: remove when we move everything into the WebCore namespace

#endif
