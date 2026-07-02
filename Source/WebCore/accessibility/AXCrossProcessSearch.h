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

// Testing support: when enabled, performSearchWithParentCoordination injects a
// mock parent search result if the parent search was dispatched. This allows
// tests to verify that the parent search is correctly skipped (e.g., for
// immediateDescendantsOnly searches) without relying on real cross-process IPC,
// which deadlocks in the test runner purely due to a limitation in our testing
// infrastructure.
//
// The deadlock occurs because the test runner (UI process) initiates accessibility
// searches via AXUIElementCopyParameterizedAttributeValue, which is a synchronous
// Mach IPC call that blocks the UI process main thread until the child web content
// process returns a result. However, the child's search dispatches a parent search
// via WebKit IPC (child -> UI process -> parent process), and the UI process must
// process this IPC on its main thread to forward it to the parent. Since the main
// thread is blocked waiting for the AXUIElementCopyParameterizedAttributeValue
// reply, the parent search IPC cannot be forwarded, and the child eventually times
// out waiting for parent results.
//
// In production, VoiceOver uses the system accessibility framework which does not
// block the UI process in the same way, so this deadlock is test-infrastructure-
// specific. If the test infrastructure is updated to avoid blocking the UI process
// main thread during accessibility searches, this mock can be removed and tests
// can rely on real cross-process parent search results instead.
WEBCORE_EXPORT void NODELETE setShouldMockParentSearchResultsForTesting(bool);
WEBCORE_EXPORT bool NODELETE shouldMockParentSearchResultsForTesting();

// Same as above but for child frame (remote frame) search. When enabled,
// dispatchRemoteFrameSearch injects a mock result instead of sending IPC.
WEBCORE_EXPORT void NODELETE setShouldMockChildFrameSearchResultsForTesting(bool);
WEBCORE_EXPORT bool NODELETE shouldMockChildFrameSearchResultsForTesting();

} // namespace WebCore
