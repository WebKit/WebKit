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
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#pragma once

#include <WebCore/SharedBuffer.h>
#include <wtf/URL.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

// The phase of a network load at which an intercept pattern takes effect.
enum class NetworkInterceptionPhase : uint8_t {
    BeforeRequestSent,
    ResponseStarted,
    AuthRequired,
};

// The consumer that registered an intercept pattern. Each consumer replaces only its own
// patterns in the authority, and a load paused for a pattern is resolved by that pattern's owner.
enum class NetworkInterceptOwner : uint8_t {
    Inspector,
    Automation,
};

// A Web Inspector URL filter: a literal URL or the source of a regular expression.
struct InspectorURLPattern {
    String url;
    bool caseSensitive { true };
    bool isRegex { false };

    friend bool operator==(const InspectorURLPattern&, const InspectorURLPattern&) = default;

    WEBCORE_EXPORT bool matches(const URL&) const;
};

// A single URL pattern registered by a consumer. The pattern is stored in its native
// representation, not as wire JSON, so that any protocol front end can produce it and every
// process matches it the same way.
struct InterceptPattern {
    NetworkInterceptOwner owner { NetworkInterceptOwner::Inspector };
    NetworkInterceptionPhase phase { NetworkInterceptionPhase::ResponseStarted };
    Variant<InspectorURLPattern> urlPattern;

    friend bool operator==(const InterceptPattern&, const InterceptPattern&) = default;

    WEBCORE_EXPORT bool matches(const URL&) const;
};

// The process-agnostic network interception configuration of a page: the union of the patterns
// of every consumer. The UI process holds the authoritative copy and pushes it down to every
// WebContent process hosting a frame of the page, including processes created later.
struct NetworkInterception {
    Vector<InterceptPattern> patterns;

    friend bool operator==(const NetworkInterception&, const NetworkInterception&) = default;

    bool isEmpty() const { return patterns.isEmpty(); }

    // Whether any consumer intercepts the URL at any phase.
    WEBCORE_EXPORT bool isConfiguredForURL(const URL&) const;

    // Whether a pattern of the owner matches the URL at the phase.
    WEBCORE_EXPORT bool matches(const URL&, NetworkInterceptionPhase, NetworkInterceptOwner) const;
};

// What the owner of a paused load decided.
enum class NetworkInterceptionAction : uint8_t {
    // Let the load go on, with the edits of the resolution applied.
    Continue,
    // Finish the load with the response and body of the resolution, without the network.
    Fulfill,
    // Fail the load with a network error.
    Fail,
};

// The kind of network error a failed load reports.
enum class NetworkInterceptionErrorType : uint8_t {
    General,
    AccessControl,
    Cancellation,
    Timeout,
};

// The resolution of a paused load, sent by the UI process to the process that paused it. Each
// edit is applied only if it is set.
struct NetworkInterceptionResolution {
    NetworkInterceptionAction action { NetworkInterceptionAction::Continue };
    std::optional<URL> url;
    String method;
    // Replaces every header of the request or response.
    std::optional<Vector<std::pair<String, String>>> headers;
    // The body of the request, or of the response when fulfilling.
    RefPtr<WebCore::SharedBuffer> body;
    std::optional<int> statusCode;
    String reasonPhrase;
    // The MIME type of the response when fulfilling, or when replacing a paused response.
    String mimeType;
    NetworkInterceptionErrorType errorType { NetworkInterceptionErrorType::General };
};

} // namespace Inspector
