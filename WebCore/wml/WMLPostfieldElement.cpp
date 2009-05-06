/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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

#if ENABLE(WML)
#include "WMLPostfieldElement.h"

#include "CString.h"
#include "TextEncoding.h"
#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "WMLDocument.h"
#include "WMLGoElement.h"
#include "WMLNames.h"

namespace WebCore {

using namespace WMLNames;

WMLPostfieldElement::WMLPostfieldElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

void WMLPostfieldElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr)
        m_name = parseValueSubstitutingVariableReferences(attr->value());
    else if (attr->name() == HTMLNames::valueAttr)
        m_value = parseValueSubstitutingVariableReferences(attr->value());
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLPostfieldElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();

    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent->hasTagName(goTag))
        return;

    static_cast<WMLGoElement*>(parent)->registerPostfieldElement(this);
}

static inline CString encodedString(const TextEncoding& encoding, const String& data)
{
    return encoding.encode(data.characters(), data.length(), EntitiesForUnencodables);
}

void WMLPostfieldElement::encodeData(const TextEncoding& encoding, CString& name, CString& value)
{
    name = encodedString(encoding, m_name);
    value = encodedString(encoding, m_value);
}

}

#endif
