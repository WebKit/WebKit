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
#if ENABLE(METER_TAG)
#include "HTMLMeterElement.h"

#include "Attribute.h"
#include "EventNames.h"
#include "FormDataList.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParser.h"
#include "RenderMeter.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

HTMLMeterElement::HTMLMeterElement(const QualifiedName& tagName, Document* document)
    : HTMLFormControlElement(tagName, document, 0, CreateHTMLElement)
{
    ASSERT(hasTagName(meterTag));
}

PassRefPtr<HTMLMeterElement> HTMLMeterElement::create(const QualifiedName& tagName, Document* document)
{
    return adoptRef(new HTMLMeterElement(tagName, document));
}

RenderObject* HTMLMeterElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderMeter(this);
}

const AtomicString& HTMLMeterElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, meter, ("meter"));
    return meter;
}

void HTMLMeterElement::parseMappedAttribute(Attribute* attribute)
{
    if (attribute->name() == valueAttr || attribute->name() == minAttr || attribute->name() == maxAttr || attribute->name() == lowAttr || attribute->name() == highAttr || attribute->name() == optimumAttr) {
        if (renderer())
            renderer()->updateFromElement();
    } else
        HTMLFormControlElement::parseMappedAttribute(attribute);
}

double HTMLMeterElement::min() const
{
    double min = 0;
    parseToDoubleForNumberType(getAttribute(minAttr), &min);
    return min;
}

void HTMLMeterElement::setMin(double min, ExceptionCode& ec)
{
    if (!isfinite(min)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(minAttr, String::number(min));
}

double HTMLMeterElement::max() const
{
    double max = std::max(1.0, min());
    parseToDoubleForNumberType(getAttribute(maxAttr), &max);
    return std::max(max, min());
}

void HTMLMeterElement::setMax(double max, ExceptionCode& ec)
{
    if (!isfinite(max)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(maxAttr, String::number(max));
}

double HTMLMeterElement::value() const
{
    double value = 0;
    parseToDoubleForNumberType(getAttribute(valueAttr), &value);
    return std::min(std::max(value, min()), max());
}

void HTMLMeterElement::setValue(double value, ExceptionCode& ec)
{
    if (!isfinite(value)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(valueAttr, String::number(value));
}

double HTMLMeterElement::low() const
{
    double low = min();
    parseToDoubleForNumberType(getAttribute(lowAttr), &low);
    return std::min(std::max(low, min()), max());
}

void HTMLMeterElement::setLow(double low, ExceptionCode& ec)
{
    if (!isfinite(low)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(lowAttr, String::number(low));
}

double HTMLMeterElement::high() const
{
    double high = max();
    parseToDoubleForNumberType(getAttribute(highAttr), &high);
    return std::min(std::max(high, low()), max());
}

void HTMLMeterElement::setHigh(double high, ExceptionCode& ec)
{
    if (!isfinite(high)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(highAttr, String::number(high));
}

double HTMLMeterElement::optimum() const
{
    double optimum = (max() + min()) / 2;
    parseToDoubleForNumberType(getAttribute(optimumAttr), &optimum);
    return std::min(std::max(optimum, min()), max());
}

void HTMLMeterElement::setOptimum(double optimum, ExceptionCode& ec)
{
    if (!isfinite(optimum)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(optimumAttr, String::number(optimum));
}

} // namespace
#endif
