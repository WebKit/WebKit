/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebScreenInfoFactory.h"

#import <AppKit/AppKit.h>

#include "WebScreenInfo.h"

@interface NSWindow (LionAPI)
- (CGFloat)backingScaleFactor;
@end

@interface NSScreen (LionAPI)
- (CGFloat)backingScaleFactor;
@end

namespace WebKit {

static NSScreen* screenForWindow(NSWindow* window)
{
    NSScreen* screen = [window screen];  // nil if the window is off-screen
    if (screen)
        return screen;

    NSArray* screens = [NSScreen screens];
    if ([screens count] > 0)
        return [screens objectAtIndex:0];  // screen containing the menubar

    return nil;
}

static WebRect convertRect(const NSRect& rect, NSWindow* destination)
{
    CGRect userRect = NSRectToCGRect(rect);
    userRect.origin.y =
        NSMaxY([screenForWindow(destination) frame]) - NSMaxY(rect); // flip
    return WebRect(userRect.origin.x,
                   userRect.origin.y,
                   userRect.size.width,
                   userRect.size.height);
}

static float deviceScaleFactor(NSView* view)
{
    NSWindow* window = [view window];
    if (window)
    {
        if ([window respondsToSelector:@selector(backingScaleFactor)])
            return [window backingScaleFactor];
        return [window userSpaceScaleFactor];
    }

    NSArray* screens = [NSScreen screens];
    if (![screens count])
        return 1;

    NSScreen* screen = [screens objectAtIndex:0];
    if ([screen respondsToSelector:@selector(backingScaleFactor)])
        return [screen backingScaleFactor];
    return [screen userSpaceScaleFactor];
}

WebScreenInfo WebScreenInfoFactory::screenInfo(NSView* view)
{
    NSScreen* screen = [NSScreen deepestScreen];

    WebScreenInfo results;

    float deviceDPI = 160 * deviceScaleFactor(view);
    results.horizontalDPI = deviceDPI;
    results.verticalDPI = deviceDPI;
    results.deviceScaleFactor = static_cast<int>(deviceScaleFactor(view));

    results.depth = NSBitsPerPixelFromDepth([screen depth]);
    results.depthPerComponent = NSBitsPerSampleFromDepth([screen depth]);
    results.isMonochrome =
        [[screen colorSpace] colorSpaceModel] == NSGrayColorSpaceModel;
    results.rect = convertRect([screenForWindow([view window]) frame], [view window]);
    results.availableRect =
        convertRect([screenForWindow([view window]) visibleFrame], [view window]);
    return results;
}

} // namespace WebKit
