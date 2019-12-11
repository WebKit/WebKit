/*
 * Copyright (C) 2006-2007, 2015 Apple Inc.  All rights reserved.
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

#ifndef DOMHTMLClasses_H
#define DOMHTMLClasses_H

#include "WebKit.h"
#include "DOMCoreClasses.h"
#include <wtf/RefPtr.h>

namespace WebCore {
    class HTMLCollection;
    class HTMLOptionsCollection;
}

class DOMHTMLCollection : public DOMObject, public IDOMHTMLCollection
{
protected:
    DOMHTMLCollection(WebCore::HTMLCollection* c);

public:
    static IDOMHTMLCollection* createInstance(WebCore::HTMLCollection*);

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

    // IDOMHTMLCollection
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ UINT* result);
    virtual HRESULT STDMETHODCALLTYPE item(UINT index, _COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE namedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode**);

protected:
    RefPtr<WebCore::HTMLCollection> m_collection;
};

class DOMHTMLOptionsCollection : public DOMObject, public IDOMHTMLOptionsCollection
{
public:
    static IDOMHTMLOptionsCollection* createInstance(WebCore::HTMLOptionsCollection*);

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

    // IDOMHTMLOptionsCollection
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ unsigned*);
    virtual HRESULT STDMETHODCALLTYPE setLength(unsigned int);
    virtual HRESULT STDMETHODCALLTYPE item(unsigned index, _COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE namedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode**);

private:
    DOMHTMLOptionsCollection(WebCore::HTMLOptionsCollection*);

    RefPtr<WebCore::HTMLOptionsCollection> m_collection;
};

class DOMHTMLDocument : public DOMDocument, public IDOMHTMLDocument
{
protected:
    DOMHTMLDocument();
public:
    DOMHTMLDocument(WebCore::Document* d) : DOMDocument(d) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMDocument::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMDocument::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMDocument::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMDocument::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMDocument::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMDocument::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMDocument::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMDocument::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMDocument::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMDocument::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMDocument::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMDocument::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMDocument::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMDocument::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMDocument::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::firstChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::nextSibling(result); }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMDocument::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMDocument::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMDocument::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMDocument::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMDocument::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMDocument::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMDocument::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMDocument::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMDocument::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMDocument::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMDocument::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMDocument::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMDocument::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMDocument::setTextContent(text);
    }
    
    // IDOMDocument
    virtual HRESULT STDMETHODCALLTYPE doctype(_COM_Outptr_opt_ IDOMDocumentType** result)
    {
        return DOMDocument::doctype(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE implementation(_COM_Outptr_opt_ IDOMImplementation** result)
    {
        return DOMDocument::implementation(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE documentElement(_COM_Outptr_opt_ IDOMElement** result)
    {
        return DOMDocument::documentElement(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createElement(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMElement** result)
    {
        return DOMDocument::createElement(tagName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createDocumentFragment(_COM_Outptr_opt_ IDOMDocumentFragment** result)
    {
        return DOMDocument::createDocumentFragment(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createTextNode(_In_ BSTR data, _COM_Outptr_opt_ IDOMText** result)
    {
        return DOMDocument::createTextNode(data, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createComment(_In_ BSTR data, _COM_Outptr_opt_ IDOMComment** result)
    {
        return DOMDocument::createComment(data, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createCDATASection(_In_ BSTR data, _COM_Outptr_opt_ IDOMCDATASection** result)
    {
        return DOMDocument::createCDATASection(data, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createProcessingInstruction(_In_ BSTR target, _In_ BSTR data, _COM_Outptr_opt_ IDOMProcessingInstruction** result)
    {
        return DOMDocument::createProcessingInstruction(target, data, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createAttribute(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMDocument::createAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createEntityReference(_In_ BSTR name, _COM_Outptr_opt_ IDOMEntityReference** result)
    {
        return DOMDocument::createEntityReference(name, result);
    }

    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMDocument::getElementsByTagName(tagName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE importNode(_In_opt_ IDOMNode* importedNode, BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMDocument::importNode(importedNode, deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createElementNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _COM_Outptr_opt_ IDOMElement** result)
    {
        return DOMDocument::createElementNS(namespaceURI, qualifiedName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE createAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMDocument::createAttributeNS(namespaceURI, qualifiedName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMDocument::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementById(_In_ BSTR elementId, _COM_Outptr_opt_ IDOMElement** result)
    {
        return DOMDocument::getElementById(elementId, result);
    }

    // IDOMHTMLDocument
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE referrer(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE domain(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE URL(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE body(_COM_Outptr_opt_ IDOMHTMLElement**);
    virtual HRESULT STDMETHODCALLTYPE setBody(_In_opt_ IDOMHTMLElement*);
    virtual HRESULT STDMETHODCALLTYPE images(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE applets(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE links(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE forms(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE anchors(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE cookie(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setCookie(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE open();
    virtual HRESULT STDMETHODCALLTYPE close();
    virtual HRESULT STDMETHODCALLTYPE write(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE writeln(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE getElementById_(_In_ BSTR elementId, _COM_Outptr_opt_ IDOMElement**);
    virtual HRESULT STDMETHODCALLTYPE getElementsByName(_In_ BSTR elementName, _COM_Outptr_opt_ IDOMNodeList**);
};

class DOMHTMLElement : public DOMElement, public IDOMHTMLElement
{
protected:
    DOMHTMLElement();
public:
    DOMHTMLElement(WebCore::Element* e) : DOMElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMElement::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result)
    {
        return DOMElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr* newAttr, _COM_Outptr_opt_ IDOMAttr** result) 
    {
        return DOMElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr* oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMElement::getAttributeNS(namespaceURI, localName, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr* newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lg);
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR);
};

class DOMHTMLFormElement : public DOMHTMLElement, public IDOMHTMLFormElement
{
protected:
    DOMHTMLFormElement();
public:
    DOMHTMLFormElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMHTMLElement::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result) 
    {
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::firstChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes( _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result) 
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMHTMLElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result) 
    { 
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result) 
    {
        return DOMHTMLElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result) { return DOMHTMLElement::tagName(result); }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr *oldAttr, _COM_Outptr_opt_ IDOMAttr** result) 
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName)
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang)
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir);
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html)
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text)
    {
        return DOMHTMLElement::setInnerText(text);
    }

    // IDOMHTMLFormElement
    virtual HRESULT STDMETHODCALLTYPE elements(_COM_Outptr_opt_ IDOMHTMLCollection**);
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE name(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE acceptCharset(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAcceptCharset(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE action(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAction(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE encType(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setEnctype(_In_opt_ BSTR*);
    virtual HRESULT STDMETHODCALLTYPE method(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setMethod(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE target(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setTarget(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE submit();
    virtual HRESULT STDMETHODCALLTYPE reset();
};

class DOMHTMLSelectElement : public DOMHTMLElement, public IDOMHTMLSelectElement, public IFormsAutoFillTransitionSelect
{
protected:
    DOMHTMLSelectElement();
public:
    DOMHTMLSelectElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMHTMLElement::nodeType(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::insertBefore(newChild, refChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result) 
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMHTMLElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr *oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr* newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName)
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang)
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir);
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html)
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text)
    {
        return DOMHTMLElement::setInnerText(text);
    }

    // IDOMHTMLSelectElement
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE selectedIndex(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setSelectedIndx(int);
    virtual HRESULT STDMETHODCALLTYPE value(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE length(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE form(_COM_Outptr_opt_ IDOMHTMLFormElement**);
    virtual HRESULT STDMETHODCALLTYPE options(_COM_Outptr_opt_ IDOMHTMLOptionsCollection**);
    virtual HRESULT STDMETHODCALLTYPE disabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDisabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE multiple(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setMultiple(BOOL);
    virtual HRESULT STDMETHODCALLTYPE name(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE size(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setSize(int);
    virtual HRESULT STDMETHODCALLTYPE tabIndex(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setTabIndex(int);
    virtual HRESULT STDMETHODCALLTYPE add(_In_opt_ IDOMHTMLElement*, _In_opt_ IDOMHTMLElement* before);
    virtual HRESULT STDMETHODCALLTYPE remove(int index);
    
    // IFormsAutoFillTransitionSelect
    virtual HRESULT STDMETHODCALLTYPE activateItemAtIndex(int);
};

class DOMHTMLOptionElement : public DOMHTMLElement, public IDOMHTMLOptionElement
{
protected:
    DOMHTMLOptionElement();
public:
    DOMHTMLOptionElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result) 
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    { 
        return DOMHTMLElement::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    { 
        return DOMHTMLElement::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize() 
    { 
        return DOMHTMLElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result) 
    {
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::namespaceURI(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::prefix(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr *oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_  BSTR value) 
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName)
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang)
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir);
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html)
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text)
    { 
        return DOMHTMLElement::setInnerText(text);
    }

    // IDOMHTMLOptionElement
    virtual HRESULT STDMETHODCALLTYPE form(_COM_Outptr_opt_ IDOMHTMLFormElement**);
    virtual HRESULT STDMETHODCALLTYPE defaultSelected(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultSelected(BOOL);
    virtual HRESULT STDMETHODCALLTYPE text(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE index(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE disabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDisabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE label(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setLabel(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE selected(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setSelected(BOOL);
    virtual HRESULT STDMETHODCALLTYPE value(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE setValue(_In_ BSTR);
};

class DOMHTMLInputElement : public DOMHTMLElement, public IDOMHTMLInputElement, public IFormsAutoFillTransition, public IFormPromptAdditions
{
protected:
    DOMHTMLInputElement();
public:
    DOMHTMLInputElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMHTMLElement::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    { 
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result) 
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::removeChild(oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result) 
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMHTMLElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr *oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result); 
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName) 
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang)
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir);
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html)
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text) 
    {
        return DOMHTMLElement::setInnerText(text);
    }

    // IDOMHTMLInputElement
    virtual HRESULT STDMETHODCALLTYPE defaultValue(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE defaultChecked(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultChecked(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE form(_COM_Outptr_opt_ IDOMHTMLElement**);
    virtual HRESULT STDMETHODCALLTYPE accept(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAccept(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE accessKey(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAccessKey(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE align(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAlign(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE alt(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAlt(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE checked(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setChecked(BOOL);
    virtual HRESULT STDMETHODCALLTYPE disabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDisabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE maxLength(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setMaxLength(int);
    virtual HRESULT STDMETHODCALLTYPE name(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE readOnly(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setReadOnly(BOOL);
    virtual HRESULT STDMETHODCALLTYPE size(_Out_ unsigned*);
    virtual HRESULT STDMETHODCALLTYPE setSize(unsigned);
    virtual HRESULT STDMETHODCALLTYPE src(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setSrc(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE tabIndex(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setTabIndex(int);
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setType(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE useMap(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setUseMap(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE value(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE select();
    virtual HRESULT STDMETHODCALLTYPE click();
    virtual HRESULT STDMETHODCALLTYPE setSelectionStart(long);
    virtual HRESULT STDMETHODCALLTYPE selectionStart(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE setSelectionEnd(long);
    virtual HRESULT STDMETHODCALLTYPE selectionEnd(_Out_ long*);

    // IFormsAutoFillTransition
    virtual HRESULT STDMETHODCALLTYPE isTextField(_Out_ BOOL*);
    
    virtual HRESULT STDMETHODCALLTYPE rectOnScreen(_Out_ LPRECT);
    virtual HRESULT STDMETHODCALLTYPE replaceCharactersInRange(int startTarget, int endTarget, _In_ BSTR replacementString, int index);
    virtual HRESULT STDMETHODCALLTYPE selectedRange(_Out_ int* start, _Out_ int* end);
    virtual HRESULT STDMETHODCALLTYPE setAutofilled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE isAutofilled(_Out_ BOOL*);
    
    // IFormPromptAdditions
    virtual HRESULT STDMETHODCALLTYPE isUserEdited(_Out_ BOOL*);

    virtual HRESULT STDMETHODCALLTYPE setValueForUser(_In_ BSTR);
};

class DOMHTMLTextAreaElement : public DOMHTMLElement, public IDOMHTMLTextAreaElement, public IFormPromptAdditions
{
protected:
    DOMHTMLTextAreaElement();
public:
    DOMHTMLTextAreaElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMHTMLElement::nodeType(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::firstChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result) 
    { 
        return DOMHTMLElement::insertBefore(newChild, refChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result) 
    {
        return DOMHTMLElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMHTMLElement::normalize(); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_  BOOL* result)
    { 
        return DOMHTMLElement::isSameNode(other, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    { 
        return DOMHTMLElement::isEqualNode(other, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_  BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr* newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr* oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result) 
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result) 
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result) 
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur() 
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName)
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang)
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir);
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html) 
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text)
    {
        return DOMHTMLElement::setInnerText(text);
    }

    // IDOMHTMLTextArea
    virtual HRESULT STDMETHODCALLTYPE defaultValue(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setDefaultValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE form(_COM_Outptr_opt_ IDOMHTMLElement**);
    virtual HRESULT STDMETHODCALLTYPE accessKey(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setAccessKey(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE cols(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setCols(int);
    virtual HRESULT STDMETHODCALLTYPE disabled(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setDisabled(BOOL);
    virtual HRESULT STDMETHODCALLTYPE name(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setName(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE readOnly(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE setReadOnly(BOOL);
    virtual HRESULT STDMETHODCALLTYPE rows(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setRows(int);
    virtual HRESULT STDMETHODCALLTYPE tabIndex(_Out_ int*);
    virtual HRESULT STDMETHODCALLTYPE setTabIndex(int);
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE value(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE setValue(_In_ BSTR);
    virtual HRESULT STDMETHODCALLTYPE select();

    // IFormPromptAdditions
    virtual HRESULT STDMETHODCALLTYPE isUserEdited(_Out_ BOOL*);
};

class DOMHTMLIFrameElement : public DOMHTMLElement, public IDOMHTMLIFrameElement
{
protected:
    DOMHTMLIFrameElement();
public:
    DOMHTMLIFrameElement(WebCore::Element* e) : DOMHTMLElement(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMHTMLElement::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMHTMLElement::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException(_In_ BSTR exceptionMessage, _Out_ BOOL* result)
    {
        return DOMHTMLElement::throwException(exceptionMessage, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod(_In_ BSTR name, __in_ecount_opt(cArgs) const VARIANT args[], int cArgs, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::callWebScriptMethod(name, args, cArgs, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript(_In_ BSTR script, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::evaluateWebScript(script, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey(_In_ BSTR name)
    {
        return DOMHTMLElement::removeWebScriptKey(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation(__deref_opt_out BSTR* stringRepresentation)
    {
        return DOMHTMLElement::stringRepresentation(stringRepresentation);
    }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex(unsigned index, _Out_ VARIANT* result)
    {
        return DOMHTMLElement::webScriptValueAtIndex(index, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex(unsigned index, VARIANT val)
    {
        return DOMHTMLElement::setWebScriptValueAtIndex(index, val);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setException(_In_ BSTR description)
    {
        return DOMHTMLElement::setException(description);
    }

    // IDOMNode
    virtual HRESULT STDMETHODCALLTYPE nodeName(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::nodeName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeValue(__deref_opt_out BSTR* result) 
    {
        return DOMHTMLElement::nodeValue(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setNodeValue(_In_ BSTR value)
    {
        return DOMHTMLElement::setNodeValue(value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nodeType(_Out_ unsigned short* result)
    {
        return DOMHTMLElement::nodeType(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE parentNode(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::parentNode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::childNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE firstChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::firstChild(result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE lastChild(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::lastChild(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE previousSibling(_COM_Outptr_opt_ IDOMNode** result)
    { return DOMHTMLElement::previousSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE nextSibling(_COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::nextSibling(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
    {
        return DOMHTMLElement::attributes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE ownerDocument(_COM_Outptr_opt_ IDOMDocument** result) 
    {
        return DOMHTMLElement::ownerDocument(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode *refChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::insertBefore(newChild, refChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE replaceChild(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::replaceChild(newChild, oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::removeChild(oldChild, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE appendChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::appendChild(oldChild, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasChildNodes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasChildNodes(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cloneNode(BOOL deep, _COM_Outptr_opt_ IDOMNode** result)
    {
        return DOMHTMLElement::cloneNode(deep, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE normalize()
    {
        return DOMHTMLElement::normalize();
    }
    
    virtual HRESULT STDMETHODCALLTYPE isSupported(_In_ BSTR feature, _In_ BSTR version, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSupported(feature, version, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE namespaceURI(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::namespaceURI(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE prefix(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::prefix(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setPrefix(_In_ BSTR prefix)
    {
        return DOMHTMLElement::setPrefix(prefix);
    }
    
    virtual HRESULT STDMETHODCALLTYPE localName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::localName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributes(_Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributes(result);
    }

    virtual HRESULT STDMETHODCALLTYPE isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
    {
        return DOMHTMLElement::isSameNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE isEqualNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result) 
    {
        return DOMHTMLElement::isEqualNode(other, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE textContent(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::textContent(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTextContent(_In_ BSTR text)
    {
        return DOMHTMLElement::setTextContent(text);
    }
    
    // IDOMElement
    virtual HRESULT STDMETHODCALLTYPE tagName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::tagName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
    { 
        return DOMHTMLElement::getAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttribute(_In_ BSTR name, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttribute(name, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttribute(_In_ BSTR name)
    {
        return DOMHTMLElement::removeAttribute(name);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNode(_In_ BSTR name, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::getAttributeNode(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNode(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result) 
    {
        return DOMHTMLElement::setAttributeNode(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNode(_In_opt_ IDOMAttr *oldAttr, _COM_Outptr_opt_ IDOMAttr** result)
    {
        return DOMHTMLElement::removeAttributeNode(oldAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagName(_In_ BSTR name, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagName(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, __deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::getAttributeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR qualifiedName, _In_ BSTR value)
    {
        return DOMHTMLElement::setAttributeNS(namespaceURI, qualifiedName, value);
    }
    
    virtual HRESULT STDMETHODCALLTYPE removeAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName)
    {
        return DOMHTMLElement::removeAttributeNS(namespaceURI, localName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getAttributeNodeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMAttr** result)
    { 
        return DOMHTMLElement::getAttributeNodeNS(namespaceURI, localName, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setAttributeNodeNS(_In_opt_ IDOMAttr *newAttr, _COM_Outptr_opt_ IDOMAttr** result) 
    {
        return DOMHTMLElement::setAttributeNodeNS(newAttr, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
    {
        return DOMHTMLElement::getElementsByTagNameNS(namespaceURI, localName, result); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttribute(_In_ BSTR name, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttribute(name, result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE hasAttributeNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _Out_ BOOL* result)
    {
        return DOMHTMLElement::hasAttributeNS(namespaceURI, localName, result);
    }

    virtual HRESULT STDMETHODCALLTYPE focus()
    {
        return DOMHTMLElement::focus();
    }
    
    virtual HRESULT STDMETHODCALLTYPE blur()
    {
        return DOMHTMLElement::blur();
    }

    // IDOMHTMLElement
    virtual HRESULT STDMETHODCALLTYPE idName(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::idName(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setIdName(_In_ BSTR idName)
    {
        return DOMHTMLElement::setIdName(idName);
    }
    
    virtual HRESULT STDMETHODCALLTYPE title(__deref_opt_out BSTR* result)
    { 
        return DOMHTMLElement::title(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setTitle(_In_ BSTR title)
    {
        return DOMHTMLElement::setTitle(title);
    }
    
    virtual HRESULT STDMETHODCALLTYPE lang(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::lang(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setLang(_In_ BSTR lang) 
    {
        return DOMHTMLElement::setLang(lang);
    }
    
    virtual HRESULT STDMETHODCALLTYPE dir(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::dir(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setDir(_In_ BSTR dir)
    {
        return DOMHTMLElement::setDir(dir); 
    }
    
    virtual HRESULT STDMETHODCALLTYPE className(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::className(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE setClassName(_In_ BSTR className)
    {
        return DOMHTMLElement::setClassName(className);
    }

    virtual HRESULT STDMETHODCALLTYPE innerHTML(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerHTML(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerHTML(_In_ BSTR html)
    {
        return DOMHTMLElement::setInnerHTML(html);
    }
        
    virtual HRESULT STDMETHODCALLTYPE innerText(__deref_opt_out BSTR* result)
    {
        return DOMHTMLElement::innerText(result);
    }
        
    virtual HRESULT STDMETHODCALLTYPE setInnerText(_In_ BSTR text)
    {
        return DOMHTMLElement::setInnerText(text); 
    }

    // IDOMHTMLIFrameElement
    virtual HRESULT STDMETHODCALLTYPE contentFrame(_COM_Outptr_opt_ IWebFrame**);
};

#endif
