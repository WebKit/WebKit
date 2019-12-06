/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include "JSHTMLElementWrapperFactory.h"
#include "JSMathMLElementWrapperFactory.h"
#include "JSNodeList.h"
#include "JSSVGElementWrapperFactory.h"
#include "MathMLElement.h"
#include "NodeList.h"
#include "SVGElement.h"


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
    if (element->isDefinedCustomElement())
        return getCachedWrapper(globalObject->world(), element);
    ASSERT(!getCachedWrapper(globalObject->world(), element));
    return createNewElementWrapper(globalObject, WTFMove(element));
}

} // namespace WebCore
