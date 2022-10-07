/*
 * Copyright (C) 2007-2018 Apple Inc. All rights reserved.
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

#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "HTMLPictureElement.h"
#include "Logging.h"
#include "MediaList.h"
#include "MediaQueryParserContext.h"
#include <wtf/IsoMallocInlines.h>

#if ENABLE(VIDEO)
#include "HTMLMediaElement.h"
#endif

#if ENABLE(MODEL_ELEMENT)
#include "HTMLModelElement.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLSourceElement);

using namespace HTMLNames;

inline HTMLSourceElement::HTMLSourceElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
    , ActiveDOMObject(document)
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

Node::InsertedIntoAncestorResult HTMLSourceElement::insertedIntoAncestor(InsertionType insertionType, ContainerNode& parentOfInsertedTree)
{
    HTMLElement::insertedIntoAncestor(insertionType, parentOfInsertedTree);
    RefPtr<Element> parent = parentElement();
    if (parent == &parentOfInsertedTree) {
#if ENABLE(VIDEO)
        if (is<HTMLMediaElement>(*parent))
            downcast<HTMLMediaElement>(*parent).sourceWasAdded(*this);
        else
#endif
#if ENABLE(MODEL_ELEMENT)
        if (is<HTMLModelElement>(*parent))
            downcast<HTMLModelElement>(*parent).sourcesChanged();
        else
#endif
        if (is<HTMLPictureElement>(*parent)) {
            // The new source element only is a relevant mutation if it precedes any img element.
            m_shouldCallSourcesChanged = true;
            for (const Node* node = previousSibling(); node; node = node->previousSibling()) {
                if (is<HTMLImageElement>(*node))
                    m_shouldCallSourcesChanged = false;
            }
            if (m_shouldCallSourcesChanged)
                downcast<HTMLPictureElement>(*parent).sourcesChanged();
        }
    }
    return InsertedIntoAncestorResult::Done;
}

void HTMLSourceElement::removedFromAncestor(RemovalType removalType, ContainerNode& oldParentOfRemovedTree)
{        
    HTMLElement::removedFromAncestor(removalType, oldParentOfRemovedTree);
    if (!parentNode() && is<Element>(oldParentOfRemovedTree)) {
#if ENABLE(VIDEO)
        if (is<HTMLMediaElement>(oldParentOfRemovedTree))
            downcast<HTMLMediaElement>(oldParentOfRemovedTree).sourceWasRemoved(*this);
        else
#endif
#if ENABLE(MODEL_ELEMENT)
        if (is<HTMLModelElement>(oldParentOfRemovedTree))
            downcast<HTMLModelElement>(oldParentOfRemovedTree).sourcesChanged();
        else
#endif
        if (m_shouldCallSourcesChanged) {
            downcast<HTMLPictureElement>(oldParentOfRemovedTree).sourcesChanged();
            m_shouldCallSourcesChanged = false;
        }
    }
}

void HTMLSourceElement::scheduleErrorEvent()
{
    LOG(Media, "HTMLSourceElement::scheduleErrorEvent - %p", this);
    if (m_errorEventCancellationGroup.hasPendingTask())
        return;

    queueCancellableTaskToDispatchEvent(*this, TaskSource::MediaElement, m_errorEventCancellationGroup, Event::create(eventNames().errorEvent, Event::CanBubble::No, Event::IsCancelable::Yes));
}

void HTMLSourceElement::cancelPendingErrorEvent()
{
    LOG(Media, "HTMLSourceElement::cancelPendingErrorEvent - %p", this);
    m_errorEventCancellationGroup.cancel();
}

bool HTMLSourceElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || HTMLElement::isURLAttribute(attribute);
}

const char* HTMLSourceElement::activeDOMObjectName() const
{
    return "HTMLSourceElement";
}

void HTMLSourceElement::stop()
{
    cancelPendingErrorEvent();
}

void HTMLSourceElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    HTMLElement::parseAttribute(name, value);
    if (name == srcsetAttr || name == sizesAttr || name == mediaAttr || name == typeAttr) {
        if (name == mediaAttr)
            m_cachedParsedMediaAttribute = std::nullopt;
        RefPtr parent = parentNode();
        if (m_shouldCallSourcesChanged)
            downcast<HTMLPictureElement>(*parent).sourcesChanged();
    }
#if ENABLE(MODEL_ELEMENT)
    if (name == srcAttr ||  name == typeAttr) {
        RefPtr<Element> parent = parentElement();
        if (is<HTMLModelElement>(parent))
            downcast<HTMLModelElement>(*parent).sourcesChanged();
    }
#endif
}

const MediaQuerySet* HTMLSourceElement::parsedMediaAttribute(Document& document) const
{
    if (!m_cachedParsedMediaAttribute) {
        RefPtr<const MediaQuerySet> parsedAttribute;
        auto& value = attributeWithoutSynchronization(mediaAttr);
        if (!value.isNull())
            parsedAttribute = MediaQuerySet::create(value, MediaQueryParserContext(document));
        m_cachedParsedMediaAttribute = WTFMove(parsedAttribute);
    }
    return m_cachedParsedMediaAttribute.value().get();
}

void HTMLSourceElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason reason)
{
    if (name == widthAttr || name == heightAttr) {
        if (RefPtr parent = parentNode(); is<HTMLPictureElement>(parent))
            downcast<HTMLPictureElement>(*parent).sourceDimensionAttributesChanged(*this);
    }
    HTMLElement::attributeChanged(name, oldValue, newValue, reason);
}

}
