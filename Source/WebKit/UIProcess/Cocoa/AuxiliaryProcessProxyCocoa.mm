/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "AuxiliaryProcessProxy.h"

#import <WebCore/SharedBuffer.h>
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>

#if USE(RUNNINGBOARD)
#include "XPCConnectionTerminationWatchdog.h"
#endif

#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebKit {

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
RefPtr<WebCore::SharedBuffer> AuxiliaryProcessProxy::fetchAudioComponentServerRegistrations()
{
    using namespace PAL;

    CFDataRef registrations { nullptr };

    if (!PAL::isAudioToolboxCoreFrameworkAvailable() || !PAL::canLoad_AudioToolboxCore_AudioComponentFetchServerRegistrations())
        return nullptr;
    
    WebCore::registerOpusDecoderIfNeeded();
    WebCore::registerVorbisDecoderIfNeeded();

    if (noErr != AudioComponentFetchServerRegistrations(&registrations) || !registrations)
        return nullptr;

    return WebCore::SharedBuffer::create(adoptCF(registrations).get());
}
#endif

Vector<String> AuxiliaryProcessProxy::platformOverrideLanguages() const
{
    static const NeverDestroyed<Vector<String>> overrideLanguages = makeVector<String>([[NSUserDefaults standardUserDefaults] valueForKey:@"AppleLanguages"]);
    return overrideLanguages;
}

void AuxiliaryProcessProxy::platformStartConnectionTerminationWatchdog()
{
#if USE(RUNNINGBOARD)
    // Deploy a watchdog in the UI process, since the child process may be suspended.
    // If 30s is insufficient for any outstanding activity to complete cleanly, then it will be killed.
    ASSERT(m_connection && m_connection->xpcConnection());
    XPCConnectionTerminationWatchdog::startConnectionTerminationWatchdog(m_connection->xpcConnection(), 30_s);
#endif
}

#if USE(EXTENSIONKIT)
RetainPtr<_SEExtensionProcess> AuxiliaryProcessProxy::extensionProcess() const
{
    if (!m_processLauncher)
        return nullptr;
    return m_processLauncher->extensionProcess();
}
#endif

void AuxiliaryProcessProxy::platformGetLaunchOptions(ProcessLauncher::LaunchOptions& launchOptions)
{
#if USE(EXTENSIONKIT)
    launchOptions.launchAsExtensions = s_manageProcessesAsExtensions;
#else
    UNUSED_PARAM(launchOptions);
#endif
}

} // namespace WebKit
