/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSKeyframeEffect.h"

#include "CSSPropertyNames.h"
#include "Document.h"
#include "JSDOMConvertObject.h"
#include "JSDOMConvertSequences.h"
#include "JSDOMConvertStrings.h"
#include "RenderStyle.h"

namespace WebCore {

using namespace JSC;

JSValue JSKeyframeEffect::getKeyframes(JSGlobalObject& lexicalGlobalObject, CallFrame&)
{
    auto lock = JSLockHolder { &lexicalGlobalObject };

    auto* context = jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject)->scriptExecutionContext();
    if (UNLIKELY(!context))
        return jsUndefined();

    auto& domGlobalObject = *jsCast<JSDOMGlobalObject*>(&lexicalGlobalObject);
    auto computedKeyframes = wrapped().getKeyframes();
    auto keyframeObjects = computedKeyframes.map([&](auto& computedKeyframe) -> Strong<JSObject> {
        auto keyframeObject = convertDictionaryToJS(lexicalGlobalObject, domGlobalObject, { computedKeyframe });
        for (auto& [customProperty, propertyValue] : computedKeyframe.customStyleStrings) {
            auto value = toJS<IDLDOMString>(lexicalGlobalObject, propertyValue);
            JSObject::defineOwnProperty(keyframeObject, &lexicalGlobalObject, customProperty.impl(), PropertyDescriptor(value, 0), false);
        }
        for (auto& [propertyID, propertyValue] : computedKeyframe.styleStrings) {
            auto propertyName = KeyframeEffect::CSSPropertyIDToIDLAttributeName(propertyID);
            auto value = toJS<IDLDOMString>(lexicalGlobalObject, propertyValue);
            JSObject::defineOwnProperty(keyframeObject, &lexicalGlobalObject, AtomString(propertyName).impl(), PropertyDescriptor(value, 0), false);
        }
        return { lexicalGlobalObject.vm(), keyframeObject };
    });

    auto throwScope = DECLARE_THROW_SCOPE(lexicalGlobalObject.vm());
    return toJS<IDLSequence<IDLObject>>(lexicalGlobalObject, domGlobalObject, throwScope, keyframeObjects);
}

} // namespace WebCore
