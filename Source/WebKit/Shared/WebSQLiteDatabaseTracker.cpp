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

namespace WebKit {
using namespace WebCore;

WebSQLiteDatabaseTracker::WebSQLiteDatabaseTracker(IsHoldingLockedFilesHandler&& isHoldingLockedFilesHandler)
    : m_isHoldingLockedFilesHandler(WTFMove(isHoldingLockedFilesHandler))
    , m_hysteresis([this](PAL::HysteresisState state) { setIsHoldingLockedFiles(state == PAL::HysteresisState::Started); })
{
    ASSERT(RunLoop::isMain());
    SQLiteDatabaseTracker::setClient(this);
}

WebSQLiteDatabaseTracker::~WebSQLiteDatabaseTracker()
{
    ASSERT(RunLoop::isMain());
    SQLiteDatabaseTracker::setClient(nullptr);

    if (m_hysteresis.state() == PAL::HysteresisState::Started)
        setIsHoldingLockedFiles(false);
}

void WebSQLiteDatabaseTracker::setIsSuspended(bool isSuspended)
{
    ASSERT(RunLoop::isMain());
    m_isSuspended = isSuspended;
}

void WebSQLiteDatabaseTracker::willBeginFirstTransaction()
{
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->m_hysteresis.start();
    });
}

void WebSQLiteDatabaseTracker::didFinishLastTransaction()
{
    RunLoop::main().dispatch([weakThis = makeWeakPtr(*this)] {
        if (weakThis)
            weakThis->m_hysteresis.stop();
    });
}

void WebSQLiteDatabaseTracker::setIsHoldingLockedFiles(bool isHoldingLockedFiles)
{
    ASSERT(RunLoop::isMain());

    if (m_isSuspended)
        return;

    m_isHoldingLockedFilesHandler(isHoldingLockedFiles);
}

} // namespace WebKit
