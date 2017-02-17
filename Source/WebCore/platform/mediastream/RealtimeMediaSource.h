/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RealtimeMediaSource_h
#define RealtimeMediaSource_h

#if ENABLE(MEDIA_STREAM)

#include "AudioSourceProvider.h"
#include "Image.h"
#include "MediaConstraints.h"
#include "MediaSample.h"
#include "PlatformLayer.h"
#include "RealtimeMediaSourceCapabilities.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class MediaTime;
}

namespace WebCore {

class AudioStreamDescription;
class FloatRect;
class GraphicsContext;
class MediaStreamPrivate;
class PlatformAudioData;
class RealtimeMediaSourceSettings;

class RealtimeMediaSource : public RefCounted<RealtimeMediaSource> {
public:
    class Observer {
    public:
        virtual ~Observer() { }
        
        // Source state changes.
        virtual void sourceStopped() { }
        virtual void sourceMutedChanged() { }
        virtual void sourceEnabledChanged() { }
        virtual void sourceSettingsChanged() { }

        // Observer state queries.
        virtual bool preventSourceFromStopping() { return false; }
        
        // Called on the main thread.
        virtual void videoSampleAvailable(MediaSample&) { }

        // May be called on a background thread.
        virtual void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t /*numberOfFrames*/) { }
    };

    virtual ~RealtimeMediaSource() { }

    const String& id() const { return m_id; }

    const String& persistentID() const { return m_persistentID; }
    virtual void setPersistentID(const String& persistentID) { m_persistentID = persistentID; }

    enum Type { None, Audio, Video };
    Type type() const { return m_type; }

    virtual const String& name() const { return m_name; }
    virtual void setName(const String& name) { m_name = name; }
    
    virtual unsigned fitnessScore() const { return m_fitnessScore; }
    virtual void setFitnessScore(const unsigned fitnessScore) { m_fitnessScore = fitnessScore; }

    virtual RefPtr<RealtimeMediaSourceCapabilities> capabilities() const = 0;
    virtual const RealtimeMediaSourceSettings& settings() const = 0;

    using SuccessHandler = std::function<void()>;
    using FailureHandler = std::function<void(const String& badConstraint, const String& errorString)>;
    void applyConstraints(const MediaConstraints&, SuccessHandler, FailureHandler);
    std::optional<std::pair<String, String>> applyConstraints(const MediaConstraints&);

    virtual bool supportsConstraints(const MediaConstraints&, String&);

    virtual void settingsDidChange();

    void videoSampleAvailable(MediaSample&);
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t);
    
    bool stopped() const { return m_stopped; }

    virtual bool muted() const { return m_muted; }
    virtual void setMuted(bool);

    virtual bool enabled() const { return m_enabled; }
    virtual void setEnabled(bool);

    virtual bool readonly() const;
    virtual void setReadonly(bool readonly) { m_readonly = readonly; }

    virtual bool remote() const { return m_remote; }
    virtual void setRemote(bool remote) { m_remote = remote; }

    void addObserver(Observer&);
    void removeObserver(Observer&);

    virtual void startProducingData() { }
    virtual void stopProducingData() { }
    virtual bool isProducingData() const { return false; }

    void stop(Observer* callingObserver = nullptr);
    void requestStop(Observer* callingObserver = nullptr);

    virtual void reset();

    virtual AudioSourceProvider* audioSourceProvider() { return nullptr; }

    virtual RefPtr<Image> currentFrameImage() { return nullptr; }
    virtual void paintCurrentFrameInContext(GraphicsContext&, const FloatRect&) { }

    void setWidth(int);
    void setHeight(int);
    const IntSize& size() const { return m_size; }
    virtual bool applySize(const IntSize&) { return false; }

    double frameRate() const { return m_frameRate; }
    void setFrameRate(double);
    virtual bool applyFrameRate(double) { return false; }

    double aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(double);
    virtual bool applyAspectRatio(double) { return false; }

    RealtimeMediaSourceSettings::VideoFacingMode facingMode() const { return m_facingMode; }
    void setFacingMode(RealtimeMediaSourceSettings::VideoFacingMode);
    virtual bool applyFacingMode(RealtimeMediaSourceSettings::VideoFacingMode) { return false; }

    double volume() const { return m_volume; }
    void setVolume(double);
    virtual bool applyVolume(double) { return false; }

    int sampleRate() const { return m_sampleRate; }
    void setSampleRate(int);
    virtual bool applySampleRate(int) { return false; }

    int sampleSize() const { return m_sampleSize; }
    void setSampleSize(int);
    virtual bool applySampleSize(int) { return false; }

    bool echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(bool);
    virtual bool applyEchoCancellation(bool) { return false; }

protected:
    RealtimeMediaSource(const String& id, Type, const String& name);

    void scheduleDeferredTask(std::function<void()>&&);

    virtual void beginConfiguration() { }
    virtual void commitConfiguration() { }

    virtual bool selectSettings(const MediaConstraints&, FlattenedConstraint&, String&);
    virtual double fitnessDistance(const MediaConstraint&);
    virtual bool supportsSizeAndFrameRate(std::optional<IntConstraint> width, std::optional<IntConstraint> height, std::optional<DoubleConstraint>, String&);
    virtual bool supportsSizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);
    virtual void applyConstraint(const MediaConstraint&);
    virtual void applyConstraints(const FlattenedConstraint&);
    virtual void applySizeAndFrameRate(std::optional<int> width, std::optional<int> height, std::optional<double>);

    bool m_muted { false };
    bool m_enabled { true };

private:
    WeakPtr<RealtimeMediaSource> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    WeakPtrFactory<RealtimeMediaSource> m_weakPtrFactory;
    String m_id;
    String m_persistentID;
    Type m_type;
    String m_name;
    Vector<Observer*> m_observers;
    IntSize m_size;
    double m_frameRate { 30 };
    double m_aspectRatio { 0 };
    double m_volume { 1 };
    double m_sampleRate { 0 };
    double m_sampleSize { 0 };
    unsigned m_fitnessScore { 0 };
    RealtimeMediaSourceSettings::VideoFacingMode m_facingMode { RealtimeMediaSourceSettings::User};

    bool m_echoCancellation { false };
    bool m_stopped { false };
    bool m_readonly { false };
    bool m_remote { false };
    bool m_pendingSettingsDidChangeNotification { false };
    bool m_suppressNotifications { true };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

#endif // RealtimeMediaSource_h
