/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2018 Google Inc. All rights reserved.
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
#include "HTMLLegendElement.h"

#include "ElementIterator.h"
#include "HTMLFieldSetElement.h"
#include "HTMLNames.h"
#include "SelectionRestorationMode.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(HTMLLegendElement);

inline HTMLLegendElement::HTMLLegendElement(const QualifiedName& tagName, Document& document)
    : HTMLElement(tagName, document)
{
    ASSERT(hasTagName(HTMLNames::legendTag));
}

Ref<HTMLLegendElement> HTMLLegendElement::create(const QualifiedName& tagName, Document& document)
{
    return adoptRef(*new HTMLLegendElement(tagName, document));
}

HTMLFormElement* HTMLLegendElement::form() const
{
    // According to the specification, If the legend has a fieldset element as
    // its parent, then the form attribute must return the same value as the
    // form attribute on that fieldset element. Otherwise, it must return null.
    RefPtr fieldset = parentNode();
    if (!is<HTMLFieldSetElement>(fieldset))
        return nullptr;
    return downcast<HTMLFieldSetElement>(*fieldset).form();
}
    
} // namespace
