/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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
#include "MathMLAnchorElement.h"

#include "AnchorElementUtils.h"
#include "DOMTokenList.h"
#include "ElementInlines.h"
#include "Event.h"
#include "EventNames.h"
#include "KeyboardEvent.h"
#include "MathMLNames.h"
#include "MouseEvent.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MathMLAnchorElement);

MathMLAnchorElement::MathMLAnchorElement(const QualifiedName& tagName, Document& document)
    : MathMLElement(tagName, document)
{
    ASSERT(hasTagName(MathMLNames::aTag));
}

MathMLAnchorElement::~MathMLAnchorElement() = default;

Ref<MathMLAnchorElement> MathMLAnchorElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new MathMLAnchorElement(tagName, document));
}

URL MathMLAnchorElement::href() const
{
    return protect(document())->encodingParseURL(attributeWithoutSynchronization(MathMLNames::hrefAttr));
}

URL MathMLAnchorElement::hrefURL() const
{
    return href();
}

AtomString MathMLAnchorElement::target() const
{
    return attributeWithoutSynchronization(MathMLNames::targetAttr);
}

void MathMLAnchorElement::setFullURL(const URL& fullURL)
{
    setAttributeWithoutSynchronization(MathMLNames::hrefAttr, AtomString { fullURL.string() });
}

void MathMLAnchorElement::defaultEventHandler(Event& event)
{
    if (isLink()) {
        if (focused() && KeyboardEvent::isEnterKeyKeydownEvent(event)) {
            event.setDefaultHandled();
            dispatchSimulatedClick(&event);
            return;
        }

        if (MouseEvent::canTriggerActivationBehavior(event)) {
            event.setDefaultHandled();
            URL completedURL = hrefURL();

            AtomString downloadAttr = AnchorElementUtils::parseDownloadAttribute(*this, completedURL, MathMLNames::downloadAttr);
            auto referrerPolicy = AnchorElementUtils::effectiveReferrerPolicy(m_linkRelations, this->referrerPolicy());

            AnchorElementUtils::sendPings(*this, MathMLNames::pingAttr, completedURL);

            AnchorElementUtils::navigateHyperlink(
                *this,
                event,
                completedURL,
                effectiveTarget(),
                m_linkRelations,
                referrerPolicy,
                downloadAttr);
            return;
        }
    }

    MathMLElement::defaultEventHandler(event);
}

void MathMLAnchorElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    MathMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);

    if (name == MathMLNames::relAttr) {
        m_linkRelations = AnchorElementUtils::relationsForRelAttribute(newValue);
        if (m_relList)
            m_relList->associatedAttributeValueChanged();
    }
}

// Falls back to using <base> element's target if the anchor does not have one.
AtomString MathMLAnchorElement::effectiveTarget() const
{
    auto effectiveTarget = target();
    if (effectiveTarget.isEmpty())
        effectiveTarget = document().baseTarget();
    return makeTargetBlankIfHasDanglingMarkup(effectiveTarget);
}

DOMTokenList& MathMLAnchorElement::relList()
{
    if (!m_relList)
        lazyInitialize(m_relList, makeUniqueWithoutRefCountedCheck<DOMTokenList>(*this, MathMLNames::relAttr, AnchorElementUtils::isSupportedRelToken));

    return *m_relList;
}

String MathMLAnchorElement::referrerPolicyForBindings() const
{
    return referrerPolicyToString(referrerPolicy());
}

ReferrerPolicy MathMLAnchorElement::referrerPolicy() const
{
    return parseReferrerPolicy(attributeWithoutSynchronization(MathMLNames::referrerpolicyAttr), ReferrerPolicySource::ReferrerPolicyAttribute).value_or(ReferrerPolicy::EmptyString);
}

} // namespace WebCore
