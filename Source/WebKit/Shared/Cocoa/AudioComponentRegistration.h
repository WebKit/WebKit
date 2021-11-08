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

#pragma once

#import "AuxiliaryProcessProxy.h"
#import "DataReference.h"
#import <WebCore/WebMAudioUtilitiesCocoa.h>
#import <wtf/BlockPtr.h>
#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebKit {

template<typename T> void sendAudioComponentRegistrations(AuxiliaryProcessProxy& process)
{
    if (!PAL::isAudioToolboxCoreFrameworkAvailable() || !PAL::canLoad_AudioToolboxCore_AudioComponentFetchServerRegistrations())
        return;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), makeBlockPtr([weakThis = WeakPtr { process }] () mutable {
        CFDataRef registrations { nullptr };

        WebCore::registerOpusDecoderIfNeeded();
        WebCore::registerVorbisDecoderIfNeeded();
        if (noErr != PAL::AudioComponentFetchServerRegistrations(&registrations) || !registrations)
            return;

        RunLoop::main().dispatch([weakThis = WTFMove(weakThis), registrations = adoptCF(registrations)] () mutable {
            if (!weakThis)
                return;
            auto registrationData = WebCore::SharedBuffer::create(registrations.get());
            weakThis->send(T({ registrationData }), 0);
        });
    }).get());
}

void consumeAudioComponentRegistrations(const IPC::DataReference&);

} // namespace WebKit
