/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "_WKActivatedElementInfoInternal.h"

#if WK_API_ENABLED

#import "ShareableBitmap.h"
#import <wtf/RetainPtr.h>

#if PLATFORM(IOS)
#import <UIKit/UIImage.h>
#endif

#if PLATFORM(MAC)
#import <AppKit/NSImage.h>
#endif

@implementation _WKActivatedElementInfo  {
    RetainPtr<NSURL> _URL;
    RetainPtr<NSString> _title;
    CGPoint _interactionLocation;
    RefPtr<WebKit::ShareableBitmap> _image;
#if PLATFORM(IOS)
    RetainPtr<UIImage> _uiImage;
#endif
#if PLATFORM(MAC)
    RetainPtr<NSImage> _nsImage;
#endif
}

- (instancetype)_initWithType:(_WKActivatedElementType)type URL:(NSURL *)url location:(CGPoint)location title:(NSString *)title rect:(CGRect)rect image:(WebKit::ShareableBitmap*)image
{
    if (!(self = [super init]))
        return nil;

    _URL = adoptNS([url copy]);
    _interactionLocation = location;
    _title = adoptNS([title copy]);
    _boundingRect = rect;
    _type = type;
    _image = image;

    return self;
}

- (NSURL *)URL
{
    return _URL.get();
}

- (NSString *)title
{
    return _title.get();
}

- (CGPoint)_interactionLocation
{
    return _interactionLocation;
}

#if PLATFORM(IOS)
- (UIImage *)image
{
    if (_uiImage)
        return [[_uiImage copy] autorelease];

    if (!_image)
        return nil;

    _uiImage = adoptNS([[UIImage alloc] initWithCGImage:_image->makeCGImageCopy().get()]);
    _image = nullptr;

    return [[_uiImage copy] autorelease];
}
#endif

#if PLATFORM(MAC)
- (NSImage *)image
{
    if (_nsImage)
        return [[_nsImage copy] autorelease];

    if (!_image)
        return nil;

    _nsImage = adoptNS([[NSImage alloc] initWithCGImage:_image->makeCGImageCopy().get() size:NSSizeFromCGSize(_boundingRect.size)]);
    _image = nullptr;

    return [[_nsImage copy] autorelease];
}
#endif

@end

#endif // WK_API_ENABLED
