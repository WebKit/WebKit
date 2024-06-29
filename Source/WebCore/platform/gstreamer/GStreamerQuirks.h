/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "MediaPlayer.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

enum class ElementRuntimeCharacteristics : uint8_t {
    IsMediaStream = 1 << 0,
    HasVideo = 1 << 1,
    HasAudio = 1 << 2,
    IsLiveStream = 1 << 3,
};

class GStreamerQuirkBase {
    WTF_MAKE_FAST_ALLOCATED;

public:
    GStreamerQuirkBase() = default;
    virtual ~GStreamerQuirkBase() = default;

    virtual const char* identifier() = 0;
};

class GStreamerQuirk : public GStreamerQuirkBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GStreamerQuirk() = default;
    virtual ~GStreamerQuirk() = default;

    virtual bool isPlatformSupported() const { return true; }
    virtual GstElement* createAudioSink() { return nullptr; }
    virtual GstElement* createWebAudioSink() { return nullptr; }
    virtual void configureElement(GstElement*, const OptionSet<ElementRuntimeCharacteristics>&) { }
    virtual std::optional<bool> isHardwareAccelerated(GstElementFactory*) { return std::nullopt; }
    virtual std::optional<GstElementFactoryListType> audioVideoDecoderFactoryListType() const { return std::nullopt; }
    virtual Vector<String> disallowedWebAudioDecoders() const { return { }; }
    virtual unsigned getAdditionalPlaybinFlags() const { return 0; }
    virtual bool shouldParseIncomingLibWebRTCBitStream() const { return true; }
};

class GStreamerHolePunchQuirk : public GStreamerQuirkBase {
    WTF_MAKE_FAST_ALLOCATED;
public:
    GStreamerHolePunchQuirk() = default;
    virtual ~GStreamerHolePunchQuirk() = default;

    virtual GstElement* createHolePunchVideoSink(bool, const MediaPlayer*) { return nullptr; }
    virtual bool setHolePunchVideoRectangle(GstElement*, const IntRect&) { return false; }
    virtual bool requiresClockSynchronization() const { return true; }
};

class GStreamerQuirksManager : public RefCounted<GStreamerQuirksManager> {
    friend NeverDestroyed<GStreamerQuirksManager>;
    WTF_MAKE_FAST_ALLOCATED;

public:
    static GStreamerQuirksManager& singleton();

    static RefPtr<GStreamerQuirksManager> createForTesting()
    {
        return adoptRef(*new GStreamerQuirksManager(true, false));
    }

    bool isEnabled() const;

    GstElement* createAudioSink();
    GstElement* createWebAudioSink();
    void configureElement(GstElement*, OptionSet<ElementRuntimeCharacteristics>&&);
    std::optional<bool> isHardwareAccelerated(GstElementFactory*) const;
    GstElementFactoryListType audioVideoDecoderFactoryListType() const;
    Vector<String> disallowedWebAudioDecoders() const;

    bool supportsVideoHolePunchRendering() const;
    GstElement* createHolePunchVideoSink(bool isLegacyPlaybin, const MediaPlayer*);
    void setHolePunchVideoRectangle(GstElement*, const IntRect&);
    bool sinksRequireClockSynchronization() const;

    void setHolePunchEnabledForTesting(bool);

    unsigned getAdditionalPlaybinFlags() const;

    bool shouldParseIncomingLibWebRTCBitStream() const;

private:
    GStreamerQuirksManager(bool, bool);

    Vector<std::unique_ptr<GStreamerQuirk>> m_quirks;
    std::unique_ptr<GStreamerHolePunchQuirk> m_holePunchQuirk;
    bool m_isForTesting { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER)
