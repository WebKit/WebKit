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

#import "config.h"
#import "NetworkProcessProxy.h"

#import "LaunchServicesDatabaseXPCConstants.h"
#import "WebProcessPool.h"
#import "XPCEndpoint.h"

namespace WebKit {

using namespace WebCore;

RefPtr<XPCEventHandler> NetworkProcessProxy::xpcEventHandler() const
{
    return adoptRef(new NetworkProcessProxy::XPCEventHandler(*this));
}

bool NetworkProcessProxy::XPCEventHandler::handleXPCEvent(xpc_object_t event) const
{
    if (!m_networkProcess)
        return false;

    if (!event || xpc_get_type(event) == XPC_TYPE_ERROR)
        return false;

    String messageName = xpc_dictionary_get_string(event, XPCEndpoint::xpcMessageNameKey);
    if (messageName.isEmpty())
        return false;

    if (messageName == LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointMessageName)
        m_networkProcess->processPool().sendNetworkProcessXPCEndpointToWebProcess(event);

    return true;
}

NetworkProcessProxy::XPCEventHandler::XPCEventHandler(const NetworkProcessProxy& networkProcess)
    : m_networkProcess(makeWeakPtr(networkProcess))
{
}

}
