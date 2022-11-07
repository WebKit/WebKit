/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "Image.h"
#include "MediaConstraints.h"
#include "MediaDeviceHashSalts.h"
#include "PlatformLayer.h"
#include "RealtimeMediaSourceCapabilities.h"
#include "RealtimeMediaSourceFactory.h"
#include "VideoFrame.h"
#include "VideoFrameTimeMetadata.h"
#include <wtf/CompletionHandler.h>
#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

#if USE(GSTREAMER)
#include <wtf/glib/GRefPtr.h>

typedef struct _GstEvent GstEvent;
#endif

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class FloatRect;
class GraphicsContext;
class MediaStreamPrivate;
class OrientationNotifier;
class PlatformAudioData;
class RealtimeMediaSourceSettings;

struct CaptureSourceOrError;

class WEBCORE_EXPORT RealtimeMediaSource
    : public ThreadSafeRefCounted<RealtimeMediaSource, WTF::DestructionThread::MainRunLoop>
    , public CanMakeWeakPtr<RealtimeMediaSource>
#if !RELEASE_LOG_DISABLED
    , public LoggerHelper
#endif
{
public:
    class Observer : public CanMakeWeakPtr<Observer> {
    public:
        virtual ~Observer();

        // Source state changes.
        virtual void sourceStarted() { }
        virtual void sourceStopped() { }
        virtual void sourceMutedChanged() { }
        virtual void sourceSettingsChanged() { }
        virtual void audioUnitWillStart() { }
        virtual void sourceConfigurationChanged() { }

        // Observer state queries.
        virtual bool preventSourceFromStopping() { return false; }

        virtual void hasStartedProducingData() { }

#if USE(GSTREAMER)
        virtual void handleDownstreamEvent(GRefPtr<GstEvent>&&) { }
#endif
    };
    class AudioSampleObserver {
    public:
        virtual ~AudioSampleObserver() = default;

        // May be called on a background thread.
        virtual void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t /*numberOfFrames*/) = 0;
    };
    class VideoFrameObserver {
    public:
        virtual ~VideoFrameObserver() = default;

        // May be called on a background thread.
        virtual void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) = 0;

#if USE(GSTREAMER_WEBRTC)
        virtual std::optional<uint64_t> queryDecodedVideoFramesCount() { return std::nullopt; }
