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
#include "JSElementInternals.h"

#include "HTMLNames.h"
#include "IDLTypes.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertSequences.h"
#include "JSElement.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/ObjectConstructor.h>

namespace WebCore {
using namespace JSC;

static JSValue getElementsArrayAttribute(JSGlobalObject& lexicalGlobalObject, const JSElementInternals& thisObject, const QualifiedName& attributeName)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* cachedObject = nullptr;
    JSValue cachedObjectValue = thisObject.getDirect(vm, builtinNames(vm).cachedAttrAssociatedElementsPrivateName());
    if (cachedObjectValue)
        cachedObject = asObject(cachedObjectValue);
    else {
        cachedObject = constructEmptyObject(vm, thisObject.globalObject()->nullPrototypeObjectStructure());
        const_cast<JSElementInternals&>(thisObject).putDirect(vm, builtinNames(vm).cachedAttrAssociatedElementsPrivateName(), cachedObject);
    }

    std::optional<Vector<RefPtr<Element>>> elements = thisObject.wrapped().getElementsArrayAttribute(attributeName);
    auto propertyName = PropertyName(Identifier::fromString(vm, attributeName.toString()));
    JSValue cachedValue = cachedObject->getDirect(vm, propertyName);
    if (!cachedValue.isEmpty()) {
        std::optional<Vector<RefPtr<Element>>> cachedElements = convert<IDLNullable<IDLFrozenArray<IDLInterface<Element>>>>(lexicalGlobalObject, cachedValue);
        if (elements == cachedElements)
            return cachedValue;
    }

    JSValue elementsValue = toJS<IDLNullable<IDLFrozenArray<IDLInterface<Element>>>>(lexicalGlobalObject, *thisObject.globalObject(), throwScope, elements);
    cachedObject->putDirect(vm, propertyName, elementsValue);
    return elementsValue;
}

JSValue JSElementInternals::ariaControlsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_controlsAttr);
}

JSValue JSElementInternals::ariaDescribedByElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_describedbyAttr);
}

JSValue JSElementInternals::ariaDetailsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_detailsAttr);
}

JSValue JSElementInternals::ariaErrorMessageElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_errormessageAttr);
}

JSValue JSElementInternals::ariaFlowToElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_flowtoAttr);
}

JSValue JSElementInternals::ariaLabelledByElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_labelledbyAttr);
}

JSValue JSElementInternals::ariaOwnsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_ownsAttr);
}

} // namespace WebCore
