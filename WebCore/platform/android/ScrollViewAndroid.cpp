/*
 * Copyright 2007, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#define LOG_TAG "WebCore"

#include "config.h"
#include "ScrollView.h"

#include "FloatRect.h"
#include "FrameView.h"
#include "IntRect.h"
#include "WebCoreFrameBridge.h"
#include "WebCoreViewBridge.h"
#include "WebViewCore.h"

/*
    This class implementation does NOT actually emulate the Qt ScrollView.
    It does provide an implementation that khtml will use to interact with
    WebKit's WebFrameView documentView and our NSScrollView subclass.

    ScrollView's view is a NSScrollView (or subclass of NSScrollView)
    in most cases. That scrollview is a subview of an
    WebCoreFrameView. The WebCoreFrameView's documentView will also be
    the scroll view's documentView.
    
    The WebCoreFrameView's size is the frame size.  The WebCoreFrameView's documentView
    corresponds to the frame content size.  The scrollview itself is autosized to the
    WebCoreFrameView's size (see Widget::resize).
*/

namespace WebCore {

IntRect ScrollView::platformVisibleContentRect(bool includeScrollbars) const
{
    IntRect rect = platformWidget()->getBounds();
    // This makes subframes draw correctly, since subframes cannot scroll.
    if (parent())
        return IntRect(0, 0, rect.width(), rect.height());
    return rect;
}

IntSize ScrollView::platformContentsSize() const
{
    return m_contentsSize;
}

void ScrollView::platformSetScrollPosition(const WebCore::IntPoint& pt)
{
    if (parent()) // don't attempt to scroll subframes; they're fully visible
        return;
    android::WebViewCore::getWebViewCore(this)->scrollTo(pt.x(), pt.y());
}

void ScrollView::platformScrollbarModes(ScrollbarMode& h, ScrollbarMode& v) const
{
    h = v = ScrollbarAlwaysOff;
}

bool ScrollView::platformProhibitsScrolling()
{
    if (!isFrameView())
        return false;
    FrameView* view = static_cast<FrameView*>(this);
    // We want to ignore requests to scroll that were not initiated by the user.  An
    // example of this is when text is inserted into a textfield/area, which results in
    // a scroll.  We ignore this because we now how to do this ourselves in the UI thread.
    // An example of it being initiated by the user is if the user clicks an anchor
    // element which simply scrolls the page.
    return !android::WebFrame::getWebFrame(view->frame())->userInitiatedClick();
}

void ScrollView::platformRepaintContentRectangle(const IntRect &rect, bool now)
{
    android::WebViewCore::getWebViewCore(this)->contentInvalidate(rect);
}

#ifdef ANDROID_CAPTURE_OFFSCREEN_PAINTS
void ScrollView::platformOffscreenContentRectangle(const IntRect& rect)
{
    android::WebViewCore::getWebViewCore(this)->offInvalidate(rect);
}
#endif

} // namespace WebCore
