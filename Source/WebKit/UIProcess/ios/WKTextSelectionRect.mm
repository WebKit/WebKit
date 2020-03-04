/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "WKTextSelectionRect.h"

#if PLATFORM(IOS_FAMILY)

// FIXME: Layering violation; WebKit2 should not include WebKit headers.
#import <WebKit/WebSelectionRect.h>

@implementation WKTextSelectionRect

- (id)initWithWebRect:(WebSelectionRect *)wRect
{
    self = [super init];
    if (self)
        self.webRect = wRect;

    return self;
}

- (void)dealloc
{
    self.webRect = nil;
    [super dealloc];
}

// FIXME: We are using this implementation for now that uses WebSelectionRect, but
// we want to provide our own based on WebCore::SelectionRect.
+ (NSArray *)textSelectionRectsWithWebRects:(NSArray *)webRects
{
    NSMutableArray *array = [NSMutableArray arrayWithCapacity:webRects.count];
    for (WebSelectionRect *webRect in webRects) {
        RetainPtr<WKTextSelectionRect> rect = adoptNS([[WKTextSelectionRect alloc] initWithWebRect:webRect]);
        [array addObject:rect.get()];
    }
    return array;
}

- (CGRect)rect
{
    return _webRect.rect;
}

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
- (UITextWritingDirection)writingDirection
{
    return (UITextWritingDirection)_webRect.writingDirection;
}
ALLOW_DEPRECATED_DECLARATIONS_END

- (UITextRange *)range
{
    return nil;
}

- (BOOL)containsStart
{
    return _webRect.containsStart;
}

- (BOOL)containsEnd
{
    return _webRect.containsEnd;
}

- (BOOL)isVertical
{
    return !_webRect.isHorizontal;
}

@end

#endif // PLATFORM(IOS_FAMILY)
