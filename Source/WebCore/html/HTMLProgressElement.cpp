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
#if ENABLE(PROGRESS_TAG)
#include "HTMLProgressElement.h"

#include "Attribute.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FormDataList.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "RenderProgress.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

HTMLProgressElement::HTMLProgressElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
{
    ASSERT(hasTagName(progressTag));
}

PassRefPtr<HTMLProgressElement> HTMLProgressElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    return adoptRef(new HTMLProgressElement(tagName, document, form));
}

RenderObject* HTMLProgressElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderProgress(this);
}

const AtomicString& HTMLProgressElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, progress, ("progress"));
    return progress;
}

void HTMLProgressElement::parseMappedAttribute(Attribute* attribute)
{
    if (attribute->name() == valueAttr) {
        if (renderer())
            renderer()->updateFromElement();
    } else if (attribute->name() == maxAttr) {
        if (renderer())
            renderer()->updateFromElement();
    } else
        HTMLFormControlElement::parseMappedAttribute(attribute);
}

void HTMLProgressElement::attach()
{
    HTMLFormControlElement::attach();
    if (renderer())
        renderer()->updateFromElement();
}

double HTMLProgressElement::value() const
{
    const AtomicString& valueString = getAttribute(valueAttr);
    double value;
    bool ok = parseToDoubleForNumberType(valueString, &value);
    if (!ok || value < 0)
        return valueString.isNull() ? 1 : 0;
    return (value > max()) ? max() : value;
}

void HTMLProgressElement::setValue(double value, ExceptionCode& ec)
{
    if (!isfinite(value)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(valueAttr, String::number(value >= 0 ? value : 0));
}

double HTMLProgressElement::max() const
{
    double max;
    bool ok = parseToDoubleForNumberType(getAttribute(maxAttr), &max);
    if (!ok || max <= 0)
        return 1;
    return max;
}

void HTMLProgressElement::setMax(double max, ExceptionCode& ec)
{
    if (!isfinite(max)) {
        ec = NOT_SUPPORTED_ERR;
        return;
    }
    setAttribute(maxAttr, String::number(max > 0 ? max : 1));
}

double HTMLProgressElement::position() const
{
    if (!hasAttribute(valueAttr))
        return -1;
    return value() / max();
}

} // namespace
#endif
