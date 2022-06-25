/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateAVCF_h
#define InbandTextTrackPrivateAVCF_h

#if PLATFORM(WIN) && ENABLE(VIDEO) && HAVE(AVFOUNDATION_MEDIA_SELECTION_GROUP)

#include "InbandTextTrackPrivateAVF.h"
#include <AVFoundationCF/AVCFPlayerItem.h>
#include <wtf/RetainPtr.h>

namespace WebCore {

class InbandTextTrackPrivateAVCF : public InbandTextTrackPrivateAVF {
public:
    static Ref<InbandTextTrackPrivateAVCF> create(AVFInbandTrackParent* player,  AVCFMediaSelectionOptionRef selection, InbandTextTrackPrivate::CueFormat format)
    {
        return adoptRef(*new InbandTextTrackPrivateAVCF(player, selection, format));
    }

    ~InbandTextTrackPrivateAVCF() = default;

    InbandTextTrackPrivate::Kind kind() const override;
    bool isClosedCaptions() const override;
    bool isSDH() const override;
    bool containsOnlyForcedSubtitles() const override;
    bool isMainProgramContent() const override;
    bool isEasyToRead() const override;
    AtomString label() const override;
    AtomString language() const override;
    bool isDefault() const override;

    void disconnect() override;

    Category textTrackCategory() const override { return InBand; }
    bool readNativeSampleBuffer(CFArrayRef nativeSamples, CFIndex, RefPtr<JSC::ArrayBuffer>&, MediaTime&, CMFormatDescriptionRef&) override;

    AVCFMediaSelectionOptionRef mediaSelectionOption() const { return m_mediaSelectionOption.get(); }

protected:
    InbandTextTrackPrivateAVCF(AVFInbandTrackParent*, AVCFMediaSelectionOptionRef, InbandTextTrackPrivate::CueFormat);
    
    RetainPtr<AVCFMediaSelectionOptionRef> m_mediaSelectionOption;
};

}

#endif
#endif
