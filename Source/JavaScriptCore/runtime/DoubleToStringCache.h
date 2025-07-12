/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "SlotVisitorMacros.h"
#include <array>
#include <wtf/HashFunctions.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class JSString;
class VM;

class DoubleToStringCache {
    WTF_MAKE_TZONE_ALLOCATED(DoubleToStringCache);
public:
    static const size_t primaryCacheSize = 4096;
    static const size_t secondaryCacheSize = 1024;

    template<typename T>
    struct CacheEntryWithJSString {
        T key { };
        String value { };
        JSString* jsString { nullptr };
    };

    const String& add(VM&, double);
    JSString* addJSString(VM&, double);

    DoubleToStringCache() = default;

    DECLARE_VISIT_AGGREGATE;
    void finalizeUnconditionally(VM&, CollectionScope);

private:
    CacheEntryWithJSString<uint64_t>& acquire(uint64_t);

    static unsigned primaryHash(uint64_t);
    static unsigned secondaryHash(uint64_t);

    std::array<CacheEntryWithJSString<uint64_t>, primaryCacheSize> m_primaryCache { };
    std::array<CacheEntryWithJSString<uint64_t>, secondaryCacheSize> m_secondaryCache { };
};

} // namespace JSC
