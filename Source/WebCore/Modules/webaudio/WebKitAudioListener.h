/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AudioListener.h"

namespace WebCore {

class WebKitAudioListener final : public AudioListener {
public:
    static Ref<WebKitAudioListener> create(BaseAudioContext& context)
    {
        return adoptRef(*new WebKitAudioListener(context));
    }

    // Velocity
    void setVelocity(float x, float y, float z) { setVelocity(FloatPoint3D(x, y, z)); }
    void setVelocity(const FloatPoint3D& velocity) { m_velocity = velocity; }
    const FloatPoint3D& velocity() const { return m_velocity; }

    // Doppler factor
    void setDopplerFactor(double dopplerFactor) { m_dopplerFactor = dopplerFactor; }
    double dopplerFactor() const { return m_dopplerFactor; }

    // Speed of sound
    void setSpeedOfSound(double speedOfSound) { m_speedOfSound = speedOfSound; }
    double speedOfSound() const { return m_speedOfSound; }

private:
    WebKitAudioListener(BaseAudioContext& context)
        : AudioListener(context)
        , m_velocity(0, 0, 0)
    { }

    bool isWebKitAudioListener() const final { return true; }

    FloatPoint3D m_velocity;
    double m_dopplerFactor { 1.0 };
    double m_speedOfSound { 343.3 };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::WebKitAudioListener)
    static bool isType(const WebCore::AudioListener& listener) { return listener.isWebKitAudioListener(); }
SPECIALIZE_TYPE_TRAITS_END()
