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
#include "HTMLMeterElement.h"

#include "Attribute.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLStyleElement.h"
#include "NodeName.h"
#include "RenderMeter.h"
#include "RenderTheme.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "UserAgentParts.h"
#include "UserAgentStyleSheets.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(HTMLMeterElement);

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(meterTag));
}

HTMLMeterElement::~HTMLMeterElement() = default;

Ref<HTMLMeterElement> HTMLMeterElement::create(const QualifiedName& tagName, Document& document)
{
    Ref meter = adoptRef(*new HTMLMeterElement(tagName, document));
    meter->ensureUserAgentShadowRoot();
    return meter;
}

RenderPtr<RenderElement> HTMLMeterElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (!RenderTheme::singleton().supportsMeter(style.usedAppearance()))
        return RenderElement::createFor(*this, WTF::move(style));

    return createRenderer<RenderMeter>(*this, WTF::move(style));
}

bool HTMLMeterElement::childShouldCreateRenderer(const Node& child) const
{
    return !is<RenderMeter>(renderer()) && HTMLElement::childShouldCreateRenderer(child);
}

void HTMLMeterElement::attributeChanged(const QualifiedName& name, const AtomString& oldValue, const AtomString& newValue, AttributeModificationReason attributeModificationReason)
{
    switch (name.nodeName()) {
    case AttributeNames::valueAttr:
    case AttributeNames::minAttr:
    case AttributeNames::maxAttr:
    case AttributeNames::lowAttr:
    case AttributeNames::highAttr:
    case AttributeNames::optimumAttr:
        didChangeElementValue();
        break;
    default:
        HTMLElement::attributeChanged(name, oldValue, newValue, attributeModificationReason);
        break;
    }
}

double HTMLMeterElement::min() const
{
    return parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(minAttr), 0);
}

double HTMLMeterElement::max() const
{
    return std::max(parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(maxAttr), std::max(1.0, min())), min());
}

double HTMLMeterElement::value() const
{
    double value = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(valueAttr), 0);
    return std::min(std::max(value, min()), max());
}

double HTMLMeterElement::low() const
{
    double low = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(lowAttr), min());
    return std::min(std::max(low, min()), max());
}

double HTMLMeterElement::high() const
{
    double high = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(highAttr), max());
    return std::min(std::max(high, low()), max());
}

double HTMLMeterElement::optimum() const
{
    double optimum = parseHTMLFloatingPointNumberValue(attributeWithoutSynchronization(optimumAttr), std::midpoint(min(), max()));
    return std::clamp(optimum, min(), max());
}

HTMLMeterElement::GaugeRegion HTMLMeterElement::gaugeRegion() const
{
    double lowValue = low();
    double highValue = high();
    double theValue = value();
    double optimumValue = optimum();

    if (optimumValue < lowValue) {
        // The optimum range stays under low
        if (theValue <= lowValue)
            return GaugeRegion::Optimum;
        if (theValue <= highValue)
            return GaugeRegion::Suboptimal;
        return GaugeRegion::EvenLessGood;
    }

    if (highValue < optimumValue) {
        // The optimum range stays over high
        if (highValue <= theValue)
            return GaugeRegion::Optimum;
        if (lowValue <= theValue)
            return GaugeRegion::Suboptimal;
        return GaugeRegion::EvenLessGood;
    }

    // The optimum range stays between high and low.
    // According to the standard, <meter> never show GaugeRegionEvenLessGood in this case
    // because the value is never less or greater than min or max.
    if (lowValue <= theValue && theValue <= highValue)
        return GaugeRegion::Optimum;
    return GaugeRegion::Suboptimal;
}

double HTMLMeterElement::valueRatio() const
{
    double min = this->min();
    double max = this->max();
    double value = this->value();

    if (max <= min)
        return 0;
    return (value - min) / (max - min);
}

static void setValueClass(HTMLElement& element, HTMLMeterElement::GaugeRegion gaugeRegion)
{
    switch (gaugeRegion) {
    case HTMLMeterElement::GaugeRegion::Optimum:
        element.setAttribute(HTMLNames::classAttr, "optimum"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterOptimumValue());
        return;
    case HTMLMeterElement::GaugeRegion::Suboptimal:
        element.setAttribute(HTMLNames::classAttr, "suboptimum"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterSuboptimumValue());
        return;
    case HTMLMeterElement::GaugeRegion::EvenLessGood:
        element.setAttribute(HTMLNames::classAttr, "even-less-good"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterEvenLessGoodValue());
        return;
    }
    ASSERT_NOT_REACHED();
}

void HTMLMeterElement::didChangeElementValue()
{
    if (RefPtr valueElement = m_valueElement) {
        valueElement->setInlineStyleProperty(CSSPropertyInlineSize, valueRatio() * 100, CSSUnitType::CSS_PERCENTAGE);
        setValueClass(*valueElement, gaugeRegion());
    }

    if (RefPtr fillElement = m_fillElement) {
        fillElement->setInlineStyleProperty(CSSPropertyTransform, makeString("translate(-"_s, (1 - valueRatio()) * 100, "%, 0)"_s));
        fillElement->invalidateStyleInternal();
    }
}

RenderMeter* HTMLMeterElement::renderMeter() const
{
    return dynamicDowncast<RenderMeter>(renderer());
}

void HTMLMeterElement::appendShadowTreeForAutoAppearance(ShadowRoot& root)
{
    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(meterElementShadowUserAgentStyleSheet));

    Ref document = this->document();
    Ref styleElement = HTMLStyleElement::create(document);
    ScriptDisallowedScope::EventAllowedScope styleScope { styleElement };
    styleElement->setTextContent(String { shadowStyle });
    ScriptDisallowedScope::EventAllowedScope rootScope { root };
    root.appendChild(WTF::move(styleElement));

    // Pseudos are set to allow author styling.
    Ref innerElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope innerScope { innerElement };
    innerElement->setIdAttribute("inner"_s);
    innerElement->setUserAgentPart(UserAgentParts::webkitMeterInnerElement());
    innerElement->setInlineStyleProperty(CSSPropertyDisplay, "-internal-auto-base(inline-block, none) !important"_s);
    root.appendChild(innerElement);

    Ref barElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope barScope { barElement };
    barElement->setIdAttribute("bar"_s);
    barElement->setUserAgentPart(UserAgentParts::webkitMeterBar());
    innerElement->appendChild(barElement);

    Ref valueElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope valueElementScope { valueElement };
    valueElement->setIdAttribute("value"_s);
    barElement->appendChild(valueElement);
    m_valueElement = valueElement;
}

void HTMLMeterElement::appendShadowTreeForBaseAppearance(ShadowRoot& root)
{
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
    m_fillElement = fillElement;
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    appendShadowTreeForAutoAppearance(root);
    if (document().settings().cssAppearanceBaseEnabled())
        appendShadowTreeForBaseAppearance(root);
    didChangeElementValue();
}

} // namespace
