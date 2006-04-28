/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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
#import "WebTextRendererFactory.h"

#import "KWQListBox.h"
#import "Page.h"
#import "WebCoreFrameBridge.h"
#import "WebCoreTextRenderer.h"
#import <kxmlcore/Assertions.h>

using namespace WebCore;

void WebCoreInitializeFont(WebCoreFont *font)
{
    font->font = nil;
    font->syntheticBold = NO;
    font->syntheticOblique = NO;
    font->forPrinter = NO;
}

void WebCoreInitializeTextRun(WebCoreTextRun *run, const UniChar *characters, unsigned int length, int from, int to)
{
    run->characters = characters;
    run->length = length;
    run->from = from;
    run->to = to;
}

void WebCoreInitializeEmptyTextStyle(WebCoreTextStyle *style)
{
    style->textColor = nil;
    style->backgroundColor = nil;
    style->letterSpacing = 0;
    style->wordSpacing = 0;
    style->padding = 0;
    style->families = nil;
    style->smallCaps = NO;
    style->rtl = NO;
    style->directionalOverride = NO;
    style->applyRunRounding = YES;
    style->applyWordRounding = YES;
    style->attemptFontSubstitution = YES;
}

void WebCoreInitializeEmptyTextGeometry(WebCoreTextGeometry *geometry)
{
    geometry->point.x = 0;
    geometry->point.y = 0;
    geometry->useFontMetricsForSelectionYAndHeight = YES;
}

@implementation WebCoreTextRendererFactory

static WebCoreTextRendererFactory *sharedFactory;

+ (WebCoreTextRendererFactory *)sharedFactory
{
    if (!sharedFactory)
        [WebTextRendererFactory createSharedFactory];
    return sharedFactory;
}

- init
{
    [super init];
    
    ASSERT(!sharedFactory);
    sharedFactory = [self retain];
    
    return self;
}

- (WebCoreFont)fontWithFamilies:(NSString **)families traits:(NSFontTraitMask)traits size:(float)size
{
    LOG_ERROR("fontWithFamilies needs to be implemented in text renderer factory subclass");
    WebCoreFont font;
    WebCoreInitializeFont(&font);
    return font;
}

- (BOOL)isFontFixedPitch:(WebCoreFont)font
{
    LOG_ERROR("isFontFixedPitch needs to be implemented in text renderer factory subclass");
    return NO;
}

- (id <WebCoreTextRenderer>)rendererWithFont:(WebCoreFont)font
{
    LOG_ERROR("rendererForFont needs to be implemented in text renderer factory subclass");
    return nil;
}

- (void)clearCaches
{
    QListBox::clearCachedTextRenderers();
    Page::setNeedsReapplyStyles();
}

@end
