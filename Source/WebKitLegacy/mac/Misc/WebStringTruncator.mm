/*
 * Copyright (C) 2005, 2006, 2014 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebStringTruncator.h"

#import <JavaScriptCore/InitializeThreading.h>
#import <WebCore/FontCascade.h>
#import <WebCore/FontPlatformData.h>
#import <WebCore/StringTruncator.h>
#import <wtf/MainThread.h>
#import <wtf/NeverDestroyed.h>

static WebCore::FontCascade& fontFromNSFont(NSFont *font)
{
    static NeverDestroyed<RetainPtr<NSFont>> currentNSFont;
    static NeverDestroyed<WebCore::FontCascade> currentFont;
    if ([font isEqual:currentNSFont.get().get()])
        return currentFont;
    currentNSFont.get() = font;
    currentFont.get() = WebCore::FontCascade(WebCore::FontPlatformData((__bridge CTFontRef)font, [font pointSize]));
    return currentFont;
}

@implementation WebStringTruncator

+ (void)initialize
{
    JSC::initialize();
    WTF::initializeMainThread();
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth
{
    static NSFont *menuFont = [[NSFont menuFontOfSize:0] retain];

    ASSERT(menuFont);
    if (!menuFont)
        return nil;

    return WebCore::StringTruncator::centerTruncate(string, maxWidth, fontFromNSFont(menuFont));
}

+ (NSString *)centerTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    if (!font)
        return nil;

    return WebCore::StringTruncator::centerTruncate(string, maxWidth, fontFromNSFont(font));
}

+ (NSString *)rightTruncateString:(NSString *)string toWidth:(float)maxWidth withFont:(NSFont *)font
{
    if (!font)
        return nil;

    return WebCore::StringTruncator::rightTruncate(string, maxWidth, fontFromNSFont(font));
}

+ (float)widthOfString:(NSString *)string font:(NSFont *)font
{
    if (!font)
        return 0;

    return WebCore::StringTruncator::width(string, fontFromNSFont(font));
}

@end
