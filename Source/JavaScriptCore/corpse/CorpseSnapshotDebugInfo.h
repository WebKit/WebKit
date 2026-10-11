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

#if ENABLE(MYA_HEAP)

#include <JavaScriptCore/CorpseAddress.h>
#include <memory>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

namespace lldb {
class SBDebugger;
class SBTarget;
class SBType;
}

namespace JSC {
namespace Corpse {

class Snapshot;
class TargetType;

// The debug info of a snapshot's images, read through liblldb, which is never given a process.
class SnapshotDebugInfo : public RefCounted<SnapshotDebugInfo> {
    WTF_MAKE_TZONE_ALLOCATED(SnapshotDebugInfo);
public:
    // An image liblldb cannot open is reported and left out.
    static Ref<SnapshotDebugInfo> create(Snapshot&);
    ~SnapshotDebugInfo();

    // The class of the complete object containing the polymorphic object at `address`, and where it starts.
    const TargetType* dynamicTypeAt(Snapshot&, Address, Address& completeObject);

private:
    SnapshotDebugInfo();

    const TargetType& type(const lldb::SBType&);

    std::unique_ptr<lldb::SBDebugger> m_debugger;
    std::unique_ptr<lldb::SBTarget> m_target;
    // Types of one name from different images share an entry.
    HashMap<String, Vector<std::unique_ptr<TargetType>>> m_types;
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA_HEAP)
