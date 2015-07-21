/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef MediaPlaybackTargetPicker_h
#define MediaPlaybackTargetPicker_h

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#include <wtf/Ref.h>

namespace WebCore {

class FloatRect;
class MediaPlaybackTarget;

class MediaPlaybackTargetPicker {
public:
    class Client {
    protected:
        virtual ~Client() { }

    public:
        virtual void setPlaybackTarget(Ref<MediaPlaybackTarget>&&) = 0;
        virtual void externalOutputDeviceAvailableDidChange(bool) = 0;

        void invalidate();
    };

    virtual ~MediaPlaybackTargetPicker();

    virtual void showPlaybackTargetPicker(const FloatRect&, bool checkActiveRoute);
    virtual void startingMonitoringPlaybackTargets();
    virtual void stopMonitoringPlaybackTargets();
    virtual void invalidatePlaybackTargets();

protected:
    explicit MediaPlaybackTargetPicker(Client&);

    Client* m_client;
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#endif // MediaPlaybackTargetPicker_h
