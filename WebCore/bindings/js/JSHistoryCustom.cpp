/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSHistoryCustom.h"

#include "Frame.h"
#include "History.h"
#include <runtime/PrototypeFunction.h>

using namespace JSC;

namespace WebCore {

static JSValue nonCachingStaticBackFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) NativeFunctionWrapper(exec, exec->lexicalGlobalObject()->prototypeFunctionStructure(), 0, propertyName, jsHistoryPrototypeFunctionBack);
}

static JSValue nonCachingStaticForwardFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) NativeFunctionWrapper(exec, exec->lexicalGlobalObject()->prototypeFunctionStructure(), 0, propertyName, jsHistoryPrototypeFunctionForward);
}

static JSValue nonCachingStaticGoFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) NativeFunctionWrapper(exec, exec->lexicalGlobalObject()->prototypeFunctionStructure(), 1, propertyName, jsHistoryPrototypeFunctionGo);
}

bool JSHistory::getOwnPropertySlotDelegate(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // When accessing History cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::getOwnPropertySlotDelegate for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (allowsAccessFromFrame(exec, impl()->frame(), message))
        return false;

    // Check for the few functions that we allow, even when called cross-domain.
    const HashEntry* entry = JSHistoryPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        // Allow access to back(), forward() and go() from any frame.
        if (entry->attributes() & Function) {
            if (entry->function() == jsHistoryPrototypeFunctionBack) {
                slot.setCustom(this, nonCachingStaticBackFunctionGetter);
                return true;
            } else if (entry->function() == jsHistoryPrototypeFunctionForward) {
                slot.setCustom(this, nonCachingStaticForwardFunctionGetter);
                return true;
            } else if (entry->function() == jsHistoryPrototypeFunctionGo) {
                slot.setCustom(this, nonCachingStaticGoFunctionGetter);
                return true;
            }
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.toString.
        if (propertyName == exec->propertyNames().toString) {
            slot.setCustom(this, objectToStringFunctionGetter);
            return true;
        }
    }

    printErrorMessageForFrame(impl()->frame(), message);
    slot.setUndefined();
    return true;
}

bool JSHistory::putDelegate(ExecState* exec, const Identifier&, JSValue, PutPropertySlot&)
{
    // Only allow putting by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return true;
    return false;
}

bool JSHistory::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

void JSHistory::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // Only allow the history object to enumerated by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return;
    Base::getPropertyNames(exec, propertyNames);
}

} // namespace WebCore
