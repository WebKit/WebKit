/*
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

#if ENABLE(MYA) && HAVE(LLDB)

#include <memory>
#include <optional>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace lldb {
class SBModule;
class SBType;
}

namespace JSC {
namespace Corpse {

class TargetType;

// A pointer or reference held somewhere in an object, to an object of class, struct or union type.
struct TargetPointer {
    uint64_t offset; // From the start of the object that holds it.
    const TargetType* pointee; // As declared. For a polymorphic pointee, perhaps a base of the object's class.
};

// A type from a snapshot's debug info, one per type, owned by the SnapshotDebugInfo.
class TargetType {
    WTF_MAKE_TZONE_ALLOCATED(TargetType);
    WTF_MAKE_NONCOPYABLE(TargetType);
public:
    ~TargetType();

    // As liblldb spells it, such as "JSC::StringFireDetail".
    const String& name() const { return m_name; }
    // The path of the image whose debug info describes this type. Another image may describe the same class.
    const String& image() const { return m_image; }
    size_t byteSize() const;
    bool isPolymorphic() const;
    // Only declared where it was found, so its size and fields are unknown.
    bool isDeclaration() const { return !byteSize(); }

private:
    friend class SnapshotDebugInfo;
    TargetType(const lldb::SBType&, const lldb::SBModule&);

    const std::unique_ptr<lldb::SBType> m_type;
    const std::unique_ptr<lldb::SBModule> m_module;
    const String m_name;
    const String m_image;
    // Filled in by SnapshotDebugInfo::pointers().
    mutable std::optional<Vector<TargetPointer>> m_pointers;
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA) && HAVE(LLDB)
