/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Stefan Schimanski (1Stein@gmx.de)
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
 */
#include "config.h"
#include "HTMLParamElement.h"

#include "Document.h"
#include "HTMLNames.h"

namespace WebCore {

using namespace HTMLNames;

HTMLParamElement::HTMLParamElement(Document *doc)
    : HTMLElement(paramTag, doc)
{
}

HTMLParamElement::~HTMLParamElement()
{
}

void HTMLParamElement::parseMappedAttribute(MappedAttribute *attr)
{
    if (attr->name() == idAttr) {
        // Must call base class so that hasID bit gets set.
        HTMLElement::parseMappedAttribute(attr);
        if (document()->htmlMode() != Document::XHtml)
            return;
        m_name = attr->value();
    } else if (attr->name() == nameAttr) {
        m_name = attr->value();
    } else if (attr->name() == valueAttr) {
        m_value = attr->value();
    } else
        HTMLElement::parseMappedAttribute(attr);
}

bool HTMLParamElement::isURLAttribute(Attribute *attr) const
{
    if (attr->name() == valueAttr) {
        Attribute *attr = attributes()->getAttributeItem(nameAttr);
        if (attr) {
            String value = attr->value().domString().lower();
            if (value == "src" || value == "movie" || value == "data")
                return true;
        }
    }
    return false;
}

void HTMLParamElement::setName(const String &value)
{
    setAttribute(nameAttr, value);
}

String HTMLParamElement::type() const
{
    return getAttribute(typeAttr);
}

void HTMLParamElement::setType(const String &value)
{
    setAttribute(typeAttr, value);
}

void HTMLParamElement::setValue(const String &value)
{
    setAttribute(valueAttr, value);
}

String HTMLParamElement::valueType() const
{
    return getAttribute(valuetypeAttr);
}

void HTMLParamElement::setValueType(const String &value)
{
    setAttribute(valuetypeAttr, value);
}

}
