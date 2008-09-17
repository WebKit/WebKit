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
#else
class NSView;
#endif
typedef NSView* PlatformWidget;
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

namespace WebCore {

class Cursor;
class Event;
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
    Widget();
    virtual ~Widget();

    void init(); // Must be called by all Widget constructors to initialize cross-platform data.

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

    virtual void setFocus();

    void setCursor(const Cursor&);
    Cursor cursor();

    virtual void show();
    virtual void hide();
    bool isSelfVisible() const { return m_selfVisible; } // Whether or not we have been explicitly marked as visible or not.
    bool isParentVisible() const { return m_parentVisible; } // Whether or not our parent is visible.
    bool isVisible() const { return m_selfVisible && m_parentVisible; } // Whether or not we are actually visible.
    virtual void setParentVisible(bool visible) { m_parentVisible = visible; }

    void setIsSelected(bool);

    void setClient(WidgetClient*);
    WidgetClient* client() const;

    virtual bool isFrameView() const;
    virtual bool isPluginView() const { return false; }

    virtual void removeFromParent();
    virtual void setParent(ScrollView* view);
    ScrollView* parent() const { return m_parent; }

    // This method is used by plugins on all platforms to obtain a clip rect that includes clips set by WebCore,
    // e.g., in overflow:auto sections.  The clip rects coordinates are in the containing window's coordinate space.
    // This clip includes any clips that the widget itself sets up for its children.
    virtual IntRect windowClipRect() const;

    virtual void handleEvent(Event*) { }

    void setContainingWindow(PlatformWidget);
    PlatformWidget containingWindow() const;

#if PLATFORM(WIN)
    virtual void geometryChanged() const {}
    
    IntRect convertToContainingWindow(const IntRect&) const;
    IntPoint convertToContainingWindow(const IntPoint&) const;
    IntPoint convertFromContainingWindow(const IntPoint&) const;

    virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
    virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;

    bool suppressInvalidation() const;
    void setSuppressInvalidation(bool);
#endif

#if PLATFORM(GTK)
    virtual void geometryChanged() const;

    IntRect convertToContainingWindow(const IntRect&) const;
    IntPoint convertToContainingWindow(const IntPoint&) const;
    IntPoint convertFromContainingWindow(const IntPoint&) const;

    virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
    virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;

    bool suppressInvalidation() const;
    void setSuppressInvalidation(bool);

    GtkWidget* gtkWidget() const;
protected:
    void setGtkWidget(GtkWidget*);
#endif

#if PLATFORM(QT)
    void setNativeWidget(QWidget *widget);
    QWidget* nativeWidget() const;

    void setIsNPAPIPlugin(bool);
    bool isNPAPIPlugin() const;

    virtual void geometryChanged() const;
    ScrollView* topLevel() const;

    IntRect convertToContainingWindow(const IntRect&) const;
    IntPoint convertToContainingWindow(const IntPoint&) const;
    IntPoint convertFromContainingWindow(const IntPoint&) const;

    virtual IntPoint convertChildToSelf(const Widget*, const IntPoint&) const;
    virtual IntPoint convertSelfToChild(const Widget*, const IntPoint&) const;

    bool suppressInvalidation() const;
    void setSuppressInvalidation(bool);
#endif

#if PLATFORM(MAC)
    Widget(NSView*);
    
    NSView* getView() const;
    NSView* getOuterView() const;
    void setView(NSView*);
    
    static void beforeMouseDown(NSView*, Widget*);
    static void afterMouseDown(NSView*, Widget*);

    void addToSuperview(NSView* superview);
    void removeFromSuperview();
    IntPoint convertToScreenCoordinate(NSView*, const IntPoint&);
    
    // FIXME: Will need to implement these methods once we actually start doing viewless widgets.
    IntPoint convertFromContainingWindow(const IntPoint&) const;
    IntRect convertToContainingWindow(const IntRect&) const;
#endif

#if PLATFORM(WX)
    Widget(wxWindow*);
    wxWindow* nativeWindow() const;
    virtual void setNativeWindow(wxWindow*);
#endif

private:
    ScrollView* m_parent;
    bool m_selfVisible;
    bool m_parentVisible;
    WidgetPrivate* data;
};

} // namespace WebCore

#endif // Widget_h
