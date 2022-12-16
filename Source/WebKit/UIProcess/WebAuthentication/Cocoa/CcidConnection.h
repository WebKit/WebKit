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

#if ENABLE(WEB_AUTHN)

#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS TKSmartCard;

namespace WebKit {

class CcidService;
using DataReceivedCallback = Function<void(Vector<uint8_t>&&)>;

class CcidConnection : public RefCounted<CcidConnection>, public CanMakeWeakPtr<CcidConnection> {
public:
    static Ref<CcidConnection> create(RetainPtr<TKSmartCard>&&, CcidService&);
    ~CcidConnection();

    void transact(Vector<uint8_t>&& data, DataReceivedCallback&&) const;
    void stop() const;
    bool contactless() const { return m_contactless; };

private:
    CcidConnection(RetainPtr<TKSmartCard>&&, CcidService&);

    void restartPolling();
    void startPolling();

    void detectContactless();

    void trySelectFidoApplet();

    RetainPtr<TKSmartCard> m_smartCard;
    WeakPtr<CcidService> m_service;
    RunLoop::Timer m_retryTimer;
    bool m_contactless { false };
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
