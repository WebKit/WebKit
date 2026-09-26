/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#pragma once

#include <JavaScriptCore/CorpsePlatform.h>

#if ENABLE(MYA)

#include <JavaScriptCore/CorpseAddress.h>
#include <stdint.h>
#include <string>
#include <string_view>
#include <wtf/TZoneMalloc.h>

namespace JSC {
namespace Corpse {

class Snapshot;

// A symbol looked up in a corpse by name. The lookup happens on construction.
//
// Only regular and absolute exports are read out of an image's trie. A re-export is
// skipped rather than followed, so a name that one image re-exports resolves in the
// image that defines it, as long as that image is loaded in the corpse. A
// thread-local is not found at all.
//
// A re-export may also rename, and then no image exports the name at all: memcpy
// exists only as libsystem_c's re-export of __platform_memmove from
// libsystem_platform, so a lookup of memcpy finds nothing while a lookup of
// __platform_memmove succeeds.
class Symbol {
    WTF_MAKE_TZONE_ALLOCATED(Symbol);
public:
    Symbol(Snapshot&, const char* name);

    const std::string& name() const { return m_name; }

    Address address() const { return m_address; } // Null means not found.
    bool isValid() const { return static_cast<bool>(m_address); }

private:
    Address lookUpName(Snapshot&);
    Address resolveInImage(const Snapshot&, Address loadAddress, std::string_view name);
    bool hasReadBudget(size_t length);

    std::string m_name;
    Address m_address;

    // What this lookup may still copy out of the corpse. Set when the search starts.
    size_t m_readBudget { 0 };
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
