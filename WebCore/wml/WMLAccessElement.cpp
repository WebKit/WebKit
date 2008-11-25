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
#include "WMLAccessElement.h"

#include "Document.h"
#include "Page.h"
#include "WMLNames.h"
#include "WMLPageState.h"
#include "WMLVariables.h"

namespace WebCore {

using namespace WMLNames;

WMLAccessElement::WMLAccessElement(const QualifiedName& tagName, Document* doc)
    : WMLElement(tagName, doc)
{
}

void WMLAccessElement::parseMappedAttribute(MappedAttribute* attr)
{
    if (attr->name() == domainAttr) {
        if (containsVariableReference(attr->value())) {
            // FIXME: Error rerporting
            // WMLHelper::tokenizer()->reportError(InvalidVariableReferenceError);
            return;
        }

        if (Page* page = document()->page())
            page->wmlPageState()->restrictDeckAccessToDomain(attr->value());
    } else if (attr->name() == pathAttr) {
        if (containsVariableReference(attr->value())) {
            // FIXME: Error reporting
            // WMLHelper::tokenizer()->reportError(InvalidVariableReferenceError);
            return;
        }

        if (Page* page = document()->page()) 
            page->wmlPageState()->restrictDeckAccessToPath(attr->value());
    } else
        WMLElement::parseMappedAttribute(attr);
}

}

#endif
