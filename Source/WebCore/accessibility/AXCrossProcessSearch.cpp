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

#include "config.h"
#include "AXCrossProcessSearch.h"

#include <WebCore/AXCoreObject.h>
#include <WebCore/AXObjectCache.h>
#include <WebCore/AXTreeStoreInlines.h>
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/Page.h>
#include <wtf/MainThread.h>
#include <wtf/MonotonicTime.h>
#include <wtf/RefCounted.h>
#include <wtf/StdLibExtras.h>
#include <wtf/threads/BinarySemaphore.h>

#if PLATFORM(COCOA)
#include <CoreFoundation/CFRunLoop.h>
#endif

#if PLATFORM(MAC)
#define PLATFORM_SUPPORTS_REMOTE_SEARCH 1
#else
#define PLATFORM_SUPPORTS_REMOTE_SEARCH 0
#endif

namespace WebCore {

static bool canDoRemoteSearch(const std::optional<AXTreeID>& treeID)
{
#if PLATFORM_SUPPORTS_REMOTE_SEARCH
    return treeID.has_value();
#else
    UNUSED_PARAM(treeID);
    return false;
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH
}

// Spins the run loop on the main thread while waiting for a condition to become true.
// In the future, we could consider changing callers to implement a solution that doesn't
// require polling as done in this function, since polling can be inefficient.
template<typename Predicate>
static DidTimeout spinRunLoopUntil(Predicate&& isComplete, Seconds timeout)
{
    AX_ASSERT(isMainThread());

    auto deadline = MonotonicTime::now() + timeout;
    while (MonotonicTime::now() < deadline) {
        if (isComplete())
            return DidTimeout::No;
#if PLATFORM(COCOA)
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.02, true);
#else
        Thread::yield();
#endif
    }
    return isComplete() ? DidTimeout::No : DidTimeout::Yes;
}

DidTimeout AXCrossProcessSearchCoordinator::waitWithTimeout(Seconds timeout)
{
    auto isComplete = [this] {
        return m_searchComplete.load(std::memory_order_acquire) && !m_pendingCount.load(std::memory_order_acquire);
    };

    // If search is already complete with no pending requests, return immediately.
    if (isComplete())
        return DidTimeout::No;

    if (isMainThread()) {
        // On the main thread, we can't block on a semaphore because IPC callbacks
        // need to run on the main thread. Instead, spin the run loop.
        return spinRunLoopUntil(isComplete, timeout);
    }

    // On background threads (e.g., the accessibility thread), we can safely
    // block on the semaphore.
    return m_semaphore.waitFor(timeout) ? DidTimeout::No : DidTimeout::Yes;
}

// Helper to merge stream entries into AccessibilitySearchResults.
// If coordinator is provided, also pulls in remote results for RemoteFrame entries.
static AccessibilitySearchResults mergeStreamResults(const Vector<SearchResultEntry>& entries, unsigned limit, AXCrossProcessSearchCoordinator* coordinator)
{
    AccessibilitySearchResults results;
    for (const auto& entry : entries) {
        if (results.size() >= limit)
            break;

        if (RefPtr object = entry.objectIfLocalResult())
            results.append(AccessibilitySearchResult::local(object.releaseNonNull()));
        else if (coordinator) {
            // The search result was from an AXRemoteFrame we contain. Pull
            // AccessibilityRemoteTokens from the search coordinator and
            // convert them into results.
            auto tokens = coordinator->takeRemoteResults(entry.streamIndex());
            for (auto& token : tokens) {
                if (results.size() >= limit)
                    break;
                results.append(AccessibilitySearchResult::remote(WTF::move(token)));
            }
        }
    }
    return results;
}

#if PLATFORM_SUPPORTS_REMOTE_SEARCH
// Computes remaining timeout from an absolute deadline, accounting for IPC overhead.
// Returns std::nullopt if the deadline has already passed (so callers can skip the search).
// Returns at least crossProcessSearchMinimumTimeout to ensure deeply nested frames
// always get some time to search.
static std::optional<Seconds> computeRemainingTimeout(std::optional<MonotonicTime> deadline)
{
    if (!deadline)
        return crossProcessSearchTimeout;

    auto remaining = *deadline - MonotonicTime::now() - crossProcessSearchIPCOverhead;
    if (remaining <= 1_ms)
        return std::nullopt;
    return std::max(crossProcessSearchMinimumTimeout, remaining);
}

