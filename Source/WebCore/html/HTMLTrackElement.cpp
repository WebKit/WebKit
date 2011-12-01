/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "config.h"

#if ENABLE(VIDEO_TRACK)
#include "HTMLTrackElement.h"

#include "Event.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Logging.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptEventListener.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

inline HTMLTrackElement::HTMLTrackElement(const QualifiedName& tagName, Document* document)
    : HTMLElement(tagName, document)
{
    LOG(Media, "HTMLTrackElement::HTMLTrackElement - %p", this);
    ASSERT(hasTagName(trackTag));
}

HTMLTrackElement::~HTMLTrackElement()
{
    if (m_track)
        m_track->clearClient();
}

PassRefPtr<HTMLTrackElement> HTMLTrackElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLTrackElement(tagName, document));
}

void HTMLTrackElement::insertedIntoTree(bool deep)
{
    HTMLElement::insertedIntoTree(deep);

    if (HTMLMediaElement* parent = mediaElement())
        parent->trackWasAdded(this);
}

void HTMLTrackElement::willRemove()
{
    if (HTMLMediaElement* parent = mediaElement())
        parent->trackWillBeRemoved(this);

    HTMLElement::willRemove();
}

void HTMLTrackElement::parseMappedAttribute(Attribute* attribute)
{
    const QualifiedName& attrName = attribute->name();

    if (attrName == onloadAttr)
        setAttributeEventListener(eventNames().loadEvent, createAttributeEventListener(this, attribute));
    else if (attrName == onerrorAttr)
        setAttributeEventListener(eventNames().errorEvent, createAttributeEventListener(this, attribute));
    else
        HTMLElement::parseMappedAttribute(attribute);
}

void HTMLTrackElement::attributeChanged(Attribute* attr, bool preserveDecls)
{
    HTMLElement::attributeChanged(attr, preserveDecls);

    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return;

    const QualifiedName& attrName = attr->name();
    if (attrName == srcAttr) {
        if (!getAttribute(srcAttr).isEmpty() && mediaElement())
            scheduleLoad();

    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // As the kind, label, and srclang attributes are set, changed, or removed, the text track must update accordingly...
    } else if (attrName == kindAttr)
        track()->setKind(attr->value());
    else if (attrName == labelAttr)
        track()->setLabel(attr->value());
    else if (attrName == srclangAttr)
        track()->setLanguage(attr->value());
}

KURL HTMLTrackElement::src() const
{
    return document()->completeURL(getAttribute(srcAttr));
}

void HTMLTrackElement::setSrc(const String& url)
{
    setAttribute(srcAttr, url);
}

String HTMLTrackElement::kind()
{
    return track()->kind();
}

void HTMLTrackElement::setKind(const String& kind)
{
    setAttribute(kindAttr, kind);
}

String HTMLTrackElement::srclang() const
{
    return getAttribute(srclangAttr);
}

void HTMLTrackElement::setSrclang(const String& srclang)
{
    setAttribute(srclangAttr, srclang);
}

String HTMLTrackElement::label() const
{
    return getAttribute(labelAttr);
}

void HTMLTrackElement::setLabel(const String& label)
{
    setAttribute(labelAttr, label);
}

bool HTMLTrackElement::isDefault() const
{
    return fastHasAttribute(defaultAttr);
}

void HTMLTrackElement::setIsDefault(bool isDefault)
{
    setBooleanAttribute(defaultAttr, isDefault);
}

LoadableTextTrack* HTMLTrackElement::ensureTrack()
{
    if (!m_track) {
        // The kind attribute is an enumerated attribute, limited only to know values. It defaults to 'subtitles' if missing or invalid.
        String kind = getAttribute(kindAttr);
        if (!TextTrack::isValidKindKeyword(kind))
            kind = TextTrack::subtitlesKeyword();
        m_track = LoadableTextTrack::create(this, kind, label(), srclang(), isDefault());
    }
    return m_track.get();
}

TextTrack* HTMLTrackElement::track()
{
    return ensureTrack();
}

