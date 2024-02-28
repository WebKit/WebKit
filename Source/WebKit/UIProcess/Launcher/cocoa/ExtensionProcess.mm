/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ExtensionProcess.h"

#if USE(EXTENSIONKIT)
#import "AssertionCapability.h"

#import <BrowserEngineKit/BrowserEngineKit.h>

namespace WebKit {

WTF_MAKE_WK_TZONE_ALLOCATED_IMPL(ExtensionProcess);

ExtensionProcess::ExtensionProcess(BEWebContentProcess *process)
    : m_process(process)
{
}

ExtensionProcess::ExtensionProcess(BENetworkingProcess *process)
    : m_process(process)
{
}

ExtensionProcess::ExtensionProcess(BERenderingProcess *process)
    : m_process(process)
{
}

void ExtensionProcess::invalidate() const
{
    WTF::switchOn(m_process, [&] (auto& process) {
        [process invalidate];
    });
}

OSObjectPtr<xpc_connection_t> ExtensionProcess::makeLibXPCConnection() const
{
    NSError *error = nil;
    OSObjectPtr<xpc_connection_t> xpcConnection;
    WTF::switchOn(m_process, [&] (auto& process) {
        xpcConnection = [process makeLibXPCConnectionError:&error];
    });
    return xpcConnection;
}

RetainPtr<BEProcessCapabilityGrant> ExtensionProcess::grantCapability(BEProcessCapability *capability) const
{
    NSError *error = nil;
    RetainPtr<BEProcessCapabilityGrant> grant;
    WTF::switchOn(m_process, [&] (auto& process) {
        grant = [process grantCapability:capability error:&error];
    });
    return grant;
}

RetainPtr<UIInteraction> ExtensionProcess::createVisibilityPropagationInteraction() const
{
    RetainPtr<UIInteraction> interaction;
    WTF::switchOn(m_process, [&] (RetainPtr<BEWebContentProcess> process) {
        interaction = [process createVisibilityPropagationInteraction];
    }, [&] (RetainPtr<BENetworkingProcess> process) {
    }, [&] (RetainPtr<BERenderingProcess> process) {
        interaction = [process createVisibilityPropagationInteraction];
    });
    return interaction;
}

} // namespace WebKit

#endif
