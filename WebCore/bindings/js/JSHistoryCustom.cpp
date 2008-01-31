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

static bool allowsAccessFromFrame(ExecState* exec, Frame* frame)
{
    if (!frame)
        return false;
    Window* win = Window::retrieveWindow(frame);
    return win && win->allowsAccessFrom(exec);
}

bool JSHistory::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // Allow access to back(), forward() and go() from any frame.
    const HashEntry* entry = Lookup::findEntry(JSHistoryPrototype::info.propHashTable, propertyName);
    if (entry && entry->attr & Function) {
        if (entry->value.functionValue == jsHistoryPrototypeFunctionBack
                || entry->value.functionValue == jsHistoryPrototypeFunctionForward
                || entry->value.functionValue == jsHistoryPrototypeFunctionGo)
            return false;
    }

    // Allow access to toString() from any frame as well.
    if (propertyName == exec->propertyNames().toString)
        return false;

    if (!allowsAccessFromFrame(exec, impl()->frame())) {
        slot.setUndefined(this);
        return true;
    }

    return false;
}

bool JSHistory::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    // Only allow putting by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return true;
    return false;
}

void JSHistory::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // Only allow the history object to enumerated by frames in the same origin.
    if (!allowsAccessFromFrame(exec, impl()->frame()))
        return;
    Base::getPropertyNames(exec, propertyNames);
}

} // namespace WebCore
