/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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

#ifndef ResourceLoadStatistics_h
#define ResourceLoadStatistics_h

#include <wtf/HashCountedSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class KeyedDecoder;
class KeyedEncoder;

struct ResourceLoadStatistics {
    bool checkAndSetAsPrevalentResourceIfNecessary(unsigned originsVisitedSoFar);

    bool hasPrevalentRedirection() const;
    bool hasPrevalentResourceCharacteristics() const;

    void encode(KeyedEncoder&, const String& origin) const;
    bool decode(KeyedDecoder&, const String& origin);

    String toString() const;

    // User interaction
    bool hadUserInteraction { false };
    
    // Top frame stats
    unsigned topFrameHasBeenRedirectedTo { 0 };
    unsigned topFrameHasBeenRedirectedFrom { 0 };
    unsigned topFrameInitialLoadCount { 0 };
    unsigned topFrameHasBeenNavigatedTo { 0 };
    unsigned topFrameHasBeenNavigatedFrom { 0 };
    bool topFrameHasBeenNavigatedToBefore { false };
    
    // Subframe stats
    HashCountedSet<String> subframeUnderTopFrameOrigins;
    unsigned subframeHasBeenRedirectedTo { 0 };
    unsigned subframeHasBeenRedirectedFrom { 0 };
    HashCountedSet<String> subframeUniqueRedirectsTo;
    unsigned subframeSubResourceCount { 0 };
    unsigned subframeHasBeenNavigatedTo { 0 };
    unsigned subframeHasBeenNavigatedFrom { 0 };
    bool subframeHasBeenLoadedBefore { false };
    
    // Subresource stats
    HashCountedSet<String> subresourceUnderTopFrameOrigins;
    unsigned subresourceHasBeenSubresourceCount { 0 };
    double subresourceHasBeenSubresourceCountDividedByTotalNumberOfOriginsVisited { 0.0 };
    unsigned subresourceHasBeenRedirectedFrom { 0 };
    unsigned subresourceHasBeenRedirectedTo { 0 };
    HashCountedSet<String> subresourceUniqueRedirectsTo;
    
    // Prevalent resource stats
    HashCountedSet<String> redirectedToOtherPrevalentResourceOrigins;
    bool isPrevalentResource { false };
};

} // namespace WebCore

#endif // ResourceLoadStatistics_h
