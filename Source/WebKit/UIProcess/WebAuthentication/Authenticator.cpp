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

#include "config.h"
#include "Authenticator.h"

#if ENABLE(WEB_AUTHN)

#include <wtf/RunLoop.h>

namespace WebKit {

void Authenticator::handleRequest(const WebAuthenticationRequestData& data)
{
    m_pendingRequestData = data;
    // Enforce asynchronous execution of makeCredential/getAssertion.
    RunLoop::main().dispatch([weakThis = WeakPtr { *this }, this] {
        if (!weakThis)
            return;
        if (std::holds_alternative<WebCore::PublicKeyCredentialCreationOptions>(m_pendingRequestData.options))
            makeCredential();
        else
            getAssertion();
    });
}

void Authenticator::receiveRespond(Respond&& respond) const
{
    ASSERT(RunLoop::isMain());
    if (!m_observer)
        return;
    m_observer->respondReceived(WTFMove(respond));
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
