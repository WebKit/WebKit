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

#ifndef DOMHTMLClasses_H
#define DOMHTMLClasses_H

#include "DOMHTML.h"
#include "DOMPrivate.h"
#include "DOMCoreClasses.h"
#include "WebScriptObject.h"

#include <WTF/RefPtr.h>

namespace WebCore {
    class HTMLCollection;
}

class DeprecatedDOMHTMLCollection : public DeprecatedDOMObject, public IDeprecatedDOMHTMLCollection
{
protected:
    DeprecatedDOMHTMLCollection(WebCore::HTMLCollection* c);

public:
    static IDeprecatedDOMHTMLCollection* createInstance(WebCore::HTMLCollection*);

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

    // IDeprecatedDOMHTMLCollection
    virtual HRESULT STDMETHODCALLTYPE length( 
        /* [retval][out] */ UINT *result);
    
    virtual HRESULT STDMETHODCALLTYPE item( 
        /* [in] */ UINT index,
        /* [retval][out] */ IDeprecatedDOMNode **node);
    
    virtual HRESULT STDMETHODCALLTYPE namedItem( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNode **node);

protected:
    RefPtr<WebCore::HTMLCollection> m_collection;
};

class DeprecatedDOMHTMLOptionsCollection : public DeprecatedDOMObject, public IDeprecatedDOMHTMLOptionsCollection
{
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

    // IDeprecatedDOMHTMLOptionsCollection
    virtual HRESULT STDMETHODCALLTYPE length( 
        /* [retval][out] */ unsigned int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setLength( 
        /* [in] */ unsigned int length);
    
    virtual HRESULT STDMETHODCALLTYPE item( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ IDeprecatedDOMNode **result);
    
    virtual HRESULT STDMETHODCALLTYPE namedItem( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNode *result);
};

class DeprecatedDOMHTMLDocument : public DeprecatedDOMDocument, public IDeprecatedDOMHTMLDocument
{
protected:
    DeprecatedDOMHTMLDocument();
public:
    DeprecatedDOMHTMLDocument(WebCore::Document* d) : DeprecatedDOMDocument(d) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMDocument::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMDocument::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMDocument::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMDocument::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMDocument::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMDocument::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMDocument::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMDocument::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMDocument::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMDocument::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMDocument::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMDocument::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMDocument::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMDocument::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMDocument::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMDocument::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMDocument::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMDocument::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMDocument::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMDocument::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMDocument::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMDocument::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMDocument::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMDocument::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMDocument::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMDocument::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMDocument::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMDocument::setTextContent(text); }
    
    // IDeprecatedDOMDocument
    virtual HRESULT STDMETHODCALLTYPE doctype( 
        /* [retval][out] */ IDeprecatedDOMDocumentType **result) { return DeprecatedDOMDocument::doctype(result); }
    
    virtual HRESULT STDMETHODCALLTYPE implementation( 
        /* [retval][out] */ IDeprecatedDOMImplementation **result) { return DeprecatedDOMDocument::implementation(result); }
    
    virtual HRESULT STDMETHODCALLTYPE documentElement( 
        /* [retval][out] */ IDeprecatedDOMElement **result) { return DeprecatedDOMDocument::documentElement(result); }
    
