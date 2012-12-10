/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef InbandTextTrackPrivateAVF_h
#define InbandTextTrackPrivateAVF_h

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)

#include "InbandTextTrackPrivate.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

class MediaPlayerPrivateAVFoundation;

class InbandTextTrackPrivateAVF : public InbandTextTrackPrivate {
public:
    ~InbandTextTrackPrivateAVF();

    String id() const { return m_currentCueId; }
    double start() const { return m_currentCueStartTime; }
    double end() const { return m_currentCueEndTime; }
    String settings() { return m_currentCueSettings.toString(); }
    String content() { return m_currentCueContent.toString(); }

    void processCue(CFArrayRef, double);

    void resetCueValues();

    virtual void setMode(InbandTextTrackPrivate::Mode) OVERRIDE;

    virtual int textTrackIndex() const OVERRIDE { return m_index; }
    void setTextTrackIndex(int index) { m_index = index; }

    virtual void disconnect();

    bool hasBeenReported() const { return m_hasBeenReported; }
    void setHasBeenReported(bool reported) { m_hasBeenReported = reported; }

protected:
    InbandTextTrackPrivateAVF(MediaPlayerPrivateAVFoundation*);

    void processCueAttributes(CFAttributedStringRef, StringBuilder& content, StringBuilder& settings);

    String m_currentCueId;
    double m_currentCueStartTime;
    double m_currentCueEndTime;
    StringBuilder m_currentCueSettings;
    StringBuilder m_currentCueContent;

    MediaPlayerPrivateAVFoundation* m_player;
    int m_index;
    bool m_havePartialCue;
    bool m_hasBeenReported;
};

} // namespace WebCore

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION) && HAVE(AVFOUNDATION_TEXT_TRACK_SUPPORT)

#endif // InbandTextTrackPrivateAVF_h
