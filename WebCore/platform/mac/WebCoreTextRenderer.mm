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
#import "WebCoreTextRenderer.h"

#import "WebTextRenderer.h"
#import "WebTextRendererFactory.h"

void WebCoreDrawTextAtPoint(const UniChar* buffer, unsigned length, NSPoint point, NSFont* font, NSColor* textColor)
{
    WebCoreFont f;
    WebCoreInitializeFont(&f);
    f.font = font;
    WebTextRenderer* renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:f];

    WebCoreTextRun run;
    WebCoreInitializeTextRun (&run, buffer, length, 0, length);
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    style.textColor = textColor;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    [renderer drawRun:&run style:&style geometry:&geometry];
}

float WebCoreTextFloatWidth(const UniChar* buffer, unsigned length , NSFont* font)
{
    WebCoreFont f;
    WebCoreInitializeFont(&f);
    f.font = font;
    WebTextRenderer* renderer = [[WebTextRendererFactory sharedFactory] rendererWithFont:f];

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, buffer, length, 0, length);
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.applyRunRounding = NO;
    style.applyWordRounding = NO;
    return [renderer floatWidthForRun:&run style:&style];
}

static bool gShouldUseFontSmoothing = true;

void WebCoreSetShouldUseFontSmoothing(bool smooth)
{
    gShouldUseFontSmoothing = smooth;
}

bool WebCoreShouldUseFontSmoothing()
{
    return gShouldUseFontSmoothing;
}

void WebCoreSetAlwaysUseATSU(bool useATSU)
{
    [WebTextRenderer setAlwaysUseATSU: useATSU];
}

NSFont* WebCoreFindFont(NSString* familyName, NSFontTraitMask traits, int size)
{
    return [[WebTextRendererFactory sharedFactory] cachedFontFromFamily:familyName traits:traits size:size];
}