bool HTMLTrackElement::isURLAttribute(Attribute* attribute) const
{
    return attribute->name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLTrackElement::scheduleLoad()
{
    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return;

    if (!mediaElement())
        return;

    if (!fastHasAttribute(srcAttr))
        return;

    ensureTrack()->scheduleLoad(getNonEmptyURLAttribute(srcAttr));
}

bool HTMLTrackElement::canLoadUrl(LoadableTextTrack*, const KURL& url)
{
    if (!RuntimeEnabledFeatures::webkitVideoTrackEnabled())
        return false;

    HTMLMediaElement* parent = mediaElement();
    if (!parent)
        return false;

    if (!parent->isSafeToLoadURL(url, HTMLMediaElement::Complain))
        return false;
    
    return dispatchBeforeLoadEvent(url.string());
}

void HTMLTrackElement::didCompleteLoad(LoadableTextTrack*, bool loadingFailed)
{
    loadingFailed ? setReadyState(HTMLTrackElement::TRACK_ERROR) : setReadyState(HTMLTrackElement::LOADED);

    ExceptionCode ec = 0;
    dispatchEvent(Event::create(loadingFailed ? eventNames().errorEvent : eventNames().loadEvent, false, false), ec);
}

// NOTE: The values in the TextTrack::ReadinessState enum must stay in sync with those in HTMLTrackElement::ReadyState.
COMPILE_ASSERT(HTMLTrackElement::NONE == static_cast<HTMLTrackElement::ReadyState>(TextTrack::NotLoaded), TextTrackEnumNotLoaded_Is_Wrong_Should_Be_HTMLTrackElementEnumNONE);
COMPILE_ASSERT(HTMLTrackElement::LOADING == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loading), TextTrackEnumLoadingIsWrong_ShouldBe_HTMLTrackElementEnumLOADING);
COMPILE_ASSERT(HTMLTrackElement::LOADED == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loaded), TextTrackEnumLoaded_Is_Wrong_Should_Be_HTMLTrackElementEnumLOADED);
COMPILE_ASSERT(HTMLTrackElement::TRACK_ERROR == static_cast<HTMLTrackElement::ReadyState>(TextTrack::FailedToLoad), TextTrackEnumFailedToLoad_Is_Wrong_Should_Be_HTMLTrackElementEnumTRACK_ERROR);

void HTMLTrackElement::setReadyState(ReadyState state)
{
    ensureTrack()->setReadinessState(static_cast<TextTrack::ReadinessState>(state));
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackReadyStateChanged(m_track.get());
}

HTMLTrackElement::ReadyState HTMLTrackElement::readyState() 
{
    return static_cast<ReadyState>(ensureTrack()->readinessState());
}
    
void HTMLTrackElement::textTrackKindChanged(TextTrack* track)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackKindChanged(track);
}

void HTMLTrackElement::textTrackModeChanged(TextTrack* track)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackModeChanged(track);
}

void HTMLTrackElement::textTrackAddCues(TextTrack* track, const TextTrackCueList* cues)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackAddCues(track, cues);
}
    
void HTMLTrackElement::textTrackRemoveCues(TextTrack* track, const TextTrackCueList* cues)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackAddCues(track, cues);
}
    
void HTMLTrackElement::textTrackAddCue(TextTrack* track, PassRefPtr<TextTrackCue> cue)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackAddCue(track, cue);
}
    
void HTMLTrackElement::textTrackRemoveCue(TextTrack* track, PassRefPtr<TextTrackCue> cue)
{
    if (HTMLMediaElement* parent = mediaElement())
        return parent->textTrackRemoveCue(track, cue);
}

HTMLMediaElement* HTMLTrackElement::mediaElement() const
{
    Element* parent = parentElement();
    if (parent && parent->isMediaElement())
        return static_cast<HTMLMediaElement*>(parentNode());

    return 0;
}

#if ENABLE(MICRODATA)
String HTMLTrackElement::itemValueText() const
{
    return getURLAttribute(srcAttr);
}

void HTMLTrackElement::setItemValueText(const String& value, ExceptionCode& ec)
{
    setAttribute(srcAttr, value, ec);
}
#endif

}

#endif
