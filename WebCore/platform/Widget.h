/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Collabora Ltd.  All rights reserved.
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

#ifndef Widget_h
#define Widget_h

#include <wtf/Platform.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSView;
@class NSWindow;
#else
class NSView;
class NSWindow;
#endif
typedef NSView* PlatformWidget;
typedef NSWindow* PlatformWindow;
#endif

#if PLATFORM(WIN)
typedef struct HWND__* HWND;
typedef HWND PlatformWidget;
#endif

#if PLATFORM(GTK)
typedef struct _GdkDrawable GdkDrawable;
typedef struct _GtkWidget GtkWidget;
typedef struct _GtkContainer GtkContainer;
typedef GtkWidget* PlatformWidget;
#endif

#if PLATFORM(QT)
#include <qglobal.h>
QT_BEGIN_NAMESPACE
class QWidget;
QT_END_NAMESPACE
typedef QWidget* PlatformWidget;
#endif

#if PLATFORM(WX)
class wxWindow;
typedef wxWindow* PlatformWidget;
#endif

#if !PLATFORM(MAC)
typedef PlatformWidget PlatformWindow;
#endif

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"

namespace WebCore {

class Cursor;
class Event;
class Font;
class GraphicsContext;
class PlatformMouseEvent;
class ScrollView;
class WidgetClient;
class WidgetPrivate;

// The Widget class serves as a base class for three kinds of objects:
// (1) Scrollable areas (ScrollView)
// (2) Scrollbars (Scrollbar)
// (3) Plugins (PluginView)
//
// A widget may or may not be backed by a platform-specific object (e.g., HWND on Windows, NSView on Mac, QWidget on Qt).
//
// Widgets are connected in a hierarchy, with the restriction that plugins and scrollbars are always leaves of the
// tree.  Only ScrollViews can have children (and therefore the Widget class has no concept of children).
//
// The rules right now for which widgets get platform-specific objects are as follows:
// ScrollView - Mac
// Scrollbar - Mac, Gtk
// Plugin - Mac, Windows (windowed only), Qt (windowed only, widget is an HWND on windows), Gtk (windowed only)
//
class Widget {
public:
    Widget();
    Widget(PlatformWidget);
    virtual ~Widget();
    
    PlatformWidget platformWidget() const { return m_widget; }
    void setPlatformWidget(PlatformWidget widget)
    { 
        if (widget != m_widget) {
            releasePlatformWidget();
            m_widget = widget;
            retainPlatformWidget();
        }
    }

    int x() const { return frameGeometry().x(); }
    int y() const { return frameGeometry().y(); }
    int width() const { return frameGeometry().width(); }
    int height() const { return frameGeometry().height(); }
    IntSize size() const { return frameGeometry().size(); }
    IntPoint pos() const { return frameGeometry().location(); }

    virtual void setFrameGeometry(const IntRect&);
    virtual IntRect frameGeometry() const;
    IntRect boundsGeometry() const { return IntRect(0, 0, width(),  height()); }

    void resize(int w, int h) { setFrameGeometry(IntRect(x(), y(), w, h)); }
    void resize(const IntSize& s) { setFrameGeometry(IntRect(pos(), s)); }
    void move(int x, int y) { setFrameGeometry(IntRect(x, y, width(), height())); }
    void move(const IntPoint& p) { setFrameGeometry(IntRect(p, size())); }

    virtual void paint(GraphicsContext*, const IntRect&);
    void invalidate() { invalidateRect(boundsGeometry()); }
    virtual void invalidateRect(const IntRect&);

    virtual void setFocus();

    void setCursor(const Cursor&);
    Cursor cursor();

    virtual void show();
    virtual void hide();
    bool isSelfVisible() const { return m_selfVisible; } // Whether or not we have been explicitly marked as visible or not.
    bool isParentVisible() const { return m_parentVisible; } // Whether or not our parent is visible.
    bool isVisible() const { return m_selfVisible && m_parentVisible; } // Whether or not we are actually visible.
    virtual void setParentVisible(bool visible) { m_parentVisible = visible; }
    void setSelfVisible(bool v) { m_selfVisible = v; }

    void setIsSelected(bool);

    virtual bool isFrameView() const { return false; }
    virtual bool isPluginView() const { return false; }

    virtual void removeFromParent();
    virtual void setParent(ScrollView* view);
    ScrollView* parent() const { return m_parent; }
    ScrollView* root() const;

    // This method is used by plugins on all platforms to obtain a clip rect that includes clips set by WebCore,
    // e.g., in overflow:auto sections.  The clip rects coordinates are in the containing window's coordinate space.
    // This clip includes any clips that the widget itself sets up for its children.
    virtual IntRect windowClipRect() const { return IntRect(); }

    virtual void handleEvent(Event*) { }

    // The containing window is used as the coordinate space for event handling.
    PlatformWindow containingWindow() const;
    void setContainingWindow(PlatformWindow window) { m_containingWindow = window; } // This method is only used by platforms that can't easily get back to their containing window.
    IntRect convertToContainingWindow(const IntRect&) const;
    IntPoint convertToContainingWindow(const IntPoint&) const;
    IntPoint convertFromContainingWindow(const IntPoint&) const;

    virtual void geometryChanged() const {}

#if PLATFORM(MAC)    
    NSView* getOuterView() const;
    
    static void beforeMouseDown(NSView*, Widget*);
    static void afterMouseDown(NSView*, Widget*);

    void addToSuperview(NSView* superview);
    void removeFromSuperview();
    IntPoint convertToScreenCoordinate(NSView*, const IntPoint&);
#endif

private:
    void init(); // Must be called by all Widget constructors to initialize cross-platform data.

    void releasePlatformWidget();
    void retainPlatformWidget();
    
private:
    ScrollView* m_parent;
    PlatformWidget m_widget;
    bool m_selfVisible;
    bool m_parentVisible;
    
    IntRect m_frame; // Not used when a native widget exists.
    PlatformWindow m_containingWindow; // Not used when a native widget exists.

#if PLATFORM(MAC) || PLATFORM(GTK)
    WidgetPrivate* data;
#endif
};

} // namespace WebCore

#endif // Widget_h
