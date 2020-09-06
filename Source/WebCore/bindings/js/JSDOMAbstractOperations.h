/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "JSDOMConvertStrings.h"

namespace WebCore {

// Implementations of the abstract operations defined at
// https://heycam.github.io/webidl/#legacy-platform-object-abstract-ops

enum class LegacyOverrideBuiltIns { No, Yes };

// An implementation of the 'named property visibility algorithm'
// https://heycam.github.io/webidl/#dfn-named-property-visibility
template<LegacyOverrideBuiltIns overrideBuiltins, class JSClass>
static bool isVisibleNamedProperty(JSC::JSGlobalObject& lexicalGlobalObject, JSClass& thisObject, JSC::PropertyName propertyName)
{
    // FIXME: It seems unfortunate that have to do two lookups for the property name,
    // one for isSupportedPropertyName and one by the user of this algorithm to access
    // that property. It would be nice if we could smuggle the result, or an iterator
    // out so the duplicate lookup could be avoided.

    // NOTE: While it is not specified, a Symbol can never be a 'supported property
    // name' so we check that first.
    if (propertyName.isSymbol())
        return false;

    auto& impl = thisObject.wrapped();

    // 1. If P is not a supported property name of O, then return false.
    if (!impl.isSupportedPropertyName(propertyNameToString(propertyName)))
        return false;
    
    // 2. If O has an own property named P, then return false.
    JSC::PropertySlot slot { &thisObject, JSC::PropertySlot::InternalMethodType::VMInquiry, &lexicalGlobalObject.vm() };
    if (JSC::JSObject::getOwnPropertySlot(&thisObject, &lexicalGlobalObject, propertyName, slot))
        return false;
    
    // 3. If O implements an interface that has the [LegacyOverrideBuiltIns] extended attribute, then return true.
    if (overrideBuiltins == LegacyOverrideBuiltIns::Yes)
        return true;

    // 4. Initialize prototype to be the value of the internal [[Prototype]] property of O.
    // 5. While prototype is not null:
    //    1. If prototype is not a named properties object, and prototype has an own property named P, then return false.
    // FIXME: Implement checking for 'named properties object'.
    //    2. Set prototype to be the value of the internal [[Prototype]] property of prototype.
    auto prototype = thisObject.getPrototypeDirect(JSC::getVM(&lexicalGlobalObject));
    if (prototype.isObject() && JSC::asObject(prototype)->getPropertySlot(&lexicalGlobalObject, propertyName, slot))
        return false;

    // 6. Return true.
    return true;
}

// An implementation of the 'named property visibility algorithm' augmented to replace the
// 'supported property name' check with direct access to the implementation value returned
// for the property name, via passed in functor. This allows us to avoid two looking up the
// the property name twice; once for 'named property visibility algorithm' check, and then
// again when the value is needed.
template<LegacyOverrideBuiltIns overrideBuiltins, class JSClass, class Functor>
static auto accessVisibleNamedProperty(JSC::JSGlobalObject& lexicalGlobalObject, JSClass& thisObject, JSC::PropertyName propertyName, Functor&& itemAccessor) -> decltype(itemAccessor(thisObject, propertyName))
{
    // NOTE: While it is not specified, a Symbol can never be a 'supported property
    // name' so we check that first.
    if (propertyName.isSymbol())
        return WTF::nullopt;

    // 1. If P is not a supported property name of O, then return false.
    auto result = itemAccessor(thisObject, propertyName);
    if (!result)
        return WTF::nullopt;

    // 2. If O has an own property named P, then return false.
    JSC::PropertySlot slot { &thisObject, JSC::PropertySlot::InternalMethodType::VMInquiry, &lexicalGlobalObject.vm() };
    if (JSC::JSObject::getOwnPropertySlot(&thisObject, &lexicalGlobalObject, propertyName, slot))
        return WTF::nullopt;

    // 3. If O implements an interface that has the [LegacyOverrideBuiltIns] extended attribute, then return true.
    if (overrideBuiltins == LegacyOverrideBuiltIns::Yes && !worldForDOMObject(thisObject).shouldDisableLegacyOverrideBuiltInsBehavior())
        return result;

    // 4. Initialize prototype to be the value of the internal [[Prototype]] property of O.
    // 5. While prototype is not null:
    //    1. If prototype is not a named properties object, and prototype has an own property named P, then return false.
    // FIXME: Implement checking for 'named properties object'.
    //    2. Set prototype to be the value of the internal [[Prototype]] property of prototype.
    auto prototype = thisObject.getPrototypeDirect(JSC::getVM(&lexicalGlobalObject));
    if (prototype.isObject() && JSC::asObject(prototype)->getPropertySlot(&lexicalGlobalObject, propertyName, slot))
        return WTF::nullopt;

    // 6. Return true.
    return result;
}

}
