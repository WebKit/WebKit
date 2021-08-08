/*
 * Copyright (C) 2006-2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NavigationAction.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "HistoryItem.h"
#include "MouseEvent.h"

namespace WebCore {

static GlobalFrameIdentifier createGlobalFrameIdentifier(const Document& document)
{
    if (document.frame())
        return { document.frame()->loader().pageID().value_or(PageIdentifier { }), document.frame()->loader().frameID().value_or(FrameIdentifier { }) };
    return GlobalFrameIdentifier();
}

NavigationAction::Requester::Requester(const Document& document)
    : m_url { URL { document.url() } }
    , m_origin { &document.securityOrigin() }
    , m_topOrigin { &document.topOrigin() }
    , m_crossOriginOpenerPolicy { document.crossOriginOpenerPolicy() }
    , m_globalFrameIdentifier(createGlobalFrameIdentifier(document))
{
}

NavigationAction::UIEventWithKeyStateData::UIEventWithKeyStateData(const UIEventWithKeyState& uiEvent)
    : isTrusted { uiEvent.isTrusted() }
    , shiftKey { uiEvent.shiftKey() }
    , ctrlKey { uiEvent.ctrlKey() }
    , altKey { uiEvent.altKey() }
    , metaKey { uiEvent.metaKey() }
{
}

NavigationAction::MouseEventData::MouseEventData(const MouseEvent& mouseEvent)
    : UIEventWithKeyStateData { mouseEvent }
    , absoluteLocation { mouseEvent.absoluteLocation() }
    , locationInRootViewCoordinates { mouseEvent.locationInRootViewCoordinates() }
    , button { mouseEvent.button() }
    , syntheticClickType { mouseEvent.syntheticClickType() }
    , buttonDown { mouseEvent.buttonDown() }
{
}

NavigationAction::NavigationAction() = default;
NavigationAction::~NavigationAction() = default;

NavigationAction::NavigationAction(const NavigationAction&) = default;
NavigationAction::NavigationAction(NavigationAction&&) = default;

NavigationAction& NavigationAction::operator=(const NavigationAction&) = default;
NavigationAction& NavigationAction::operator=(NavigationAction&&) = default;

static bool shouldTreatAsSameOriginNavigation(const Document& document, const URL& url)
{
    return url.protocolIsAbout() || url.protocolIsData() || (url.protocolIsBlob() && document.securityOrigin().canRequest(url));
}

static std::optional<NavigationAction::UIEventWithKeyStateData> keyStateDataForFirstEventWithKeyState(Event* event)
{
    if (UIEventWithKeyState* uiEvent = findEventWithKeyState(event))
        return NavigationAction::UIEventWithKeyStateData { *uiEvent };
    return std::nullopt;
}

static std::optional<NavigationAction::MouseEventData> mouseEventDataForFirstMouseEvent(Event* event)
{
    for (Event* e = event; e; e = e->underlyingEvent()) {
        if (e->isMouseEvent())
            return NavigationAction::MouseEventData { static_cast<const MouseEvent&>(*e) };
    }
    return std::nullopt;
}

NavigationAction::NavigationAction(Document& requester, const ResourceRequest& resourceRequest, InitiatedByMainFrame initiatedByMainFrame, NavigationType type, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, Event* event, const AtomString& downloadAttribute)
    : m_requester { requester }
    , m_resourceRequest { resourceRequest }
    , m_type { type }
    , m_shouldOpenExternalURLsPolicy { shouldOpenExternalURLsPolicy }
    , m_initiatedByMainFrame { initiatedByMainFrame }
    , m_keyStateEventData { keyStateDataForFirstEventWithKeyState(event) }
    , m_mouseEventData { mouseEventDataForFirstMouseEvent(event) }
    , m_downloadAttribute { downloadAttribute }
    , m_treatAsSameOriginNavigation { shouldTreatAsSameOriginNavigation(requester, resourceRequest.url()) }
{
}

static NavigationType navigationType(FrameLoadType frameLoadType, bool isFormSubmission, bool haveEvent)
{
    if (isFormSubmission)
        return NavigationType::FormSubmitted;
    if (haveEvent)
        return NavigationType::LinkClicked;
    if (isReload(frameLoadType))
        return NavigationType::Reload;
    if (isBackForwardLoadType(frameLoadType))
        return NavigationType::BackForward;
    return NavigationType::Other;
}

NavigationAction::NavigationAction(Document& requester, const ResourceRequest& resourceRequest, InitiatedByMainFrame initiatedByMainFrame, FrameLoadType frameLoadType, bool isFormSubmission, Event* event, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, const AtomString& downloadAttribute)
    : m_requester { requester }
    , m_resourceRequest { resourceRequest }
    , m_type { navigationType(frameLoadType, isFormSubmission, !!event) }
    , m_shouldOpenExternalURLsPolicy { shouldOpenExternalURLsPolicy }
    , m_initiatedByMainFrame { initiatedByMainFrame }
    , m_keyStateEventData { keyStateDataForFirstEventWithKeyState(event) }
    , m_mouseEventData { mouseEventDataForFirstMouseEvent(event) }
    , m_downloadAttribute { downloadAttribute }
    , m_treatAsSameOriginNavigation { shouldTreatAsSameOriginNavigation(requester, resourceRequest.url()) }
{
}

NavigationAction NavigationAction::copyWithShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy) const
{
    NavigationAction result(*this);
    result.m_shouldOpenExternalURLsPolicy = shouldOpenExternalURLsPolicy;
    return result;
}

void NavigationAction::setTargetBackForwardItem(HistoryItem& item)
{
    m_targetBackForwardItemIdentifier = item.identifier();
}

void NavigationAction::setSourceBackForwardItem(HistoryItem* item)
{
    m_sourceBackForwardItemIdentifier = item ? std::make_optional(item->identifier()) : std::nullopt;
}

}
