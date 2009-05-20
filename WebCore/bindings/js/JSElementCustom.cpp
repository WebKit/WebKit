/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSElement.h"

#include "CSSHelper.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "HTMLFrameElementBase.h"
#include "HTMLNames.h"
#include "JSAttr.h"
#include "JSHTMLElementWrapperFactory.h"
#include "JSNodeList.h"
#include "NodeList.h"

#if ENABLE(SVG)
#include "JSSVGElementWrapperFactory.h"
#include "SVGElement.h"
#endif

using namespace JSC;

namespace WebCore {

using namespace HTMLNames;

static inline bool allowSettingSrcToJavascriptURL(ExecState* exec, Element* element, const String& name, const String& value)
{
    if ((element->hasTagName(iframeTag) || element->hasTagName(frameTag)) && equalIgnoringCase(name, "src") && protocolIsJavaScript(parseURL(value))) {
        HTMLFrameElementBase* frame = static_cast<HTMLFrameElementBase*>(element);
        if (!checkNodeSecurity(exec, frame->contentDocument()))
            return false;
    }
    return true;
} 

JSValue JSElement::setAttribute(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    AtomicString name = args.at(0).toString(exec);
    AtomicString value = args.at(1).toString(exec);

    Element* imp = impl();
    if (!allowSettingSrcToJavascriptURL(exec, imp, name, value))
        return jsUndefined();

    imp->setAttribute(name, value, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSElement::setAttributeNode(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    Attr* newAttr = toAttr(args.at(0));
    if (!newAttr) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    Element* imp = impl();
    if (!allowSettingSrcToJavascriptURL(exec, imp, newAttr->name(), newAttr->value()))
        return jsUndefined();

    JSValue result = toJS(exec, WTF::getPtr(imp->setAttributeNode(newAttr, ec)));
    setDOMException(exec, ec);
    return result;
}

JSValue JSElement::setAttributeNS(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    AtomicString namespaceURI = valueToStringWithNullCheck(exec, args.at(0));
    AtomicString qualifiedName = args.at(1).toString(exec);
    AtomicString value = args.at(2).toString(exec);

    Element* imp = impl();
    if (!allowSettingSrcToJavascriptURL(exec, imp, qualifiedName, value))
        return jsUndefined();

    imp->setAttributeNS(namespaceURI, qualifiedName, value, ec);
    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSElement::setAttributeNodeNS(ExecState* exec, const ArgList& args)
{
    ExceptionCode ec = 0;
    Attr* newAttr = toAttr(args.at(0));
    if (!newAttr) {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return jsUndefined();
    }

    Element* imp = impl();
    if (!allowSettingSrcToJavascriptURL(exec, imp, newAttr->name(), newAttr->value()))
        return jsUndefined();

    JSValue result = toJS(exec, WTF::getPtr(imp->setAttributeNodeNS(newAttr, ec)));
    setDOMException(exec, ec);
    return result;
}

JSValue toJSNewlyCreated(ExecState* exec, Element* element)
{
    if (!element)
        return jsNull();

    ASSERT(!getCachedDOMNodeWrapper(element->document(), element));

    JSNode* wrapper;        
    if (element->isHTMLElement())
        wrapper = createJSHTMLWrapper(exec, static_cast<HTMLElement*>(element));
#if ENABLE(SVG)
    else if (element->isSVGElement())
        wrapper = createJSSVGWrapper(exec, static_cast<SVGElement*>(element));
#endif
    else
        wrapper = CREATE_DOM_NODE_WRAPPER(exec, Element, element);

    return wrapper;    
}
    
} // namespace WebCore
