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
#include "JSHistory.h"

#include "Frame.h"
#include "History.h"
#include "kjs_window.h"

using namespace KJS;

namespace WebCore {

bool JSHistory::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // When accessing History cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::customGetOwnPropertySlot for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (allowsAccessFromFrame(exec, impl()->frame(), message))
        return false;

    // Check for the few functions that we allow, even when called cross-domain.
    const HashEntry* entry = Lookup::findEntry(JSHistoryPrototype::info.propHashTable, propertyName);
    if (entry) {
        // Allow access to back(), forward() and go() from any frame.
        if ((entry->attr & Function)
                && (entry->value.functionValue == jsHistoryPrototypeFunctionBack
                    || entry->value.functionValue == jsHistoryPrototypeFunctionForward
                    || entry->value.functionValue == jsHistoryPrototypeFunctionGo)) {
            slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
            return true;
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.toString.
        if (propertyName == exec->propertyNames().toString) {
            slot.setCustom(this, objectToStringFunctionGetter);
            return true;
        }
    }

    printErrorMessageForFrame(impl()->frame(), message);
    slot.setUndefined(this);
    return true;
}

bool JSHistory::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
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

bool JSHistory::customGetPropertyNames(ExecState* exec, PropertyNameArray&)
{
    // Only allow the history object to enumerated by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return true;
    return false;
}

} // namespace WebCore