// Dispatches an IPC request to search a remote frame.
// The coordinator's responseReceived() will be called when the response arrives (or on failure).
static void dispatchRemoteFrameSearch(Ref<AXCrossProcessSearchCoordinator> coordinator, FrameIdentifier frameID, AccessibilitySearchCriteriaIPC criteria, size_t streamIndex, AXTreeID treeID)
{
    ensureOnMainThread([coordinator = WTF::move(coordinator), frameID, criteria = WTF::move(criteria), streamIndex, treeID]() mutable {
        AX_ASSERT(isMainThread());

        WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(treeID);
        RefPtr page = cache ? cache->page() : nullptr;
        if (!page) {
            coordinator->responseReceived();
            return;
        }

        page->chrome().client().performAccessibilitySearchInRemoteFrame(frameID, criteria,
            [coordinator = WTF::move(coordinator), streamIndex](Vector<AccessibilityRemoteToken>&& tokens) mutable {
                coordinator->storeRemoteResults(streamIndex, WTF::move(tokens));
                coordinator->responseReceived();
            });
    });
}
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH

AccessibilitySearchResults performCrossProcessSearch(AccessibilitySearchResultStream&& stream, const AccessibilitySearchCriteriaIPC& criteriaForIPC, std::optional<AXTreeID> treeID, unsigned originalLimit, std::optional<FrameIdentifier> requestingFrameID)
{
    if (!canDoRemoteSearch(treeID)) {
        UNUSED_PARAM(criteriaForIPC);
        UNUSED_PARAM(requestingFrameID);
        return mergeStreamResults(stream.entries(), originalLimit, nullptr);
    }

#if PLATFORM_SUPPORTS_REMOTE_SEARCH
    // Calculate how many results to request from each remote frame.
    // We need to account for local results that precede each remote frame in tree order.
    Vector<std::pair<const SearchResultEntry*, unsigned>> remoteFrameRequests;
    unsigned localCountSoFar = 0;
    for (const auto& entry : stream.entries()) {
        if (entry.isLocalResult())
            ++localCountSoFar;
        else {
            // For this remote frame, request enough to potentially fill remaining quota.
            unsigned remaining = originalLimit > localCountSoFar ? originalLimit - localCountSoFar : 0;
            // If we've already filled our quota with local results before this remote frame,
            // we don't need to query it.
            if (remaining > 0)
                remoteFrameRequests.append({ &entry, remaining });
        }
    }

    if (remoteFrameRequests.isEmpty()) {
        // All remote frames were skipped because local results filled the quota.
        return mergeStreamResults(stream.entries(), originalLimit, nullptr);
    }

    // We have remote frames to query. Create a coordinator for synchronization.
    Ref coordinator = AXCrossProcessSearchCoordinator::create();

    if (requestingFrameID) {
        // Pre-populate with requesting frame to prevent re-searching it.
        coordinator->markFrameAsSearched(*requestingFrameID);
    }

    // Dispatch IPC for each remote frame.
    for (const auto& [entry, maxResults] : remoteFrameRequests) {
        if (!entry->frameID()) {
            // No frame ID, nothing to dispatch.
            continue;
        }

        // Skip frames we've already searched.
        if (!coordinator->markFrameAsSearched(*entry->frameID()))
            continue;

        coordinator->addPendingRequest();

        auto slotCriteria = criteriaForIPC;
        slotCriteria.resultsLimit = maxResults;

        dispatchRemoteFrameSearch(coordinator.copyRef(), *entry->frameID(), WTF::move(slotCriteria), entry->streamIndex(), *treeID);
    }

    // Mark search complete (all remote frames have been dispatched).
    coordinator->markSearchComplete();

    // Wait for all responses using the cascading timeout (remaining time from deadline).
    if (std::optional remainingTimeout = computeRemainingTimeout(criteriaForIPC.deadline))
        coordinator->waitWithTimeout(*remainingTimeout);

    // Merge results in tree order.
    return mergeStreamResults(stream.entries(), originalLimit, coordinator.ptr());
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH
}

