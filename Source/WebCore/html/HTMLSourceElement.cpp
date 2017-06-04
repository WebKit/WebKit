/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#include "HTMLSourceElement.h"

#include "Event.h"
#include "EventNames.h"
#include "HTMLDocument.h"
#include "HTMLNames.h"
#include "HTMLPictureElement.h"
#include "Logging.h"
#include "MediaList.h"

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#endif

namespace WebCore {

using namespace HTMLNames;

inline HTMLSourceElement::HTMLSourceElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(&document)
    , m_errorEventTimer(*this, &HTMLSourceElement::errorEventTimerFired)
{
    LOG(Media, "HTMLSourceElement::HTMLSourceElement - %p", this);
    ASSERT(hasTagName(sourceTag));
}

Ref<HTMLSourceElement> HTMLSourceElement::create(const QualifiedName& tagName, Document& document)
{
    auto sourceElement = adoptRef(*new HTMLSourceElement(tagName, document));
    sourceElement->suspendIfNeeded();
    return sourceElement;
}

Ref<HTMLSourceElement> HTMLSourceElement::create(Document& document)
{
    return create(sourceTag, document);
}

Node::InsertionNotificationRequest HTMLSourceElement::insertedInto(ContainerNode& insertionPoint)
{
    HTMLElement::insertedInto(insertionPoint);
    if (auto* parent = parentElement()) {
#if ENABLE(VIDEO)
        if (is<HTMLMediaElement>(*parent))
            downcast<HTMLMediaElement>(*parent).sourceWasAdded(*this);
        else
#endif
        if (is<HTMLPictureElement>(*parent))
            downcast<HTMLPictureElement>(*parent).sourcesChanged();
    }
    return InsertionDone;
}

void HTMLSourceElement::removedFrom(ContainerNode& removalRoot)
{
    Element* parent = parentElement();
    if (!parent && is<Element>(removalRoot))
        parent = &downcast<Element>(removalRoot);
    if (parent) {
#if ENABLE(VIDEO)
        if (is<HTMLMediaElement>(*parent))
            downcast<HTMLMediaElement>(*parent).sourceWasRemoved(*this);
        else
#endif
        if (is<HTMLPictureElement>(*parent))
            downcast<HTMLPictureElement>(*parent).sourcesChanged();
    }
    HTMLElement::removedFrom(removalRoot);
}

void HTMLSourceElement::scheduleErrorEvent()
{
    LOG(Media, "HTMLSourceElement::scheduleErrorEvent - %p", this);
    if (m_errorEventTimer.isActive())
        return;

    m_errorEventTimer.startOneShot(0_s);
}

void HTMLSourceElement::cancelPendingErrorEvent()
{
    LOG(Media, "HTMLSourceElement::cancelPendingErrorEvent - %p", this);
    m_errorEventTimer.stop();
}

void HTMLSourceElement::errorEventTimerFired()
{
    LOG(Media, "HTMLSourceElement::errorEventTimerFired - %p", this);
    dispatchEvent(Event::create(eventNames().errorEvent, false, true));
}

bool HTMLSourceElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

const char* HTMLSourceElement::activeDOMObjectName() const
{
    return "HTMLSourceElement";
}

bool HTMLSourceElement::canSuspendForDocumentSuspension() const
{
    return true;
}

void HTMLSourceElement::suspend(ReasonForSuspension why)
{
    if (why == PageCache) {
        m_shouldRescheduleErrorEventOnResume = m_errorEventTimer.isActive();
        m_errorEventTimer.stop();
    }
}

void HTMLSourceElement::resume()
{
    if (m_shouldRescheduleErrorEventOnResume) {
        m_errorEventTimer.startOneShot(0_s);
        m_shouldRescheduleErrorEventOnResume = false;
    }
}

void HTMLSourceElement::stop()
{
    cancelPendingErrorEvent();
}

void HTMLSourceElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    HTMLElement::parseAttribute(name, value);
    if (name == srcsetAttr || name == sizesAttr || name == mediaAttr || name == typeAttr) {
        if (name == mediaAttr)
            m_cachedParsedMediaAttribute = std::nullopt;
        auto* parent = parentNode();
        if (is<HTMLPictureElement>(parent))
            downcast<HTMLPictureElement>(*parent).sourcesChanged();
    }
}

const MediaQuerySet* HTMLSourceElement::parsedMediaAttribute() const
{
    if (!m_cachedParsedMediaAttribute) {
        RefPtr<const MediaQuerySet> parsedAttribute;
        auto& value = attributeWithoutSynchronization(mediaAttr);
        if (!value.isNull())
            parsedAttribute = MediaQuerySet::create(value);
        m_cachedParsedMediaAttribute = WTFMove(parsedAttribute);
    }
    return m_cachedParsedMediaAttribute.value().get();
}

}
