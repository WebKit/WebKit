/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "HTMLMarqueeElement.h"

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

 // WinIE uses 60ms as the minimum delay by default.
const int defaultMinimumDelay = 60;

HTMLMarqueeElement::HTMLMarqueeElement(Document* doc)
    : HTMLElement(marqueeTag, doc)
    , m_minimumDelay(defaultMinimumDelay)
{
}

bool HTMLMarqueeElement::mapToEntry(const QualifiedName& attrName, MappedAttributeEntry& result) const
{
    if (attrName == widthAttr ||
        attrName == heightAttr ||
        attrName == bgcolorAttr ||
        attrName == vspaceAttr ||
        attrName == hspaceAttr ||
        attrName == scrollamountAttr ||
        attrName == scrolldelayAttr ||
        attrName == loopAttr ||
        attrName == behaviorAttr ||
        attrName == directionAttr) {
        result = eUniversal;
        return false;
    }

    return HTMLElement::mapToEntry(attrName, result);
}

void HTMLMarqueeElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == widthAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_WIDTH, attr->value());
    } else if (attr->name() == heightAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP_HEIGHT, attr->value());
    } else if (attr->name() == bgcolorAttr) {
        if (!attr->value().isEmpty())
            addCSSColor(attr, CSS_PROP_BACKGROUND_COLOR, attr->value());
    } else if (attr->name() == vspaceAttr) {
        if (!attr->value().isEmpty()) {
            addCSSLength(attr, CSS_PROP_MARGIN_TOP, attr->value());
            addCSSLength(attr, CSS_PROP_MARGIN_BOTTOM, attr->value());
        }
    } else if (attr->name() == hspaceAttr) {
        if (!attr->value().isEmpty()) {
            addCSSLength(attr, CSS_PROP_MARGIN_LEFT, attr->value());
            addCSSLength(attr, CSS_PROP_MARGIN_RIGHT, attr->value());
        }
    } else if (attr->name() == scrollamountAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP__WEBKIT_MARQUEE_INCREMENT, attr->value());
    } else if (attr->name() == scrolldelayAttr) {
        if (!attr->value().isEmpty())
            addCSSLength(attr, CSS_PROP__WEBKIT_MARQUEE_SPEED, attr->value());
    } else if (attr->name() == loopAttr) {
        if (!attr->value().isEmpty()) {
            if (attr->value() == "-1" || equalIgnoringCase(attr->value(), "infinite"))
                addCSSProperty(attr, CSS_PROP__WEBKIT_MARQUEE_REPETITION, CSS_VAL_INFINITE);
            else
                addCSSLength(attr, CSS_PROP__WEBKIT_MARQUEE_REPETITION, attr->value());
        }
    } else if (attr->name() == behaviorAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP__WEBKIT_MARQUEE_STYLE, attr->value());
    } else if (attr->name() == directionAttr) {
        if (!attr->value().isEmpty())
            addCSSProperty(attr, CSS_PROP__WEBKIT_MARQUEE_DIRECTION, attr->value());
    } else if (attr->name() == truespeedAttr)
        m_minimumDelay = !attr->isNull() ? 0 : defaultMinimumDelay;
    else
        HTMLElement::parseMappedAttribute(attr);
}

}
