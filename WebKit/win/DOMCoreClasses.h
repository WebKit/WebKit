/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef DOMCoreClasses_H
#define DOMCoreClasses_H

#include "DOMCore.h"
#include "DOMCSS.h"
#include "DOMEvents.h"
#include "DOMExtensions.h"
#include "DOMPrivate.h"
#include "WebScriptObject.h"

// {79A193A5-D783-4c73-9AD9-D10678B943DE}
DEFINE_GUID(IID_DeprecatedDOMNode, 0x79a193a5, 0xd783, 0x4c73, 0x9a, 0xd9, 0xd1, 0x6, 0x78, 0xb9, 0x43, 0xde);

namespace WebCore {
    class Element;
    class Document;
    class Node;
    class NodeList;
}

class DeprecatedDOMObject : public WebScriptObject, public IDeprecatedDOMObject
{
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return WebScriptObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return WebScriptObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return WebScriptObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return WebScriptObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return WebScriptObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return WebScriptObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return WebScriptObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return WebScriptObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return WebScriptObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return WebScriptObject::setException(description); }
};

class DeprecatedDOMNode : public DeprecatedDOMObject, public IDeprecatedDOMNode, public IDeprecatedDOMEventTarget
{
protected:
    DeprecatedDOMNode(WebCore::Node* n);
    ~DeprecatedDOMNode();

public:
    static IDeprecatedDOMNode* createInstance(WebCore::Node* n);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value);
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result);
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result);
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result);
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result);
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void);
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix);
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result);

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text);

    // IDeprecatedDOMEventTarget
    virtual HRESULT STDMETHODCALLTYPE addEventListener( 
        /* [in] */ BSTR type,
        /* [in] */ IDeprecatedDOMEventListener *listener,
        /* [in] */ BOOL useCapture);
    
    virtual HRESULT STDMETHODCALLTYPE removeEventListener( 
        /* [in] */ BSTR type,
        /* [in] */ IDeprecatedDOMEventListener *listener,
        /* [in] */ BOOL useCapture);
    
    virtual HRESULT STDMETHODCALLTYPE dispatchEvent( 
        /* [in] */ IDeprecatedDOMEvent *evt,
        /* [retval][out] */ BOOL *result);

    // DeprecatedDOMNode
    WebCore::Node* node() const { return m_node; }

protected:
    WebCore::Node* m_node;
};

class DeprecatedDOMNodeList : public DeprecatedDOMObject, public IDeprecatedDOMNodeList
{
protected:
    DeprecatedDOMNodeList(WebCore::NodeList* l);
    ~DeprecatedDOMNodeList();

public:
    static IDeprecatedDOMNodeList* createInstance(WebCore::NodeList* l);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMNodeList
    virtual HRESULT STDMETHODCALLTYPE item( 
        /* [in] */ UINT index,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE length( 
        /* [retval][out] */ UINT *result);

protected:
    WebCore::NodeList* m_nodeList;
};

class DeprecatedDOMDocument : public DeprecatedDOMNode, public IDeprecatedDOMDocument, public IDeprecatedDOMViewCSS, public IDeprecatedDOMDocumentEvent
{
protected:
    DeprecatedDOMDocument(WebCore::Document* d);
    ~DeprecatedDOMDocument();

public:
    static IDeprecatedDOMDocument* createInstance(WebCore::Document* d);

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMNode::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMNode::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMNode::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMNode::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMNode::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMNode::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMNode::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMNode::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMNode::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMNode::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMNode::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMNode::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMNode::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMNode::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMNode::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMNode::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMNode::setTextContent(text); }
    
    // IDeprecatedDOMDocument
    virtual HRESULT STDMETHODCALLTYPE doctype( 
        /* [retval][out] */ IDeprecatedDOMDocumentType **result);
    
    virtual HRESULT STDMETHODCALLTYPE implementation( 
        /* [retval][out] */ IDeprecatedDOMImplementation **result);
    
