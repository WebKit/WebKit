/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#if ENABLE(XSLT)

#include "JSXSLTProcessor.h"

#include "XSLTProcessor.h"
#include "JSXSLTProcessor.lut.h"
#include "kjs_dom.h"
#include "JSDocument.h"
#include "Document.h"
#include "DocumentFragment.h"

using namespace WebCore;

namespace KJS {

const ClassInfo JSXSLTProcessor::info = { "XSLTProcessor", 0, 0 };

/*
@begin XSLTProcessorPrototypeTable 7
  importStylesheet      &JSXSLTProcessorPrototypeFunctionImportStylesheet::create     DontDelete|Function 1
  transformToFragment   &JSXSLTProcessorPrototypeFunctionTransformToFragment::create  DontDelete|Function 2
  transformToDocument   &JSXSLTProcessorPrototypeFunctionTransformToDocument::create  DontDelete|Function 2
  setParameter          &JSXSLTProcessorPrototypeFunctionSetParameter::create         DontDelete|Function 3
  getParameter          &JSXSLTProcessorPrototypeFunctionGetParameter::create         DontDelete|Function 2
  removeParameter       &JSXSLTProcessorPrototypeFunctionRemoveParameter::create      DontDelete|Function 2
  clearParameters       &JSXSLTProcessorPrototypeFunctionClearParameters::create      DontDelete|Function 0
  reset                 &JSXSLTProcessorPrototypeFunctionReset::create                DontDelete|Function 0
@end
*/

KJS_DEFINE_PROTOTYPE(XSLTProcessorPrototype)
KJS_IMPLEMENT_PROTOTYPE("XSLTProcessor", XSLTProcessorPrototype)

JSXSLTProcessor::JSXSLTProcessor(ExecState *exec) : m_impl(new XSLTProcessor())
{
    setPrototype(XSLTProcessorPrototype::self(exec));
}

JSXSLTProcessor::~JSXSLTProcessor()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* JSXSLTProcessorPrototypeFunctionImportStylesheet::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    if (nodeVal->isObject(&JSNode::info)) {
        JSNode* node = static_cast<JSNode*>(nodeVal);
        processor.importStylesheet(node->impl());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionTransformToFragment::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    JSValue *docVal = args[1];
    if (nodeVal->isObject(&JSNode::info) && docVal->isObject(&JSDocument::info)) {
        Node* node = static_cast<JSNode*>(nodeVal)->impl();
        Document* doc = static_cast<Document*>(static_cast<JSDocument *>(docVal)->impl());
        return toJS(exec, processor.transformToFragment(node, doc).get());
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionTransformToDocument::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    if (nodeVal->isObject(&JSNode::info)) {
        JSNode* node = static_cast<JSNode*>(nodeVal);
        RefPtr<Document> resultDocument = processor.transformToDocument(node->impl());
        if (resultDocument)
            return toJS(exec, resultDocument.get());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionSetParameter::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    if (args[1]->isUndefinedOrNull() || args[2]->isUndefinedOrNull())
        return jsUndefined(); // Throw exception?
    String namespaceURI = args[0]->toString(exec);
    String localName = args[1]->toString(exec);
    String value = args[2]->toString(exec);
    processor.setParameter(namespaceURI, localName, value);
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionGetParameter::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    if (args[1]->isUndefinedOrNull())
        return jsUndefined();
    String namespaceURI = args[0]->toString(exec);
    String localName = args[1]->toString(exec);
    String value = processor.getParameter(namespaceURI, localName);
    if (!value.isNull())
        return jsString(value);
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionRemoveParameter::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    if (args[1]->isUndefinedOrNull())
        return jsUndefined();
    String namespaceURI = args[0]->toString(exec);
    String localName = args[1]->toString(exec);
    processor.removeParameter(namespaceURI, localName);
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionClearParameters::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    processor.clearParameters();
    return jsUndefined();
}

JSValue* JSXSLTProcessorPrototypeFunctionReset::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&KJS::JSXSLTProcessor::info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    processor.reset();
    return jsUndefined();
}

XSLTProcessorConstructorImp::XSLTProcessorConstructorImp(ExecState *exec)
{
    setPrototype(exec->lexicalGlobalObject()->objectPrototype());
    putDirect(exec->propertyNames().prototype, XSLTProcessorPrototype::self(exec), None);
}

}

#endif // ENABLE(XSLT)
