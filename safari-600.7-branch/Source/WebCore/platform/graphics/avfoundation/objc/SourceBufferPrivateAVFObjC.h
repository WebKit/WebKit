/*
 * Copyright (C) 2013-2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef SourceBufferPrivateAVFObjC_h
#define SourceBufferPrivateAVFObjC_h

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#include "SourceBufferPrivate.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/MediaTime.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/AtomicString.h>

OBJC_CLASS AVAsset;
OBJC_CLASS AVStreamDataParser;
OBJC_CLASS AVSampleBufferAudioRenderer;
OBJC_CLASS AVSampleBufferDisplayLayer;
OBJC_CLASS NSData;
OBJC_CLASS NSError;
OBJC_CLASS NSObject;
OBJC_CLASS WebAVStreamDataParserListener;
OBJC_CLASS WebAVSampleBufferErrorListener;

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;

namespace WebCore {

class MediaSourcePrivateAVFObjC;
class TimeRanges;
class AudioTrackPrivate;
class VideoTrackPrivate;
class AudioTrackPrivateMediaSourceAVFObjC;
class VideoTrackPrivateMediaSourceAVFObjC;

class SourceBufferPrivateAVFObjCErrorClient {
public:
    virtual ~SourceBufferPrivateAVFObjCErrorClient() { }
    virtual void layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *, bool& shouldIgnore) = 0;
    virtual void rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *, bool& shouldIgnore) = 0;
};

class SourceBufferPrivateAVFObjC final : public SourceBufferPrivate {
public:
    static RefPtr<SourceBufferPrivateAVFObjC> create(MediaSourcePrivateAVFObjC*);
    virtual ~SourceBufferPrivateAVFObjC();

    void clearMediaSource() { m_mediaSource = nullptr; }

    // AVStreamDataParser delegate methods
    void didParseStreamDataAsAsset(AVAsset*);
    void didFailToParseStreamDataWithError(NSError*);
    void didProvideMediaDataForTrackID(int trackID, CMSampleBufferRef, const String& mediaType, unsigned flags);
    void didReachEndOfTrackWithTrackID(int trackID, const String& mediaType);
    void willProvideContentKeyRequestInitializationDataForTrackID(int trackID);
    void didProvideContentKeyRequestInitializationDataForTrackID(NSData*, int trackID);

    bool processCodedFrame(int trackID, CMSampleBufferRef, const String& mediaType);

    bool hasVideo() const;
    bool hasAudio() const;

    void trackDidChangeEnabled(VideoTrackPrivateMediaSourceAVFObjC*);
    void trackDidChangeEnabled(AudioTrackPrivateMediaSourceAVFObjC*);

    void seekToTime(MediaTime);
    MediaTime fastSeekTimeForMediaTime(MediaTime, MediaTime negativeThreshold, MediaTime positiveThreshold);
    FloatSize naturalSize();

    int protectedTrackID() const { return m_protectedTrackID; }
    AVStreamDataParser* parser() const { return m_parser.get(); }

    void flush();

    void registerForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient*);
    void unregisterForErrorNotifications(SourceBufferPrivateAVFObjCErrorClient*);
    void layerDidReceiveError(AVSampleBufferDisplayLayer *, NSError *);
    void rendererDidReceiveError(AVSampleBufferAudioRenderer *, NSError *);

private:
    explicit SourceBufferPrivateAVFObjC(MediaSourcePrivateAVFObjC*);

    // SourceBufferPrivate overrides
    virtual void setClient(SourceBufferPrivateClient*) override;
    virtual void append(const unsigned char* data, unsigned length) override;
    virtual void abort() override;
    virtual void removedFromMediaSource() override;
    virtual MediaPlayer::ReadyState readyState() const override;
    virtual void setReadyState(MediaPlayer::ReadyState) override;
    virtual void flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>, AtomicString trackID) override;
    virtual void enqueueSample(PassRefPtr<MediaSample>, AtomicString trackID) override;
    virtual bool isReadyForMoreSamples(AtomicString trackID) override;
    virtual void setActive(bool) override;
    virtual void notifyClientWhenReadyForMoreSamples(AtomicString trackID) override;

    void flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>, AVSampleBufferAudioRenderer*);
    void flushAndEnqueueNonDisplayingSamples(Vector<RefPtr<MediaSample>>, AVSampleBufferDisplayLayer*);

    void didBecomeReadyForMoreSamples(int trackID);
    void appendCompleted();
    void destroyParser();
    void destroyRenderers();

    WeakPtr<SourceBufferPrivateAVFObjC> createWeakPtr() { return m_weakFactory.createWeakPtr(); }

    Vector<RefPtr<VideoTrackPrivateMediaSourceAVFObjC>> m_videoTracks;
    Vector<RefPtr<AudioTrackPrivateMediaSourceAVFObjC>> m_audioTracks;
    Vector<SourceBufferPrivateAVFObjCErrorClient*> m_errorClients;

    WeakPtrFactory<SourceBufferPrivateAVFObjC> m_weakFactory;

    RetainPtr<AVStreamDataParser> m_parser;
    RetainPtr<AVAsset> m_asset;
    RetainPtr<AVSampleBufferDisplayLayer> m_displayLayer;
    HashMap<int, RetainPtr<AVSampleBufferAudioRenderer>> m_audioRenderers;
    RetainPtr<WebAVStreamDataParserListener> m_delegate;
    RetainPtr<WebAVSampleBufferErrorListener> m_errorListener;

    MediaSourcePrivateAVFObjC* m_mediaSource;
    SourceBufferPrivateClient* m_client;

    FloatSize m_cachedSize;
    bool m_parsingSucceeded;
    int m_enabledVideoTrackID;
    int m_protectedTrackID;
};

}

#endif // ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#endif
