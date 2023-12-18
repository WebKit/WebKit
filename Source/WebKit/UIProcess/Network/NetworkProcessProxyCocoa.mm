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
#import "NetworkProcessMessages.h"
#import "WebProcessPool.h"
#import "XPCEndpoint.h"

#if PLATFORM(IOS_FAMILY)
#import <UIKit/UIKit.h>
#import <wtf/BlockPtr.h>
#import <wtf/WeakPtr.h>
#endif

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

    auto* messageName = xpc_dictionary_get_string(event, XPCEndpoint::xpcMessageNameKey);
    if (!messageName || !*messageName)
        return false;

    if (LaunchServicesDatabaseXPCConstants::xpcLaunchServicesDatabaseXPCEndpointMessageName == messageName) {
        m_networkProcess->m_endpointMessage = event;
        for (auto& processPool : WebProcessPool::allProcessPools()) {
            for (Ref process : processPool->processes())
                m_networkProcess->sendXPCEndpointToProcess(process);
        }
#if ENABLE(GPU_PROCESS)
        if (auto gpuProcess = GPUProcessProxy::singletonIfCreated())
            m_networkProcess->sendXPCEndpointToProcess(*gpuProcess);
#endif
    }

    return true;
}

NetworkProcessProxy::XPCEventHandler::XPCEventHandler(const NetworkProcessProxy& networkProcess)
    : m_networkProcess(networkProcess)
{
}

bool NetworkProcessProxy::sendXPCEndpointToProcess(AuxiliaryProcessProxy& process)
{
    RELEASE_LOG(Process, "%p - NetworkProcessProxy::sendXPCEndpointToProcess(%p) state = %d has connection = %d XPC endpoint message = %p", this, &process, process.state(), process.hasConnection(), xpcEndpointMessage());

    if (process.state() != AuxiliaryProcessProxy::State::Running)
        return false;
    if (!process.hasConnection())
        return false;
    auto message = xpcEndpointMessage();
    if (!message)
        return false;
    auto xpcConnection = process.connection()->xpcConnection();
    RELEASE_ASSERT(xpcConnection);
    xpc_connection_send_message(xpcConnection, message);
    return true;
}

#if PLATFORM(IOS_FAMILY)

void NetworkProcessProxy::addBackgroundStateObservers()
{
    m_backgroundObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationDidEnterBackgroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }](NSNotification *) {
        if (weakThis)
            weakThis->applicationDidEnterBackground();
    }).get()];
    m_foregroundObserver = [[NSNotificationCenter defaultCenter] addObserverForName:UIApplicationWillEnterForegroundNotification object:[UIApplication sharedApplication] queue:nil usingBlock:makeBlockPtr([weakThis = WeakPtr { *this }](NSNotification *) {
        if (weakThis)
            weakThis->applicationWillEnterForeground();
    }).get()];
}

void NetworkProcessProxy::removeBackgroundStateObservers()
{
    [[NSNotificationCenter defaultCenter] removeObserver:m_backgroundObserver.get()];
    [[NSNotificationCenter defaultCenter] removeObserver:m_foregroundObserver.get()];
}

void NetworkProcessProxy::setBackupExclusionPeriodForTesting(PAL::SessionID sessionID, Seconds period, CompletionHandler<void()>&& completionHandler)
{
    sendWithAsyncReply(Messages::NetworkProcess::SetBackupExclusionPeriodForTesting(sessionID, period), WTFMove(completionHandler));
}

#endif

}
