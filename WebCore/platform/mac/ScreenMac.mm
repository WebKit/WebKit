/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "Screen.h"

#import "FloatRect.h"
#import "Frame.h"
#import "FrameView.h"
#import "Page.h"

namespace WebCore {

static NSWindow *windowForPage(Page* page)
{
    Frame* frame = page->mainFrame();
    if (!frame)
        return nil;
    
    FrameView* frameView = frame->view();
    if (!frameView)
        return nil;
    
    return [frameView->getView() window];
}

static NSScreen *screenForWindow(NSWindow *window)
{
    NSScreen *s = [window screen]; // nil if the window is off-screen
    if (s)
        return s;
    
    NSArray *screens = [NSScreen screens];
    if ([screens count] > 0)
        return [screens objectAtIndex:0]; // screen containing the menubar
    
    return nil;
}

int Screen::depth() const
{
    return NSBitsPerPixelFromDepth([[NSScreen deepestScreen] depth]);
}

int Screen::depthPerComponent() const
{
    return NSBitsPerSampleFromDepth([[NSScreen deepestScreen] depth]);
}

bool Screen::isMonochrome() const
{
    NSString *colorSpace = NSColorSpaceFromDepth([[NSScreen deepestScreen] depth]);
    return colorSpace == NSCalibratedWhiteColorSpace
        || colorSpace == NSCalibratedBlackColorSpace
        || colorSpace == NSDeviceWhiteColorSpace
        || colorSpace == NSDeviceBlackColorSpace;
}

// These functions scale between screen and page coordinates because JavaScript/DOM operations 
// assume that the screen and the page share the same coordinate system.

FloatRect Screen::rect() const
{
    NSWindow *w = windowForPage(m_page);
    return toUserSpace([screenForWindow(w) frame], w);
}

FloatRect Screen::usableRect() const
{
    NSWindow *w = windowForPage(m_page);
    return toUserSpace([screenForWindow(w) visibleFrame], w);
}

FloatRect toUserSpace(const NSRect& rect, NSWindow *destination)
{
    FloatRect userRect = rect;
    userRect.setY(NSMaxY([[destination screen] frame]) - (userRect.y() + userRect.height())); // flip
    userRect.scale(1 / [destination userSpaceScaleFactor]); // scale down
    return userRect;
}

NSRect toDeviceSpace(const FloatRect& rect, NSWindow *source)
{
    FloatRect deviceRect = rect;
    deviceRect.scale([source userSpaceScaleFactor]); // scale up
    deviceRect.setY(NSMaxY([[source screen] frame]) - (deviceRect.y() + deviceRect.height())); // flip
    return deviceRect;
}

NSPoint flipScreenPoint(const NSPoint& screenPoint, NSScreen *screen)
{
    NSPoint flippedPoint = screenPoint;
    flippedPoint.y = NSMaxY([screen frame]) - flippedPoint.y;
    return flippedPoint;
}

} // namespace WebCore
