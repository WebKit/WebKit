/*
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "OptionElement.h"

#include "Element.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include <wtf/Assertions.h>

// FIXME: Activate code once WMLOptionElement is available
#if ENABLE(WML) && 0
#include "WMLOptionElement.h"
#include "WMLNames.h"
#endif

namespace WebCore {

OptionElement* optionElementForElement(Element* element)
{
    if (element->isHTMLElement() && element->hasTagName(HTMLNames::optionTag))
        return static_cast<HTMLOptionElement*>(element);

    // FIXME: Activate code once WMLOptionElement is available
#if ENABLE(WML) && 0
    if (element->isWMLElement() && element->hasTagName(WMLNames::optionTag))
        return static_cast<WMLOptionElement*>(element);
#endif

    return 0;
}

}
