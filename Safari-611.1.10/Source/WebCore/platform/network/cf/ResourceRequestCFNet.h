/*
 * Copyright (C) 2006 Apple Inc.  All rights reserved.
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

#pragma once

#include "HTTPHeaderMap.h"
#include "ResourceLoadPriority.h"
#include "ResourceRequestBase.h"

#if USE(CFURLCONNECTION)
#include <pal/spi/win/CFNetworkSPIWin.h>
#elif PLATFORM(COCOA)
#include <pal/spi/cf/CFNetworkSPI.h>
#endif

namespace WebCore {

class ResourceRequest;

#if USE(CFURLCONNECTION)
void getResourceRequest(ResourceRequest&, CFURLRequestRef);
CFURLRequestRef cfURLRequest(const ResourceRequest&);
#endif

inline ResourceLoadPriority toResourceLoadPriority(CFURLRequestPriority priority)
{
    // FIXME: switch VeryLow back to 0 priority when CFNetwork fixes <rdar://problem/56621205>
    switch (priority) {
    case -1:
        return ResourceLoadPriority::VeryLow;
    case 0:
        return ResourceLoadPriority::Low;
    case 1:
        return ResourceLoadPriority::Medium;
    case 2:
        return ResourceLoadPriority::High;
    case 3:
        return ResourceLoadPriority::VeryHigh;
    default:
        ASSERT_NOT_REACHED();
        return ResourceLoadPriority::Lowest;
    }
}

inline CFURLRequestPriority toPlatformRequestPriority(ResourceLoadPriority priority)
{
    // FIXME: switch VeryLow back to 0 priority when CFNetwork fixes <rdar://problem/56621205>
    switch (priority) {
    case ResourceLoadPriority::VeryLow:
        return -1;
    case ResourceLoadPriority::Low:
        return 0;
    case ResourceLoadPriority::Medium:
        return 1;
    case ResourceLoadPriority::High:
        return 2;
    case ResourceLoadPriority::VeryHigh:
        return 3;
    }

    ASSERT_NOT_REACHED();
    return 0;
}

inline RetainPtr<CFStringRef> httpHeaderValueUsingSuitableEncoding(HTTPHeaderMap::const_iterator::KeyValue header)
{
    if (header.keyAsHTTPHeaderName && *header.keyAsHTTPHeaderName == HTTPHeaderName::LastEventID && !header.value.isAllASCII()) {
        auto utf8Value = header.value.utf8();
        // Constructing a string with the UTF-8 bytes but claiming that it’s Latin-1 is the way to get CFNetwork to put those UTF-8 bytes on the wire.
        return adoptCF(CFStringCreateWithBytes(nullptr, reinterpret_cast<const UInt8*>(utf8Value.data()), utf8Value.length(), kCFStringEncodingISOLatin1, false));
    }
    return header.value.createCFString();
}

} // namespace WebCore