AccessibilitySearchResults performSearchWithCrossProcessCoordination(AXCoreObject& anchorObject, AccessibilitySearchCriteria&& criteria)
{
    unsigned originalLimit = criteria.resultsLimit;
    std::optional treeID = anchorObject.treeID();
    if (!canDoRemoteSearch(treeID)) {
        criteria.anchorObject = &anchorObject;
        auto stream = AXSearchManager().findMatchingObjectsAsStream(WTF::move(criteria));
        return mergeStreamResults(stream.entries(), originalLimit, nullptr);
    }

#if PLATFORM_SUPPORTS_REMOTE_SEARCH
    auto criteriaForIPC = AccessibilitySearchCriteriaIPC(criteria);

    // If no deadline has been set, set one now. This establishes the timeout budget
    // for the entire search tree, ensuring nested frames share the same deadline.
    if (!criteriaForIPC.deadline)
        criteriaForIPC.deadline = MonotonicTime::now() + crossProcessSearchTimeout;

    // Create coordinator upfront for eager IPC dispatch.
    Ref coordinator = AXCrossProcessSearchCoordinator::create();

    // Callback invoked when a remote frame is encountered during search.
    // Dispatches IPC immediately so remote search runs in parallel with local search.
    auto remoteFrameCallback = [&coordinator, &criteriaForIPC, originalLimit, treeID](FrameIdentifier frameID, size_t streamIndex, unsigned localResultCount) {
        // Skip frames we've already searched.
        if (!coordinator->markFrameAsSearched(frameID))
            return;

        // Calculate how many results we need from this remote frame.
        unsigned remaining = originalLimit > localResultCount ? originalLimit - localResultCount : 0;
        if (!remaining) {
            // Local results already filled quota, skip this remote frame.
            return;
        }

        coordinator->addPendingRequest();

        auto slotCriteria = criteriaForIPC;
        slotCriteria.resultsLimit = remaining;

        dispatchRemoteFrameSearch(coordinator.copyRef(), frameID, WTF::move(slotCriteria), streamIndex, *treeID);
    };

    criteria.anchorObject = &anchorObject;
    auto stream = AXSearchManager().findMatchingObjectsAsStream(WTF::move(criteria), WTF::move(remoteFrameCallback));

    // Mark search complete so coordinator knows all remote frames have been encountered.
    coordinator->markSearchComplete();

    // Wait for all responses using the cascading timeout (remaining time from deadline).
    if (std::optional remainingTimeout = computeRemainingTimeout(criteriaForIPC.deadline))
        coordinator->waitWithTimeout(*remainingTimeout);

    // Merge results in tree order.
    return mergeStreamResults(stream.entries(), originalLimit, coordinator.ptr());
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH
}

AccessibilitySearchResults mergeParentSearchResults(AccessibilitySearchResults&& localResults, Vector<AccessibilityRemoteToken>&& parentTokens, bool isForwardSearch, unsigned limit)
{
    if (parentTokens.isEmpty())
        return WTF::move(localResults);

    if (isForwardSearch) {
        // Forward search: local results first, then parent results (elements after the frame).
        for (auto& token : parentTokens) {
            if (localResults.size() >= limit)
                break;
            localResults.append(AccessibilitySearchResult::remote(WTF::move(token)));
        }
        return WTF::move(localResults);
    }

    // Backward search: parent results first (elements before the frame), then local results.
    AccessibilitySearchResults mergedResults;
    unsigned localCount = localResults.size();
    for (auto& token : parentTokens) {
        if (mergedResults.size() + localCount >= limit)
            break;
        mergedResults.append(AccessibilitySearchResult::remote(WTF::move(token)));
    }
    mergedResults.appendVector(WTF::move(localResults));
    return mergedResults;
}

#if PLATFORM_SUPPORTS_REMOTE_SEARCH
// Ref-counted context for coordinating search continuation into a parent frame.
// When a child frame's search needs results from its parent frame (e.g. elements
// before or after the iframe in tree order), this context manages the IPC roundtrip
// and prevents use-after-free if the calling thread times out before the callback.
class ParentFrameSearchContext : public RefCounted<ParentFrameSearchContext> {
    WTF_MAKE_NONCOPYABLE(ParentFrameSearchContext);
    WTF_MAKE_TZONE_ALLOCATED_INLINE(ParentFrameSearchContext);
public:
    ParentFrameSearchContext() = default;

    void signal()
    {
        if (m_shouldSignal.exchange(false, std::memory_order_acq_rel))
            m_semaphore.signal();
    }

