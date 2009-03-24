/*
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "Color.h"
#import "ColorMac.h"

#import <AppKit/AppKit.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>
#import <wtf/RetainPtr.h>

@interface WebCoreControlTintObserver : NSObject
+ (void)controlTintDidChange;
@end

namespace WebCore {

// NSColor calls don't throw, so no need to block Cocoa exceptions in this file

static RGBA32 oldAquaFocusRingColor = 0xFF7DADD9;
static RGBA32 systemFocusRingColor;
static bool useOldAquaFocusRingColor;


static RGBA32 makeRGBAFromNSColor(NSColor *c)
{
    return makeRGBA((int)(255 * [c redComponent]), (int)(255 * [c greenComponent]), (int)(255 * [c blueComponent]), (int)(255 * [c alphaComponent]));
}

Color colorFromNSColor(NSColor *c)
{
    return Color(makeRGBAFromNSColor(c));
}

NSColor* nsColor(const Color& color)
{
    unsigned c = color.rgb();
    switch (c) {
        case 0: {
            // Need this to avoid returning nil because cachedRGBAValues will default to 0.
            DEFINE_STATIC_LOCAL(RetainPtr<NSColor>, clearColor, ([NSColor colorWithDeviceRed:0.0f green:0.0f blue:0.0f alpha:0.0f]));
            return clearColor.get();
        }
        case Color::black: {
            DEFINE_STATIC_LOCAL(RetainPtr<NSColor>, blackColor, ([NSColor colorWithDeviceRed:0.0f green:0.0f blue:0.0f alpha:1.0f]));
            return blackColor.get();
        }
        case Color::white: {
            DEFINE_STATIC_LOCAL(RetainPtr<NSColor>, whiteColor, ([NSColor colorWithDeviceRed:1.0f green:1.0f blue:1.0f alpha:1.0f]));
            return whiteColor.get();
        }
        default: {
            const int cacheSize = 32;
            static unsigned cachedRGBAValues[cacheSize];
            static RetainPtr<NSColor>* cachedColors = new RetainPtr<NSColor>[cacheSize];

            for (int i = 0; i != cacheSize; ++i)
                if (cachedRGBAValues[i] == c)
                    return cachedColors[i].get();

            NSColor* result = [NSColor colorWithDeviceRed:color.red() / 255.0f
                                                    green:color.green() / 255.0f
                                                     blue:color.blue() / 255.0f
                                                    alpha:color.alpha() /255.0f];

            static int cursor;
            cachedRGBAValues[cursor] = c;
            cachedColors[cursor] = result;
            if (++cursor == cacheSize)
                cursor = 0;
            return result;
        }
    }
}

static CGColorRef CGColorFromNSColor(NSColor* color)
{
    // This needs to always use device colorspace so it can de-calibrate the color for
    // CGColor to possibly recalibrate it.
    NSColor* deviceColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    CGFloat red = [deviceColor redComponent];
    CGFloat green = [deviceColor greenComponent];
    CGFloat blue = [deviceColor blueComponent];
    CGFloat alpha = [deviceColor alphaComponent];
    const CGFloat components[4] = { red, green, blue, alpha };
    static CGColorSpaceRef deviceRGBColorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef cgColor = CGColorCreate(deviceRGBColorSpace, components);
    return cgColor;
}

CGColorRef createCGColor(const Color& c)
{
    // We could directly create a CGColor here, but that would
    // skip any RGB caching the nsColor method does. A direct 
    // creation could be investigated for a possible performance win.
    return CGColorFromNSColor(nsColor(c));
}

Color focusRingColor()
{
    static bool tintIsKnown = false;
    if (!tintIsKnown) {
        [[NSNotificationCenter defaultCenter] addObserver:[WebCoreControlTintObserver class]
                                                 selector:@selector(controlTintDidChange)
                                                     name:NSControlTintDidChangeNotification
                                                   object:NSApp];
        [WebCoreControlTintObserver controlTintDidChange];
        tintIsKnown = true;
    }
    
    if (usesTestModeFocusRingColor())
        return oldAquaFocusRingColor;
    
    return systemFocusRingColor;
}

bool usesTestModeFocusRingColor()
{
    return useOldAquaFocusRingColor;
}

void setUsesTestModeFocusRingColor(bool newValue)
{
    useOldAquaFocusRingColor = newValue;
}

}

@implementation WebCoreControlTintObserver

+ (void)controlTintDidChange
{
    NSColor* color = [[NSColor keyboardFocusIndicatorColor] colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    WebCore::systemFocusRingColor = WebCore::makeRGBAFromNSColor(color);
}

@end
