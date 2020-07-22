/*
 * Copyright (C) 2006-2007, 2014-2015 Apple Inc.  All rights reserved.
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

#ifndef DOMCoreClasses_H
#define DOMCoreClasses_H

#include "WebKit.h"
#include "WebScriptObject.h"

namespace WebCore {
class Element;
class Document;
class DOMWindow;
class Node;
class NodeList;
class Range;
class NamedNodeMap;
struct SimpleRange;
}

class DOMObject : public WebScriptObject, public IDOMObject {
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return WebScriptObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return WebScriptObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return WebScriptObject::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return WebScriptObject::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return WebScriptObject::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return WebScriptObject::removeWebScriptKey(name); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return WebScriptObject::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return WebScriptObject::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return WebScriptObject::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return WebScriptObject::setException(description);
    }
};

class DECLSPEC_UUID("062AEEE3-9E42-44DC-A8A9-236B216FE011") DOMNode : public DOMObject, public IDOMNode, public IDOMEventTarget {
protected:
    DOMNode(WebCore::Node*);
    ~DOMNode();

public:
    static IDOMNode* createInstance(WebCore::Node*);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMObject::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMObject::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMObject::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMObject::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMObject::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMObject::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMObject::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMObject::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short*);
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList**);
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap**);
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument**);
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE normalize();
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR);

    // IDOMEventTarget
    virtual HRESULT STDMETHODCALLTYPE addEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener*, BOOL useCapture);
    virtual HRESULT STDMETHODCALLTYPE removeEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener*, BOOL useCapture);
    virtual HRESULT STDMETHODCALLTYPE dispatchEvent(_In_opt_ IDOMEvent*, _Out_ BOOL* result);

    // DOMNode
    WebCore::Node* node() const { return m_node; }

protected:
    WebCore::Node* m_node { nullptr };
};

class DOMNodeList : public DOMObject, public IDOMNodeList {
protected:
    DOMNodeList(WebCore::NodeList* l);
    ~DOMNodeList();

public:
    static IDOMNodeList* createInstance(WebCore::NodeList* l);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMObject::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMObject::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMObject::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMObject::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMObject::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMObject::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMObject::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMObject::setException(description);
    }

    // IDOMNodeList
    virtual HRESULT STDMETHODCALLTYPE item(UINT index, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ UINT* result);

protected:
    WebCore::NodeList* m_nodeList { nullptr };
};

class DOMDocument : public DOMNode, public IDOMDocument, public IDOMViewCSS, public IDOMDocumentEvent {
protected:
    DOMDocument(WebCore::Document* d);
    ~DOMDocument();

public:
    static IDOMDocument* createInstance(WebCore::Document* d);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMNode::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMNode::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMNode::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMNode::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMNode::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMNode::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMNode::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMNode::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMNode::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMNode::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMNode::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMNode::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMNode::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMNode::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMNode::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::firstChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::lastChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMNode::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMNode::ownerDocument(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::insertBefore(newChild, refChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::replaceChild(newChild, oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMNode::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::cloneNode(deep, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMNode::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMNode::isSupported(feature, version, result);
    }

    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMNode::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMNode::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMNode::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMNode::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMNode::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMNode::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMNode::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMNode::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMNode::setTextContent(text);
    }
    
    // IDOMDocument
    virtual HRESULT STDMETHODCALLTYPE doctype(_COM_Outptr_opt_ IDOMDocumentType**);
    virtual HRESULT STDMETHODCALLTYPE implementation(_COM_Outptr_opt_ IDOMImplementation**);
    virtual HRESULT STDMETHODCALLTYPE documentElement(_COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE createElement(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE createDocumentFragment(_COM_Outptr_opt_ IDOMDocumentFragment**);
    virtual HRESULT STDMETHODCALLTYPE createTextNode(_In_ BSTR data, _COM_Outptr_opt_ IDOMText**);
    virtual HRESULT STDMETHODCALLTYPE createComment(_In_ BSTR data, _COM_Outptr_opt_ IDOMComment**);
    virtual HRESULT STDMETHODCALLTYPE createCDATASection(_In_ BSTR data, _COM_Outptr_opt_ IDOMCDATASection**);
    virtual HRESULT STDMETHODCALLTYPE createProcessingInstruction(_In_ BSTR target, _In_ BSTR data, _COM_Outptr_opt_ IDOMProcessingInstruction**);
    virtual HRESULT STDMETHODCALLTYPE createAttribute(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE createEntityReference(_In_ BSTR name, _COM_Outptr_opt_ IDOMEntityReference**);
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMNodeList**);
    virtual HRESULT STDMETHODCALLTYPE importNode(_In_opt_ IDOMNode* importedNode, BOOL deep, _COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE createElementNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE createAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList**);
    virtual HRESULT STDMETHODCALLTYPE getElementById(_In_ BSTR elementId, _COM_Outptr_opt_ IDOMElement**);

    // IDOMViewCSS
    virtual HRESULT STDMETHODCALLTYPE getComputedStyle(_In_opt_ IDOMElement* elt, _In_ BSTR pseudoElt, _COM_Outptr_opt_ IDOMCSSStyleDeclaration**);

    // IDOMDocumentEvent
    virtual HRESULT STDMETHODCALLTYPE createEvent(_In_ BSTR eventType, _COM_Outptr_opt_ IDOMEvent**);

    // DOMDocument
    WebCore::Document* document() { return m_document; }

protected:
    WebCore::Document* m_document;
};

class DOMWindow : public DOMObject, public IDOMWindow, public IDOMEventTarget {
protected:
    DOMWindow(WebCore::DOMWindow*);
    ~DOMWindow();

public:
    static IDOMWindow* createInstance(WebCore::DOMWindow*);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMObject::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMObject::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMObject::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMObject::removeWebScriptKey(name); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMObject::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMObject::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMObject::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMObject::setException(description);
    }

    virtual HRESULT STDMETHODCALLTYPE document(_COM_Outptr_opt_ IDOMDocument**);
    virtual HRESULT STDMETHODCALLTYPE getComputedStyle(_In_opt_ IDOMElement*, _In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE getMatchedCSSRules(_In_opt_ IDOMElement*, _In_ BSTR, BOOL, _COM_Outptr_opt_ IDOMCSSRuleList**);
    virtual HRESULT STDMETHODCALLTYPE devicePixelRatio(_Out_ double*);
    virtual HRESULT STDMETHODCALLTYPE addEventListener(_In_ BSTR, _In_opt_ IDOMEventListener*, BOOL);
    virtual HRESULT STDMETHODCALLTYPE removeEventListener(_In_ BSTR, _In_opt_ IDOMEventListener*, BOOL);
    virtual HRESULT STDMETHODCALLTYPE dispatchEvent(_In_opt_ IDOMEvent *, _Out_ BOOL*);

    // DOMWindow
    WebCore::DOMWindow* window() { return m_window; }

protected:
    WebCore::DOMWindow* m_window;
};



class DOMElement : public DOMNode, public IDOMElement, public IDOMElementPrivate, public IDOMNodeExtensions, public IDOMElementCSSInlineStyle, public IDOMElementExtensions {
protected:
    DOMElement(WebCore::Element* e);
    ~DOMElement();

public:
    static IDOMElement* createInstance(WebCore::Element* e);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMNode::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMNode::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMNode::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMNode::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMNode::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMNode::removeWebScriptKey(name); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMNode::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMNode::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMNode::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMNode::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMNode::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMNode::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMNode::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMNode::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMNode::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMNode::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMNode::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_  IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_  IDOMNode* newChild, _In_opt_  IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_  IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMNode::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMNode::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMNode::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_  BSTR version, _Out_ BOOL* result)
    {
        return DOMNode::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMNode::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMNode::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMNode::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMNode::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMNode::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMNode::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMNode::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMNode::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMNode::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value);
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name);
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr* newAttr, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr* oldAttr, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList**);
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value);
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName);
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr*, _COM_Outptr_opt_ IDOMAttr**);
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList**);
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE focus();
    virtual HRESULT STDMETHODCALLTYPE blur();

    // IDOMNodeExtensions
    virtual HRESULT STDMETHODCALLTYPE boundingBox(_Out_ LPRECT);
    virtual HRESULT STDMETHODCALLTYPE lineBoxRects(__inout_ecount_full(cRects) RECT*, int cRects);

    // IDOMElementPrivate
    virtual HRESULT STDMETHODCALLTYPE coreElement(__deref_opt_out void** element);
    virtual HRESULT STDMETHODCALLTYPE isEqual(_In_opt_ IDOMElement* other, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE isFocused(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE font(_Out_ WebFontDescription*);
    virtual HRESULT STDMETHODCALLTYPE renderedImage(__deref_opt_out HBITMAP* image);
    virtual HRESULT STDMETHODCALLTYPE markerTextForListItem(__deref_opt_out BSTR* markerText);
    virtual HRESULT STDMETHODCALLTYPE shadowPseudoId(__deref_opt_out BSTR* result);

    // IDOMElementCSSInlineStyle
    virtual HRESULT STDMETHODCALLTYPE style(_COM_Outptr_opt_ IDOMCSSStyleDeclaration**);

    // IDOMElementExtensions
    virtual HRESULT STDMETHODCALLTYPE offsetLeft(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE offsetTop(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE offsetWidth(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE offsetHeight(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE offsetParent(_COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE clientWidth(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE clientHeight(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE scrollLeft(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setScrollLeft(int);
    virtual HRESULT STDMETHODCALLTYPE scrollTop(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setScrollTop(int);
    virtual HRESULT STDMETHODCALLTYPE scrollWidth(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE scrollHeight(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE scrollIntoView(BOOL alignWithTop);
    virtual HRESULT STDMETHODCALLTYPE scrollIntoViewIfNeeded(BOOL centerIfNeeded);

    // DOMElement
    WebCore::Element* element() { return m_element; }

protected:
    WebCore::Element* m_element;
};

class DOMRange : public DOMObject, public IDOMRange {
protected:
    DOMRange(WebCore::Range*);
    ~DOMRange();

public:
    static IDOMRange* createInstance(WebCore::Range*);
    static IDOMRange* createInstance(const Optional<WebCore::SimpleRange>&);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMObject::throwException(exceptionMessage, result);
    }

    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMObject::callWebScriptMethod(name, args, cArgs, result);
    }

    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMObject::evaluateWebScript(script, result);
    }

    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMObject::removeWebScriptKey(name);
    }

    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMObject::stringRepresentation(stringRepresentation);
    }

    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMObject::webScriptValueAtIndex(index, result);
    }

    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMObject::setWebScriptValueAtIndex(index, val);
    }

    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMObject::setException(description);
    }

    virtual HRESULT STDMETHODCALLTYPE startContainer(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE startOffset(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE endContainer(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE endOffset(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE collapsed(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE commonAncestorContainer(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE setStart(_In_opt_ IDOMNode*, int offset);
    virtual HRESULT STDMETHODCALLTYPE setEnd(_In_opt_ IDOMNode*, int offset);
    virtual HRESULT STDMETHODCALLTYPE setStartBefore(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE setStartAfter(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE setEndBefore(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE setEndAfter(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE collapse(BOOL);
    virtual HRESULT STDMETHODCALLTYPE selectNode(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE selectNodeContents(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE compareBoundaryPoints(unsigned short how, _In_opt_ IDOMRange* sourceRange);
    virtual HRESULT STDMETHODCALLTYPE deleteContents();
    virtual HRESULT STDMETHODCALLTYPE extractContents(_COM_Outptr_opt_ IDOMDocumentFragment**);
    virtual HRESULT STDMETHODCALLTYPE cloneContents(_COM_Outptr_opt_ IDOMDocumentFragment**);
    virtual HRESULT STDMETHODCALLTYPE insertNode(_In_opt_ IDOMNode*);
    virtual HRESULT STDMETHODCALLTYPE surroundContents(_In_opt_ IDOMNode* newParent);
    virtual HRESULT STDMETHODCALLTYPE cloneRange(_COM_Outptr_opt_ IDOMRange**);
    virtual HRESULT STDMETHODCALLTYPE toString(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE detach();

protected:
    WebCore::Range* m_range;
};

class DOMNamedNodeMap : public DOMObject, public IDOMNamedNodeMap {
protected:
    DOMNamedNodeMap(WebCore::NamedNodeMap*);
    ~DOMNamedNodeMap();

public:
    static IDOMNamedNodeMap* createInstance(WebCore::NamedNodeMap*);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DOMObject::Release(); }

    virtual HRESULT STDMETHODCALLTYPE getNamedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE setNamedItem(_In_opt_ IDOMNode* arg, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE removeNamedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE item(_In_ UINT index, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ UINT* result);
    virtual HRESULT STDMETHODCALLTYPE getNamedItemNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE setNamedItemNS(_In_opt_ IDOMNode* arg, _COM_Outptr_opt_ IDOMNode** result);
    virtual HRESULT STDMETHODCALLTYPE removeNamedItemNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNode** result);

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMObject::throwException(exceptionMessage, result);
    }

    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, _In_ const VARIANT args[], _In_ int cArgs, _Out_ VARIANT* result)
    {
        return DOMObject::callWebScriptMethod(name, args, cArgs, result);
    }

    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMObject::evaluateWebScript(script, result);
    }

    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMObject::removeWebScriptKey(name);
    }

    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(_Out_ BSTR* stringRepresentation)
    {
        return DOMObject::stringRepresentation(stringRepresentation);
    }

    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(_In_ unsigned index, _Out_ VARIANT* result)
    {
        return DOMObject::webScriptValueAtIndex(index, result);
    }

    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(_In_ unsigned index, _In_ VARIANT val)
    {
        return DOMObject::setWebScriptValueAtIndex(index, val);
    }

    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMObject::setException(description);
    }

protected:
    WebCore::NamedNodeMap* m_nodeMap;
};

#endif
