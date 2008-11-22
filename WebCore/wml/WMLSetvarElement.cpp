/**
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 *               http://www.torchmobile.com/
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

#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "Document.h"
#include "HTMLNames.h"
#include "NodeList.h"
#include "WMLNames.h"
#include "WMLGoElement.h"
#include "WMLPrevElement.h"
#include "WMLRefreshElement.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

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
        const AtomicString& value = attr->value();

        bool isValid = isValidVariableName(value, true);
        if (isValid) {
            m_name = substituteVariableReferences(value, document());
            isValid = isValidVariableName(m_name, true);
        }

        if (!isValid) {
            // FIXME: Error reporting
            // WMLHelper::tokenizer()->reportError(InvalidVariableNameError);
        }
    } else if (attr->name() == HTMLNames::valueAttr)
        m_value = substituteVariableReferences(attr->value(), document());
    else
        WMLElement::parseMappedAttribute(attr);

}

void WMLSetvarElement::insertedIntoDocument()
{
    WMLElement::insertedIntoDocument();
    
    Node* ancestor = parentNode();
    WMLTaskElement* taskElement = 0;

    if (ancestor->hasTagName(goTag))
        taskElement = static_cast<WMLGoElement*>(ancestor);
    else if (ancestor->hasTagName(prevTag))
        taskElement = static_cast<WMLPrevElement*>(ancestor);
    else if (ancestor->hasTagName(refreshTag))
        taskElement = static_cast<WMLRefreshElement*>(ancestor);

    if (taskElement)
        taskElement->registerVariableSetter(this);
}

String WMLSetvarElement::name() const
{
    return m_name;
}

String WMLSetvarElement::value() const
{
    return m_value;
}

}

#endif