#endif
    };

    virtual ~RealtimeMediaSource() = default;

    virtual Ref<RealtimeMediaSource> clone() { return *this; }

    const AtomString& hashedId() const;
    const MediaDeviceHashSalts& deviceIDHashSalts() const;

    const String& persistentID() const { return m_device.persistentId(); }

    enum class Type : bool { Audio, Video };
    Type type() const { return m_type; }
    bool isVideo() const { return m_type == Type::Video; }
    bool isAudio() const { return m_type == Type::Audio; }

    virtual void whenReady(CompletionHandler<void(String)>&&);

    virtual bool isProducingData() const;
    void start();
    void stop();
    void endImmediatly() { end(nullptr); }
    virtual void requestToEnd(Observer& callingObserver);
    bool isEnded() const { return m_isEnded; }

    bool muted() const { return m_muted; }
    virtual void setMuted(bool);

    bool captureDidFail() const { return m_captureDidFailed; }

    virtual bool interrupted() const { return false; }

    const AtomString& name() const { return m_name; }

    unsigned fitnessScore() const { return m_fitnessScore; }

    WEBCORE_EXPORT void addObserver(Observer&);
    WEBCORE_EXPORT void removeObserver(Observer&);

    WEBCORE_EXPORT void addAudioSampleObserver(AudioSampleObserver&);
    WEBCORE_EXPORT void removeAudioSampleObserver(AudioSampleObserver&);

    WEBCORE_EXPORT void addVideoFrameObserver(VideoFrameObserver&);
    WEBCORE_EXPORT void removeVideoFrameObserver(VideoFrameObserver&);

    const IntSize size() const;
    void setSize(const IntSize&);

    IntSize intrinsicSize() const;
    void setIntrinsicSize(const IntSize&, bool notifyObservers = true);

    double frameRate() const { return m_frameRate; }
    void setFrameRate(double);

    double aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(double);

    RealtimeMediaSourceSettings::VideoFacingMode facingMode() const { return m_facingMode; }
    void setFacingMode(RealtimeMediaSourceSettings::VideoFacingMode);

    double volume() const { return m_volume; }
    void setVolume(double);

    int sampleRate() const { return m_sampleRate; }
    void setSampleRate(int);
    virtual std::optional<Vector<int>> discreteSampleRates() const;

    int sampleSize() const { return m_sampleSize; }
    void setSampleSize(int);
    virtual std::optional<Vector<int>> discreteSampleSizes() const;

    bool echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(bool);

    virtual const RealtimeMediaSourceCapabilities& capabilities() = 0;
    virtual const RealtimeMediaSourceSettings& settings() = 0;

    struct ApplyConstraintsError {
        String badConstraint;
        String message;
    };
    using ApplyConstraintsHandler = CompletionHandler<void(std::optional<ApplyConstraintsError>&&)>;
    virtual void applyConstraints(const MediaConstraints&, ApplyConstraintsHandler&&);
    std::optional<ApplyConstraintsError> applyConstraints(const MediaConstraints&);

    bool supportsConstraints(const MediaConstraints&, String&);
    bool supportsConstraint(const MediaConstraint&);

    virtual bool isMockSource() const { return false; }
    virtual bool isCaptureSource() const { return false; }
    virtual CaptureDevice::DeviceType deviceType() const { return CaptureDevice::DeviceType::Unknown; }
    virtual bool isVideoSource() const;

    virtual void monitorOrientation(OrientationNotifier&) { }

    virtual void captureFailed();

    virtual bool isSameAs(RealtimeMediaSource& source) const { return this == &source; }
    virtual bool isIncomingAudioSource() const { return false; }
    virtual bool isIncomingVideoSource() const { return false; }

#if !RELEASE_LOG_DISABLED
    virtual void setLogger(const Logger&, const void*);
    const Logger* loggerPtr() const { return m_logger.get(); }
    const Logger& logger() const final { ASSERT(m_logger); return *m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const override { return "RealtimeMediaSource"; }
    WTFLogChannel& logChannel() const final;
#endif

    // Testing only
    virtual void delaySamples(Seconds) { };
    virtual void setInterruptedForTesting(bool);

    virtual bool setShouldApplyRotation(bool) { return false; }
    virtual void setIsInBackground(bool);

    PageIdentifier pageIdentifier() const { return m_pageIdentifier; }

    const CaptureDevice& captureDevice() const { return m_device; }
    bool isEphemeral() const { return m_device.isEphemeral(); }

protected:
    RealtimeMediaSource(const CaptureDevice&, MediaDeviceHashSalts&& hashSalts = { }, PageIdentifier = { });

    void scheduleDeferredTask(Function<void()>&&);

    virtual void beginConfiguration() { }
    virtual void commitConfiguration() { }

    bool selectSettings(const MediaConstraints&, FlattenedConstraint&, String&);
    double fitnessDistance(const MediaConstraint&);
    void applyConstraint(const MediaConstraint&);
    void applyConstraints(const FlattenedConstraint&);
    bool supportsSizeAndFrameRate(std::optional<IntConstraint> width, std::optional<IntConstraint> height, std::optional<DoubleConstraint>, String&, double& fitnessDistance);

    virtual bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);
    virtual void setSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);

    void notifyMutedObservers();
    void notifyMutedChange(bool muted);
    void notifySettingsDidChangeObservers(OptionSet<RealtimeMediaSourceSettings::Flag>);

    void initializeVolume(double volume) { m_volume = volume; }
    void initializeSampleRate(int sampleRate) { m_sampleRate = sampleRate; }
    void initializeEchoCancellation(bool echoCancellation) { m_echoCancellation = echoCancellation; }

    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata);
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t);

    void forEachObserver(const Function<void(Observer&)>&);
    void forEachVideoFrameObserver(const Function<void(VideoFrameObserver&)>&);

    void end(Observer* = nullptr);

    void setType(Type);

    void setName(const AtomString&);
    void setPersistentId(const String&);

