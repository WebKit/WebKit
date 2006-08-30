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
#import "Page.h"
#import "WebCorePageBridge.h"

namespace WebCore {

static NSScreen* screen(const Page* page)
{
    if (page)
        if (NSScreen* screen = [[[page->bridge() outerView] window] screen])
            return screen;
    return [NSScreen mainScreen];
}

NSRect flipScreenRect(NSRect rect)
{
    rect.origin.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - NSMaxY(rect);
    return rect;
}

NSPoint flipScreenPoint(NSPoint point)
{
    point.y = NSMaxY([[[NSScreen screens] objectAtIndex:0] frame]) - point.y;
    return point;
}

FloatRect scaleScreenRectToPageCoordinates(const FloatRect& rect, const Page* page)
{
    NSSize scaleSize = [[page->bridge() outerView] convertSize:NSMakeSize(1.0, 1.0) fromView:nil];
    float scaleX = scaleSize.width;
    float scaleY = scaleSize.height;

    return FloatRect(rect.x() * scaleX, rect.y() * scaleY, rect.width() * scaleX, rect.height() * scaleY);
}

FloatRect scalePageRectToScreenCoordinates(const FloatRect& rect, const Page* page)
{
    NSSize scaleSize = [[page->bridge() outerView] convertSize:NSMakeSize(1.0, 1.0) toView:nil];
    float scaleX = scaleSize.width;
    float scaleY = scaleSize.height;

    return FloatRect(rect.x() * scaleX, rect.y() * scaleY, rect.width() * scaleX, rect.height() * scaleY);
}

int screenDepth(const Page* page)
{
    return NSBitsPerPixelFromDepth([screen(page) depth]);
}

int screenDepthPerComponent(const Page* page)
{
    return NSBitsPerSampleFromDepth([screen(page) depth]);
}

bool screenIsMonochrome(const Page* page)
{
    NSScreen* s = screen(page);
    NSDictionary* dd = [s deviceDescription];
    NSString* colorSpaceName = [dd objectForKey:NSDeviceColorSpaceName];
    return colorSpaceName == NSCalibratedWhiteColorSpace
        || colorSpaceName == NSCalibratedBlackColorSpace
        || colorSpaceName == NSDeviceWhiteColorSpace
        || colorSpaceName == NSDeviceBlackColorSpace;
}

float scaleFactor(const Page* page)
{
    if (page)
        return [[[page->bridge() outerView] window] userSpaceScaleFactor];
    
    return 1.0f;
}

// These methods scale between window and WebView coordinates because JavaScript/DOM operations 
// assume that the WebView and the window share the same coordinate system.

FloatRect screenRect(const Page* page)
{
    return scaleScreenRectToPageCoordinates(flipScreenRect([screen(page) frame]), page);
}

FloatRect usableScreenRect(const Page* page)
{
    return scaleScreenRectToPageCoordinates(flipScreenRect([screen(page) visibleFrame]), page);
}

} // namespace WebCore
