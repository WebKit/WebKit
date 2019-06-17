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

#if ENABLE(METER_ELEMENT)

#include "Attribute.h"
#include "ElementIterator.h"
#include "HTMLDivElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLStyleElement.h"
#include "Page.h"
#include "RenderMeter.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"
#include "UserAgentStyleSheets.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLMeterElement);

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(const QualifiedName& tagName, Document& document)
    : LabelableElement(tagName, document)
{
    ASSERT(hasTagName(meterTag));
}

HTMLMeterElement::~HTMLMeterElement() = default;

Ref<HTMLMeterElement> HTMLMeterElement::create(const QualifiedName& tagName, Document& document)
{
    Ref<HTMLMeterElement> meter = adoptRef(*new HTMLMeterElement(tagName, document));
    meter->ensureUserAgentShadowRoot();
    return meter;
}

RenderPtr<RenderElement> HTMLMeterElement::createElementRenderer(RenderStyle&& style, const RenderTreePosition&)
{
    if (!RenderTheme::singleton().supportsMeter(style.appearance()))
        return RenderElement::createFor(*this, WTFMove(style));

    return createRenderer<RenderMeter>(*this, WTFMove(style));
}

bool HTMLMeterElement::childShouldCreateRenderer(const Node& child) const
{
    return !is<RenderMeter>(renderer()) && HTMLElement::childShouldCreateRenderer(child);
}

void HTMLMeterElement::parseAttribute(const QualifiedName& name, const AtomString& value)
{
    if (name == valueAttr || name == minAttr || name == maxAttr || name == lowAttr || name == highAttr || name == optimumAttr)
        didElementStateChange();
    else
        LabelableElement::parseAttribute(name, value);
}

double HTMLMeterElement::min() const
{
    return parseToDoubleForNumberType(attributeWithoutSynchronization(minAttr), 0);
}

void HTMLMeterElement::setMin(double min)
{
    setAttributeWithoutSynchronization(minAttr, AtomString::number(min));
}

double HTMLMeterElement::max() const
{
    return std::max(parseToDoubleForNumberType(attributeWithoutSynchronization(maxAttr), std::max(1.0, min())), min());
}

void HTMLMeterElement::setMax(double max)
{
    setAttributeWithoutSynchronization(maxAttr, AtomString::number(max));
}

double HTMLMeterElement::value() const
{
    double value = parseToDoubleForNumberType(attributeWithoutSynchronization(valueAttr), 0);
    return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value)
{
    setAttributeWithoutSynchronization(valueAttr, AtomString::number(value));
}

double HTMLMeterElement::low() const
{
    double low = parseToDoubleForNumberType(attributeWithoutSynchronization(lowAttr), min());
    return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low)
{
    setAttributeWithoutSynchronization(lowAttr, AtomString::number(low));
}

double HTMLMeterElement::high() const
{
    double high = parseToDoubleForNumberType(attributeWithoutSynchronization(highAttr), max());
    return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high)
{
    setAttributeWithoutSynchronization(highAttr, AtomString::number(high));
}

double HTMLMeterElement::optimum() const
{
    double optimum = parseToDoubleForNumberType(attributeWithoutSynchronization(optimumAttr), (max() + min()) / 2);
    return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum)
{
    setAttributeWithoutSynchronization(optimumAttr, AtomString::number(optimum));
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
        element.setAttribute(HTMLNames::classAttr, "optimum");
        element.setPseudo("-webkit-meter-optimum-value");
        return;
    case HTMLMeterElement::GaugeRegionSuboptimal:
        element.setAttribute(HTMLNames::classAttr, "suboptimum");
        element.setPseudo("-webkit-meter-suboptimum-value");
        return;
    case HTMLMeterElement::GaugeRegionEvenLessGood:
        element.setAttribute(HTMLNames::classAttr, "even-less-good");
        element.setPseudo("-webkit-meter-even-less-good-value");
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void HTMLMeterElement::didElementStateChange()
{
    m_value->setInlineStyleProperty(CSSPropertyWidth, valueRatio()*100, CSSPrimitiveValue::CSS_PERCENTAGE);
    setValueClass(*m_value, gaugeRegion());

    if (RenderMeter* render = renderMeter())
        render->updateFromElement();
}

RenderMeter* HTMLMeterElement::renderMeter() const
{
    if (is<RenderMeter>(renderer()))
        return downcast<RenderMeter>(renderer());
    return nullptr;
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot& root)
{
    ASSERT(!m_value);

    static NeverDestroyed<String> shadowStyle(meterElementShadowUserAgentStyleSheet, String::ConstructFromLiteral);

    auto style = HTMLStyleElement::create(HTMLNames::styleTag, document(), false);
    style->setTextContent(shadowStyle);
    root.appendChild(style);

    // Pseudos are set to allow author styling.
    auto inner = HTMLDivElement::create(document());
    inner->setIdAttribute("inner");
    inner->setPseudo("-webkit-meter-inner-element");
    root.appendChild(inner);

    auto bar = HTMLDivElement::create(document());
    bar->setIdAttribute("bar");
    bar->setPseudo("-webkit-meter-bar");
    inner->appendChild(bar);

    m_value = HTMLDivElement::create(document());
    m_value->setIdAttribute("value");
    bar->appendChild(*m_value);

    didElementStateChange();
}

} // namespace
#endif
