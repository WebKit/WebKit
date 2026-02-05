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
#include "HTMLMeterElement.h"

#include "Attribute.h"
#include "ContainerNodeInlines.h"
#include "ElementInlines.h"
#include "ElementIterator.h"
#include "HTMLDivElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLStyleElement.h"
#include "NodeName.h"
#include "Page.h"
#include "RenderMeter.h"
#include "RenderStyle+GettersInlines.h"
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
        didElementStateChange();
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
            return GaugeRegionOptimum;
        if (theValue <= highValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }
    
    if (highValue < optimumValue) {
        // The optimum range stays over high
        if (highValue <= theValue)
            return GaugeRegionOptimum;
        if (lowValue <= theValue)
            return GaugeRegionSuboptimal;
        return GaugeRegionEvenLessGood;
    }

    // The optimum range stays between high and low.
    // According to the standard, <meter> never show GaugeRegionEvenLessGood in this case
    // because the value is never less or greater than min or max.
    if (lowValue <= theValue && theValue <= highValue)
        return GaugeRegionOptimum;
    return GaugeRegionSuboptimal;
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
    case HTMLMeterElement::GaugeRegionOptimum:
        element.setAttribute(HTMLNames::classAttr, "optimum"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterOptimumValue());
        return;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        element.setAttribute(HTMLNames::classAttr, "suboptimum"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterSuboptimumValue());
        return;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        element.setAttribute(HTMLNames::classAttr, "even-less-good"_s);
        element.setUserAgentPart(UserAgentParts::webkitMeterEvenLessGoodValue());
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void HTMLMeterElement::didElementStateChange()
{
    Ref valueElement = *m_valueElement;
    valueElement->setInlineStyleProperty(CSSPropertyInlineSize, valueRatio() * 100, CSSUnitType::CSS_PERCENTAGE);
    setValueClass(valueElement, gaugeRegion());

    if (CheckedPtr renderer = renderMeter())
        renderer->updateFromElement();
}

RenderMeter* HTMLMeterElement::renderMeter() const
{
    return dynamicDowncast<RenderMeter>(renderer());
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_valueElement);

    static MainThreadNeverDestroyed<const String> shadowStyle(StringImpl::createWithoutCopying(meterElementShadowUserAgentStyleSheet));

    Ref document = this->document();
    Ref style = HTMLStyleElement::create(document);
    ScriptDisallowedScope::EventAllowedScope styleScope { style };
    style->setTextContent(String { shadowStyle });
    ScriptDisallowedScope::EventAllowedScope rootScope { root };
    root.appendChild(WTF::move(style));

    // Pseudos are set to allow author styling.
    Ref inner = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope innerScope { inner };
    inner->setIdAttribute("inner"_s);
    inner->setUserAgentPart(UserAgentParts::webkitMeterInnerElement());
    root.appendChild(inner);

    Ref bar = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope barScope { bar };
    bar->setIdAttribute("bar"_s);
    bar->setUserAgentPart(UserAgentParts::webkitMeterBar());
    inner->appendChild(bar);

    Ref valueElement = HTMLDivElement::create(document);
    ScriptDisallowedScope::EventAllowedScope valueElementScope { valueElement };
    valueElement->setIdAttribute("value"_s);
    bar->appendChild(valueElement);
    m_valueElement = WTF::move(valueElement);

    didElementStateChange();
}

} // namespace
