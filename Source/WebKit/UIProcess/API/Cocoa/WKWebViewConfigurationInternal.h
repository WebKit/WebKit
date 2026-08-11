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

#import <WebKit/WKWebViewConfigurationPrivate.h>

#if !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

#import "APIPageConfiguration.h"
#import "WKObject.h"
#import <wtf/AlignedStorage.h>
#import <wtf/Ref.h>

#endif // !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

NS_HEADER_AUDIT_BEGIN(nullability, sendability)

#if !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

namespace WebKit {

template<> struct WrapperTraits<API::PageConfiguration> {
    using WrapperClass = WKWebViewConfiguration;
};

}

@interface WKWebViewConfiguration () <WKObject> {
@package
    AlignedStorage<API::PageConfiguration> _pageConfiguration;
}

@end

#endif // !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

@interface WKWebViewConfiguration ()

@property (nonatomic, readonly, nullable) NSString *_applicationNameForDesktopUserAgent;

+ (BOOL)_isValidCustomScheme:(NSString *)urlScheme;

@end

#if !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

#if PLATFORM(IOS_FAMILY)
_WKDragLiftDelay toDragLiftDelay(NSUInteger);
_WKDragLiftDelay toWKDragLiftDelay(WebKit::DragLiftDelay);
WebKit::DragLiftDelay fromWKDragLiftDelay(_WKDragLiftDelay);
#endif

#endif // !__has_feature(modules) || (defined(WK_SUPPORTS_SWIFT_OBJCXX_INTEROP) && WK_SUPPORTS_SWIFT_OBJCXX_INTEROP)

NS_HEADER_AUDIT_END(nullability, sendability)
