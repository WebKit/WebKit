/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ProgressShadowElement.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderProgress.h"
#include "RenderStyle+GettersInlines.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLProgressElement);

using namespace HTMLNames;

const double HTMLProgressElement::IndeterminatePosition = -1;
const double HTMLProgressElement::InvalidPosition = -2;

HTMLProgressElement::HTMLProgressElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document, TypeFlag::HasCustomStyleResolveCallbacks)
{
    ASSERT(hasTagName(progressTag));
}

HTMLProgressElement::~HTMLProgressElement() = default;

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
    if (auto* renderProgress = dynamicDowncast<RenderProgress>(renderer()))
        return renderProgress;
    return downcast<RenderProgress>(descendantsOfType<Element>(*protect(userAgentShadowRoot())).first()->renderer());
}

RefPtr<ProgressValueElement> HTMLProgressElement::protectedValueElement()
{
    return m_valueElement.get();
}

void HTMLProgressElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    if (name == valueAttr) {
        updateDeterminateState();
        didElementStateChange();
    } else if (name == maxAttr)
        didElementStateChange();
    else
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
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

void HTMLProgressElement::didElementStateChange()
{
    protectedValueElement()->setInlineSizePercentage(position() * 100);
    if (CheckedPtr renderer = renderProgress())
        renderer->updateFromElement();

    if (CheckedPtr cache = protect(document())->existingAXObjectCache())
        cache->valueChanged(*this);
}

void HTMLProgressElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_valueElement);

    Ref document = this->document();
    Ref inner = ProgressInnerElement::create(document);
    ScriptDisallowedScope::EventAllowedScope rootScope { root };
    root.appendChild(inner);

    Ref bar = ProgressBarElement::create(document);
    Ref valueElement = ProgressValueElement::create(document);
    ScriptDisallowedScope::EventAllowedScope valueElementScope { valueElement };
    valueElement->setInlineSizePercentage(HTMLProgressElement::IndeterminatePosition * 100);
    ScriptDisallowedScope::EventAllowedScope barScope { bar };
    bar->appendChild(valueElement);
    m_valueElement = WTF::move(valueElement);

    inner->appendChild(bar);
}

bool HTMLProgressElement::matchesIndeterminatePseudoClass() const
{
    return !isDeterminate();
}

} // namespace
