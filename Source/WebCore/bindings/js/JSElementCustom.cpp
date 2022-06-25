/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
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
#include "JSElement.h"

#include "Document.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "JSAttr.h"
#include "JSDOMBinding.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertSequences.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSMathMLElementWrapperFactory.h"
#include "JSNodeList.h"
#include "JSSVGElementWrapperFactory.h"
#include "MathMLElement.h"
#include "NodeList.h"
#include "SVGElement.h"
#include "WebCoreJSClientData.h"


namespace WebCore {
using namespace JSC;

using namespace HTMLNames;

static JSValue createNewElementWrapper(JSDOMGlobalObject* globalObject, Ref<Element>&& element)
{
    if (is<HTMLElement>(element))
        return createJSHTMLWrapper(globalObject, static_reference_cast<HTMLElement>(WTFMove(element)));
    if (is<SVGElement>(element))
        return createJSSVGWrapper(globalObject, static_reference_cast<SVGElement>(WTFMove(element)));
#if ENABLE(MATHML)
    if (is<MathMLElement>(element))
        return createJSMathMLWrapper(globalObject, static_reference_cast<MathMLElement>(WTFMove(element)));
#endif
    return createWrapper<Element>(globalObject, WTFMove(element));
}

JSValue toJS(JSGlobalObject*, JSDOMGlobalObject* globalObject, Element& element)
{
    if (auto* wrapper = getCachedWrapper(globalObject->world(), element))
        return wrapper;
    return createNewElementWrapper(globalObject, element);
}

JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<Element>&& element)
{
    if (element->isDefinedCustomElement()) {
        JSValue result = getCachedWrapper(globalObject->world(), element);
        if (result)
            return result;
        ASSERT(!globalObject->vm().exceptionForInspection());
    }
    ASSERT(!getCachedWrapper(globalObject->world(), element));
    return createNewElementWrapper(globalObject, WTFMove(element));
}

static JSValue getElementsArrayAttribute(JSGlobalObject& lexicalGlobalObject, const JSElement& thisObject, const QualifiedName& attributeName)
{
    auto& vm = JSC::getVM(&lexicalGlobalObject);
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    JSObject* cachedObject = nullptr;
    JSValue cachedObjectValue = thisObject.getDirect(vm, builtinNames(vm).cachedAttrAssociatedElementsPrivateName());
    if (cachedObjectValue)
        cachedObject = asObject(cachedObjectValue);
    else {
        cachedObject = constructEmptyObject(vm, thisObject.globalObject()->nullPrototypeObjectStructure());
        const_cast<JSElement&>(thisObject).putDirect(vm, builtinNames(vm).cachedAttrAssociatedElementsPrivateName(), cachedObject);
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

JSValue JSElement::ariaControlsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_controlsAttr);
}

JSValue JSElement::ariaDescribedByElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_describedbyAttr);
}

JSValue JSElement::ariaDetailsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_detailsAttr);
}

JSValue JSElement::ariaFlowToElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_flowtoAttr);
}

JSValue JSElement::ariaLabelledByElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_labelledbyAttr);
}

JSValue JSElement::ariaOwnsElements(JSGlobalObject& lexicalGlobalObject) const
{
    return getElementsArrayAttribute(lexicalGlobalObject, *this, WebCore::HTMLNames::aria_ownsAttr);
}

} // namespace WebCore