    DidTimeout waitWithTimeout(Seconds timeout)
    {
        DidTimeout didTimeout;
        if (isMainThread()) {
            // On the main thread, we can't block on a semaphore because IPC callbacks
            // need to run on the main thread. Instead, spin the run loop.
            auto isComplete = [this] {
                return !m_shouldSignal.load(std::memory_order_acquire);
            };
            didTimeout = spinRunLoopUntil(isComplete, timeout);
        } else
            didTimeout = m_semaphore.waitFor(timeout) ? DidTimeout::No : DidTimeout::Yes;

        if (didTimeout == DidTimeout::Yes)
            m_shouldSignal.exchange(false, std::memory_order_acq_rel);
        return didTimeout;
    }

    void markParentDispatched() { m_dispatchedParent.store(true, std::memory_order_release); }
    bool didDispatchParent() const { return m_dispatchedParent.load(std::memory_order_acquire); }

    void setParentTokens(Vector<AccessibilityRemoteToken>&& tokens)
    {
        Locker locker { m_lock };
        m_parentTokens = WTF::move(tokens);
    }

    Vector<AccessibilityRemoteToken> takeParentTokens()
    {
        Locker locker { m_lock };
        return std::exchange(m_parentTokens, { });
    }

private:
    BinarySemaphore m_semaphore;
    std::atomic<bool> m_shouldSignal { true };
    std::atomic<bool> m_dispatchedParent { false };
    Lock m_lock;
    Vector<AccessibilityRemoteToken> m_parentTokens WTF_GUARDED_BY_LOCK(m_lock);
};
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH

AccessibilitySearchResults performSearchWithParentCoordination(AXCoreObject& anchorObject, AccessibilitySearchCriteria&& criteria, std::optional<FrameIdentifier> currentFrameID)
{
    std::optional treeID = anchorObject.treeID();
    if (!canDoRemoteSearch(treeID)) {
        UNUSED_PARAM(currentFrameID);
        return performSearchWithCrossProcessCoordination(anchorObject, WTF::move(criteria));
    }

#if PLATFORM_SUPPORTS_REMOTE_SEARCH
    // Save original parameters for parent coordination.
    unsigned originalLimit = criteria.resultsLimit;
    bool isForward = criteria.searchDirection == AccessibilitySearchDirection::Next;
    auto criteriaForParent = AccessibilitySearchCriteriaIPC(criteria);

    // If no deadline has been set, set one now. This establishes the timeout budget
    // for the entire search tree, ensuring nested frames share the same deadline.
    if (!criteriaForParent.deadline)
        criteriaForParent.deadline = MonotonicTime::now() + crossProcessSearchTimeout;

    // Use ref-counted context to safely coordinate between threads.
    Ref context = adoptRef(*new ParentFrameSearchContext);

    ensureOnMainThread([context, criteriaForParent, treeID, currentFrameID]() mutable {
        WeakPtr cache = AXTreeStore<AXObjectCache>::axObjectCacheForID(*treeID);
        RefPtr document = cache ? cache->document() : nullptr;
        RefPtr frame = document ? document->frame() : nullptr;
        RefPtr page = frame ? frame->page() : nullptr;

        if (!frame || !page || frame->isMainFrame() || !page->settings().siteIsolationEnabled()) {
            // Not in a child frame, or site isolation is disabled (so no cross-process coordination needed).
            context->signal();
            return;
        }

        context->markParentDispatched();

        // Use the provided frameID if available, otherwise use the frame's own ID.
        FrameIdentifier frameIDToUse = currentFrameID.value_or(frame->frameID());

        // Request full limit from parent - we'll truncate during merge.
        page->chrome().client().continueAccessibilitySearchFromChildFrame(frameIDToUse, criteriaForParent,
            [context](Vector<AccessibilityRemoteToken>&& tokens) mutable {
                context->setParentTokens(WTF::move(tokens));
                context->signal();
            });
    });

    // Perform local + nested remote frame search (runs in parallel with parent search).
    auto searchResults = performSearchWithCrossProcessCoordination(anchorObject, WTF::move(criteria));

    // Wait for parent search to complete using the cascading timeout.
    if (auto remainingTimeout = computeRemainingTimeout(criteriaForParent.deadline))
        context->waitWithTimeout(*remainingTimeout);

    // Merge parent results with local results based on search direction.
    if (context->didDispatchParent())
        searchResults = mergeParentSearchResults(WTF::move(searchResults), context->takeParentTokens(), isForward, originalLimit);

    return searchResults;
#else
    RELEASE_ASSERT_NOT_REACHED();
#endif // PLATFORM_SUPPORTS_REMOTE_SEARCH
}

} // namespace WebCore
