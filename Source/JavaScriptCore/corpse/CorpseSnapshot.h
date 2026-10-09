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
#include <JavaScriptCore/CorpseImage.h>
#include <JavaScriptCore/CorpseMemory.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseRegion.h>
#include <JavaScriptCore/CorpseSymbol.h>
#include <JavaScriptCore/CorpseThread.h>
#include <memory>
#include <optional>
#include <utility>
#include <wtf/DoublyLinkedList.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>

#if OS(DARWIN)
struct dyld_all_image_infos;
struct dyld_image_info;
#endif

namespace JSC {

class VM;

namespace Corpse {

// Owns a corpse: a read-only Mach snapshot of a process on Darwin, and on Linux
// a copy of it that the process forked.
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

    // A snapshot of a target that cooperates. The target calls offer(), or
    // offerSafepoint() at a GC safepoint of its VM, and stops there; receive()
    // waits for it to stop, takes the snapshot, and continues it. On Linux the
    // target first forks a copy, which keeps its memory as it was at the fork and
    // lives until the snapshot ends or the target exits. offer() runs on WTF's main thread.
    static void offer();
    static void offerSafepoint(VM&);
    static std::unique_ptr<Snapshot> receive(pid_t target);

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    Snapshot(Snapshot&& other) = delete;

    bool isValid() const { return isValidTaskHandle(corpsePort()); }

    // A monotonically increasing identifier assigned at construction. IDs are
    // never reused, so they stay stable as snapshots are added and removed.
    unsigned id() const { return m_id; }

    Process* process() const { return m_process.get(); }
    // The path of the process's executable, read when the snapshot was taken.
    const UTF8CString& executablePath() const { return m_executablePath; }
    // On Linux, the pid of the copy.
    TaskHandle corpsePort() const { return taskHandle(m_corpsePort); }

    Memory& memory() LIFETIME_BOUND
    {
        RELEASE_ASSERT(isValid());
        return m_memory;
    }

    // The threads and images captured in this corpse, read and cached on the first call.
    const Vector<Thread>& threads();
    const Vector<Image>& images();
    // The regions of the snapshot's memory, listed and cached on the first call.
    const Vector<Region>& regions();

#if OS(DARWIN)
    // dyld's record of the loaded images, and the image list it points to.
    Memory::Ptr<dyld_all_image_infos> dyldAllImageInfos();
    Memory::Span<dyld_image_info> dyldImageInfos(const Memory::Ptr<dyld_all_image_infos>&);
#endif

    // The address of `name` in this corpse, null if it is not there.
    Address symbol(const char* name);

private:
    static std::unique_ptr<Snapshot> snapshotOfStopped(pid_t target);
#if !OS(DARWIN)
    Snapshot(RefPtr<Process>, pid_t copy);
#endif

    static unsigned s_nextId;

    RefPtr<Process> m_process;
    OwnedTaskHandle m_corpsePort;
    UTF8CString m_executablePath;
    unsigned m_id;

    std::optional<Vector<Thread>> m_threads;
    std::optional<Vector<Image>> m_images;
    std::optional<Vector<Region>> m_regions;
    HashMap<String, std::unique_ptr<Symbol>> m_symbols;
    Memory m_memory;

    Snapshot* m_prev { nullptr }; // Required by DoublyLinkedListNode.
    Snapshot* m_next { nullptr }; // Required by DoublyLinkedListNode.

    friend class WTF::DoublyLinkedListNode<Snapshot>;
};

} // namespace Corpse
} // namespace JSC

#endif // ENABLE(MYA)
