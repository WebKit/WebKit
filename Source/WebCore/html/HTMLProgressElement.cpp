/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "HTMLProgressElement.h"

#include "AXObjectCache.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderProgress.h"
#include "RenderStyle+GettersInlines.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "UserAgentParts.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLProgressElement);

using namespace HTMLNames;

HTMLProgressElement::HTMLProgressElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
    ASSERT(hasTagName(progressTag));
}

Ref<HTMLProgressElement> HTMLProgressElement::create(const QualifiedName& tagName, Document& document)
{
    Ref progress = adoptRef(*new HTMLProgressElement(tagName, document));
    progress->ensureUserAgentShadowRoot();
    return progress;
}

RenderPtr<RenderElement> HTMLProgressElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (!style.hasUsedAppearance())
        return RenderElement::createFor(*this, WTF::move(style));

    return createRenderer<RenderProgress>(*this, WTF::move(style));
}

RenderProgress* HTMLProgressElement::renderProgress() const
{
    return dynamicDowncast<RenderProgress>(renderer());
}

bool HTMLProgressElement::childShouldCreateRenderer(const Node& child) const
{
    return !is<RenderProgress>(renderer()) && HTMLElement::childShouldCreateRenderer(child);
}

void HTMLProgressElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::valueAttr:
        updateDeterminateState();
        didChangeElementValue();
        return;

    case AttributeNames::maxAttr:
        didChangeElementValue();
        return;

    default:
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
    }
}

void HTMLProgressElement::didAttachRenderers()
{
    if (CheckedPtr renderer = renderProgress())
        renderer->updateFromElement();
}

double HTMLProgressElement::value() const
{
    double value = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(valueAttr));
    return !std::isfinite(value) || value < 0 ? 0 : std::min(value, max());
}

double HTMLProgressElement::max() const
{
    double max = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(maxAttr));
    return !std::isfinite(max) || max <= 0 ? 1 : max;
}

void HTMLProgressElement::setMax(double max)
{
    if (max > 0)
        setAttributeWithoutSynchronization(maxAttr, AtomString::number(max));
}

double HTMLProgressElement::position() const
{
    if (!isDeterminate())
        return HTMLProgressElement::IndeterminatePosition;
    return value() / max();
}

void HTMLProgressElement::updateDeterminateState()
{
    bool newIsDeterminate = hasAttributeWithoutSynchronization(valueAttr);
    if (m_isDeterminate == newIsDeterminate)
        return;
    Style::PseudoClassChangeInvalidation styleInvalidation(*this, CSSSelector::PseudoClass::Indeterminate, !newIsDeterminate);
    m_isDeterminate = newIsDeterminate;
}

void HTMLProgressElement::didChangeElementValue()
{
    double percentageValue = std::max(0.0, position() * 100);

    if (RefPtr valueElement = m_valueElement)
        valueElement->setInlineStyleProperty(CSSPropertyInlineSize, percentageValue, CSSUnitType::CSS_PERCENTAGE);

    if (RefPtr fillElement = m_fillElement) {
        fillElement->setInlineStyleProperty(CSSPropertyTransform, makeString("translate(-"_s, 100 - percentageValue, "%, 0)"_s));
        fillElement->invalidateStyleInternal();
    }

    if (CheckedPtr renderer = renderProgress())
        renderer->updateFromElement();

    if (CheckedPtr cache = protect(document())->existingAXObjectCache())
        cache->valueChanged(*this);
}

void HTMLProgressElement::appendShadowTreeForAutoAppearance(ShadowRoot& root)
{
    ASSERT(!m_valueElement);

    Ref document = this->document();

    Ref innerElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope innerScope { innerElement };
    innerElement->setUserAgentPart(UserAgentParts::webkitProgressInnerElement());
    innerElement->setInlineStyleProperty(CSSPropertyAppearance, "inherit"_s);
    innerElement->setInlineStyleProperty(CSSPropertyDisplay, "-internal-auto-base(inline-block, none) !important"_s);
    root.appendChild(innerElement);

    Ref barElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope barScope { barElement };
    barElement->setUserAgentPart(UserAgentParts::webkitProgressBar());
    innerElement->appendChild(barElement);

    Ref valueElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope valueElementScope { valueElement };
    valueElement->setUserAgentPart(UserAgentParts::webkitProgressValue());
    barElement->appendChild(valueElement);
    m_valueElement = WTF::move(valueElement);
}

void HTMLProgressElement::appendShadowTreeForBaseAppearance(ShadowRoot& root)
{
    ASSERT(!m_fillElement);

    Ref document = this->document();

    Ref trackElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope trackScope { trackElement };
    trackElement->setUserAgentPart(UserAgentParts::sliderTrack());
    trackElement->setInlineStyleProperty(CSSPropertyAppearance, "inherit"_s);
    trackElement->setInlineStyleProperty(CSSPropertyDisplay, "-internal-auto-base(none, inline-block) !important"_s);
    root.appendChild(trackElement);

    Ref fillElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope fillScope { fillElement };
    fillElement->setUserAgentPart(UserAgentParts::sliderFill());
    trackElement->appendChild(fillElement);
    m_fillElement = WTF::move(fillElement);
}

void HTMLProgressElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    appendShadowTreeForAutoAppearance(root);
    if (document().settings().cssAppearanceBaseEnabled())
        appendShadowTreeForBaseAppearance(root);
    didChangeElementValue();
}

} // namespace