private:
    virtual void startProducingData() { }
    virtual void stopProducingData() { }
    virtual void endProducingData() { stop(); }
    virtual void settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag>) { }

    virtual void stopBeingObserved() { stop(); }

    virtual void didEnd() { }

    void updateHasStartedProducingData();
    void initializePersistentId();

#if !RELEASE_LOG_DISABLED
    RefPtr<const Logger> m_logger;
    const void* m_logIdentifier;
    MonotonicTime m_lastFrameLogTime;
    unsigned m_frameCount { 0 };
#endif

    PageIdentifier m_pageIdentifier;
    MediaDeviceHashSalts m_idHashSalts;
    AtomString m_hashedID;
    AtomString m_ephemeralHashedID;
    Type m_type;
    AtomString m_name;
    WeakHashSet<Observer> m_observers;

    mutable Lock m_audioSampleObserversLock;
    HashSet<AudioSampleObserver*> m_audioSampleObservers WTF_GUARDED_BY_LOCK(m_audioSampleObserversLock);

    mutable Lock m_videoFrameObserversLock;
    HashSet<VideoFrameObserver*> m_videoFrameObservers WTF_GUARDED_BY_LOCK(m_videoFrameObserversLock);

    CaptureDevice m_device;

    // Set on the main thread from constraints.
    IntSize m_size;
    // Set on sample generation thread.
    IntSize m_intrinsicSize;
    double m_frameRate { 30 };
    double m_aspectRatio { 0 };
    double m_volume { 1 };
    double m_sampleRate { 0 };
    double m_sampleSize { 0 };
    double m_fitnessScore { 0 };
    RealtimeMediaSourceSettings::VideoFacingMode m_facingMode { RealtimeMediaSourceSettings::User};

    bool m_muted { false };
    bool m_pendingSettingsDidChangeNotification { false };
    bool m_echoCancellation { false };
    bool m_isProducingData { false };
    bool m_captureDidFailed { false };
    bool m_isEnded { false };
    bool m_hasStartedProducingData { false };
};

struct CaptureSourceOrError {
    CaptureSourceOrError() = default;
    CaptureSourceOrError(Ref<RealtimeMediaSource>&& source) : captureSource(WTFMove(source)) { }
    CaptureSourceOrError(String&& message) : errorMessage(WTFMove(message)) { }
    
    operator bool()  const { return !!captureSource; }
    Ref<RealtimeMediaSource> source() { return captureSource.releaseNonNull(); }
    
    RefPtr<RealtimeMediaSource> captureSource;
    String errorMessage;
};

String convertEnumerationToString(RealtimeMediaSource::Type);

inline void RealtimeMediaSource::setName(const AtomString& name)
{
    m_name = name;
}

inline void RealtimeMediaSource::whenReady(CompletionHandler<void(String)>&& callback)
{
    callback({ });
}

inline bool RealtimeMediaSource::isVideoSource() const
{
    return false;
}

inline bool RealtimeMediaSource::isProducingData() const
{
    return m_isProducingData;
}

inline void RealtimeMediaSource::setIsInBackground(bool)
{
}

} // namespace WebCore

namespace WTF {

template<typename Type>
struct LogArgument;

template <>
struct LogArgument<WebCore::RealtimeMediaSource::Type> {
    static String toString(const WebCore::RealtimeMediaSource::Type type)
    {
        return convertEnumerationToString(type);
    }
};

}; // namespace WTF

#endif // ENABLE(MEDIA_STREAM)
