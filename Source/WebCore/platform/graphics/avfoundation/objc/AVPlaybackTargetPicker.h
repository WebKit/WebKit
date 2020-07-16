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

#pragma once

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include <wtf/WeakPtr.h>

OBJC_CLASS AVOutputContext;
OBJC_CLASS NSView;

namespace WebCore {

class FloatRect;

class AVPlaybackTargetPicker : public CanMakeWeakPtr<AVPlaybackTargetPicker> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AVPlaybackTargetPicker);
public:
    class Client : public CanMakeWeakPtr<Client> {
    protected:
        virtual ~Client() = default;

    public:
        virtual void pickerWasDismissed() = 0;
        virtual void availableDevicesChanged() = 0;
        virtual void currentDeviceChanged() = 0;
    };

    explicit AVPlaybackTargetPicker(Client& client)
        : m_client(makeWeakPtr(&client))
    {
    }
    virtual ~AVPlaybackTargetPicker() = default;

    virtual void showPlaybackTargetPicker(NSView *, const FloatRect&, bool checkActiveRoute, bool useDarkAppearance) = 0;
    virtual void startingMonitoringPlaybackTargets() = 0;
    virtual void stopMonitoringPlaybackTargets() = 0;
    virtual void invalidatePlaybackTargets() = 0;
    virtual bool externalOutputDeviceAvailable() = 0;

    virtual AVOutputContext* outputContext() = 0;

    WeakPtr<AVPlaybackTargetPicker::Client> client() const { return m_client; }

private:
    WeakPtr<AVPlaybackTargetPicker::Client> m_client;
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
