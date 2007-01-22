/*
 * Copyright (C) 2007 Apple, Inc.
 * Copyright (C) 2007 Alexey Proskuryakov (ap@webkit.org)
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
#include "JSHTMLSelectElement.h"

#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"

namespace WebCore {

using namespace KJS;
using namespace HTMLNames;

JSValue* JSHTMLSelectElement::remove(ExecState* exec, const List& args)
{
    HTMLSelectElement& select = *static_cast<HTMLSelectElement*>(impl());

    // we support both options index and options objects
    HTMLElement* element = toHTMLElement(args[0]);
    if (element && element->hasTagName(optionTag))
        select.remove(static_cast<HTMLOptionElement*>(element)->index());
    else
        select.remove(static_cast<int>(args[0]->toNumber(exec)));

    return jsUndefined();
}

}
