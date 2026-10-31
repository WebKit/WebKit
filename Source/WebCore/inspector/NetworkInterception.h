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

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace Inspector {

// The phase of a network load at which an intercept pattern takes effect.
//
// AuthRequired is representable so that a client's configuration round-trips
// unchanged, but no phase-specific handling is serviced for it yet.
enum class NetworkInterceptionPhase : uint8_t {
    BeforeRequestSent,
    ResponseStarted,
    AuthRequired,
};

// A single URL pattern registered by a client. The pattern is stored in its
// native representation (a literal URL or a regular expression source), not as
// wire JSON, so that any protocol front end can produce and consume it.
struct InterceptPattern {
    String urlPattern;
    bool caseSensitive { true };
    bool isRegex { false };
    NetworkInterceptionPhase phase { NetworkInterceptionPhase::ResponseStarted };

    friend bool operator==(const InterceptPattern&, const InterceptPattern&) = default;

    WEBCORE_EXPORT bool matches(const String& url) const;
};

// The process-agnostic network interception configuration. The UIProcess holds
// the authoritative copy for a page and pushes it down to every WebContent
// process hosting a frame of that page; each of those processes stores the copy
// it receives. Configuration is union-additive: adding a pattern that is already
// present is a no-op.
struct NetworkInterception {
    bool enabled { false };
    Vector<InterceptPattern> patterns;

    friend bool operator==(const NetworkInterception&, const NetworkInterception&) = default;

    bool isEmpty() const { return !enabled && patterns.isEmpty(); }

    // Union-additive: returns true if the configuration changed.
    bool add(const InterceptPattern& pattern)
    {
        if (patterns.contains(pattern))
            return false;
        patterns.append(pattern);
        return true;
    }

    // Returns true if the configuration changed.
    bool remove(const InterceptPattern& pattern)
    {
        return patterns.removeFirst(pattern);
    }

    WEBCORE_EXPORT bool isConfiguredForURL(const String& url) const;
};

} // namespace Inspector
