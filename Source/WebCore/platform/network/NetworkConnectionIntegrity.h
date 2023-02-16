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

namespace WebCore {

enum class NetworkConnectionIntegrity : uint8_t {
    Enabled = 1 << 0,
    HTTPSFirst = 1 << 1,
    HTTPSOnly = 1 << 2,
    HTTPSOnlyExplicitlyBypassedForDomain = 1 << 3,
    FailClosed = 1 << 4,
    WebSearchContent = 1 << 5,
    EnhancedTelemetry = 1 << 6,
    RequestValidation = 1 << 7,
};

}

namespace WTF {

template<> struct EnumTraits<WebCore::NetworkConnectionIntegrity> {
    using values = EnumValues<
        WebCore::NetworkConnectionIntegrity,
        WebCore::NetworkConnectionIntegrity::Enabled,
        WebCore::NetworkConnectionIntegrity::HTTPSFirst,
        WebCore::NetworkConnectionIntegrity::HTTPSOnly,
        WebCore::NetworkConnectionIntegrity::HTTPSOnlyExplicitlyBypassedForDomain,
        WebCore::NetworkConnectionIntegrity::FailClosed,
        WebCore::NetworkConnectionIntegrity::WebSearchContent,
        WebCore::NetworkConnectionIntegrity::EnhancedTelemetry,
        WebCore::NetworkConnectionIntegrity::RequestValidation
    >;
};

} // namespace WTF
