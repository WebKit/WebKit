/*
 * Copyright (C) 2003-6 Apple Computer, Inc.  All rights reserved.
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
#import "Color.h"

namespace WebCore {

// NSColor calls don't throw, so no need to block Cocoa exceptions in this file

NSColor *nsColor(const Color &color)
{
    unsigned c = color.rgb();
    switch (c) {
        case 0: {
            // Need this to avoid returning nil because cachedRGBAValues will default to 0.
            static NSColor *clearColor = [[NSColor clearColor] retain];
            return clearColor;
        }
        case Color::black: {
            static NSColor *blackColor = [[NSColor blackColor] retain];
            return blackColor;
        }
        case Color::white: {
            static NSColor *whiteColor = [[NSColor whiteColor] retain];
            return whiteColor;
        }
        default: {
            const int cacheSize = 32;
            static unsigned cachedRGBAValues[cacheSize];
            static NSColor *cachedColors[cacheSize];

            for (int i = 0; i != cacheSize; ++i) {
                if (cachedRGBAValues[i] == c) {
                    return cachedColors[i];
                }
            }

#if COLORMATCH_EVERYTHING
            NSColor *result = [NSColor colorWithCalibratedRed:color.red() / 255.0
                                                        green:color.green() / 255.0
                                                         blue:color.blue() / 255.0
                                                        alpha:color.alpha() /255.0];
#else
            NSColor *result = [NSColor colorWithDeviceRed:color.red() / 255.0
                                                    green:color.green() / 255.0
                                                     blue:color.blue() / 255.0
                                                    alpha:color.alpha() /255.0];
#endif

            static int cursor;
            cachedRGBAValues[cursor] = c;
            [cachedColors[cursor] autorelease];
            cachedColors[cursor] = [result retain];
            if (++cursor == cacheSize) {
                cursor = 0;
            }

            return result;
        }
    }
}

static CGColorRef CGColorFromNSColor(NSColor *color)
{
    // this needs to always use device colorspace so it can de-calibrate the color for
    // CGColor to possibly recalibrate it
    NSColor* deviceColor = [color colorUsingColorSpaceName:NSDeviceRGBColorSpace];
    float red = [deviceColor redComponent];
    float green = [deviceColor greenComponent];
    float blue = [deviceColor blueComponent];
    float alpha = [deviceColor alphaComponent];
    const float components[] = { red, green, blue, alpha };
    
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGColorRef cgColor = CGColorCreate(colorSpace, components);
    CGColorSpaceRelease(colorSpace);
    return cgColor;
}

CGColorRef cgColor(const Color &c)
{
    // We could directly create a CGColor here, but that would
    // skip any rgb caching the nsColor method does.  A direct 
    // creation should be investigated for a possible performance win.
    return CGColorFromNSColor(nsColor(c));
}

}