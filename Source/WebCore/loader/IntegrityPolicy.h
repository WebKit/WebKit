/*
 * Copyright (C) 2025 Shopify Inc. All rights reserved.
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

#include "HTTPHeaderNames.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>


namespace WebCore {

class ResourceResponse;
class ScriptExecutionContext;
class SecurityContext;
struct ResourceLoaderOptions;

// https://w3c.github.io/webappsec-subresource-integrity/#integrity-policy-struct
struct IntegrityPolicy {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(IntegrityPolicy);

public:
    // https://w3c.github.io/webappsec-subresource-integrity/#blocked-destinations
    Vector<String> blockedDestinations;
    // https://w3c.github.io/webappsec-subresource-integrity/#sources
    Vector<String> sources;
    // https://w3c.github.io/webappsec-subresource-integrity/#endpoints
    Vector<String> endpoints;
};

// https://w3c.github.io/webappsec-subresource-integrity/#processing-an-integrity-policy
std::unique_ptr<IntegrityPolicy> processIntegrityPolicy(const ResourceResponse&, HTTPHeaderName);
// https://w3c.github.io/webappsec-subresource-integrity/#should-request-be-blocked-by-integrity-policy-section
bool shouldRequestBeBlockedByIntegrityPolicy(ScriptExecutionContext&, const ResourceLoaderOptions&, const URL&);

} // namespace WebCore

