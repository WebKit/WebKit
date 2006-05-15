/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderSlider.h"

#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLInputElement.h"
#include "KWQSlider.h"

using std::min;

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

RenderSlider::RenderSlider(HTMLInputElement* element)
    : RenderFormElement(element)
{
    setWidget(new QSlider);
}

void RenderSlider::calcMinMaxWidth()
{
    ASSERT(!minMaxKnown());
    
    // Let the widget tell us how big it wants to be.
    IntSize s(widget()->sizeHint());
    bool widthSet = !style()->width().isAuto();
    bool heightSet = !style()->height().isAuto();
    if (heightSet && !widthSet) {
        // Flip the intrinsic dimensions.
        int barLength = s.width();
        s = IntSize(s.height(), barLength);
    }
    setIntrinsicWidth(s.width());
    setIntrinsicHeight(s.height());
    
    RenderFormElement::calcMinMaxWidth();
}

void RenderSlider::updateFromElement()
{
    String value = static_cast<HTMLInputElement*>(node())->value();
    const AtomicString& minStr = static_cast<HTMLInputElement*>(node())->getAttribute(minAttr);
    const AtomicString& maxStr = static_cast<HTMLInputElement*>(node())->getAttribute(maxAttr);
    const AtomicString& precision = static_cast<HTMLInputElement*>(node())->getAttribute(precisionAttr);
    
    double minVal = minStr.isNull() ? 0.0 : minStr.deprecatedString().toDouble();
    double maxVal = maxStr.isNull() ? 100.0 : maxStr.deprecatedString().toDouble();
    minVal = min(minVal, maxVal); // Make sure the range is sane.
    
    double val = value.isNull() ? (maxVal + minVal)/2.0 : value.deprecatedString().toDouble();
    val = max(minVal, min(val, maxVal)); // Make sure val is within min/max.
    
    // Force integer value if not float.
    if (!equalIgnoringCase(precision, "float"))
        val = (int)(val + 0.5);

    static_cast<HTMLInputElement*>(node())->setValue(String::number(val));

    QSlider* slider = static_cast<QSlider*>(widget());
     
    slider->setMinValue(minVal);
    slider->setMaxValue(maxVal);
    slider->setValue(val);

    RenderFormElement::updateFromElement();
}

void RenderSlider::valueChanged(Widget*)
{
    QSlider* slider = static_cast<QSlider*>(widget());

    double val = slider->value();
    const AtomicString& precision = static_cast<HTMLInputElement*>(node())->getAttribute(precisionAttr);

    // Force integer value if not float.
    if (!equalIgnoringCase(precision, "float"))
        val = (int)(val + 0.5);

    static_cast<HTMLInputElement*>(node())->setValue(String::number(val));
    
    // Fire the "input" DOM event.
    static_cast<HTMLInputElement*>(node())->dispatchHTMLEvent(inputEvent, true, false);
}

} // namespace WebCore
