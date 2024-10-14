/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "WebPushDaemonConnection.h"

#if ENABLE(WEB_PUSH_NOTIFICATIONS)

#include "DaemonDecoder.h"
#include "DaemonEncoder.h"
#include "NetworkSession.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit::WebPushD {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Connection);

Ref<Connection> Connection::create(CString&& machServiceName, WebPushDaemonConnectionConfiguration&& configuration)
{
    return adoptRef(*new Connection(WTFMove(machServiceName), WTFMove(configuration)));
}

Connection::Connection(CString&& machServiceName, WebPushDaemonConnectionConfiguration&& configuration)
    : Daemon::ConnectionToMachService<ConnectionTraits>(WTFMove(machServiceName))
    , m_configuration(WTFMove(configuration))
{
    LOG(Push, "Creating WebPushD connection to mach service: %s", this->machServiceName().data());
}

} // namespace WebKit::WebPushD

#endif // ENABLE(WEB_PUSH_NOTIFICATIONS)
