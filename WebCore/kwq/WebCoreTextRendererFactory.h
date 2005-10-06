/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifdef __OBJC__
#import <Cocoa/Cocoa.h>
@protocol WebCoreTextRenderer;
#else
class NSFont;
#endif

typedef struct WebCoreFont {
    NSFont *font;
    unsigned syntheticBold : 1;
    unsigned syntheticOblique : 1;
    unsigned forPrinter : 1;
} WebCoreFont;

#ifdef __cplusplus
extern "C" {
#endif

extern void WebCoreInitializeFont(WebCoreFont *font);

#ifdef __cplusplus
}
#endif

#ifdef __OBJC__

@interface WebCoreTextRendererFactory : NSObject

+ (WebCoreTextRendererFactory *)sharedFactory;

- (WebCoreFont)fontWithFamilies:(NSString **)families traits:(NSFontTraitMask)traits size:(float)size;
- (BOOL)isFontFixedPitch:(WebCoreFont)font;
- (id <WebCoreTextRenderer>)rendererWithFont:(WebCoreFont)font;

- (void)clearCaches;

@end

#endif
