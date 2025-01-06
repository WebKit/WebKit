/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include <wtf/Compiler.h>

#include <WebCore/LibWebRTCMacros.h>

WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/api/async_dns_resolver.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END

#include <wtf/Function.h>
#include <wtf/TZoneMalloc.h>

namespace WebKit {

class LibWebRTCDnsResolverFactory final : public webrtc::AsyncDnsResolverFactoryInterface {
    WTF_MAKE_TZONE_ALLOCATED(LibWebRTCDnsResolverFactory);
public:
    class Resolver : public webrtc::AsyncDnsResolverInterface {
    public:
        virtual void start(const rtc::SocketAddress&, Function<void()>&&) = 0;

    private:
        // webrtc::AsyncDnsResolverInterface
        void Start(const rtc::SocketAddress&, absl::AnyInvocable<void()>) final;
        void Start(const rtc::SocketAddress&, int family, absl::AnyInvocable<void()>) final;
    };

private:
    std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(const rtc::SocketAddress&, absl::AnyInvocable<void()>) final;
    std::unique_ptr<webrtc::AsyncDnsResolverInterface> CreateAndResolve(const rtc::SocketAddress&, int family, absl::AnyInvocable<void()>) final;
    std::unique_ptr<webrtc::AsyncDnsResolverInterface> Create() final;
};

} // namespace WebKit

#endif // USE(LIBWEBRTC)
