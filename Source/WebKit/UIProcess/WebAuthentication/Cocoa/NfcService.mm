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

#import "config.h"
#import "NfcService.h"

#if ENABLE(WEB_AUTHN)

#import "CtapNfcDriver.h"
#import "NearFieldSPI.h"
#import "NfcConnection.h"
#import <wtf/BlockPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>

#import "NearFieldSoftLink.h"

namespace WebKit {

NfcService::NfcService(Observer& observer)
    : FidoService(observer)
{
}

NfcService::~NfcService()
{
}

void NfcService::didConnectTag()
{
#if HAVE(NEAR_FIELD)
    getInfo(WTFMove(m_driver));
#endif
}

#if HAVE(NEAR_FIELD)
void NfcService::setDriver(std::unique_ptr<CtapNfcDriver>&& driver)
{
    m_driver = WTFMove(driver);
}
#endif

void NfcService::startDiscoveryInternal()
{
    platformStartDiscovery();
}

void NfcService::platformStartDiscovery()
{
#if HAVE(NEAR_FIELD)
    if (![[getNFHardwareManagerClass() sharedHardwareManager] areFeaturesSupported:NFFeatureReaderMode outError:nil])
        return;

    // Will be executed in a different thread.
    auto callback = makeBlockPtr([weakThis = makeWeakPtr(*this), this] (NFReaderSession *session, NSError *error) mutable {
        ASSERT(!RunLoop::isMain());
        if (error) {
            LOG_ERROR("Couldn't start a NFC reader session: %@", error);
            return;
        }

        RunLoop::main().dispatch([weakThis = WTFMove(weakThis), this, session = retainPtr(session)] () mutable {
            if (!weakThis) {
                [session endSession];
                return;
            }

            // CtapNfcDriver and NfcConnection will take care of polling tags and connecting to them.
            m_driver = WTF::makeUnique<CtapNfcDriver>(makeUniqueRef<NfcConnection>(WTFMove(session), *this));
        });
    });
    [[getNFHardwareManagerClass() sharedHardwareManager] startReaderSessionWithActionSheetUI:callback.get()];
#endif // HAVE(NEAR_FIELD)
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
