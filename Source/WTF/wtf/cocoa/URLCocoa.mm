/*
 * Copyright (C) 2004, 2008 Apple Inc. All rights reserved.
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

#import <wtf/ObjCRuntimeExtras.h>
#import <wtf/URLParser.h>
#import <wtf/cf/CFURLExtras.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/text/CString.h>

@interface NSString (WTFNSURLExtras)
- (BOOL)_web_looksLikeIPAddress;
@end

namespace WTF {

URL::URL(NSURL *url)
{
    if (!url) {
        invalidate();
        return;
    }

    // FIXME: Why is it OK to ignore base URL here?
    CString urlBytes;
    WTF::getURLBytes((__bridge CFURLRef)url, urlBytes);
    URLParser parser(urlBytes.data());
    *this = parser.result();
}

URL::operator NSURL *() const
{
    // Creating a toll-free bridged CFURL because creation with NSURL methods would not preserve the original string.
    // We'll need fidelity when round-tripping via CFURLGetBytes().
    return createCFURL().bridgingAutorelease();
}

RetainPtr<CFURLRef> URL::createCFURL() const
{
    if (isNull())
        return nullptr;

    if (isEmpty()) {
        // We use the toll-free bridge between NSURL and CFURL to create a CFURLRef supporting both empty and null values.
        return (__bridge CFURLRef)adoptNS([[NSURL alloc] initWithString:@""]).get();
    }

    RetainPtr<CFURLRef> cfURL;

    // Fast path if the input data is 8-bit to avoid copying into a temporary buffer.
    if (LIKELY(m_string.is8Bit()))
        cfURL = WTF::createCFURLFromBuffer(reinterpret_cast<const char*>(m_string.characters8()), m_string.length());
    else {
        // Slower path.
        WTF::URLCharBuffer buffer;
        copyToBuffer(buffer);
        cfURL = WTF::createCFURLFromBuffer(buffer.data(), buffer.size());
    }

    if (protocolIsInHTTPFamily() && !WTF::isCFURLSameOrigin(cfURL.get(), *this))
        return nullptr;

    return cfURL;
}

bool URL::hostIsIPAddress(StringView host)
{
    return [host.createNSStringWithoutCopying().get() _web_looksLikeIPAddress];
}

}
