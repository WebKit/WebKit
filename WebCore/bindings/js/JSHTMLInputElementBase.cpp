/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "JSHTMLInputElementBase.h"

#include "HTMLInputElement.h"

using namespace KJS;

namespace WebCore {

static JSValue* jsHTMLInputElementBaseFunctionSetSelectionRange(ExecState*, JSObject*, JSValue*, const ArgList&);

}

#include "JSHTMLInputElementBaseTable.cpp"

namespace WebCore {

/*
@begin JSHTMLInputElementBaseTable
  selectionStart        WebCore::JSHTMLInputElementBase::SelectionStart            DontDelete
  selectionEnd          WebCore::JSHTMLInputElementBase::SelectionEnd              DontDelete
@end
@begin JSHTMLInputElementBasePrototypeTable
@end
@begin JSHTMLInputElementBaseFunctionTable
  setSelectionRange     WebCore::jsHTMLInputElementBaseFunctionSetSelectionRange         DontDelete|Function 2
@end
*/

KJS_IMPLEMENT_PROTOTYPE("HTMLInputElementBase", JSHTMLInputElementBasePrototype)

// SetSelectionRange is implemented on the class instead of on the prototype
// to make it easier to enable/disable lookup of the function based on input type.
JSValue* jsHTMLInputElementBaseFunctionSetSelectionRange(ExecState* exec, JSObject*, JSValue* thisValue, const ArgList& args)
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(static_cast<JSHTMLInputElementBase*>(thisValue)->impl());
    input.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
}

const ClassInfo JSHTMLInputElementBase::s_info = { "HTMLInputElementBase", &JSHTMLElement::s_info, &JSHTMLInputElementBaseTable, 0 };

JSHTMLInputElementBase::JSHTMLInputElementBase(KJS::JSObject* prototype, PassRefPtr<HTMLInputElement> e)
    : JSHTMLElement(prototype, e.get())
{
}

bool JSHTMLInputElementBase::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(impl());
    
    // if this element doesn't support selection, we have nothing to do, try our parent
    if (!input.canHaveSelection())
        return JSHTMLElement::getOwnPropertySlot(exec, propertyName, slot);
    
    // otherwise, do our own function lookup on our function table
    const HashEntry* entry = JSHTMLInputElementBaseFunctionTable.entry(exec, propertyName);
    if (entry && (entry->attributes & KJS::Function) && entry->functionValue == jsHTMLInputElementBaseFunctionSetSelectionRange) {
        slot.setStaticEntry(this, entry, staticFunctionGetter);
        return true;
    }
    ASSERT(!entry);
    
    // finally try value lookup or walk the parent chain
    return getStaticValueSlot<JSHTMLInputElementBase, JSHTMLElement>(exec, &JSHTMLInputElementBaseTable, this, propertyName, slot);
}

JSValue* JSHTMLInputElementBase::getValueProperty(ExecState* exec, int token) const
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(impl());
    ASSERT(input.canHaveSelection());
    switch (token) {
    case SelectionStart:
        return jsNumber(exec, input.selectionStart());
    case SelectionEnd:
        return jsNumber(exec, input.selectionEnd());
    }
    ASSERT_NOT_REACHED();
    return jsUndefined();
}

void JSHTMLInputElementBase::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    lookupPut<JSHTMLInputElementBase, JSHTMLElement>(exec, propertyName, value, &JSHTMLInputElementBaseTable, this);
}

void JSHTMLInputElementBase::putValueProperty(ExecState* exec, int token, JSValue* value)
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(impl());
    ASSERT(input.canHaveSelection());
    switch (token) {
    case SelectionStart:
        input.setSelectionStart(value->toInt32(exec));
        return;
    case SelectionEnd:
        input.setSelectionEnd(value->toInt32(exec));
        return;
    }
    ASSERT_NOT_REACHED();
}

} // namespace WebCore
