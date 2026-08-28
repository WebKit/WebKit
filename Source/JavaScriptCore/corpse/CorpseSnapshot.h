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

#if (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)

#include <JavaScriptCore/CorpseAddress.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSymbol.h>
#include <JavaScriptCore/CorpseThread.h>
#include <mach/mach.h>
#include <memory>
#include <optional>
#include <utility>
#include <wtf/DoublyLinkedList.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

namespace JSC {
namespace Corpse {

// Owns a corpse (a read-only Mach snapshot of a process)
// Check isValid() to see whether acquisition succeeded.
//
// Snapshots are linked into a DoublyLinkedList by their owner. The list is
// intrusive and does not own its nodes: whoever appends a Snapshot must remove
// it from the list before destroying it.
class Snapshot : public DoublyLinkedListNode<Snapshot> {
    WTF_MAKE_TZONE_ALLOCATED(Snapshot);
public:
    explicit Snapshot(RefPtr<Process>);
    ~Snapshot();

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&& other) = delete;

    bool isValid() const { return MACH_PORT_VALID(m_corpsePort); }

    // A monotonically increasing identifier assigned at construction. IDs are
    // never reused, so they stay stable as snapshots are added and removed.
    unsigned id() const { return m_id; }

    Process* process() const { return m_process.get(); }
    mach_port_t corpsePort() const { return m_corpsePort; }

    // The threads captured in this corpse, read and cached on the first call.
    const Vector<Thread>& threads();

    // The address of `name` in this corpse, null if it is not there.
    Address symbol(const char* name);

private:
    static unsigned s_nextId;

    RefPtr<Process> m_process;
    mach_port_t m_corpsePort { MACH_PORT_NULL };
    unsigned m_id;

    std::optional<Vector<Thread>> m_threads;
    HashMap<String, std::unique_ptr<Symbol>> m_symbols;

    Snapshot* m_prev { nullptr }; // Required by DoublyLinkedListNode.
    Snapshot* m_next { nullptr }; // Required by DoublyLinkedListNode.

    friend class WTF::DoublyLinkedListNode<Snapshot>;
};

} // namespace Corpse
} // namespace JSC

#endif // (OS(MACOS) || USE(APPLE_INTERNAL_SDK)) && !PLATFORM(MACCATALYST) && !PLATFORM(IOS_FAMILY_SIMULATOR)