    virtual HRESULT STDMETHODCALLTYPE createElement( 
        /* [in] */ BSTR tagName,
        /* [retval][out] */ IDeprecatedDOMElement **result) { return DeprecatedDOMDocument::createElement(tagName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createDocumentFragment( 
        /* [retval][out] */ IDeprecatedDOMDocumentFragment **result) { return DeprecatedDOMDocument::createDocumentFragment(result); }
    
    virtual HRESULT STDMETHODCALLTYPE createTextNode( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMText **result) { return DeprecatedDOMDocument::createTextNode(data, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createComment( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMComment **result) { return DeprecatedDOMDocument::createComment(data, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createCDATASection( 
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMCDATASection **result) { return DeprecatedDOMDocument::createCDATASection(data, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createProcessingInstruction( 
        /* [in] */ BSTR target,
        /* [in] */ BSTR data,
        /* [retval][out] */ IDeprecatedDOMProcessingInstruction **result) { return DeprecatedDOMDocument::createProcessingInstruction(target, data, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMDocument::createAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createEntityReference( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMEntityReference **result) { return DeprecatedDOMDocument::createEntityReference(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR tagName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMDocument::getElementsByTagName(tagName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE importNode( 
        /* [in] */ IDeprecatedDOMNode *importedNode,
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMDocument::importNode(importedNode, deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createElementNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [retval][out] */ IDeprecatedDOMElement **result) { return DeprecatedDOMDocument::createElementNS(namespaceURI, qualifiedName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE createAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMDocument::createAttributeNS(namespaceURI, qualifiedName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMDocument::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementById( 
        /* [in] */ BSTR elementId,
        /* [retval][out] */ IDeprecatedDOMElement **result) { return DeprecatedDOMDocument::getElementById(elementId, result); }

    // IDeprecatedDOMHTMLDocument
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title);
    
    virtual HRESULT STDMETHODCALLTYPE referrer( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE domain( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE URL( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE body( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement **bodyElement);
    
    virtual HRESULT STDMETHODCALLTYPE setBody( 
        /* [in] */ IDeprecatedDOMHTMLElement *body);
    
    virtual HRESULT STDMETHODCALLTYPE images( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **collection);
    
    virtual HRESULT STDMETHODCALLTYPE applets( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **collection);
    
    virtual HRESULT STDMETHODCALLTYPE links( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **collection);
    
    virtual HRESULT STDMETHODCALLTYPE forms( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **collection);
    
    virtual HRESULT STDMETHODCALLTYPE anchors( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **collection);
    
    virtual HRESULT STDMETHODCALLTYPE cookie( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setCookie( 
        /* [in] */ BSTR cookie);
    
    virtual HRESULT STDMETHODCALLTYPE open( void);
    
    virtual HRESULT STDMETHODCALLTYPE close( void);
    
    virtual HRESULT STDMETHODCALLTYPE write( 
        /* [in] */ BSTR text);
    
    virtual HRESULT STDMETHODCALLTYPE writeln( 
        /* [in] */ BSTR text);
    
    virtual HRESULT STDMETHODCALLTYPE getElementById_( 
        /* [in] */ BSTR elementId,
        /* [retval][out] */ IDeprecatedDOMElement **element);
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByName( 
        /* [in] */ BSTR elementName,
        /* [retval][out] */ IDeprecatedDOMNodeList **nodeList);
};

class DeprecatedDOMHTMLElement : public DeprecatedDOMElement, public IDeprecatedDOMHTMLElement
{
protected:
    DeprecatedDOMHTMLElement();
public:
    DeprecatedDOMHTMLElement(WebCore::Element* e) : DeprecatedDOMElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName);
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title);
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang);
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir);
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className);

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result);
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html);
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result);
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text);        

};

class DeprecatedDOMHTMLFormElement : public DeprecatedDOMHTMLElement, public IDeprecatedDOMHTMLFormElement
{
protected:
    DeprecatedDOMHTMLFormElement();
public:
    DeprecatedDOMHTMLFormElement(WebCore::Element* e) : DeprecatedDOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMHTMLElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMHTMLElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMHTMLElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMHTMLElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMHTMLElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMHTMLElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMHTMLElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMHTMLElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMHTMLElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMHTMLElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMHTMLElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMHTMLElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::idName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName) { return DeprecatedDOMHTMLElement::setIdName(idName); }
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::title(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title) { return DeprecatedDOMHTMLElement::setTitle(title); }
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::lang(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang) { return DeprecatedDOMHTMLElement::setLang(lang); }
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::dir(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir) { return DeprecatedDOMHTMLElement::setDir(dir); }
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::className(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className) { return DeprecatedDOMHTMLElement::setClassName(className); }

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerHTML(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html) { return DeprecatedDOMHTMLElement::setInnerHTML(html); }
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerText(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setInnerText(text); }

    // IDeprecatedDOMHTMLFormElement
    virtual HRESULT STDMETHODCALLTYPE elements( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection **result);
    
    virtual HRESULT STDMETHODCALLTYPE length( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setName( 
        /* [in] */ BSTR name);
    
    virtual HRESULT STDMETHODCALLTYPE acceptCharset( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAcceptCharset( 
        /* [in] */ BSTR acceptCharset);
    
    virtual HRESULT STDMETHODCALLTYPE action( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAction( 
        /* [in] */ BSTR action);
    
    virtual HRESULT STDMETHODCALLTYPE encType( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setEnctype( 
        /* [retval][out] */ BSTR *encType);
    
    virtual HRESULT STDMETHODCALLTYPE method( 
        /* [retval][out] */ BSTR *method);
    
    virtual HRESULT STDMETHODCALLTYPE setMethod( 
        /* [in] */ BSTR method);
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTarget( 
        /* [in] */ BSTR target);
    
    virtual HRESULT STDMETHODCALLTYPE submit( void);
    
    virtual HRESULT STDMETHODCALLTYPE reset( void);
};

class DeprecatedDOMHTMLSelectElement : public DeprecatedDOMHTMLElement, public IDeprecatedDOMHTMLSelectElement, public IFormsAutoFillTransitionSelect
{
protected:
    DeprecatedDOMHTMLSelectElement();
public:
    DeprecatedDOMHTMLSelectElement(WebCore::Element* e) : DeprecatedDOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMHTMLElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMHTMLElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMHTMLElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMHTMLElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMHTMLElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMHTMLElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMHTMLElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMHTMLElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMHTMLElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMHTMLElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMHTMLElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMHTMLElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::idName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName) { return DeprecatedDOMHTMLElement::setIdName(idName); }
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::title(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title) { return DeprecatedDOMHTMLElement::setTitle(title); }
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::lang(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang) { return DeprecatedDOMHTMLElement::setLang(lang); }
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::dir(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir) { return DeprecatedDOMHTMLElement::setDir(dir); }
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::className(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className) { return DeprecatedDOMHTMLElement::setClassName(className); }

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerHTML(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html) { return DeprecatedDOMHTMLElement::setInnerHTML(html); }
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerText(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setInnerText(text); }

    // IDeprecatedDOMHTMLSelectElement
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE selectedIndex( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setSelectedIndx( 
        /* [in] */ int selectedIndex);
    
    virtual HRESULT STDMETHODCALLTYPE value( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ BSTR value);
    
    virtual HRESULT STDMETHODCALLTYPE length( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE form( 
        /* [retval][out] */ IDeprecatedDOMHTMLFormElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE options( 
        /* [retval][out] */ IDeprecatedDOMHTMLOptionsCollection **result);
    
    virtual HRESULT STDMETHODCALLTYPE disabled( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDisabled( 
        /* [in] */ BOOL disabled);
    
    virtual HRESULT STDMETHODCALLTYPE multiple( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setMultiple( 
        /* [in] */ BOOL multiple);
    
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setName( 
        /* [in] */ BSTR name);
    
    virtual HRESULT STDMETHODCALLTYPE size( 
        /* [retval][out] */ int *size);
    
    virtual HRESULT STDMETHODCALLTYPE setSize( 
        /* [in] */ int size);
    
    virtual HRESULT STDMETHODCALLTYPE tabIndex( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTabIndex( 
        /* [in] */ int tabIndex);
    
    virtual HRESULT STDMETHODCALLTYPE add( 
        /* [in] */ IDeprecatedDOMHTMLElement *element,
        /* [in] */ IDeprecatedDOMHTMLElement *before);
    
    virtual HRESULT STDMETHODCALLTYPE remove( 
        /* [in] */ int index);
    
    // IFormsAutoFillTransitionSelect
    virtual HRESULT STDMETHODCALLTYPE activateItemAtIndex( 
        /* [in] */ int index);
};

class DeprecatedDOMHTMLOptionElement : public DeprecatedDOMHTMLElement, public IDeprecatedDOMHTMLOptionElement
{
protected:
    DeprecatedDOMHTMLOptionElement();
public:
    DeprecatedDOMHTMLOptionElement(WebCore::Element* e) : DeprecatedDOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMHTMLElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMHTMLElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMHTMLElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMHTMLElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMHTMLElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMHTMLElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMHTMLElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMHTMLElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMHTMLElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMHTMLElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMHTMLElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMHTMLElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::idName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName) { return DeprecatedDOMHTMLElement::setIdName(idName); }
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::title(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title) { return DeprecatedDOMHTMLElement::setTitle(title); }
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::lang(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang) { return DeprecatedDOMHTMLElement::setLang(lang); }
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::dir(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir) { return DeprecatedDOMHTMLElement::setDir(dir); }
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::className(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className) { return DeprecatedDOMHTMLElement::setClassName(className); }

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerHTML(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html) { return DeprecatedDOMHTMLElement::setInnerHTML(html); }
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerText(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setInnerText(text); }

    // IDeprecatedDOMHTMLOptionElement
    virtual HRESULT STDMETHODCALLTYPE form( 
        /* [retval][out] */ IDeprecatedDOMHTMLFormElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE defaultSelected( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDefaultSelected( 
        /* [in] */ BOOL defaultSelected);
    
    virtual HRESULT STDMETHODCALLTYPE text( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE index( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE disabled( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDisabled( 
        /* [in] */ BOOL disabled);
    
    virtual HRESULT STDMETHODCALLTYPE label( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setLabel( 
        /* [in] */ BSTR label);
    
    virtual HRESULT STDMETHODCALLTYPE selected( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setSelected( 
        /* [in] */ BOOL selected);
    
    virtual HRESULT STDMETHODCALLTYPE value( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ BSTR value);
};

class DeprecatedDOMHTMLInputElement : public DeprecatedDOMHTMLElement, public IDeprecatedDOMHTMLInputElement, public IFormsAutoFillTransition, public IFormPromptAdditions
{
protected:
    DeprecatedDOMHTMLInputElement();
public:
    DeprecatedDOMHTMLInputElement(WebCore::Element* e) : DeprecatedDOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMHTMLElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMHTMLElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMHTMLElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMHTMLElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMHTMLElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMHTMLElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMHTMLElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMHTMLElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMHTMLElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMHTMLElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMHTMLElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMHTMLElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::idName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName) { return DeprecatedDOMHTMLElement::setIdName(idName); }
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::title(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title) { return DeprecatedDOMHTMLElement::setTitle(title); }
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::lang(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang) { return DeprecatedDOMHTMLElement::setLang(lang); }
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::dir(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir) { return DeprecatedDOMHTMLElement::setDir(dir); }
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::className(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className) { return DeprecatedDOMHTMLElement::setClassName(className); }

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerHTML(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html) { return DeprecatedDOMHTMLElement::setInnerHTML(html); }
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerText(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setInnerText(text); }

    // IDeprecatedDOMHTMLInputElement
    virtual HRESULT STDMETHODCALLTYPE defaultValue( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDefaultValue( 
        /* [in] */ BSTR val);
    
    virtual HRESULT STDMETHODCALLTYPE defaultChecked( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDefaultChecked( 
        /* [in] */ BSTR checked);
    
    virtual HRESULT STDMETHODCALLTYPE form( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE accept( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAccept( 
        /* [in] */ BSTR accept);
    
    virtual HRESULT STDMETHODCALLTYPE accessKey( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAccessKey( 
        /* [in] */ BSTR key);
    
    virtual HRESULT STDMETHODCALLTYPE align( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAlign( 
        /* [in] */ BSTR align);
    
    virtual HRESULT STDMETHODCALLTYPE alt( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAlt( 
        /* [in] */ BSTR alt);
    
    virtual HRESULT STDMETHODCALLTYPE checked( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setChecked( 
        /* [in] */ BOOL checked);
    
    virtual HRESULT STDMETHODCALLTYPE disabled( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDisabled( 
        /* [in] */ BOOL disabled);
    
    virtual HRESULT STDMETHODCALLTYPE maxLength( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setMaxLength( 
        /* [in] */ int maxLength);
    
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *name);
    
    virtual HRESULT STDMETHODCALLTYPE setName( 
        /* [in] */ BSTR name);
    
    virtual HRESULT STDMETHODCALLTYPE readOnly( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setReadOnly( 
        /* [in] */ BOOL readOnly);
    
    virtual HRESULT STDMETHODCALLTYPE size( 
        /* [retval][out] */ unsigned int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setSize( 
        /* [in] */ unsigned int size);
    
    virtual HRESULT STDMETHODCALLTYPE src( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setSrc( 
        /* [in] */ BSTR src);
    
    virtual HRESULT STDMETHODCALLTYPE tabIndex( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTabIndex( 
        /* [in] */ int tabIndex);
    
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setType( 
        /* [in] */ BSTR type);
    
    virtual HRESULT STDMETHODCALLTYPE useMap( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setUseMap( 
        /* [in] */ BSTR useMap);
    
    virtual HRESULT STDMETHODCALLTYPE value( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ BSTR value);
        
    virtual HRESULT STDMETHODCALLTYPE select( void);
    
    virtual HRESULT STDMETHODCALLTYPE click( void);

    virtual HRESULT STDMETHODCALLTYPE setSelectionStart( 
        /* [in] */ long start);
    
    virtual HRESULT STDMETHODCALLTYPE selectionStart( 
        /* [retval][out] */ long *start);
    
    virtual HRESULT STDMETHODCALLTYPE setSelectionEnd( 
        /* [in] */ long end);
    
    virtual HRESULT STDMETHODCALLTYPE selectionEnd( 
        /* [retval][out] */ long *end);

    // IFormsAutoFillTransition
    virtual HRESULT STDMETHODCALLTYPE isTextField(
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE rectOnScreen( 
        /* [retval][out] */ LPRECT rect);
    
    virtual HRESULT STDMETHODCALLTYPE replaceCharactersInRange( 
        /* [in] */ int startTarget,
        /* [in] */ int endTarget,
        /* [in] */ BSTR replacementString,
        /* [in] */ int index);
    
    virtual HRESULT STDMETHODCALLTYPE selectedRange( 
        /* [out] */ int *start,
        /* [out] */ int *end);
    
    virtual HRESULT STDMETHODCALLTYPE setAutofilled( 
        /* [in] */ BOOL filled);

    // IFormPromptAdditions
    virtual HRESULT STDMETHODCALLTYPE isUserEdited( 
        /* [retval][out] */ BOOL *result);
};

class DeprecatedDOMHTMLTextAreaElement : public DeprecatedDOMHTMLElement, public IDeprecatedDOMHTMLTextAreaElement, public IFormPromptAdditions
{
protected:
    DeprecatedDOMHTMLTextAreaElement();
public:
    DeprecatedDOMHTMLTextAreaElement(WebCore::Element* e) : DeprecatedDOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMHTMLElement::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT *result) { return DeprecatedDOMHTMLElement::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMHTMLElement::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMHTMLElement::setException(description); }

    // IDeprecatedDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::nodeValue(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue( 
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setNodeValue(value); }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType( 
        /* [retval][out] */ unsigned short *result) { return DeprecatedDOMHTMLElement::nodeType(result); }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::parentNode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes( 
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::childNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::firstChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::lastChild(result); }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::previousSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling( 
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes( 
        /* [retval][out] */ IDeprecatedDOMNamedNodeMap **result) { return DeprecatedDOMHTMLElement::attributes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument( 
        /* [retval][out] */ IDeprecatedDOMDocument **result) { return DeprecatedDOMHTMLElement::ownerDocument(result); }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *refChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::insertBefore(newChild, refChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild( 
        /* [in] */ IDeprecatedDOMNode *newChild,
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::replaceChild(newChild, oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::removeChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild( 
        /* [in] */ IDeprecatedDOMNode *oldChild,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::appendChild(oldChild, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasChildNodes(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode( 
        /* [in] */ BOOL deep,
        /* [retval][out] */ IDeprecatedDOMNode **result) { return DeprecatedDOMHTMLElement::cloneNode(deep, result); }
    
    virtual HRESULT STDMETHODCALLTYPE normalize( void) { return DeprecatedDOMHTMLElement::normalize(); }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported( 
        /* [in] */ BSTR feature,
        /* [in] */ BSTR version,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::isSupported(feature, version, result); }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::namespaceURI(result); }
    
    virtual HRESULT STDMETHODCALLTYPE prefix( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::prefix(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix( 
        /* [in] */ BSTR prefix) { return DeprecatedDOMHTMLElement::setPrefix(prefix); }
    
    virtual HRESULT STDMETHODCALLTYPE localName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::localName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes( 
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributes(result); }

    virtual HRESULT STDMETHODCALLTYPE isSameNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isSameNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode( 
        /* [in] */ IDeprecatedDOMNode* other,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMHTMLElement::isEqualNode(other, result); }
    
    virtual HRESULT STDMETHODCALLTYPE textContent( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMHTMLElement::textContent(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setTextContent(text); }
    
    // IDeprecatedDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttribute(name, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute( 
        /* [in] */ BSTR name) { return DeprecatedDOMHTMLElement::removeAttribute(name); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNode(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNode(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr *oldAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::removeAttributeNode(oldAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName( 
        /* [in] */ BSTR name,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagName(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::getAttributeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR qualifiedName,
        /* [in] */ BSTR value) { return DeprecatedDOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value); }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName) { return DeprecatedDOMHTMLElement::removeAttributeNS(namespaceURI, localName); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr *newAttr,
        /* [retval][out] */ IDeprecatedDOMAttr **result) { return DeprecatedDOMHTMLElement::setAttributeNodeNS(newAttr, result); }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ IDeprecatedDOMNodeList **result) { return DeprecatedDOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttribute(name, result); }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS( 
        /* [in] */ BSTR namespaceURI,
        /* [in] */ BSTR localName,
        /* [retval][out] */ BOOL *result) { return DeprecatedDOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); }

    virtual HRESULT STDMETHODCALLTYPE focus( void) { return DeprecatedDOMHTMLElement::focus(); }
    
    virtual HRESULT STDMETHODCALLTYPE blur( void) { return DeprecatedDOMHTMLElement::blur(); }

    // IDeprecatedDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::idName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName( 
        /* [in] */ BSTR idName) { return DeprecatedDOMHTMLElement::setIdName(idName); }
    
    virtual HRESULT STDMETHODCALLTYPE title( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::title(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle( 
        /* [in] */ BSTR title) { return DeprecatedDOMHTMLElement::setTitle(title); }
    
    virtual HRESULT STDMETHODCALLTYPE lang( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::lang(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setLang( 
        /* [in] */ BSTR lang) { return DeprecatedDOMHTMLElement::setLang(lang); }
    
    virtual HRESULT STDMETHODCALLTYPE dir( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::dir(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setDir( 
        /* [in] */ BSTR dir) { return DeprecatedDOMHTMLElement::setDir(dir); }
    
    virtual HRESULT STDMETHODCALLTYPE className( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::className(result); }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName( 
        /* [in] */ BSTR className) { return DeprecatedDOMHTMLElement::setClassName(className); }

    virtual HRESULT STDMETHODCALLTYPE innerHTML( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerHTML(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML( 
        /* [in] */ BSTR html) { return DeprecatedDOMHTMLElement::setInnerHTML(html); }
        
    virtual HRESULT STDMETHODCALLTYPE innerText( 
        /* [retval][out] */ BSTR *result) { return DeprecatedDOMHTMLElement::innerText(result); }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText( 
        /* [in] */ BSTR text) { return DeprecatedDOMHTMLElement::setInnerText(text); }

    // IDeprecatedDOMHTMLTextArea
    virtual HRESULT STDMETHODCALLTYPE defaultValue( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDefaultValue( 
        /* [in] */ BSTR val);
    
    virtual HRESULT STDMETHODCALLTYPE form( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement **result);
    
    virtual HRESULT STDMETHODCALLTYPE accessKey( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setAccessKey( 
        /* [in] */ BSTR key);
    
    virtual HRESULT STDMETHODCALLTYPE cols( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setCols( 
        /* [in] */ int cols);
    
    virtual HRESULT STDMETHODCALLTYPE disabled( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setDisabled( 
        /* [in] */ BOOL disabled);
    
    virtual HRESULT STDMETHODCALLTYPE name( 
        /* [retval][out] */ BSTR *name);
    
    virtual HRESULT STDMETHODCALLTYPE setName( 
        /* [in] */ BSTR name);
    
    virtual HRESULT STDMETHODCALLTYPE readOnly( 
        /* [retval][out] */ BOOL *result);
    
    virtual HRESULT STDMETHODCALLTYPE setReadOnly( 
        /* [in] */ BOOL readOnly);
    
    virtual HRESULT STDMETHODCALLTYPE rows( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setRows( 
        /* [in] */ int rows);
    
    virtual HRESULT STDMETHODCALLTYPE tabIndex( 
        /* [retval][out] */ int *result);
    
    virtual HRESULT STDMETHODCALLTYPE setTabIndex( 
        /* [in] */ int tabIndex);
    
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE value( 
        /* [retval][out] */ BSTR *result);
    
    virtual HRESULT STDMETHODCALLTYPE setValue( 
        /* [in] */ BSTR value);
        
    virtual HRESULT STDMETHODCALLTYPE select( void);

    // IFormPromptAdditions
    virtual HRESULT STDMETHODCALLTYPE isUserEdited( 
        /* [retval][out] */ BOOL *result);
};

#endif
