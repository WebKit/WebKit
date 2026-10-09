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
#include <optional>
#include <stdint.h>
#include <wtf/Vector.h>
#if !OS(DARWIN)
#include <string_view>
#include <wtf/text/WTFString.h>
#endif

namespace JSC {
namespace Corpse {

// One mapped region of a task's address space, as the kernel describes it.
class Region {
public:
    // The region containing `address`, or nullopt if not found in any region.
    static std::optional<Region> findContaining(TaskHandle, Address);

    // Every region of the task, by address. Empty if they cannot all be listed.
    static Vector<Region> all(TaskHandle);

    Address base() const { return m_base; }
    size_t size() const { return m_size; }
    Address end() const { return m_base + m_size; }
    bool contains(Address address) const { return address >= m_base && address < end(); }

    uint64_t pageCount() const;
    uint64_t residentPageCount() const { return m_residentPageCount; }
    uint64_t dirtyPageCount() const { return m_dirtyPageCount; }

#if !OS(DARWIN)
    // The path of the file the region maps, or a label such as "[stack]", as /proc/<pid>/maps gives it.
    const String& name() const { return m_name; }
    uint64_t fileOffset() const { return m_fileOffset; }
    uint64_t inode() const { return m_inode; }
    bool isExecutable() const { return m_isExecutable; }
#endif

private:
#if OS(DARWIN)
    static kern_return_t atOrAbove(TaskHandle, Address, Region&);
#else
    static std::optional<Region> parseMapsLine(std::string_view);
#endif

    Address m_base;
    size_t m_size { 0 };
    uint64_t m_residentPageCount { 0 };
    uint64_t m_dirtyPageCount { 0 };
#if !OS(DARWIN)
    String m_name;
    uint64_t m_fileOffset { 0 };
    uint64_t m_inode { 0 };
    bool m_isExecutable { false };
#endif
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
