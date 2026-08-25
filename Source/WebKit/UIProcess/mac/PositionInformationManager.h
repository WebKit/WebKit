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

#include "InteractionInformationAtPosition.h"
#include "InteractionInformationRequest.h"
#include <optional>
#include <utility>
#include <wtf/CompletionHandler.h>
#include <wtf/Platform.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

// FIXME: Share this logic with iOS.
// FIXME: Convert this to Swift for more compile-time safety and guarantees.

namespace WebKit {

class WebPageProxy;

// Owns the asynchronous hit-test "position information" cache and the queue of completions waiting
// on it. A completion registered via doAfterUpdate() is guaranteed to run exactly once: either with
// the resolved information, or with std::nullopt when the request is abandoned (the press ended
// before the reply arrived, the page went away, or the manager is reset / destroyed).
class PositionInformationManager {
    WTF_MAKE_TZONE_ALLOCATED(PositionInformationManager);
public:
    using UpdateCompletionHandler = CompletionHandler<void(const std::optional<InteractionInformationAtPosition>&)>;

    explicit PositionInformationManager(WebPageProxy&);
    ~PositionInformationManager();

    // Runs the completion handler once the cache holds information valid for the request. If the cache is already
    // valid the callback runs synchronously; otherwise it is queued and an asynchronous update is
    // requested (coalesced with any in-flight request that would also satisfy this one).
    void doAfterUpdate(const InteractionInformationRequest&, UpdateCompletionHandler&&);

    // Called when fresh information arrives from the web process; resolves all queued callbacks that
    // the new information satisfies.
    void didChange(const InteractionInformationAtPosition&);

    // Drops the cached information (e.g. on a main-frame load commit). Does not touch queued callbacks.
    void invalidate();

    // Full teardown: invalidates the cache, forgets any in-flight request, and resolves every queued
    // callback with std::nullopt.
    void reset();

    // Abandons the in-flight request (if any) and resolves the queued callbacks waiting on it with
    // std::nullopt. Used when a deferring gesture's press ends before the reply arrives.
    void abandonOutstandingRequest();

    bool currentIsValid(const InteractionInformationRequest&) const;
    bool currentIsApproximatelyValid(const InteractionInformationRequest&, int radius) const;
    bool hasValidCurrentInformation() const { return m_hasValidCurrentInformation; }
    const InteractionInformationAtPosition& currentInformation() const { return m_currentInformation; }

private:
    void requestUpdate(const InteractionInformationRequest&);
    bool hasValidOutstandingRequest(const InteractionInformationRequest&) const;

    // Invokes (exactly once) and removes every queued callback whose request satisfies `matches`,
    // passing `information` to each. Reentrancy-safe: callbacks may re-enter and mutate the queue, so
    // consumed slots are left as tombstones and only compacted once the outermost invocation unwinds.
    void invokeAndRemovePendingHandlers(Function<bool(InteractionInformationRequest)>&& matches, const std::optional<InteractionInformationAtPosition>&);

    struct RequestAndCallback {
        InteractionInformationRequest request;
        UpdateCompletionHandler callback;
    };

    WeakPtr<WebPageProxy> m_page;
    InteractionInformationAtPosition m_currentInformation;
    std::optional<InteractionInformationRequest> m_lastOutstandingRequest;
    Vector<std::optional<RequestAndCallback>> m_pendingHandlers;
    uint64_t m_callbackDepth { 0 };
    bool m_hasValidCurrentInformation { false };
};

} // namespace WebKit
