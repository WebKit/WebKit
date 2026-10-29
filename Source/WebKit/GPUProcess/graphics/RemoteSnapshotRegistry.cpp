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

#include "config.h"
#include "RemoteSnapshotRegistry.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteSnapshot.h"
#include <WebCore/FloatSize.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(RemoteSnapshotRegistry);

// Long enough for any frame whose process is responsive. One whose process is not, or went away
// before connecting here, draws nothing rather than holding up whoever is waiting for the snapshot.
static constexpr Seconds snapshotDeadline = 10_s;

Ref<RemoteSnapshotRegistry> RemoteSnapshotRegistry::create()
{
    return adoptRef(*new RemoteSnapshotRegistry);
}

RemoteSnapshotRegistry::RemoteSnapshotRegistry() = default;

RemoteSnapshotRegistry::~RemoteSnapshotRegistry() = default;

Ref<RemoteSnapshot> RemoteSnapshotRegistry::ensureSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, std::optional<FrameIdentifier> rootFrameIdentifier)
{
    RefPtr<RemoteSnapshot> snapshot;
    bool isNewEntry = false;
    {
        Locker locker(m_lock);
        auto result = m_snapshots.ensure(snapshotIdentifier, [&] {
            return RemoteSnapshot::create(rootFrameIdentifier);
        });
        snapshot = result.iterator->value.ptr();
        isNewEntry = result.isNewEntry;
    }
    // Only one frame can be the root.
    if (snapshot->rootFrameIdentifier() != rootFrameIdentifier)
        snapshot->fail();
    if (isNewEntry) {
        // Identifiers are never reused, so finding one when the deadline comes means it is this snapshot.
        callOnMainRunLoop([weakThis = ThreadSafeWeakPtr { *this }, snapshotIdentifier] {
            RunLoop::mainSingleton().dispatchAfter(snapshotDeadline, [weakThis, snapshotIdentifier] {
                if (RefPtr protectedThis = weakThis)
                    protectedThis->snapshotDeadlineExpired(snapshotIdentifier);
            });
        });
    }
    return snapshot.releaseNonNull();
}

void RemoteSnapshotRegistry::snapshotDeadlineExpired(RemoteSnapshotIdentifier snapshotIdentifier)
{
    RefPtr snapshot = this->snapshot(snapshotIdentifier);
    if (!snapshot)
        return;
    bool isAwaited = snapshot->isAwaited();
    snapshot->abandonUnresolvedFrames();
    // Whoever is waiting takes it now. Anything else is not coming for it, such as a root whose
    // process went away after the snapshot was released, or a GPU process that was relaunched under
    // whoever asked for it.
    if (!isAwaited || snapshot->hasFailed())
        releaseSnapshot(snapshotIdentifier);
}

void RemoteSnapshotRegistry::createSnapshot(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier rootFrameIdentifier, const FloatSize& size, ProcessIdentifier rootProcessIdentifier)
{
    // May already exist, if whoever draws it asked first.
    Ref snapshot = ensureSnapshot(snapshotIdentifier, rootFrameIdentifier);
    snapshot->setSize(size);
    snapshot->setFrameOwner(rootFrameIdentifier, rootProcessIdentifier);
}

void RemoteSnapshotRegistry::failSnapshot(RemoteSnapshotIdentifier snapshotIdentifier)
{
    // Created failed if it does not exist yet, so that whoever asks to draw it afterwards does not
    // wait for it.
    ensureSnapshot(snapshotIdentifier, std::nullopt)->fail();
}

Ref<RemoteSnapshot> RemoteSnapshotRegistry::snapshotForRecorder(RemoteSnapshotIdentifier snapshotIdentifier)
{
    if (RefPtr snapshot = this->snapshot(snapshotIdentifier))
        return snapshot.releaseNonNull();
    // Already released, so let the recorder fill a snapshot that is dropped with it.
    return RemoteSnapshot::create(std::nullopt);
}

RefPtr<RemoteSnapshot> RemoteSnapshotRegistry::snapshot(RemoteSnapshotIdentifier snapshotIdentifier)
{
    Locker locker(m_lock);
    return m_snapshots.get(snapshotIdentifier);
}

void RemoteSnapshotRegistry::abandonSnapshotFrame(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier frameIdentifier)
{
    if (RefPtr snapshot = this->snapshot(snapshotIdentifier))
        snapshot->abandonFrame(frameIdentifier);
}

void RemoteSnapshotRegistry::snapshotFrameWillBeDrawnByProcess(RemoteSnapshotIdentifier snapshotIdentifier, FrameIdentifier frameIdentifier, ProcessIdentifier processIdentifier)
{
    // A process that has not connected yet may still do so to record the frame, so only its
    // going away abandons the frame. The UI process says so if it has not connected by then.
    if (RefPtr snapshot = this->snapshot(snapshotIdentifier))
        snapshot->setFrameOwner(frameIdentifier, processIdentifier);
}

