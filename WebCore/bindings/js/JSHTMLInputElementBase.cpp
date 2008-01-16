/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "JSHTMLInputElementBaseTable.cpp"

using namespace KJS;

namespace WebCore {

/*
@begin JSHTMLInputElementBaseTable 3
  selectionStart        WebCore::JSHTMLInputElementBase::SelectionStart            DontDelete
  selectionEnd          WebCore::JSHTMLInputElementBase::SelectionEnd              DontDelete
@end
@begin JSHTMLInputElementBasePrototypeTable 0
@end
@begin JSHTMLInputElementBaseFunctionTable 1
  setSelectionRange     WebCore::jsHTMLInputElementBaseFunctionSetSelectionRange         DontDelete|Function 2
@end
*/

KJS_IMPLEMENT_PROTOTYPE("JSHTMLInputElementBase", JSHTMLInputElementBasePrototype)

JSValue* jsHTMLInputElementBaseFunctionSetSelectionRange(ExecState* exec, JSObject* thisObj, const List& args)
{
    HTMLInputElement& input = *static_cast<HTMLInputElement*>(static_cast<JSHTMLInputElementBase*>(thisObj)->impl());
    input.setSelectionRange(args[0]->toInt32(exec), args[1]->toInt32(exec));
    return jsUndefined();
}

const ClassInfo JSHTMLInputElementBase::info = { "JSHTMLInputElementBase", &JSHTMLElement::info, &JSHTMLInputElementBaseTable };

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
    const HashEntry* entry = Lookup::findEntry(&JSHTMLInputElementBaseFunctionTable, propertyName);
    if (entry && (entry->attr & KJS::Function) && entry->value.functionValue == jsHTMLInputElementBaseFunctionSetSelectionRange) {
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
        return jsNumber(input.selectionStart());
    case SelectionEnd:
        return jsNumber(input.selectionEnd());
    }
    ASSERT_NOT_REACHED();
    return jsUndefined();
}

void JSHTMLInputElementBase::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<JSHTMLInputElementBase, JSHTMLElement>(exec, propertyName, value, attr, &JSHTMLInputElementBaseTable, this);
}

void JSHTMLInputElementBase::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
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
