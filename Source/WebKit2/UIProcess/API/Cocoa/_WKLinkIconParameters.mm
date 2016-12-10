/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#import "_WKLinkIconParametersInternal.h"

#if WK_API_ENABLED

#import <WebCore/LinkIcon.h>

@implementation _WKLinkIconParameters {
    RetainPtr<NSURL> _url;
    WKLinkIconType _iconType;
    RetainPtr<NSString> _mimeType;
    RetainPtr<NSNumber> _size;
}

- (instancetype)_initWithLinkIcon:(const WebCore::LinkIcon&)linkIcon
{
    if (!(self = [super init]))
        return nil;

    _url = adoptNS([(NSURL *)linkIcon.url copy]);
    _mimeType = adoptNS([(NSString *)linkIcon.mimeType copy]);

    if (linkIcon.size)
        _size = adoptNS([[NSNumber alloc] initWithUnsignedInt:linkIcon.size.value()]);

    switch (linkIcon.type) {
    case WebCore::LinkIconType::Favicon:
        _iconType = WKLinkIconTypeFavicon;
        break;
    case WebCore::LinkIconType::TouchIcon:
        _iconType = WKLinkIconTypeTouchIcon;
        break;
    case WebCore::LinkIconType::TouchPrecomposedIcon:
        _iconType = WKLinkIconTypeTouchPrecomposedIcon;
        break;
    }

    return self;
}

- (NSURL *)url
{
    return _url.get();
}

- (NSString *)mimeType
{
    return _mimeType.get();
}

- (NSNumber *)size
{
    return _size.get();
}

- (WKLinkIconType)iconType
{
    return _iconType;
}

@end

#endif // WK_API_ENABLED
