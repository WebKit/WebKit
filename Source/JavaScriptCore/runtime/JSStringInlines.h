/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSStringInlines_h
#define JSStringInlines_h

#include "CommonIdentifiers.h"
#include "JSCJSValueInlines.h"
#include "JSString.h"

namespace JSC {

ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().length) {
        slot.setValue(this, DontEnum | DontDelete | ReadOnly, jsNumber(m_length));
        return true;
    }
    
    unsigned i = propertyName.asIndex();
    if (i < m_length) {
        ASSERT(i != PropertyName::NotAnIndex); // No need for an explicit check, the above test would always fail!
        slot.setValue(this, DontDelete | ReadOnly, getIndex(exec, i));
        return true;
    }
    
    return false;
}

ALWAYS_INLINE bool JSString::getStringPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    if (propertyName < m_length) {
        slot.setValue(this, DontDelete | ReadOnly, getIndex(exec, propertyName));
        return true;
    }
    
    return false;
}

ALWAYS_INLINE String inlineJSValueNotStringtoString(const JSValue& value, ExecState* exec)
{
    VM& vm = exec->vm();
    if (value.isInt32())
        return vm.numericStrings.add(value.asInt32());
    if (value.isDouble())
        return vm.numericStrings.add(value.asDouble());
    if (value.isTrue())
        return vm.propertyNames->trueKeyword.string();
    if (value.isFalse())
        return vm.propertyNames->falseKeyword.string();
    if (value.isNull())
        return vm.propertyNames->nullKeyword.string();
    if (value.isUndefined())
        return vm.propertyNames->undefinedKeyword.string();
    return value.toString(exec)->value(exec);
}
    
ALWAYS_INLINE String JSValue::toWTFStringInline(ExecState* exec) const
{
    if (isString())
        return static_cast<JSString*>(asCell())->value(exec);
    
    return inlineJSValueNotStringtoString(*this, exec);
}

}

#endif
