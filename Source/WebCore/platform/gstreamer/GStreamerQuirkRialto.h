/*
 * Copyright 2024 RDK Management
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

#if USE(GSTREAMER)

#include "GStreamerQuirks.h"

namespace WebCore {

class GStreamerQuirkRialto final : public GStreamerQuirk {
public:
    GStreamerQuirkRialto();
    const char* identifier() final { return "Rialto"; }

    void configureElement(GstElement*, const OptionSet<ElementRuntimeCharacteristics>&) final;
    GstElement* createAudioSink() final;
    GstElement* createWebAudioSink() final;
    std::optional<bool> isHardwareAccelerated(GstElementFactory*) final;
    bool shouldParseIncomingLibWebRTCBitStream() const final { return false; }
    unsigned getAdditionalPlaybinFlags() const { return getGstPlayFlag("text") | getGstPlayFlag("native-audio") | getGstPlayFlag("native-video"); }

private:
    GRefPtr<GstCaps> m_sinkCaps;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
