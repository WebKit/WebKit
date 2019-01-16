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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef AudioSourceProviderAVFObjC_h
#define AudioSourceProviderAVFObjC_h

#if ENABLE(WEB_AUDIO) && USE(MEDIATOOLBOX)

#include "AudioSourceProvider.h"
#include <atomic>
#include <wtf/MediaTime.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS AVAssetTrack;
OBJC_CLASS AVPlayerItem;
OBJC_CLASS AVMutableAudioMix;

typedef const struct opaqueMTAudioProcessingTap *MTAudioProcessingTapRef;
typedef struct AudioBufferList AudioBufferList;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef uint32_t MTAudioProcessingTapFlags;
typedef signed long CMItemCount;

namespace WebCore {

class CARingBuffer;

class AudioSourceProviderAVFObjC : public RefCounted<AudioSourceProviderAVFObjC>, public AudioSourceProvider {
public:
    static RefPtr<AudioSourceProviderAVFObjC> create(AVPlayerItem*);
    virtual ~AudioSourceProviderAVFObjC();

    void setPlayerItem(AVPlayerItem *);
    void setAudioTrack(AVAssetTrack *);

private:
    AudioSourceProviderAVFObjC(AVPlayerItem *);

    void destroyMix();
    void createMix();

    // AudioSourceProvider
    void provideInput(AudioBus*, size_t framesToProcess) override;
    void setClient(AudioSourceProviderClient*) override;

    static void initCallback(MTAudioProcessingTapRef, void*, void**);
    static void finalizeCallback(MTAudioProcessingTapRef);
    static void prepareCallback(MTAudioProcessingTapRef, CMItemCount, const AudioStreamBasicDescription*);
    static void unprepareCallback(MTAudioProcessingTapRef);
    static void processCallback(MTAudioProcessingTapRef, CMItemCount, MTAudioProcessingTapFlags, AudioBufferList*, CMItemCount*, MTAudioProcessingTapFlags*);

    void init(void* clientInfo, void** tapStorageOut);
    void finalize();
    void prepare(CMItemCount maxFrames, const AudioStreamBasicDescription *processingFormat);
    void unprepare();
    void process(MTAudioProcessingTapRef, CMItemCount numberFrames, MTAudioProcessingTapFlags flagsIn, AudioBufferList *bufferListInOut, CMItemCount *numberFramesOut, MTAudioProcessingTapFlags *flagsOut);

    RetainPtr<AVPlayerItem> m_avPlayerItem;
    RetainPtr<AVAssetTrack> m_avAssetTrack;
    RetainPtr<AVMutableAudioMix> m_avAudioMix;
    RetainPtr<MTAudioProcessingTapRef> m_tap;
    RetainPtr<AudioConverterRef> m_converter;
    std::unique_ptr<AudioBufferList> m_list;
    std::unique_ptr<AudioStreamBasicDescription> m_tapDescription;
    std::unique_ptr<AudioStreamBasicDescription> m_outputDescription;
    std::unique_ptr<CARingBuffer> m_ringBuffer;

    MediaTime m_startTimeAtLastProcess;
    MediaTime m_endTimeAtLastProcess;
    std::atomic<uint64_t> m_writeAheadCount { 0 };
    uint64_t m_readCount { 0 };
    enum { NoSeek = std::numeric_limits<uint64_t>::max() };
    std::atomic<uint64_t> m_seekTo { NoSeek };
    bool m_paused { true };
    AudioSourceProviderClient* m_client { nullptr };

    class TapStorage;
    RefPtr<TapStorage> m_tapStorage;
};
    
}

#endif

#endif
