/*
 * Copyright (C) 2007, 2010 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSHTMLSelectElementCustom.h"

#include "CustomElementReactionQueue.h"
#include "ExceptionCode.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "JSHTMLOptionElement.h"
#include "JSHTMLSelectElement.h"

namespace WebCore {

using namespace JSC;
using namespace HTMLNames;

void selectElementIndexSetter(JSC::ExecState& state, HTMLSelectElement& element, unsigned index, JSC::JSValue value)
{
    VM& vm = state.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    auto* option = convert<IDLNullable<IDLInterface<HTMLOptionElement>>>(state, value);
    RETURN_IF_EXCEPTION(throwScope, void());

    propagateException(state, throwScope, element.setItem(index, option));
}

void JSHTMLSelectElement::indexSetter(JSC::ExecState* state, unsigned index, JSC::JSValue value)
{
    CustomElementReactionStack customElementReactionStack;
    selectElementIndexSetter(*state, wrapped(), index, value);
}

}
