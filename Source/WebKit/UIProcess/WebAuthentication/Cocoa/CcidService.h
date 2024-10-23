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

#include "FidoService.h"
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS NSArray;
OBJC_CLASS TKSmartCardSlot;
OBJC_CLASS TKSmartCard;
OBJC_CLASS _WKSmartCardSlotObserver;
OBJC_CLASS _WKSmartCardSlotStateObserver;

namespace WebKit {

class CcidConnection;

class CcidService : public FidoService {
public:
    static Ref<CcidService> create(AuthenticatorTransportServiceObserver&);
    ~CcidService();

    static bool isAvailable();

    void didConnectTag();

    void updateSlots(NSArray *slots);
    void onValidCard(RetainPtr<TKSmartCard>&&);

protected:
    explicit CcidService(AuthenticatorTransportServiceObserver&);

private:
    void startDiscoveryInternal() final;
    void restartDiscoveryInternal() final;

    void removeObservers();

    virtual void platformStartDiscovery();

    RunLoop::Timer m_restartTimer;
    RefPtr<CcidConnection> m_connection;
    RetainPtr<_WKSmartCardSlotObserver> m_slotsObserver;
    HashMap<String, RetainPtr<_WKSmartCardSlotStateObserver>> m_slotObservers;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
