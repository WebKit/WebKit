/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CustomGetterSetter.h"

#include "JSCJSValueInlines.h"
#include <wtf/Assertions.h>

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(CustomGetterSetter);

const ClassInfo CustomGetterSetter::s_info = { "CustomGetterSetter", nullptr, nullptr, nullptr, CREATE_METHOD_TABLE(CustomGetterSetter) };

TriState callCustomSetter(JSGlobalObject* globalObject, CustomGetterSetter::CustomSetter setter, bool isAccessor, JSObject* slotBase, JSValue thisValue, JSValue value, PropertyName propertyName)
{
    if (isAccessor) {
        if (!setter)
            return TriState::False;
        setter(globalObject, JSValue::encode(thisValue), JSValue::encode(value), propertyName);
        // Always return true if there is a setter and it is observed as an accessor to users.
        return TriState::True;
    }

    if (!setter)
        return TriState::Indeterminate;
    return triState(setter(globalObject, JSValue::encode(slotBase), JSValue::encode(value), propertyName));
}

} // namespace JSC
