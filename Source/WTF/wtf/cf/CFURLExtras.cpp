/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include <wtf/cf/CFURLExtras.h>

#include <wtf/URL.h>

namespace WTF {

RetainPtr<CFDataRef> bytesAsCFData(std::span<const uint8_t> bytes)
{
    return adoptCF(CFDataCreate(nullptr, bytes.data(), bytes.size()));
}

RetainPtr<CFDataRef> bytesAsCFData(CFURLRef url)
{
    if (!url)
        return nullptr;
    auto bytesLength = CFURLGetBytes(url, nullptr, 0);
    RELEASE_ASSERT(bytesLength != -1);
    auto buffer = static_cast<uint8_t*>(malloc(bytesLength));
    RELEASE_ASSERT(buffer);
    CFURLGetBytes(url, buffer, bytesLength);
    return adoptCF(CFDataCreateWithBytesNoCopy(nullptr, buffer, bytesLength, kCFAllocatorMalloc));
}

String bytesAsString(CFURLRef url)
{
    if (!url)
        return { };
    auto bytesLength = CFURLGetBytes(url, nullptr, 0);
    RELEASE_ASSERT(bytesLength != -1);
    RELEASE_ASSERT(bytesLength <= static_cast<CFIndex>(String::MaxLength));
    std::span<LChar> buffer;
    auto result = String::createUninitialized(bytesLength, buffer);
    CFURLGetBytes(url, buffer.data(), buffer.size());
    return result;
}

Vector<uint8_t, URLBytesVectorInlineCapacity> bytesAsVector(CFURLRef url)
{
    if (!url)
        return { };

    Vector<uint8_t, URLBytesVectorInlineCapacity> result(URLBytesVectorInlineCapacity);
    auto bytesLength = CFURLGetBytes(url, result.data(), URLBytesVectorInlineCapacity);
    if (bytesLength != -1)
        result.shrink(bytesLength);
    else {
        bytesLength = CFURLGetBytes(url, nullptr, 0);
        RELEASE_ASSERT(bytesLength != -1);
        result.grow(bytesLength);
        CFURLGetBytes(url, result.data(), bytesLength);
    }

    // This may look like it copies the bytes in the vector, but due to the return value optimization it does not.
    return result;
}

bool isSameOrigin(CFURLRef a, const URL& b)
{
    ASSERT(b.protocolIsInHTTPFamily());

    if (b.hasCredentials())
        return protocolHostAndPortAreEqual(a, b);

    auto aBytes = bytesAsVector(a);
    RELEASE_ASSERT(aBytes.size() <= String::MaxLength);

    StringView aString { aBytes.span() };
    StringView bString { b.string() };

    if (!b.hasPath())
        return aString == bString;

    unsigned afterPathSeparator = b.pathStart() + 1;
    return aString.left(afterPathSeparator) == bString.left(afterPathSeparator);
}

}
