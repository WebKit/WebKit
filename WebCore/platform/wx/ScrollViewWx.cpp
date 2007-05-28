/*
 * Copyright (C) 2007 Kevin Ollivier <kevino@theolliviers.com>
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

#include "config.h"
#include "ScrollView.h"

#include <algorithm>
#include "FloatRect.h"
#include "IntRect.h"

#include <stdio.h>
#include "NotImplemented.h"

#include <wx/defs.h>
#include <wx/scrolwin.h>

using namespace std;

namespace WebCore {

class ScrollView::ScrollViewPrivate {
public:
    ScrollViewPrivate()
        : hasStaticBackground(false)
        , suppressScrollbars(false)
        , vScrollbarMode(ScrollbarAuto)
        , hScrollbarMode(ScrollbarAuto)
    {
    }
    IntSize scrollOffset;
    IntSize contentsSize;
    bool hasStaticBackground;
    bool suppressScrollbars;
    ScrollbarMode vScrollbarMode;
    ScrollbarMode hScrollbarMode;
};

ScrollView::ScrollView()
    : m_data(new ScrollViewPrivate())
{
}

ScrollView::~ScrollView()
{
    delete m_data;
}

void ScrollView::updateContents(const IntRect& updateRect, bool now)
{
    // we need to convert coordinates to scrolled position
    wxRect contentsRect = updateRect;
    contentsRect.Offset(-contentsX(), -contentsY());
    wxScrolledWindow* win = nativeWindow();
    if (win){
        win->RefreshRect(contentsRect, true);
        if (now)
            win->Update();
    }
}

void ScrollView::update()
{
    wxScrolledWindow* win = nativeWindow();
    if (win){
        win->Update();
    }
}

int ScrollView::visibleWidth() const
{
    int width = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
        win->GetClientSize(&width, NULL);
    
    return width;
}

int ScrollView::visibleHeight() const
{
    int height = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
        win->GetClientSize(NULL, &height);
    
    return height;
}

FloatRect ScrollView::visibleContentRect() const
{
    return FloatRect(contentsX(),contentsY(),visibleWidth(),visibleHeight());
}

void ScrollView::setContentsPos(int newX, int newY)
{
    int dx = newX - contentsX();
    int dy = newY - contentsY();
    scrollBy(dx, dy);
}

void ScrollView::resizeContents(int w,int h)
{
    wxScrolledWindow* win = nativeWindow();
    if (win)
    {
        win->SetVirtualSize(w, h);
        win->SetScrollRate(20, 20);
    }
}

int ScrollView::contentsX() const
{
    int x = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
    {
        int sUnitX = 1;
        win->GetViewStart(&x, NULL);
        win->GetScrollPixelsPerUnit(&sUnitX, NULL);
        x *= sUnitX;
    }
    return x;
}

int ScrollView::contentsY() const
{
    int y = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
    {
        int sUnitY = 1;
        win->GetViewStart(NULL, &y);
        win->GetScrollPixelsPerUnit(&sUnitY, NULL);
        y *= sUnitY;
    }
    return y;
}

int ScrollView::contentsWidth() const
{
    int width = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
        win->GetVirtualSize(&width, NULL);
    return width;
}

int ScrollView::contentsHeight() const
{
    int height = 0;
    wxScrolledWindow* win = nativeWindow();
    if (win)
        win->GetVirtualSize(NULL, &height);
    return height;
}

FloatRect ScrollView::visibleContentRectConsideringExternalScrollers() const
{
    // FIXME: clip this rect if parent scroll views cut off the visible
    // area.
    return visibleContentRect();
}

IntSize ScrollView::scrollOffset() const
{
    return IntSize(contentsX(), contentsY());
}

IntSize ScrollView::maximumScroll() const
{
    notImplemented();
    return IntSize(0, 0);
}

void ScrollView::scrollBy(int dx, int dy)
{
    wxScrolledWindow* win = nativeWindow();
    if (win)
        win->Scroll(dx, dy);
}

WebCore::ScrollbarMode ScrollView::hScrollbarMode() const
{
    return m_data->hScrollbarMode;
}

WebCore::ScrollbarMode ScrollView::vScrollbarMode() const
{
    return m_data->vScrollbarMode;
}

void ScrollView::setHScrollbarMode(ScrollbarMode newMode)
{
    notImplemented();
}

void ScrollView::setVScrollbarMode(ScrollbarMode newMode)
{
    notImplemented();
}

void ScrollView::updateScrollBars()
{
    wxScrolledWindow* win = nativeWindow();
    if (win)
    {
        //if (m_data->vScrollbarMode != ScrollBarMode::ScrollbarAlwaysOff)
    }
}

void ScrollView::setStaticBackground(bool flag)
{
    m_data->hasStaticBackground = flag;
}

void ScrollView::suppressScrollbars(bool suppressed, bool repaintOnSuppress)
{
    m_data->suppressScrollbars = suppressed;
}

void ScrollView::setScrollbarsMode(ScrollbarMode newMode)
{
    m_data->hScrollbarMode = m_data->vScrollbarMode = newMode;
}

IntPoint ScrollView::contentsToWindow(const IntPoint& point) const
{
    return point - scrollOffset();
}

IntPoint ScrollView::windowToContents(const IntPoint& point) const
{
    return point + scrollOffset();
}

bool ScrollView::inWindow() const 
{
    // NB: This is called from RenderObject::willRenderImage
    // and really seems to be more of a "is the window in a valid state" test,
    // despite the API name.
    return nativeWindow() != NULL; 
}

void ScrollView::wheelEvent(PlatformWheelEvent&)
{ 
    // do nothing, 
    // FIXME: not sure if any ports need to handle this, actually...
}

// used for subframes support
void ScrollView::addChild(Widget*) 
{
    // NB: In all cases I'm aware of,
    // by the time this is called the ScrollView is already a child
    // of its parent Widget by wx port APIs, so I don't think
    // we need to do anything here.
}

void ScrollView::removeChild(Widget* widget) 
{ 
    if (nativeWindow() && widget->nativeWindow())
    {
        nativeWindow()->RemoveChild(widget->nativeWindow());
        // FIXME: Is this the right place to do deletion? I see 
        // detachFromParent2/3/4, initiated by FrameLoader::detachFromParent,
        // but I'm not sure if it's better to handle there or not.
        widget->nativeWindow()->Destroy();
    }
}

void ScrollView::scrollRectIntoViewRecursively(const IntRect& rect) 
{ 
    // NB: This is used by RenderLayer::scrollRectToVisible and the idea
    // is that if this position is not visible due to parent scroll views,
    // those parents are scrolled as well to make it visible.
    
    // For now, just scroll the current window.
    setContentsPos(rect.x(), rect.y());
}


PlatformScrollbar* ScrollView::scrollbarUnderMouse(const PlatformMouseEvent& mouseEvent) 
{
    // AFAICT this is only used for platforms that provide 
    // feedback when mouse is hovered over.
    return 0; 
}


}
