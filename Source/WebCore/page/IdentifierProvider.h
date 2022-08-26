/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <JavaScriptCore/Uint8ClampedArray.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

// FIXME: This is canvas-specific. Add abstract class to generalize to other JS APIs.
class IdentifierProvider final : public ThreadSafeRefCounted<IdentifierProvider> {
public:
    static Ref<IdentifierProvider> create() { return adoptRef(*new IdentifierProvider()); }

    IdentifierProvider() = default;

    void setCanvasIdentifier(const JSC::Uint8ClampedArray&);
    void setDomainIdentifier(const unsigned, const uint64_t);
    
    bool shouldUseNoisyAPIs() { return m_shouldUseNoisyAPIs; }
    void setShouldUseNoisyAPIs(bool shouldUseNoisyAPIs) { m_shouldUseNoisyAPIs = shouldUseNoisyAPIs; }

    unsigned domainIdentifier() const { return m_domainIdentifier; }
    unsigned canvasIdentifier() const { return m_canvasIdentifier; }

    virtual ~IdentifierProvider() { }
private:
    uint64_t m_domainIdentifier { 0 };
    uint64_t m_canvasIdentifier { 0 };
    bool m_shouldUseNoisyAPIs { false };
};

} // namespace WebCore
