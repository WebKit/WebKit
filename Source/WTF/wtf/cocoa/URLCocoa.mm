/*
 * Copyright (C) 2004-2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/URL.h>

#import <wtf/URLParser.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/CString.h>

@interface NSString (WTFNSURLExtras)
- (BOOL)_web_looksLikeIPAddress;
@end

namespace WTF {

URL::URL(NSURL *cocoaURL)
    : URL(bridge_cast(cocoaURL))
{
}

URL::operator NSURL *() const
{
    // Creating a toll-free bridged CFURL because creation with NSURL methods would not preserve the original string.
    // We'll need fidelity when round-tripping via CFURLGetBytes().
    return createCFURL().bridgingAutorelease();
}

RetainPtr<CFURLRef> URL::emptyCFURL()
{
    // We use the toll-free bridge to create an empty value that is distinct from null that no CFURL function can create.
    // FIXME: When we originally wrote this, we thought that creating empty CF URLs was valuable; can we do without it now?
    return bridge_cast(adoptNS([[NSURL alloc] initWithString:@""]));
}

bool URL::hostIsIPAddress(StringView host)
{
    return [host.createNSStringWithoutCopying().get() _web_looksLikeIPAddress];
}

RetainPtr<id> makeNSArrayElement(const URL& vectorElement)
{
    return bridge_cast(vectorElement.createCFURL());
}

std::optional<URL> makeVectorElement(const URL*, id arrayElement)
{
    if (![arrayElement isKindOfClass:NSURL.class])
        return std::nullopt;
    return { { arrayElement } };
}

}
