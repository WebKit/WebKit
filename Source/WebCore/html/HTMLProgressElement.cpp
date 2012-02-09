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
#include "HTMLDivElement.h"
#include "HTMLFormElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "ProgressShadowElement.h"
#include "RenderProgress.h"
#include "ShadowRoot.h"
#include <wtf/StdLibExtras.h>

namespace WebCore {

using namespace HTMLNames;

const double HTMLProgressElement::IndeterminatePosition = -1;
const double HTMLProgressElement::InvalidPosition = -2;

HTMLProgressElement::HTMLProgressElement(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
    : HTMLFormControlElement(tagName, document, form)
{
    ASSERT(hasTagName(progressTag));
}

HTMLProgressElement::~HTMLProgressElement()
{
}

PassRefPtr<HTMLProgressElement> HTMLProgressElement::create(const QualifiedName& tagName, Document* document, HTMLFormElement* form)
{
    RefPtr<HTMLProgressElement> progress = adoptRef(new HTMLProgressElement(tagName, document, form));
    progress->createShadowSubtree();
    return progress;
}

RenderObject* HTMLProgressElement::createRenderer(RenderArena* arena, RenderStyle*)
{
    return new (arena) RenderProgress(this);
}

bool HTMLProgressElement::supportsFocus() const
{
    return Node::supportsFocus() && !disabled();
}

const AtomicString& HTMLProgressElement::formControlType() const
{
    DEFINE_STATIC_LOCAL(const AtomicString, progress, ("progress"));
    return progress;
}

void HTMLProgressElement::parseAttribute(Attribute* attribute)
{
    if (attribute->name() == valueAttr)
        didElementStateChange();
    else if (attribute->name() == maxAttr)
        didElementStateChange();
    else
        HTMLFormControlElement::parseAttribute(attribute);
}

void HTMLProgressElement::attach()
{
    HTMLFormControlElement::attach();
    didElementStateChange();
}

double HTMLProgressElement::value() const
{
    double value;
    bool ok = parseToDoubleForNumberType(fastGetAttribute(valueAttr), &value);
    if (!ok || value < 0)
        return 0;
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
    if (!isDeterminate())
        return HTMLProgressElement::IndeterminatePosition;
    return value() / max();
}

bool HTMLProgressElement::isDeterminate() const
{
    return fastHasAttribute(valueAttr);
}
    
void HTMLProgressElement::didElementStateChange()
{
    m_value->setWidthPercentage(position() * 100);
    if (renderer()) {
        RenderProgress* render = toRenderProgress(renderer());
        bool wasDeterminate = render->isDeterminate();
        renderer()->updateFromElement();
        if (wasDeterminate != isDeterminate())
            setNeedsStyleRecalc();
    }
}

void HTMLProgressElement::createShadowSubtree()
{
    ASSERT(!shadowRoot());

    RefPtr<ProgressBarElement> bar = ProgressBarElement::create(document());
    m_value = ProgressValueElement::create(document());
    bar->appendChild(m_value, ASSERT_NO_EXCEPTION);

    RefPtr<ShadowRoot> root = ShadowRoot::create(this, ASSERT_NO_EXCEPTION);
    root->appendChild(bar, ASSERT_NO_EXCEPTION);
}

} // namespace
#endif
