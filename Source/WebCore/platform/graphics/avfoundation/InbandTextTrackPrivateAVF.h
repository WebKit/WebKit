/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateAVF_h
#define InbandTextTrackPrivateAVF_h

#include <wtf/Platform.h>
#if ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#include <WebCore/InbandTextTrackPrivate.h>
#include <WebCore/InbandTextTrackPrivateClient.h>
#include <wtf/AbstractRefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;

namespace JSC {
class ArrayBuffer;
}

namespace WebCore {

class InbandTextTrackPrivateAVF : public InbandTextTrackPrivate {
    WTF_MAKE_TZONE_ALLOCATED(InbandTextTrackPrivateAVF);
public:
    virtual ~InbandTextTrackPrivateAVF();

    TrackID id() const final { return m_trackID; }
    Kind kind() const override { return m_kind; }
    String label() const override { return m_label; }
    String language() const override { return m_language; }

    void setMode(InbandTextTrackPrivate::Mode) final;

    int trackIndex() const final { return m_index; }
    void setTextTrackIndex(int index) { m_index = index; }

    virtual bool isOutOfBandTextTrackPrivateAVF() const { return false; }
    virtual bool isInbandTextTrackPrivateAVFObjC() const { return false; }

    virtual void disconnect();

    bool hasBeenReported() const { return m_hasBeenReported; }
    void setHasBeenReported(bool reported) { m_hasBeenReported = reported; }

    virtual void processCue(CFArrayRef attributedStrings, CFArrayRef nativeSamples, const MediaTime&);
    virtual void resetCueValues();

    void beginSeeking();
    void endSeeking() { m_seeking = false; }
    bool seeking() const { return m_seeking; }
    
    enum Category {
        LegacyClosedCaption,
        OutOfBand,
        InBand
    };
    virtual Category textTrackCategory() const = 0;
    
    MediaTime startTimeVariance() const final { return MediaTime(1, 4); }

    InbandTextTrackType inbandTextTrackType() const override { return InbandTextTrackType::AVFTrack; }
    void processVTTSample(CMSampleBufferRef, const MediaTime&);

    using ModeChangedCallback = Function<void()>;

protected:
    InbandTextTrackPrivateAVF(TrackID, CueFormat, ModeChangedCallback&&);

    void setKind(Kind kind) { m_kind = kind; }
    void setId(TrackID newId) { m_trackID = newId; }
    void setLabel(const String& label) { m_label = label; }
    void setLanguage(const String& language) { m_language = language; }

    Ref<InbandGenericCue> processCueAttributes(CFAttributedStringRef);
    void processAttributedStrings(CFArrayRef, const MediaTime&);
    void processVTTSamples(CFArrayRef, const MediaTime&);
    void removeCompletedCues();

    Vector<uint8_t> m_sampleInputBuffer;

private:
#if !RELEASE_LOG_DISABLED
    ASCIILiteral logClassName() const final { return "InbandTextTrackPrivateAVF"_s; }
#endif

    bool processVTTFileHeader(CMFormatDescriptionRef);
    bool readVTTSampleBuffer(CMSampleBufferRef, CMFormatDescriptionRef&);

    MediaTime m_currentCueStartTime;
    MediaTime m_currentCueEndTime;

    Vector<Ref<InbandGenericCue>> m_cues;
    ModeChangedCallback m_modeChangedCallback;

    enum PendingCueStatus {
        None,
        DeliveredDuringSeek,
        Valid
    };
    PendingCueStatus m_pendingCueStatus { None };

    Kind m_kind { Kind::None };
    String m_label;
    String m_language;
    int m_index { 0 };
    TrackID m_trackID { 0 };
    bool m_hasBeenReported { false };
    bool m_seeking { false };
    bool m_haveReportedVTTHeader { false };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::InbandTextTrackPrivateAVF)
static bool isType(const WebCore::InbandTextTrackPrivate& track) { return track.inbandTextTrackType() == WebCore::InbandTextTrackPrivate::InbandTextTrackType::AVFTrack; }
SPECIALIZE_TYPE_TRAITS_END()

#endif //  ENABLE(VIDEO) && (USE(AVFOUNDATION) || PLATFORM(IOS_FAMILY))

#endif // InbandTextTrackPrivateAVF_h
