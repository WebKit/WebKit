/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "PowerSourceNotifier.h"

#import "SystemBattery.h"
#import <notify.h>
#import <pal/spi/cocoa/IOPSLibSPI.h>

namespace WebCore {

PowerSourceNotifier::PowerSourceNotifier(PowerSourceNotifierCallback&& callback)
    : m_callback(WTFMove(callback))
{
    int token = 0;
    auto status = notify_register_dispatch(kIOPSNotifyPowerSource, &token, dispatch_get_main_queue(), [weakThis = makeWeakPtr(*this)] (int) {
        if (weakThis)
            weakThis->notifyPowerSourceChanged();
    });
    if (status == NOTIFY_STATUS_OK)
        m_tokenID = token;

    // If the current value of systemHasAC() is uncached, force a notification.
    if (!cachedSystemHasAC()) {
        dispatch_async(dispatch_get_main_queue(), [weakThis = makeWeakPtr(*this)] {
            if (weakThis)
                weakThis->notifyPowerSourceChanged();
        });
    }
}

PowerSourceNotifier::~PowerSourceNotifier()
{
    if (m_tokenID)
        notify_cancel(*m_tokenID);
}

void PowerSourceNotifier::notifyPowerSourceChanged()
{
    resetSystemHasAC();
    if (m_callback)
        m_callback(systemHasAC());
}

}