void RemoteSnapshotRegistry::abandonSnapshotFramesOwnedBy(ProcessIdentifier processIdentifier)
{
    Vector<Ref<RemoteSnapshot>> snapshots;
    {
        Locker locker(m_lock);
        snapshots = copyToVector(m_snapshots.values());
    }
    for (Ref snapshot : snapshots)
        snapshot->abandonFramesOwnedBy(processIdentifier);
}

void RemoteSnapshotRegistry::takeSnapshotWhenComplete(RemoteSnapshotIdentifier identifier, FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<RemoteSnapshot>&&)>&& completionHandler)
{
    // May be asked for before its root has created it, since the UI process asks as soon as it
    // has asked for the root. Anything that went wrong since is reported as failure.
    Ref snapshot = ensureSnapshot(identifier, rootFrameIdentifier);
    snapshot->whenComplete([protectedThis = Ref { *this }, identifier, snapshot, completionHandler = WTF::move(completionHandler)](bool success) mutable {
        {
            Locker locker(protectedThis->m_lock);
            auto iterator = protectedThis->m_snapshots.find(identifier);
            if (iterator != protectedThis->m_snapshots.end() && iterator->value.ptr() == snapshot.ptr())
                protectedThis->m_snapshots.remove(iterator);
        }
        completionHandler(success ? RefPtr { WTF::move(snapshot) } : nullptr);
    });
}

#if PLATFORM(COCOA)

void RemoteSnapshotRegistry::sinkCompletedSnapshotToPDF(RemoteSnapshotIdentifier identifier, FrameIdentifier rootFrameIdentifier, CompletionHandler<void(RefPtr<SharedBuffer>&&)>&& completionHandler)
{
    takeSnapshotWhenComplete(identifier, rootFrameIdentifier, [rootFrameIdentifier, completionHandler = WTF::move(completionHandler)](RefPtr<RemoteSnapshot>&& snapshot) mutable {
        if (!snapshot) {
            completionHandler({ });
            return;
        }
        auto result = snapshot->drawToPDF(snapshot->size(), rootFrameIdentifier);
        if (!result) {
            ASSERT_NOT_REACHED();
            completionHandler({ });
            return;
        }
        completionHandler(WTF::move(*result));
    });
}

#endif

void RemoteSnapshotRegistry::sinkCompletedSnapshotToBitmap(RemoteSnapshotIdentifier identifier, FrameIdentifier rootFrameIdentifier, CompletionHandler<void(std::optional<ShareableBitmap::Handle>&&)>&& completionHandler)
{
    takeSnapshotWhenComplete(identifier, rootFrameIdentifier, [rootFrameIdentifier, completionHandler = WTF::move(completionHandler)](RefPtr<RemoteSnapshot>&& snapshot) mutable {
        if (!snapshot) {
            completionHandler({ });
            return;
        }
        completionHandler(snapshot->drawToBitmap(snapshot->size(), rootFrameIdentifier));
    });
}

void RemoteSnapshotRegistry::releaseSnapshot(RemoteSnapshotIdentifier identifier)
{
    // Currently it's not possible to know if a snapshot exists, hence no ASSERT.
    RefPtr<RemoteSnapshot> snapshot;
    {
        Locker locker(m_lock);
        snapshot = m_snapshots.take(identifier);
    }
    // Anything still waiting for it would otherwise wait for frames that will never be asked for.
    if (snapshot)
        snapshot->fail();
}

void RemoteSnapshotRegistry::waitForSnapshot(RemoteSnapshotIdentifier identifier, CompletionHandler<void()>&& completionHandler)
{
    // Sent after the sink, keeping its order, so a snapshot that is not here has already been drawn.
    RefPtr snapshot = this->snapshot(identifier);
    if (!snapshot) {
        completionHandler();
        return;
    }
    // After the sink waiting for it, so that its reply is sent first.
    snapshot->whenComplete([completionHandler = WTF::move(completionHandler)](bool) mutable {
        completionHandler();
    });
}

void RemoteSnapshotRegistry::removeSnapshotsForProcess(ProcessIdentifier processIdentifier)
{
    Vector<Ref<RemoteSnapshot>> snapshots;
    {
        Locker locker(m_lock);
        m_snapshots.removeIf([&](auto& entry) {
            if (entry.key.processIdentifier() != processIdentifier)
                return false;
            snapshots.append(entry.value);
            return true;
        });
    }
    for (Ref snapshot : snapshots)
        snapshot->fail();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
