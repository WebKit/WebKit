/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSHTMLEmbedElement.h"

#include "HTMLEmbedElement.h"
#include "kjs_dom.h"
#include "kjs_html.h"

namespace WebCore {

using namespace KJS;

bool JSHTMLEmbedElement::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return runtimeObjectCustomGetOwnPropertySlot(exec, propertyName, slot, this, static_cast<HTMLElement*>(impl()));
}

bool JSHTMLEmbedElement::customPut(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    return runtimeObjectCustomPut(exec, propertyName, value, attr, static_cast<HTMLElement*>(impl()));
}

bool JSHTMLEmbedElement::implementsCall() const
{
    return runtimeObjectImplementsCall(static_cast<HTMLElement*>(impl()));
}

JSValue* JSHTMLEmbedElement::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    return runtimeObjectCallAsFunction(exec, thisObj, args, static_cast<HTMLElement*>(impl()));
}

bool JSHTMLEmbedElement::canGetItemsForName(ExecState*, HTMLEmbedElement*, const Identifier& propertyName)
{
    return propertyName == "__apple_runtime_object";
}

JSValue* JSHTMLEmbedElement::nameGetter(ExecState* exec, JSObject* originalObject, const Identifier& propertyName, const PropertySlot& slot)
{
    return runtimeObjectGetter(exec, originalObject, propertyName, slot);
}

} // namespace WebCore
