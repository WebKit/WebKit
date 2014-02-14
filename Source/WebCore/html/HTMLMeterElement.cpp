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
#if ENABLE(METER_ELEMENT)
#include "HTMLMeterElement.h"

#include "Attribute.h"
#include "ElementIterator.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FormDataList.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "MeterShadowElement.h"
#include "Page.h"
#include "RenderMeter.h"
#include "RenderTheme.h"
#include "ShadowRoot.h"

namespace WebCore {

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(const QualifiedName& tagName, Document& document)
    : LabelableElement(tagName, document)
{
    ASSERT(hasTagName(meterTag));
}

HTMLMeterElement::~HTMLMeterElement()
{
}

PassRefPtr<HTMLMeterElement> HTMLMeterElement::create(const QualifiedName& tagName, Document& document)
{
    RefPtr<HTMLMeterElement> meter = adoptRef(new HTMLMeterElement(tagName, document));
    meter->ensureUserAgentShadowRoot();
    return meter;
}

RenderPtr<RenderElement> HTMLMeterElement::createElementRenderer(PassRef<RenderStyle> style)
{
    if (!document().page()->theme().supportsMeter(style.get().appearance()))
        return RenderElement::createFor(*this, std::move(style));

    return createRenderer<RenderMeter>(*this, std::move(style));
}

bool HTMLMeterElement::childShouldCreateRenderer(const Node& child) const
{
    return hasShadowRootParent(child) && HTMLElement::childShouldCreateRenderer(child);
}

void HTMLMeterElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == valueAttr || name == minAttr || name == maxAttr || name == lowAttr || name == highAttr || name == optimumAttr)
        didElementStateChange();
    else
        LabelableElement::parseAttribute(name, value);
}

double HTMLMeterElement::min() const
{
    return parseToDoubleForNumberType(getAttribute(minAttr), 0);
}

void HTMLMeterElement::setMin(double min, ExceptionCode& ec)
{
    if (!std::isfinite(min)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(minAttr, AtomicString::number(min));
}

double HTMLMeterElement::max() const
{
    return std::max(parseToDoubleForNumberType(getAttribute(maxAttr), std::max(1.0, min())), min());
}

void HTMLMeterElement::setMax(double max, ExceptionCode& ec)
{
    if (!std::isfinite(max)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(maxAttr, AtomicString::number(max));
}

double HTMLMeterElement::value() const
{
    double value = parseToDoubleForNumberType(getAttribute(valueAttr), 0);
    return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value, ExceptionCode& ec)
{
    if (!std::isfinite(value)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(valueAttr, AtomicString::number(value));
}

double HTMLMeterElement::low() const
{
    double low = parseToDoubleForNumberType(getAttribute(lowAttr), min());
    return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low, ExceptionCode& ec)
{
    if (!std::isfinite(low)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(lowAttr, AtomicString::number(low));
}

double HTMLMeterElement::high() const
{
    double high = parseToDoubleForNumberType(getAttribute(highAttr), max());
    return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high, ExceptionCode& ec)
{
    if (!std::isfinite(high)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(highAttr, AtomicString::number(high));
}

double HTMLMeterElement::optimum() const
{
    double optimum = parseToDoubleForNumberType(getAttribute(optimumAttr), (max() + min()) / 2);
    return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum, ExceptionCode& ec)
{
    if (!std::isfinite(optimum)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(optimumAttr, AtomicString::number(optimum));
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

void HTMLMeterElement::didElementStateChange()
{
    m_value->setWidthPercentage(valueRatio()*100);
    m_value->updatePseudo();
    if (RenderMeter* render = renderMeter())
        render->updateFromElement();
}

RenderMeter* HTMLMeterElement::renderMeter() const
{
    if (renderer() && renderer()->isMeter())
        return toRenderMeter(renderer());
    return toRenderMeter(descendantsOfType<Element>(*userAgentShadowRoot()).first()->renderer());
}

void HTMLMeterElement::didAddUserAgentShadowRoot(ShadowRoot* root)
{
    ASSERT(!m_value);

    RefPtr<MeterInnerElement> inner = MeterInnerElement::create(document());
    root->appendChild(inner);

    RefPtr<MeterBarElement> bar = MeterBarElement::create(document());
    m_value = MeterValueElement::create(document());
    m_value->setWidthPercentage(0);
    m_value->updatePseudo();
    bar->appendChild(m_value, ASSERT_NO_EXCEPTION);

    inner->appendChild(bar, ASSERT_NO_EXCEPTION);
}

} // namespace
#endif
