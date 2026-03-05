/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AXID.h>
#include <WebCore/AXSearchManager.h>
#include <WebCore/AccessibilityRemoteToken.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/threads/BinarySemaphore.h>

namespace WebCore {

enum class DidTimeout : bool;

// Timeout for cross-process accessibility search operations.
constexpr Seconds crossProcessSearchTimeout = 200_ms;
// Buffer to account for IPC overhead when cascading timeouts to nested frames.
constexpr Seconds crossProcessSearchIPCOverhead = 10_ms;
// Minimum timeout to ensure deeply nested frames always get some search time.
constexpr Seconds crossProcessSearchMinimumTimeout = 20_ms;

// Coordinates cross-process accessibility search, handling synchronization
// and storage of remote results as tokens. Platform-specific conversion
// of tokens to accessibility elements is handled by the caller.
class AXCrossProcessSearchCoordinator : public ThreadSafeRefCounted<AXCrossProcessSearchCoordinator> {
    WTF_MAKE_NONCOPYABLE(AXCrossProcessSearchCoordinator);
    WTF_MAKE_TZONE_ALLOCATED_INLINE(AXCrossProcessSearchCoordinator);
public:
    static Ref<AXCrossProcessSearchCoordinator> create()
    {
        return adoptRef(*new AXCrossProcessSearchCoordinator());
    }

    // Wait for all pending responses or timeout.
    // Returns DidTimeout::No if all responses were received, DidTimeout::Yes if timed out.
    // On the main thread, spins the run loop to allow IPC callbacks to be processed.
    WEBCORE_EXPORT DidTimeout waitWithTimeout(Seconds timeout);

    // Increment the pending request count. Called when dispatching an IPC request.
    void addPendingRequest() { ++m_pendingCount; }

    // Mark that the local search has completed. If there are no pending requests,
    // signals the semaphore immediately.
    void markSearchComplete()
    {
        m_searchComplete.store(true, std::memory_order_release);
        checkCompletion();
    }

    // Signal that a response was received. If the search is complete and this
    // was the last pending response, signals the semaphore.
    void responseReceived()
    {
        --m_pendingCount;
        checkCompletion();
    }

    // Store remote tokens for a given stream index.
    void storeRemoteResults(size_t streamIndex, Vector<AccessibilityRemoteToken>&& tokens)
    {
        Locker locker { m_lock };
        m_remoteResults.set(streamIndex, WTF::move(tokens));
    }

    // Take stored remote results for a stream index.
    Vector<AccessibilityRemoteToken> takeRemoteResults(size_t streamIndex)
    {
        Locker locker { m_lock };
        return m_remoteResults.take(streamIndex);
    }

    // Returns true if this is a new frame (not already searched), false if duplicate.
    bool markFrameAsSearched(FrameIdentifier frameID)
    {
        Locker locker { m_lock };
        return m_searchedFrames.add(frameID).isNewEntry;
    }

private:
    AXCrossProcessSearchCoordinator() = default;

    void checkCompletion()
    {
        // Only signal completion when:
        // 1. The local search has finished (so we know all remote frames have been encountered)
        // 2. All pending IPC requests have received responses
        if (m_searchComplete.load(std::memory_order_acquire) && !m_pendingCount.load(std::memory_order_acquire))
            m_semaphore.signal();
    }

    BinarySemaphore m_semaphore;
    std::atomic<size_t> m_pendingCount { 0 };
    std::atomic<bool> m_searchComplete { false };
    Lock m_lock;
    HashMap<size_t, Vector<AccessibilityRemoteToken>> m_remoteResults WTF_GUARDED_BY_LOCK(m_lock);
    HashSet<FrameIdentifier> m_searchedFrames WTF_GUARDED_BY_LOCK(m_lock);
};

// Performs cross-process search coordination:
// 1. Takes a stream with local results + remote frame placeholders
// 2. Sends IPC to each remote frame via ChromeClient (on main thread)
// 3. Waits for responses (with timeout)
// 4. Returns merged AccessibilitySearchResults in tree order
//
// If treeID is nullopt or no remote frames exist, returns only local results.
// requestingFrameID is pre-populated in the coordinator to prevent re-searching
// the child frame that requested search continuation in its parent.
WEBCORE_EXPORT AccessibilitySearchResults performCrossProcessSearch(AccessibilitySearchResultStream&&, const AccessibilitySearchCriteriaIPC&, std::optional<AXTreeID>, unsigned originalLimit, std::optional<FrameIdentifier> requestingFrameID = std::nullopt);

// High-level search function that handles cross-process coordination automatically.
// Sets anchorObject, performs the search, and coordinates with remote frames.
WEBCORE_EXPORT AccessibilitySearchResults performSearchWithCrossProcessCoordination(AXCoreObject& anchorObject, AccessibilitySearchCriteria&&);

// Merges parent frame search results with local results based on search direction.
// For forward search: local results first, then parent results (elements after the frame).
// For backward search: parent results first (elements before the frame), then local results.
// Results are truncated to limit.
WEBCORE_EXPORT AccessibilitySearchResults mergeParentSearchResults(AccessibilitySearchResults&& localResults, Vector<AccessibilityRemoteToken>&& parentTokens, bool isForwardSearch, unsigned limit);

// Performs accessibility search with automatic parent frame coordination.
// If the search originates from a child frame, dispatches to the parent in parallel
// and merges results. Uses performSearchWithCrossProcessCoordination internally for
// nested remote frames.
//
// currentFrameID: If provided, this frame's ID is passed to the parent to prevent
// re-searching this frame during parent continuation.
WEBCORE_EXPORT AccessibilitySearchResults performSearchWithParentCoordination(AXCoreObject& anchorObject, AccessibilitySearchCriteria&&, std::optional<FrameIdentifier> currentFrameID = std::nullopt);

} // namespace WebCore
