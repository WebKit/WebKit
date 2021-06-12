/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "GPUErrorScopes.h"

#if ENABLE(WEBGPU)

namespace WebCore {

const unsigned maxUncapturedErrorEventsAllowed = 256;

Ref<GPUErrorScopes> GPUErrorScopes::create(UncapturedErrorCallback&& callback)
{
    return adoptRef(*new GPUErrorScopes(WTFMove(callback)));
}

GPUErrorScopes::GPUErrorScopes(UncapturedErrorCallback&& callback)
    : m_uncapturedErrorCallback(WTFMove(callback))
    , m_numUncapturedErrorEventsAllowed(maxUncapturedErrorEventsAllowed)
{
}

void GPUErrorScopes::pushErrorScope(GPUErrorFilter filter)
{
    m_errorScopes.append(ErrorScope { filter, std::nullopt });
}

std::optional<GPUError> GPUErrorScopes::popErrorScope(String& failMessage)
{
    if (m_errorScopes.isEmpty()) {
        failMessage = "No error scope exists!";
        return std::nullopt;
    }

    auto scope = m_errorScopes.takeLast();
    return scope.filter == GPUErrorFilter::None ? std::nullopt : scope.error;
}

void GPUErrorScopes::generateError(const String& message, GPUErrorFilter filter)
{
    auto iterator = std::find_if(m_errorScopes.rbegin(), m_errorScopes.rend(), [filter](const ErrorScope& scope) {
        return scope.filter == GPUErrorFilter::None || scope.filter == filter;
    });

    if (iterator == m_errorScopes.rend()) {
        if (!m_numUncapturedErrorEventsAllowed)
            return;

        m_uncapturedErrorCallback(createError(filter, message));
        if (!(--m_numUncapturedErrorEventsAllowed))
            m_uncapturedErrorCallback(createError(GPUErrorFilter::Validation, "WebGPU: Too many errors; no more error events will fire on this GPUDevice."));

        return;
    }

    // If the scope has already captured an error, ignore this new one.
    if (iterator->error)
        return;

    iterator->error = createError(filter, message);
}

void GPUErrorScopes::generatePrefixedError(const String& message)
{
    generateError(m_prefix + message, GPUErrorFilter::Validation);
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