    virtual HRESULT STDMETHODCALLTYPE documentElement( 
        /* [retval][out] */ IDeprecatedDOMElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE createElement( 
        /* [in] */ BSTR tagName,
        /* [retval][out] */ IDeprecatedDOMElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE createDocumentFragment( 
        /* [retval][out] */ IDeprecatedDOMDocumentFragment **result);
    
    virtual HRESULT STDMETHODCALLTYPE createTextNode( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMText **result);
    
    virtual HRESULT STDMETHODCALLTYPE createComment( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMComment **result);
    
    virtual HRESULT STDMETHODCALLTYPE createCDATASection( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMCDATASection **result);
    
    virtual HRESULT STDMETHODCALLTYPE createProcessingInstruction( 
        /* [in] */ BSTR target,
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMProcessingInstruction **result);
    
    virtual HRESULT STDMETHODCALLTYPE createAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE createEntityReference( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMEntityReference **result);
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR tagName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result);
    
    virtual HRESULT STDMETHODCALLTYPE importNode( 
        /* [in] */ IDeprecatedDOMNode *importedNode,
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE createElementNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [retval][out] */ IDeprecatedDOMElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE createAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result);
    
    virtual HRESULT STDMETHODCALLTYPE getElementById( 
        /* [in] */ BSTR elementId,
        /* [retval][out] */ IDeprecatedDOMElement **result);

    // IDeprecatedDOMViewCSS
    virtual HRESULT STDMETHODCALLTYPE getComputedStyle( 
        /* [in] */ IDeprecatedDOMElement *elt,
        /* [in] */ BSTR pseudoElt,
        /* [retval][out] */ IDeprecatedDOMCSSStyleDeclaration **result);

    // IDeprecatedDOMDocumentEvent
    virtual HRESULT STDMETHODCALLTYPE createEvent( 
        /* [in] */ BSTR eventType,
        /* [retval][out] */ IDeprecatedDOMEvent **result);

    // DeprecatedDOMDocument
    WebCore::Document* document() { return m_document; }

protected:
    WebCore::Document* m_document;
};

class DeprecatedDOMElement : public DeprecatedDOMNode, public IDeprecatedDOMElement, public IDeprecatedDOMElementPrivate, public IDeprecatedDOMNodeExtensions, public IDeprecatedDOMElementCSSInlineStyle, public IDeprecatedDOMElementExtensions
{
protected:
    DeprecatedDOMElement(WebCore::Element* e);
    ~DeprecatedDOMElement();

public:
    static IDeprecatedDOMElement* createInstance(WebCore::Element* e);

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMNode::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMNode::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMNode::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMNode::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMNode::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMNode::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMNode::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMNode::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMNode::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMNode::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMNode::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMNode::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMNode::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMNode::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMNode::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMNode::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMNode::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMNode::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMNode::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMNode::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMNode::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value);
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name);
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result);
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value);
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName);
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result);
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result);
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result);

    virtual HRESULT STDMETHODCALLTYPE focus( void);
    
    virtual HRESULT STDMETHODCALLTYPE blur( void);

    // IDeprecatedDOMNodeExtensions
    virtual HRESULT STDMETHODCALLTYPE boundingBox( 
        /* [retval][out] */ LPRECT rect);
    
    virtual HRESULT STDMETHODCALLTYPE lineBoxRects( 
        /* [size_is][in] */ RECT* rects,
        /* [in] */ int cRects);

    // IDeprecatedDOMElementPrivate
    virtual HRESULT STDMETHODCALLTYPE coreElement( 
        void** element);

    virtual HRESULT STDMETHODCALLTYPE isEqual( 
        /* [in] */ IDeprecatedDOMElement *other,
        /* [retval][out] */ BOOL *result);

    virtual HRESULT STDMETHODCALLTYPE isFocused( 
        /* [retval][out] */ BOOL *result);

    virtual HRESULT STDMETHODCALLTYPE innerText(
        /* [retval][out] */ BSTR* result);

    // IDeprecatedDOMElementCSSInlineStyle
    virtual HRESULT STDMETHODCALLTYPE style( 
        /* [retval][out] */ IDeprecatedDOMCSSStyleDeclaration **result);

    // IDeprecatedDOMElementExtensions
    virtual HRESULT STDMETHODCALLTYPE offsetLeft( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetTop( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetWidth( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetHeight( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetParent( 
        /* [retval][out] */ IDeprecatedDOMElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE clientWidth( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE clientHeight( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE scrollLeft( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setScrollLeft( 
        /* [in] */ int newScrollLeft);
    
    virtual HRESULT STDMETHODCALLTYPE scrollTop( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setScrollTop( 
        /* [in] */ int newScrollTop);
    
    virtual HRESULT STDMETHODCALLTYPE scrollWidth( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE scrollHeight( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE scrollIntoView( 
        /* [in] */ BOOL alignWithTop);
    
    virtual HRESULT STDMETHODCALLTYPE scrollIntoViewIfNeeded( 
        /* [in] */ BOOL centerIfNeeded);

    // DeprecatedDOMElement
    WebCore::Element* element() { return m_element; }

protected:
    WebCore::Element* m_element;
};

#endif
