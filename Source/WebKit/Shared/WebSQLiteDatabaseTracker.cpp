/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "WebSQLiteDatabaseTracker.h"

#include "NetworkProcess.h"
#include "NetworkProcessProxyMessages.h"
#include "WebProcess.h"
#include "WebProcessProxyMessages.h"
#include <WebCore/SQLiteDatabaseTracker.h>
#include <wtf/MainThread.h>

namespace WebKit {
using namespace WebCore;

WebSQLiteDatabaseTracker::WebSQLiteDatabaseTracker(WebProcess& process)
    : m_process(process)
    , m_processType(AuxiliaryProcess::ProcessType::WebContent)
    , m_hysteresis([this](PAL::HysteresisState state) { hysteresisUpdated(state); })
{
    SQLiteDatabaseTracker::setClient(this);
}

WebSQLiteDatabaseTracker::WebSQLiteDatabaseTracker(NetworkProcess& process)
    : m_process(process)
    , m_processType(AuxiliaryProcess::ProcessType::Network)
    , m_hysteresis([this](PAL::HysteresisState state) { hysteresisUpdated(state); })
{
    SQLiteDatabaseTracker::setClient(this);
}

void WebSQLiteDatabaseTracker::willBeginFirstTransaction()
{
    callOnMainThread([this] {
        m_hysteresis.start();
    });
}

void WebSQLiteDatabaseTracker::didFinishLastTransaction()
{
    callOnMainThread([this] {
        m_hysteresis.stop();
    });
}

void WebSQLiteDatabaseTracker::hysteresisUpdated(PAL::HysteresisState state)
{
    switch (m_processType) {
    case AuxiliaryProcess::ProcessType::WebContent:
        m_process.parentProcessConnection()->send(Messages::WebProcessProxy::SetIsHoldingLockedFiles(state == PAL::HysteresisState::Started), 0);
        break;
    case AuxiliaryProcess::ProcessType::Network:
        m_process.parentProcessConnection()->send(Messages::NetworkProcessProxy::SetIsHoldingLockedFiles(state == PAL::HysteresisState::Started), 0);
        break;
    default:
        ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
