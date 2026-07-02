/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/ScriptFetchParameters.h>

#include <wtf/HashMap.h>

namespace JSC {

using ModuleMapKey = std::pair<UniquedStringImpl*, ScriptFetchParameters::Type>;

struct ModuleMapHash {
    static unsigned hash(const ModuleMapKey& key)
    {
        unsigned identifierHash = key.first ? key.first->existingSymbolAwareHash() : 0;
        unsigned enumHash(key.second);
        return WTF::pairIntHash(identifierHash, enumHash);
    }

    static bool equal(const ModuleMapKey& a, const ModuleMapKey& b)
    {
        return a.first == b.first && a.second == b.second;
    }

    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template <typename T>
using ModuleMap = UncheckedKeyHashMap<ModuleMapKey, T, ModuleMapHash>;

using ResolutionMapKey = std::pair<UniquedStringImpl*, UniquedStringImpl*>;

struct ResolutionMapHash {
    static unsigned hash(const ResolutionMapKey& key)
    {
        unsigned referrerHash = key.first ? key.first->existingSymbolAwareHash() : 0;
        unsigned specifierHash = key.second ? key.second->existingSymbolAwareHash() : 0;
        return WTF::pairIntHash(referrerHash, specifierHash);
    }

    static bool equal(const ResolutionMapKey& a, const ResolutionMapKey& b)
    {
        return a.first == b.first && a.second == b.second;
    }

    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

template <typename T>
using ResolutionMap = UncheckedKeyHashMap<ResolutionMapKey, T, ResolutionMapHash>;

} // namespace JSC
