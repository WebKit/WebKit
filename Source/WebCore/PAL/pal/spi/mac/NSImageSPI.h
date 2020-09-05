/*
 * Copyright (C) 2018 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <wtf/Platform.h>

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#import <AppKit/NSImage_Private.h>

#else

NS_ASSUME_NONNULL_BEGIN

@interface NSImage ()
+ (instancetype)imageWithImageRep:(NSImageRep *)imageRep;
- (instancetype)_imageWithConfiguration:(NSDictionary *)configuration;
- (void)lockFocusWithRect:(NSRect)rect context:(nullable NSGraphicsContext *)context hints:(nullable NSDictionary *)hints flipped:(BOOL)flipped;
@end

@interface NSImage (NSSystemSymbols)
+ (nullable NSImage *)_imageWithSystemSymbolName:(NSString *) symbolName;
@end

WTF_EXTERN_C_BEGIN

extern const NSString *NSImageAlternateCriterionFont;
extern const NSString *NSImageAlternateCriterionSymbolScale;

WTF_EXTERN_C_END

NS_ASSUME_NONNULL_END

#endif

#if HAVE(ALTERNATE_ICONS)

NS_ASSUME_NONNULL_BEGIN

extern const NSImageHintKey NSImageHintSymbolFont;
extern const NSImageHintKey NSImageHintSymbolScale;

NS_ASSUME_NONNULL_END

#endif // HAVE(ALTERNATE_ICONS)

#endif // USE(APPLE_INTERNAL_SDK)
