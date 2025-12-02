/*
 * Copyright (C) 2011, 2013 Google Inc. All rights reserved.
 * Copyright (C) 2011-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "HTMLTrackElement.h"

#if ENABLE(VIDEO)

#include "ContentSecurityPolicy.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "EventTargetInlines.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "LoadableTextTrack.h"
#include "Logging.h"
#include "NodeName.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(HTMLTrackElement);

using namespace HTMLNames;

#if !LOG_DISABLED

static String urlForLoggingTrack(const URL& url)
{
    static const unsigned maximumURLLengthForLogging = 128;
    
    if (url.string().length() < maximumURLLengthForLogging)
        return url.string();
    return makeString(StringView(url.string()).left(maximumURLLengthForLogging), "..."_s);
}

#endif
    
inline HTMLTrackElement::HTMLTrackElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasDidMoveToNewDocument)
    , ActiveDOMObject(document)
    , m_track(LoadableTextTrack::create(*this, nullAtom(), nullAtom(), nullAtom()))
{
    m_track->addClient(*this);
    LOG(Media, "HTMLTrackElement::HTMLTrackElement - %p", this);
    ASSERT(hasTagName(trackTag));
}

HTMLTrackElement::~HTMLTrackElement()
{
    m_track->clearClient(*this);
}

Ref<HTMLTrackElement> HTMLTrackElement::create(const QualifiedName& tagName, Document& document)
{
    auto trackElement = adoptRef(*new HTMLTrackElement(tagName, document));
    trackElement->suspendIfNeeded();
    return trackElement;
}

Node::InsertedIntoAncestorResult HTMLTrackElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);

    if (parentNode() == &parentOfInsertedTree) {
        if (auto* mediaElement = dynamicDowncast<HTMLMediaElement>(parentOfInsertedTree)) {
            mediaElement->didAddTextTrack(*this);
            scheduleLoad();
        }
    }

    return InsertedIntoAncestorResult::Done;
}

void HTMLTrackElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);

    if (!parentNode()) {
        if (auto* mediaElement = dynamicDowncast<HTMLMediaElement>(oldParentOfRemovedTree))
            mediaElement->didRemoveTextTrack(*this);
    }
}

void HTMLTrackElement::didMoveToNewDocument(Document& oldDocument, Document& newDocument)
{
    HTMLElement::didMoveToNewDocument(oldDocument, newDocument);
    ActiveDOMObject::didMoveToNewDocument(newDocument);
}

void HTMLTrackElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    // 4.8.10.12.3 Sourcing out-of-band text tracks
    // As the kind, label, and srclang attributes are set, changed, or removed, the text track must update accordingly...
    switch (name.nodeName()) {
    case AttributeNames::srcAttr:
        scheduleLoad();
        break;
    case AttributeNames::kindAttr:
        track().setKindKeywordIgnoringASCIICase(newValue.string());
        break;
    case AttributeNames::labelAttr:
        track().setLabel(newValue);
        break;
    case AttributeNames::srclangAttr:
        track().setLanguage(newValue);
        break;
    default:
        break;
    }

    HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
}

const AtomString& HTMLTrackElement::kind()
{
    return track().kindKeyword();
}

bool HTMLTrackElement::isDefault() const
{
    return hasAttributeWithoutSynchronization(defaultAttr);
}

TextTrack& HTMLTrackElement::track()
{
    return m_track.get();
}

bool HTMLTrackElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

void HTMLTrackElement::scheduleLoad()
{
    // 1. If another occurrence of this algorithm is already running for this text track and its track element,
    // abort these steps, letting that other algorithm take care of this element.
    if (m_loadPending)
        return;

    // 2. If the text track's text track mode is not set to one of hidden or showing, abort these steps.
    if (track().mode() != TextTrack::Mode::Hidden && track().mode() != TextTrack::Mode::Showing)
        return;

    // 3. If the text track's track element does not have a media element as a parent, abort these steps.
    if (!mediaElement())
        return;

    // 4. Run the remainder of these steps asynchronously, allowing whatever caused these steps to run to continue.
    m_loadPending = true;
    scheduleTask([](auto& track) mutable {

        SetForScope loadPending { track.m_loadPending, true, false };

        // 6. Set the text track readiness state to loading.
        track.setReadyState(HTMLTrackElement::LOADING);

        // 7. Let URL be the track URL of the track element.
        URL trackURL = track.getNonEmptyURLAttribute(srcAttr);

        // ... if URL is the empty string, then queue a task to first change the text track readiness state
        // to failed to load and then fire an event named error at the track element.
        // 8. If the track element's parent is a media element then let CORS mode be the state of the parent media
        // element's crossorigin content attribute. Otherwise, let CORS mode be No CORS.
        if (!track.canLoadURL(trackURL)) {
            track.track().removeAllCues();
            track.didCompleteLoad(HTMLTrackElement::Failure);
            return;
        }

        track.m_track->scheduleLoad(trackURL);
    });
}

void HTMLTrackElement::scheduleTask(Function<void(HTMLTrackElement&)>&& task)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::MediaElement, [task = WTFMove(task)](auto& track) mutable {
        task(track);
    });
}

bool HTMLTrackElement::canLoadURL(const URL& url)
{
    auto parent = mediaElement();
    if (!parent)
        return false;

    // 4.8.10.12.3 Sourcing out-of-band text tracks

    // 4. Download: If URL is not the empty string, perform a potentially CORS-enabled fetch of URL, with the
    // mode being the state of the media element's crossorigin content attribute, the origin being the
    // origin of the media element's Document, and the default origin behaviour set to fail.
    if (url.isEmpty())
        return false;

    Ref document = this->document();
    ASSERT(document->contentSecurityPolicy());
    // Elements in user agent show tree should load whatever the embedding document policy is.
    if (!isInUserAgentShadowTree() && !document->checkedContentSecurityPolicy()->allowMediaFromSource(url)) {
        LOG(Media, "HTMLTrackElement::canLoadURL(%s) -> rejected by Content Security Policy", urlForLoggingTrack(url).utf8().data());
        return false;
    }

    return true;
}

void HTMLTrackElement::didCompleteLoad(LoadStatus status)
{
    // Make sure the JS wrapper stays alive until the end of this method, even though we update the
    // readyState to no longer be LOADING.
    auto wrapperProtector = makePendingActivity(*this);

    // 4.8.10.12.3 Sourcing out-of-band text tracks (continued)
    
    // 4. Download: ...
    // If the fetching algorithm fails for any reason (network error, the server returns an error 
    // code, a cross-origin check fails, etc), or if URL is the empty string or has the wrong origin 
    // as determined by the condition at the start of this step, or if the fetched resource is not in
    // a supported format, then queue a task to first change the text track readiness state to failed
    // to load and then fire a simple event named error at the track element; and then, once that task
    // is queued, move on to the step below labeled monitoring.

    if (status == Failure) {
        setReadyState(HTMLTrackElement::TRACK_ERROR);
        dispatchEvent(Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::No));
        return;
    }

    // If the fetching algorithm does not fail, then the final task that is queued by the networking
    // task source must run the following steps:
    //     1. Change the text track readiness state to loaded.
    setReadyState(HTMLTrackElement::LOADED);

    //     2. If the file was successfully processed, fire a simple event named load at the 
    //        track element.
    dispatchEvent(Event::create(eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

// NOTE: The values in the TextTrack::ReadinessState enum must stay in sync with those in HTMLTrackElement::ReadyState.
static_assert(HTMLTrackElement::NONE == static_cast<HTMLTrackElement::ReadyState>(TextTrack::NotLoaded), "TextTrackEnumNotLoaded is wrong. Should be HTMLTrackElementEnumNONE");
static_assert(HTMLTrackElement::LOADING == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loading), "TextTrackEnumLoading is wrong. Should be HTMLTrackElementEnumLOADING");
static_assert(HTMLTrackElement::LOADED == static_cast<HTMLTrackElement::ReadyState>(TextTrack::Loaded), "TextTrackEnumLoaded is wrong. Should be HTMLTrackElementEnumLOADED");
static_assert(HTMLTrackElement::TRACK_ERROR == static_cast<HTMLTrackElement::ReadyState>(TextTrack::FailedToLoad), "TextTrackEnumFailedToLoad is wrong. Should be HTMLTrackElementEnumTRACK_ERROR");

void HTMLTrackElement::setReadyState(ReadyState state)
{
    track().setReadinessState(static_cast<TextTrack::ReadinessState>(state));
    if (auto parent = mediaElement())
        parent->textTrackReadyStateChanged(m_track.ptr());
}

HTMLTrackElement::ReadyState HTMLTrackElement::readyState() const
{
    return static_cast<ReadyState>(m_track->readinessState());
}

const AtomString& HTMLTrackElement::mediaElementCrossOriginAttribute() const
{
    if (auto parent = mediaElement())
        return parent->attributeWithoutSynchronization(HTMLNames::crossoriginAttr);
    return nullAtom();
}

void HTMLTrackElement::textTrackModeChanged(TextTrack&)
{
    // Since we've moved to a new parent, we may now be able to load.
    if (readyState() == HTMLTrackElement::NONE)
        scheduleLoad();
}

RefPtr<HTMLMediaElement> HTMLTrackElement::mediaElement() const
{
    return dynamicDowncast<HTMLMediaElement>(parentElement());
}

void HTMLTrackElement::eventListenersDidChange()
{
    m_hasRelevantLoadEventsListener = hasEventListeners(eventNames().errorEvent)
        || hasEventListeners(eventNames().loadEvent);
}

bool HTMLTrackElement::virtualHasPendingActivity() const
{
    return m_hasRelevantLoadEventsListener && readyState() == HTMLTrackElement::LOADING;
}

}

#endif
