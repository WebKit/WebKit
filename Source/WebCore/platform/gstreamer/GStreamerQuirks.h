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
#include <wtf/Noncopyable.h>
#include <wtf/Nonmovable.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaPlayerPrivateGStreamer;

enum class ElementRuntimeCharacteristics : uint8_t {
    IsMediaStream = 1 << 0,
    HasVideo = 1 << 1,
    HasAudio = 1 << 2,
    IsLiveStream = 1 << 3,
};

class GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirkBase);

public:
    GStreamerQuirkBase() = default;
    virtual ~GStreamerQuirkBase() = default;

    virtual const ASCIILiteral identifier() const = 0;

    // Interface of classes supplied to MediaPlayerPrivateGStreamer to store values that the quirks will need for their job.
    class GStreamerQuirkState {
        WTF_DEPRECATED_MAKE_FAST_ALLOCATED(GStreamerQuirkState);
        // Prevent accidental https://en.wikipedia.org/wiki/Object_slicing.
        WTF_MAKE_NONCOPYABLE(GStreamerQuirkState);
        WTF_MAKE_NONMOVABLE(GStreamerQuirkState);
    public:
        GStreamerQuirkState()
        {
        }
        virtual ~GStreamerQuirkState() = default;
    };
};

class GStreamerQuirk : public GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirk);
public:
    GStreamerQuirk() = default;
    virtual ~GStreamerQuirk() = default;

    virtual bool isPlatformSupported() const = 0;
    virtual GstElement* createAudioSink() { return nullptr; }
    virtual GstElement* createWebAudioSink() { return nullptr; }
    virtual void configureElement(GstElement*, const OptionSet<ElementRuntimeCharacteristics>&) { }
    virtual std::optional<bool> isHardwareAccelerated(GstElementFactory*) { return std::nullopt; }
    virtual std::optional<GstElementFactoryListType> audioVideoDecoderFactoryListType() const { return std::nullopt; }
    virtual Vector<String> disallowedWebAudioDecoders() const { return { }; }
    virtual unsigned getAdditionalPlaybinFlags() const { return 0; }
    virtual bool shouldParseIncomingLibWebRTCBitStream() const { return true; }

    virtual bool needsBufferingPercentageCorrection() const { return false; }
    // Returns name of the queried GstElement, or nullptr if no element was queried.
    virtual ASCIILiteral queryBufferingPercentage(MediaPlayerPrivateGStreamer*, const GRefPtr<GstQuery>&) const { return nullptr; }
    virtual int correctBufferingPercentage(MediaPlayerPrivateGStreamer*, int originalBufferingPercentage, GstBufferingMode) const { return originalBufferingPercentage; }
    virtual void resetBufferingPercentage(MediaPlayerPrivateGStreamer*, int) const { };
    virtual void setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer*, GstState, GstState, GRefPtr<GstElement>&&) const { }
    virtual bool needsCustomInstantRateChange() const { return false; }
    // Note: Change these pairs and single return types as needed, or even use a struct, if more return fields
    // have to be added in the future.
    // Returning processed and didInstantRateChange.
    virtual std::pair<bool, bool> applyCustomInstantRateChange(
        bool isPipelinePlaying, bool isPipelineWaitingPreroll, float playbackRate, bool mute, GstElement* pipeline) const;
    // Returning forwardToAllPads.
    virtual bool analyzeWebKitMediaSrcCustomEvent(GRefPtr<GstEvent>) const;
    // Returning rate.
    virtual std::optional<double> processWebKitMediaSrcCustomEvent(GRefPtr<GstEvent>, bool handledByAnyStream, bool handledByAllTheStreams) const;

    // Subclass must return true if it wants to override the default behaviour of sibling platforms.
    virtual bool processWebAudioSilentBuffer(GstBuffer* buffer) const
    {
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_GAP);
        GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DROPPABLE);
        return false;
    }

    [[nodiscard]] virtual GRefPtr<GstCaps> videoSinkGLCapsFormat() const { return nullptr;  }
};

class GStreamerHolePunchQuirk : public GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerHolePunchQuirk);
public:
    GStreamerHolePunchQuirk() = default;
    virtual ~GStreamerHolePunchQuirk() = default;

    virtual GstElement* createHolePunchVideoSink(bool, const MediaPlayer*) { return nullptr; }
    virtual bool setHolePunchVideoRectangle(GstElement*, const IntRect&) { return false; }
    virtual bool requiresClockSynchronization() const { return true; }
};

class GStreamerQuirksManager : public RefCounted<GStreamerQuirksManager> {
    friend NeverDestroyed<GStreamerQuirksManager>;
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirksManager);

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

    [[nodiscard]] GRefPtr<GstCaps> videoSinkGLCapsFormat() const;

    bool supportsVideoHolePunchRendering() const;
    GstElement* createHolePunchVideoSink(bool isLegacyPlaybin, const MediaPlayer*);
    void setHolePunchVideoRectangle(GstElement*, const IntRect&);
    bool sinksRequireClockSynchronization() const;

    void setHolePunchEnabledForTesting(bool);

    unsigned getAdditionalPlaybinFlags() const;

    bool shouldParseIncomingLibWebRTCBitStream() const;

    bool needsBufferingPercentageCorrection() const;
    // Returns name of the queried GstElement, or nullptr if no element was queried.
    ASCIILiteral queryBufferingPercentage(MediaPlayerPrivateGStreamer*, const GRefPtr<GstQuery>&) const;
    int correctBufferingPercentage(MediaPlayerPrivateGStreamer*, int originalBufferingPercentage, GstBufferingMode) const;
    void resetBufferingPercentage(MediaPlayerPrivateGStreamer*, int bufferingPercentage) const;
    void setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer*, GstState currentState, GstState newState, GRefPtr<GstElement>&&) const;

    void processWebAudioSilentBuffer(GstBuffer*) const;

    bool needsCustomInstantRateChange() const;
    // Returning processed and didInstantRateChange.
    std::pair<bool, bool> applyCustomInstantRateChange(
        bool isPipelinePlaying, bool isPipelineWaitingPreroll, float playbackRate, bool mute, GstElement* pipeline) const;
    bool processCustomInstantRateChangeEvent() const;
    // Returning forwardToAllPads.
    bool analyzeWebKitMediaSrcCustomEvent(GRefPtr<GstEvent>) const;
    // Returning rate.
    std::optional<double> processWebKitMediaSrcCustomEvent(GRefPtr<GstEvent>, bool handledByAnyStream, bool handledByAllStreams) const;

private:
    GStreamerQuirksManager(bool, bool);

    Vector<std::unique_ptr<GStreamerQuirk>> m_quirks;
    std::unique_ptr<GStreamerHolePunchQuirk> m_holePunchQuirk;
    bool m_isForTesting { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER)
