/*
 * Copyright (C) 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2014 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
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

#ifndef TextTrackRegion_h
#define TextTrackRegion_h

#if ENABLE(VIDEO_TRACK) && ENABLE(WEBVTT_REGIONS)

#include "ContextDestructionObserver.h"
#include "Document.h"
#include "FloatPoint.h"
#include "TextTrack.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class HTMLDivElement;
class VTTCueBox;

class TextTrackRegion : public RefCounted<TextTrackRegion>, public ContextDestructionObserver {
public:
    static PassRefPtr<TextTrackRegion> create(ScriptExecutionContext& context)
    {
        return adoptRef(new TextTrackRegion(context));
    }

    virtual ~TextTrackRegion();

    TextTrack* track() const { return m_track; }
    void setTrack(TextTrack*);

    const String& id() const { return m_id; }
    void setId(const String&);

    double width() const { return m_width; }
    void setWidth(double, ExceptionCode&);

    long height() const { return m_heightInLines; }
    void setHeight(long, ExceptionCode&);

    double regionAnchorX() const { return m_regionAnchor.x(); }
    void setRegionAnchorX(double, ExceptionCode&);

    double regionAnchorY() const { return m_regionAnchor.y(); }
    void setRegionAnchorY(double, ExceptionCode&);

    double viewportAnchorX() const { return m_viewportAnchor.x(); }
    void setViewportAnchorX(double, ExceptionCode&);

    double viewportAnchorY() const { return m_viewportAnchor.y(); }
    void setViewportAnchorY(double, ExceptionCode&);

    const AtomicString scroll() const;
    void setScroll(const AtomicString&, ExceptionCode&);

    void updateParametersFromRegion(TextTrackRegion*);

    const String& regionSettings() const { return m_settings; }
    void setRegionSettings(const String&);

    bool isScrollingRegion() { return m_scroll; }

    PassRefPtr<HTMLDivElement> getDisplayTree();
    
    void appendTextTrackCueBox(PassRefPtr<VTTCueBox>);
    void displayLastTextTrackCueBox();
    void willRemoveTextTrackCueBox(VTTCueBox*);

private:
    TextTrackRegion(ScriptExecutionContext&);

    Document* ownerDocument() { return toDocument(m_scriptExecutionContext); }

    void prepareRegionDisplayTree();

    // The timer is needed to continue processing when cue scrolling ended.
    void startTimer();
    void stopTimer();
    void scrollTimerFired(Timer<TextTrackRegion>*);

    enum RegionSetting {
        None,
        Id,
        Width,
        Height,
        RegionAnchor,
        ViewportAnchor,
        Scroll
    };

    RegionSetting getSettingFromString(const String&);

    void parseSettingValue(RegionSetting, const String&);
    void parseSetting(const String&, unsigned*);

    static const AtomicString& textTrackCueContainerShadowPseudoId();
    static const AtomicString& textTrackCueContainerScrollingClass();
    static const AtomicString& textTrackRegionShadowPseudoId();

    String m_id;
    String m_settings;

    double m_width;
    unsigned m_heightInLines;

    FloatPoint m_regionAnchor;
    FloatPoint m_viewportAnchor;

    bool m_scroll;

    // The cue container is the container that is scrolled up to obtain the
    // effect of scrolling cues when this is enabled for the regions.
    RefPtr<HTMLDivElement> m_cueContainer;
    RefPtr<HTMLDivElement> m_regionDisplayTree;

    // The member variable track can be a raw pointer as it will never
    // reference a destroyed TextTrack, as this member variable
    // is cleared in the TextTrack destructor and it is generally
    // set/reset within the addRegion and removeRegion methods.
    TextTrack* m_track;

    // Keep track of the current numeric value of the css "top" property.
    double m_currentTop;

    // The timer is used to display the next cue line after the current one has
    // been displayed. It's main use is for scrolling regions and it triggers as
    // soon as the animation for rolling out one line has finished, but
    // currently it is used also for non-scrolling regions to use a single
    // code path.
    Timer<TextTrackRegion> m_scrollTimer;
};

} // namespace WebCore

#endif
#endif
