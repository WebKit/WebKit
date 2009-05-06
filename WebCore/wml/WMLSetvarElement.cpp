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
#include "WMLSetvarElement.h"

#include "HTMLNames.h"
#include "MappedAttribute.h"
#include "WMLErrorHandling.h"
#include "WMLTaskElement.h"
#include "WMLVariables.h"

namespace WebCore {

WMLSetvarElement::WMLSetvarElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

WMLSetvarElement::~WMLSetvarElement()
{
}

void WMLSetvarElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == HTMLNames::nameAttr) {
        String name = parseValueSubstitutingVariableReferences(attr->value(), WMLErrorInvalidVariableName);
        if (!isValidVariableName(name)) {
            reportWMLError(document(), WMLErrorInvalidVariableName);
            return;
        }

        m_name = name;
    } else if (attr->name() == HTMLNames::valueAttr)
        m_value = parseValueSubstitutingVariableReferences(attr->value());
    else
        WMLElement::parseMappedAttribute(attr);
}

void WMLSetvarElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();
 
    Node* parent = parentNode();
    ASSERT(parent);

    if (!parent || !parent->isWMLElement())
        return;

    if (static_cast<WMLElement*>(parent)->isWMLTaskElement())
        static_cast<WMLTaskElement*>(parent)->registerVariableSetter(this);
}

}

#endif
