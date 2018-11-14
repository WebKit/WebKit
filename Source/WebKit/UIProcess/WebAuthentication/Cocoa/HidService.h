/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include "AuthenticatorTransportService.h"
#include <IOKit/hid/IOHIDManager.h>
#include <wtf/UniqueRef.h>

namespace WebKit {

class CtapHidDriver;
class HidConnection;

class HidService : public AuthenticatorTransportService {
public:
    explicit HidService(Observer&);
    ~HidService();

    void deviceAdded(IOHIDDeviceRef);

private:
    void startDiscoveryInternal() final;

    // Overrided by MockHidService.
    virtual void platformStartDiscovery();
    virtual UniqueRef<HidConnection> createHidConnection(IOHIDDeviceRef) const;

    void continueAddDeviceAfterGetInfo(CtapHidDriver* deviceRef, Vector<uint8_t>&& info);

    RetainPtr<IOHIDManagerRef> m_manager;
    // Keeping drivers alive when they are initializing authenticators.
    HashSet<std::unique_ptr<CtapHidDriver>> m_drivers;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
