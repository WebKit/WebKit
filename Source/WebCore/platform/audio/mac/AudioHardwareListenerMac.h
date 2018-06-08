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

#ifndef AudioHardwareListenerMac_h
#define AudioHardwareListenerMac_h

#include "AudioHardwareListener.h"
#include <wtf/WeakPtr.h>

#if PLATFORM(MAC)

#include <CoreAudio/AudioHardware.h>

namespace WebCore {

class AudioHardwareListenerMac : public AudioHardwareListener, public CanMakeWeakPtr<AudioHardwareListenerMac> {
public:
    static WTF::Ref<AudioHardwareListenerMac> create(Client&);

private:
    AudioHardwareListenerMac(Client&);
    virtual ~AudioHardwareListenerMac();

    void processIsRunningChanged();
    void outputDeviceChanged();

    void propertyChanged(UInt32, const AudioObjectPropertyAddress[]);

    AudioObjectPropertyListenerBlock m_block;
};

}

#endif // PLATFORM(MAC)

#endif // AudioHardwareListenerMac_h
