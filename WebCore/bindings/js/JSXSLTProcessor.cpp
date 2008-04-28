/*
 * Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "DocumentFragment.h"
#include "JSDocument.h"
#include "XSLTProcessor.h"

using namespace KJS;
using namespace WebCore;

#include "JSXSLTProcessor.lut.h"

namespace WebCore {

const ClassInfo JSXSLTProcessor::s_info = { "XSLTProcessor", 0, 0, 0 };

/*
@begin JSXSLTProcessorPrototypeTable 7
  importStylesheet      jsXSLTProcessorPrototypeFunctionImportStylesheet     DontDelete|Function 1
  transformToFragment   jsXSLTProcessorPrototypeFunctionTransformToFragment  DontDelete|Function 2
  transformToDocument   jsXSLTProcessorPrototypeFunctionTransformToDocument  DontDelete|Function 2
  setParameter          jsXSLTProcessorPrototypeFunctionSetParameter         DontDelete|Function 3
  getParameter          jsXSLTProcessorPrototypeFunctionGetParameter         DontDelete|Function 2
  removeParameter       jsXSLTProcessorPrototypeFunctionRemoveParameter      DontDelete|Function 2
  clearParameters       jsXSLTProcessorPrototypeFunctionClearParameters      DontDelete|Function 0
  reset                 jsXSLTProcessorPrototypeFunctionReset                DontDelete|Function 0
@end
*/

KJS_DEFINE_PROTOTYPE(JSXSLTProcessorPrototype)
KJS_IMPLEMENT_PROTOTYPE("XSLTProcessor", JSXSLTProcessorPrototype)

JSXSLTProcessor::JSXSLTProcessor(JSObject* prototype)
    : DOMObject(prototype)
    , m_impl(XSLTProcessor::create())
{
}

JSXSLTProcessor::~JSXSLTProcessor()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

JSValue* jsXSLTProcessorPrototypeFunctionImportStylesheet(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    if (nodeVal->isObject(&JSNode::s_info)) {
        JSNode* node = static_cast<JSNode*>(nodeVal);
        processor.importStylesheet(node->impl());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* jsXSLTProcessorPrototypeFunctionTransformToFragment(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    JSValue *docVal = args[1];
    if (nodeVal->isObject(&JSNode::s_info) && docVal->isObject(&JSDocument::s_info)) {
        WebCore::Node* node = static_cast<JSNode*>(nodeVal)->impl();
        Document* doc = static_cast<Document*>(static_cast<JSDocument *>(docVal)->impl());
        return toJS(exec, processor.transformToFragment(node, doc).get());
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* jsXSLTProcessorPrototypeFunctionTransformToDocument(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    JSValue *nodeVal = args[0];
    if (nodeVal->isObject(&JSNode::s_info)) {
        JSNode* node = static_cast<JSNode*>(nodeVal);
        RefPtr<Document> resultDocument = processor.transformToDocument(node->impl());
        if (resultDocument)
            return toJS(exec, resultDocument.get());
        return jsUndefined();
    }
    // Throw exception?
    return jsUndefined();
}

JSValue* jsXSLTProcessorPrototypeFunctionSetParameter(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
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

JSValue* jsXSLTProcessorPrototypeFunctionGetParameter(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
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

JSValue* jsXSLTProcessorPrototypeFunctionRemoveParameter(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    if (args[1]->isUndefinedOrNull())
        return jsUndefined();
    String namespaceURI = args[0]->toString(exec);
    String localName = args[1]->toString(exec);
    processor.removeParameter(namespaceURI, localName);
    return jsUndefined();
}

JSValue* jsXSLTProcessorPrototypeFunctionClearParameters(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    processor.clearParameters();
    return jsUndefined();
}

JSValue* jsXSLTProcessorPrototypeFunctionReset(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSXSLTProcessor::s_info))
        return throwError(exec, TypeError);
    XSLTProcessor& processor = *static_cast<JSXSLTProcessor*>(thisObj)->impl();

    processor.reset();
    return jsUndefined();
}

const ClassInfo JSXSLTProcessorConstructor::s_info = { "XSLTProcessorConsructor", 0, 0, 0 };

JSXSLTProcessorConstructor::JSXSLTProcessorConstructor(ExecState* exec)
    : DOMObject(exec->lexicalGlobalObject()->objectPrototype())
{
    putDirect(exec->propertyNames().prototype, JSXSLTProcessorPrototype::self(exec), None);
}

bool JSXSLTProcessorConstructor::implementsConstruct() const
{
    return true;
}

JSObject* JSXSLTProcessorConstructor::construct(ExecState* exec, const List& args)
{
    return new JSXSLTProcessor(JSXSLTProcessorPrototype::self(exec));
}

} // namespace WebCore

#endif // ENABLE(XSLT)
