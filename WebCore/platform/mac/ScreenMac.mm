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
#import "Widget.h"

namespace WebCore {

static NSScreen* screen(Widget* widget)
{
    if (widget)
        if (NSScreen* screen = [[widget->getView() window] screen])
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

FloatRect scaleScreenRectToWidget(FloatRect rect, Widget* widget)
{
    NSSize scaleSize = [widget->getOuterView() convertSize:NSMakeSize(1.0, 1.0) fromView:nil];
    float scaleX = scaleSize.width;
    float scaleY = scaleSize.height;
    
    rect.setWidth(rect.width() * scaleX);
    rect.setX(rect.x() * scaleX);
    
    rect.setHeight(rect.height() * scaleY);
    rect.setY(rect.y() * scaleY);
    
    return rect;
}

FloatRect scaleWidgetRectToScreen(FloatRect rect, Widget* widget)
{
    NSSize scaleSize = [widget->getOuterView() convertSize:NSMakeSize(1.0, 1.0) toView:nil];
    float scaleX = scaleSize.width;
    float scaleY = scaleSize.height;
    
    rect.setWidth(rect.width() * scaleX);
    rect.setX(rect.x() * scaleX);
    
    rect.setHeight(rect.height() * scaleY);
    rect.setY(rect.y() * scaleY);
    
    return rect;
}

int screenDepth(Widget* widget)
{
    return NSBitsPerPixelFromDepth([screen(widget) depth]);
}

int screenDepthPerComponent(Widget* widget)
{
   return NSBitsPerSampleFromDepth([screen(widget) depth]);
}

bool screenIsMonochrome(Widget* widget)
{
    NSScreen* s = screen(widget);
    NSDictionary* dd = [s deviceDescription];
    NSString* colorSpaceName = [dd objectForKey:NSDeviceColorSpaceName];
    return colorSpaceName == NSCalibratedWhiteColorSpace
        || colorSpaceName == NSCalibratedBlackColorSpace
        || colorSpaceName == NSDeviceWhiteColorSpace
        || colorSpaceName == NSDeviceBlackColorSpace;
}

// These methods scale between window and WebView coordinates because JavaScript/DOM operations 
// assume that the WebView and the window share the same coordinate system.

FloatRect screenRect(Widget* widget)
{
    return scaleScreenRectToWidget(flipScreenRect([screen(widget) frame]), widget);
}

FloatRect usableScreenRect(Widget* widget)
{
    return scaleScreenRectToWidget(flipScreenRect([screen(widget) visibleFrame]), widget);
}

}
