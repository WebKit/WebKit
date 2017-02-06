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

#pragma once

#if ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM)

#include "AudioCaptureSourceProviderObjC.h"
#include "AudioSourceObserverObjC.h"
#include "AudioSourceProvider.h"
#include <wtf/Lock.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

typedef struct AudioBufferList AudioBufferList;
typedef struct OpaqueAudioConverter* AudioConverterRef;
typedef struct AudioStreamBasicDescription AudioStreamBasicDescription;
typedef const struct opaqueCMFormatDescription *CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CARingBuffer;

class WebAudioSourceProviderAVFObjC : public RefCounted<WebAudioSourceProviderAVFObjC>, public AudioSourceProvider, public AudioSourceObserverObjC {
public:
    static Ref<WebAudioSourceProviderAVFObjC> create(AudioCaptureSourceProviderObjC&);
    virtual ~WebAudioSourceProviderAVFObjC();

private:
    WebAudioSourceProviderAVFObjC(AudioCaptureSourceProviderObjC&);

    // AudioSourceProvider
    void provideInput(AudioBus*, size_t) override;
    void setClient(AudioSourceProviderClient*) override;

    // AudioSourceObserverObjC
    void prepare(const AudioStreamBasicDescription *) final;
    void unprepare() final;
    void process(CMFormatDescriptionRef, CMSampleBufferRef) final;

    size_t m_listBufferSize { 0 };
    std::unique_ptr<AudioBufferList> m_list;
    AudioConverterRef m_converter;
    std::unique_ptr<AudioStreamBasicDescription> m_inputDescription;
    std::unique_ptr<AudioStreamBasicDescription> m_outputDescription;
    std::unique_ptr<CARingBuffer> m_ringBuffer;

    uint64_t m_writeCount { 0 };
    uint64_t m_readCount { 0 };
    AudioSourceProviderClient* m_client { nullptr };
    AudioCaptureSourceProviderObjC* m_captureSource { nullptr };
    Lock m_mutex;
    bool m_connected { false };
};
    
}

#endif
