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

#include "config.h"
#include "WebKitDLL.h"
#include "DOMCoreClasses.h"

#include "COMPtr.h"
#include "DOMCSSClasses.h"
#include "DOMEventsClasses.h"
#include "DOMHTMLClasses.h"
#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/NodeList.h>
#include <WebCore/RenderObject.h>
#pragma warning(pop)

#include <initguid.h>
// {79A193A5-D783-4c73-9AD9-D10678B943DE}
DEFINE_GUID(IID_DeprecatedDOMNode, 0x79a193a5, 0xd783, 0x4c73, 0x9a, 0xd9, 0xd1, 0x6, 0x78, 0xb9, 0x43, 0xde);
// {3B0C0EFF-478B-4b0b-8290-D2321E08E23E}
DEFINE_GUID(IID_DeprecatedDOMElement, 0x3b0c0eff, 0x478b, 0x4b0b, 0x82, 0x90, 0xd2, 0x32, 0x1e, 0x8, 0xe2, 0x3e);

using namespace WebCore;
using namespace HTMLNames;

// DeprecatedDOMObject - IUnknown -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMObject::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMObject))
        *ppvObject = static_cast<IDeprecatedDOMObject*>(this);
    else
        return WebScriptObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMNode - IUnknown ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMNode))
        *ppvObject = static_cast<IDeprecatedDOMNode*>(this);
    else if (IsEqualGUID(riid, IID_DeprecatedDOMNode))
        *ppvObject = static_cast<DeprecatedDOMNode*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMNode --------------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::nodeName( 
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::nodeValue( 
    /* [retval][out] */ BSTR* result)
{
    if (!m_node)
        return E_FAIL;
    WebCore::String nodeValueStr = m_node->nodeValue();
    *result = SysAllocStringLen(nodeValueStr.characters(), nodeValueStr.length());
    if (nodeValueStr.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::setNodeValue( 
    /* [in] */ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::nodeType( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::parentNode( 
    /* [retval][out] */ IDeprecatedDOMNode** result)
{
    *result = 0;
    if (!m_node || !m_node->parentNode())
        return E_FAIL;
    *result = DeprecatedDOMNode::createInstance(m_node->parentNode());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::childNodes( 
    /* [retval][out] */ IDeprecatedDOMNodeList** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::firstChild( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::lastChild( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::previousSibling( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::nextSibling( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::attributes( 
    /* [retval][out] */ IDeprecatedDOMNamedNodeMap** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::ownerDocument( 
    /* [retval][out] */ IDeprecatedDOMDocument** result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_node)
        return E_FAIL;
    *result = DeprecatedDOMDocument::createInstance(m_node->ownerDocument());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::insertBefore( 
    /* [in] */ IDeprecatedDOMNode* /*newChild*/,
    /* [in] */ IDeprecatedDOMNode* /*refChild*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::replaceChild( 
    /* [in] */ IDeprecatedDOMNode* /*newChild*/,
    /* [in] */ IDeprecatedDOMNode* /*oldChild*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::removeChild( 
    /* [in] */ IDeprecatedDOMNode* /*oldChild*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::appendChild( 
    /* [in] */ IDeprecatedDOMNode* /*oldChild*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::hasChildNodes( 
    /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::cloneNode( 
    /* [in] */ BOOL /*deep*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::normalize( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::isSupported( 
    /* [in] */ BSTR /*feature*/,
    /* [in] */ BSTR /*version*/,
    /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::namespaceURI( 
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::prefix( 
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::setPrefix( 
    /* [in] */ BSTR /*prefix*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::localName( 
    /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::hasAttributes( 
    /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::isSameNode( 
    /* [in] */ IDeprecatedDOMNode* other,
    /* [retval][out] */ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = FALSE;

    if (!other)
        return E_POINTER;

    COMPtr<DeprecatedDOMNode> domOther;
    HRESULT hr = other->QueryInterface(IID_DeprecatedDOMNode, (void**)&domOther);
    if (FAILED(hr))
        return hr;

    *result = m_node->isSameNode(domOther->node()) ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::isEqualNode( 
    /* [in] */ IDeprecatedDOMNode* /*other*/,
    /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::textContent( 
    /* [retval][out] */ BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_node->textContent()).release();

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::setTextContent( 
    /* [in] */ BSTR /*text*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMNode - IDeprecatedDOMEventTarget --------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::addEventListener( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ IDeprecatedDOMEventListener* /*listener*/,
    /* [in] */ BOOL /*useCapture*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::removeEventListener( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ IDeprecatedDOMEventListener* /*listener*/,
    /* [in] */ BOOL /*useCapture*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNode::dispatchEvent( 
    /* [in] */ IDeprecatedDOMEvent* evt,
    /* [retval][out] */ BOOL* result)
{
    if (!m_node || !evt)
        return E_FAIL;

#if 0   // FIXME - raise dom exceptions
    if (![self _node]->isEventTargetNode())
        WebCore::raiseDOMException(DOM_NOT_SUPPORTED_ERR);
#endif

    COMPtr<DeprecatedDOMEvent> domEvent;
    HRESULT hr = evt->QueryInterface(IID_DeprecatedDOMEvent, (void**) &domEvent);
    if (FAILED(hr))
        return hr;

    WebCore::ExceptionCode ec = 0;
    *result = WebCore::EventTargetNodeCast(m_node)->dispatchEvent(domEvent->coreEvent(), ec) ? TRUE : FALSE;
#if 0   // FIXME - raise dom exceptions
    WebCore::raiseOnDOMError(ec);
#endif
    return S_OK;
}

// DeprecatedDOMNode - DeprecatedDOMNode ----------------------------------------------------------

DeprecatedDOMNode::DeprecatedDOMNode(WebCore::Node* n)
: m_node(0)
{
    if (n)
        n->ref();

    m_node = n;
}

DeprecatedDOMNode::~DeprecatedDOMNode()
{
    if (m_node)
        m_node->deref();
}

IDeprecatedDOMNode* DeprecatedDOMNode::createInstance(WebCore::Node* n)
{
    if (!n)
        return 0;

    HRESULT hr = S_OK;
    IDeprecatedDOMNode* domNode = 0;
    WebCore::Node::NodeType nodeType = n->nodeType();

    switch (nodeType) {
        case WebCore::Node::ELEMENT_NODE: 
        {
            IDeprecatedDOMElement* newElement = DeprecatedDOMElement::createInstance(static_cast<WebCore::Element*>(n));
            if (newElement) {
                hr = newElement->QueryInterface(IID_IDeprecatedDOMNode, (void**)&domNode);
                newElement->Release();
            }
        }
        break;
        case WebCore::Node::DOCUMENT_NODE:
        {
            IDeprecatedDOMDocument* newDocument = DeprecatedDOMDocument::createInstance(n->document());
            if (newDocument) {
                hr = newDocument->QueryInterface(IID_IDeprecatedDOMNode, (void**)&domNode);
                newDocument->Release();
            }
        }
        break;
        default:
        {
            DeprecatedDOMNode* newNode = new DeprecatedDOMNode(n);
            hr = newNode->QueryInterface(IID_IDeprecatedDOMNode, (void**)&domNode);
        }
        break;
    }

    if (FAILED(hr))
        return 0;

    return domNode;
}

// DeprecatedDOMNodeList - IUnknown -----------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMNodeList::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMNodeList))
        *ppvObject = static_cast<IDeprecatedDOMNodeList*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// IDeprecatedDOMNodeList ---------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMNodeList::item( 
    /* [in] */ UINT index,
    /* [retval][out] */ IDeprecatedDOMNode **result)
{
    *result = 0;
    if (!m_nodeList)
        return E_FAIL;

    WebCore::Node* itemNode = m_nodeList->item(index);
    if (!itemNode)
        return E_FAIL;

    *result = DeprecatedDOMNode::createInstance(itemNode);
    if (!(*result))
        return E_FAIL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMNodeList::length( 
        /* [retval][out] */ UINT *result)
{
    *result = 0;
    if (!m_nodeList)
        return E_FAIL;
    *result = m_nodeList->length();
    return S_OK;
}

// DeprecatedDOMNodeList - DeprecatedDOMNodeList --------------------------------------------------

DeprecatedDOMNodeList::DeprecatedDOMNodeList(WebCore::NodeList* l)
: m_nodeList(0)
{
    if (l)
        l->ref();

    m_nodeList = l;
}

DeprecatedDOMNodeList::~DeprecatedDOMNodeList()
{
    if (m_nodeList)
        m_nodeList->deref();
}

IDeprecatedDOMNodeList* DeprecatedDOMNodeList::createInstance(WebCore::NodeList* l)
{
    if (!l)
        return 0;

    IDeprecatedDOMNodeList* domNodeList = 0;
    DeprecatedDOMNodeList* newNodeList = new DeprecatedDOMNodeList(l);
    if (FAILED(newNodeList->QueryInterface(IID_IDeprecatedDOMNodeList, (void**)&domNodeList)))
        return 0;

    return domNodeList;
}

// DeprecatedDOMDocument - IUnknown -----------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMDocument))
        *ppvObject = static_cast<IDeprecatedDOMDocument*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMViewCSS))
        *ppvObject = static_cast<IDeprecatedDOMViewCSS*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMDocumentEvent))
        *ppvObject = static_cast<IDeprecatedDOMDocumentEvent*>(this);
    else
        return DeprecatedDOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMDocument ----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::doctype( 
    /* [retval][out] */ IDeprecatedDOMDocumentType** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::implementation( 
    /* [retval][out] */ IDeprecatedDOMImplementation** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::documentElement( 
    /* [retval][out] */ IDeprecatedDOMElement** result)
{
    *result = DeprecatedDOMElement::createInstance(m_document->documentElement());
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createElement( 
    /* [in] */ BSTR tagName,
    /* [retval][out] */ IDeprecatedDOMElement** result)
{
    if (!m_document)
        return E_FAIL;

    String tagNameString(tagName);
    ExceptionCode ec;
    *result = DeprecatedDOMElement::createInstance(m_document->createElement(tagNameString, ec).get());
    if (!(*result))
        return E_FAIL;
    return S_OK;    
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createDocumentFragment( 
    /* [retval][out] */ IDeprecatedDOMDocumentFragment** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createTextNode( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDeprecatedDOMText** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createComment( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDeprecatedDOMComment** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createCDATASection( 
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDeprecatedDOMCDATASection** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createProcessingInstruction( 
    /* [in] */ BSTR /*target*/,
    /* [in] */ BSTR /*data*/,
    /* [retval][out] */ IDeprecatedDOMProcessingInstruction** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createAttribute( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createEntityReference( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDeprecatedDOMEntityReference** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::getElementsByTagName( 
    /* [in] */ BSTR tagName,
    /* [retval][out] */ IDeprecatedDOMNodeList** result)
{
    if (!m_document)
        return E_FAIL;

    String tagNameString(tagName);
    *result = DeprecatedDOMNodeList::createInstance(m_document->getElementsByTagName(tagNameString).get());
    if (!(*result))
        return E_FAIL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::importNode( 
    /* [in] */ IDeprecatedDOMNode* /*importedNode*/,
    /* [in] */ BOOL /*deep*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createElementNS( 
    /* [in] */ BSTR /*namespaceURI*/,
    /* [in] */ BSTR /*qualifiedName*/,
    /* [retval][out] */ IDeprecatedDOMElement** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createAttributeNS( 
    /* [in] */ BSTR /*namespaceURI*/,
    /* [in] */ BSTR /*qualifiedName*/,
    /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::getElementsByTagNameNS( 
    /* [in] */ BSTR namespaceURI,
    /* [in] */ BSTR localName,
    /* [retval][out] */ IDeprecatedDOMNodeList** result)
{
    if (!m_document)
        return E_FAIL;

    String namespaceURIString(namespaceURI);
    String localNameString(localName);
    *result = DeprecatedDOMNodeList::createInstance(m_document->getElementsByTagNameNS(namespaceURIString, localNameString).get());
    if (!(*result))
        return E_FAIL;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::getElementById( 
    /* [in] */ BSTR /*elementId*/,
    /* [retval][out] */ IDeprecatedDOMElement** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMDocument - IDeprecatedDOMViewCSS --------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::getComputedStyle( 
    /* [in] */ IDeprecatedDOMElement* elt,
    /* [in] */ BSTR pseudoElt,
    /* [retval][out] */ IDeprecatedDOMCSSStyleDeclaration** result)
{
    if (!elt || !result)
        return E_POINTER;

    COMPtr<DeprecatedDOMElement> domEle;
    HRESULT hr = elt->QueryInterface(IID_DeprecatedDOMElement, (void**)&domEle);
    if (FAILED(hr))
        return hr;
    Element* element = domEle->element();

    WebCore::DOMWindow* dv = m_document->defaultView();
    String pseudoEltString(pseudoElt);
    
    if (!dv)
        return E_FAIL;
    
    *result = DeprecatedDOMCSSStyleDeclaration::createInstance(dv->getComputedStyle(element, pseudoEltString.impl()).get());
    return S_OK;
}

// DeprecatedDOMDocument - IDeprecatedDOMDocumentEvent --------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMDocument::createEvent( 
    /* [in] */ BSTR eventType,
    /* [retval][out] */ IDeprecatedDOMEvent **result)
{
    String eventTypeString(eventType, SysStringLen(eventType));
    WebCore::ExceptionCode ec = 0;
    *result = DeprecatedDOMEvent::createInstance(m_document->createEvent(eventTypeString, ec));
    return S_OK;
}

// DeprecatedDOMDocument - DeprecatedDOMDocument --------------------------------------------------

DeprecatedDOMDocument::DeprecatedDOMDocument(WebCore::Document* d)
: DeprecatedDOMNode(d)
, m_document(d)
{
}

DeprecatedDOMDocument::~DeprecatedDOMDocument()
{
}

IDeprecatedDOMDocument* DeprecatedDOMDocument::createInstance(WebCore::Document* d)
{
    if (!d)
        return 0;

    HRESULT hr;
    IDeprecatedDOMDocument* domDocument = 0;

    if (d->isHTMLDocument()) {
        DeprecatedDOMHTMLDocument* newDocument = new DeprecatedDOMHTMLDocument(d);
        hr = newDocument->QueryInterface(IID_IDeprecatedDOMDocument, (void**)&domDocument);
    } else {
        DeprecatedDOMDocument* newDocument = new DeprecatedDOMDocument(d);
        hr = newDocument->QueryInterface(IID_IDeprecatedDOMDocument, (void**)&domDocument);
    }

    if (FAILED(hr))
        return 0;

    return domDocument;
}

// DeprecatedDOMElement - IUnknown ------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMElement))
        *ppvObject = static_cast<IDeprecatedDOMElement*>(this);
    else if (IsEqualGUID(riid, IID_DeprecatedDOMElement))
        *ppvObject = static_cast<DeprecatedDOMElement*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMElementPrivate))
        *ppvObject = static_cast<IDeprecatedDOMElementPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMNodeExtensions))
        *ppvObject = static_cast<IDeprecatedDOMNodeExtensions*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMElementCSSInlineStyle))
        *ppvObject = static_cast<IDeprecatedDOMElementCSSInlineStyle*>(this);
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMElementExtensions))
        *ppvObject = static_cast<IDeprecatedDOMElementExtensions*>(this);
    else
        return DeprecatedDOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMElement - IDeprecatedDOMNodeExtensions---------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::boundingBox( 
    /* [retval][out] */ LPRECT rect)
{
    ::SetRectEmpty(rect);

    if (!m_element)
        return E_FAIL;

    WebCore::RenderObject *renderer = m_element->renderer();
    if (renderer) {
        IntRect boundsIntRect = renderer->absoluteBoundingBoxRect();
        rect->left = boundsIntRect.x();
        rect->top = boundsIntRect.y();
        rect->right = boundsIntRect.x() + boundsIntRect.width();
        rect->bottom = boundsIntRect.y() + boundsIntRect.height();
    }

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::lineBoxRects( 
    /* [size_is][in] */ RECT* /*rects*/,
    /* [in] */ int /*cRects*/)
{
    return E_NOTIMPL;
}

// IDeprecatedDOMElement ----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::tagName( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getAttribute( 
        /* [in] */ BSTR name,
        /* [retval][out] */ BSTR* result)
{
    if (!m_element)
        return E_FAIL;
    WebCore::String nameString(name, SysStringLen(name));
    WebCore::String& attrValueString = (WebCore::String&) m_element->getAttribute(nameString);
    *result = SysAllocStringLen(attrValueString.characters(), attrValueString.length());
    if (attrValueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setAttribute( 
        /* [in] */ BSTR name,
        /* [in] */ BSTR value)
{
    if (!m_element)
        return E_FAIL;

    WebCore::String nameString(name, SysStringLen(name));
    WebCore::String valueString(value, SysStringLen(value));
    WebCore::ExceptionCode ec = 0;
    m_element->setAttribute(nameString, valueString, ec);
    return ec ? E_FAIL : S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::removeAttribute( 
        /* [in] */ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getAttributeNode( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr* /*newAttr*/,
        /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::removeAttributeNode( 
        /* [in] */ IDeprecatedDOMAttr* /*oldAttr*/,
        /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getElementsByTagName( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ IDeprecatedDOMNodeList** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*qualifiedName*/,
        /* [in] */ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::removeAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getAttributeNodeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setAttributeNodeNS( 
        /* [in] */ IDeprecatedDOMAttr* /*newAttr*/,
        /* [retval][out] */ IDeprecatedDOMAttr** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::getElementsByTagNameNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ IDeprecatedDOMNodeList** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::hasAttribute( 
        /* [in] */ BSTR /*name*/,
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::hasAttributeNS( 
        /* [in] */ BSTR /*namespaceURI*/,
        /* [in] */ BSTR /*localName*/,
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::focus( void)
{
    if (!m_element)
        return E_FAIL;
    m_element->focus();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::blur( void)
{
    if (!m_element)
        return E_FAIL;
    m_element->blur();
    return S_OK;
}

// IDeprecatedDOMElementPrivate ---------------------------------------------------------

HRESULT DeprecatedDOMElement::coreElement(void **element)
{
    if (!m_element)
        return E_FAIL;
    *element = (void*) m_element;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::isEqual( 
    /* [in] */ IDeprecatedDOMElement *other,
    /* [retval][out] */ BOOL *result)
{
    *result = FALSE;

    if (!other || !result)
        return E_POINTER;

    IDeprecatedDOMElementPrivate* otherPriv;
    HRESULT hr = other->QueryInterface(IID_IDeprecatedDOMElementPrivate, (void**) &otherPriv);
    if (FAILED(hr))
        return hr;
    
    void* otherCoreEle;
    hr = otherPriv->coreElement(&otherCoreEle);
    otherPriv->Release();
    if (FAILED(hr))
        return hr;

    *result = (otherCoreEle == (void*)m_element) ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::isFocused( 
    /* [retval][out] */ BOOL *result)
{
    if (!m_element)
        return E_FAIL;

    if (m_element->document()->focusedNode() == m_element)
        *result = TRUE;
    else
        *result = FALSE;

    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::innerText(
    /* [retval][out] */ BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    if (!m_element) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    *result = BString(m_element->innerText()).release();
    return S_OK;
}

// IDeprecatedDOMElementCSSInlineStyle --------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::style( 
    /* [retval][out] */ IDeprecatedDOMCSSStyleDeclaration** result)
{
    if (!result)
        return E_POINTER;
    if (!m_element)
        return E_FAIL;

    WebCore::CSSStyleDeclaration* style = m_element->style();
    if (!style)
        return E_FAIL;

    *result = DeprecatedDOMCSSStyleDeclaration::createInstance(style);
    return S_OK;
}

// IDeprecatedDOMElementExtensions ------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::offsetLeft( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetLeft();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::offsetTop( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetTop();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::offsetWidth( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetWidth();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::offsetHeight( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetHeight();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::offsetParent( 
    /* [retval][out] */ IDeprecatedDOMElement** /*result*/)
{
    // FIXME
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::clientWidth( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->clientWidth();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::clientHeight( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->clientHeight();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollLeft( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollLeft();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setScrollLeft( 
    /* [in] */ int /*newScrollLeft*/)
{
    // FIXME
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollTop( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollTop();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::setScrollTop( 
    /* [in] */ int /*newScrollTop*/)
{
    // FIXME
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollWidth( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollWidth();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollHeight( 
    /* [retval][out] */ int* result)
{
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollHeight();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollIntoView( 
    /* [in] */ BOOL alignWithTop)
{
    if (!m_element)
        return E_FAIL;

    m_element->scrollIntoView(!!alignWithTop);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMElement::scrollIntoViewIfNeeded( 
    /* [in] */ BOOL centerIfNeeded)
{
    if (!m_element)
        return E_FAIL;

    m_element->scrollIntoViewIfNeeded(!!centerIfNeeded);
    return S_OK;
}

// DeprecatedDOMElement -----------------------------------------------------------------

DeprecatedDOMElement::DeprecatedDOMElement(WebCore::Element* e)
: DeprecatedDOMNode(e)
, m_element(e)
{
}

DeprecatedDOMElement::~DeprecatedDOMElement()
{
}

IDeprecatedDOMElement* DeprecatedDOMElement::createInstance(WebCore::Element* e)
{
    if (!e)
        return 0;

    HRESULT hr;
    IDeprecatedDOMElement* domElement = 0;

    if (e->hasTagName(formTag)) {
        DeprecatedDOMHTMLFormElement* newElement = new DeprecatedDOMHTMLFormElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else if (e->hasTagName(selectTag)) {
        DeprecatedDOMHTMLSelectElement* newElement = new DeprecatedDOMHTMLSelectElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else if (e->hasTagName(optionTag)) {
        DeprecatedDOMHTMLOptionElement* newElement = new DeprecatedDOMHTMLOptionElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else if (e->hasTagName(inputTag)) {
        DeprecatedDOMHTMLInputElement* newElement = new DeprecatedDOMHTMLInputElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else if (e->hasTagName(textareaTag)) {
        DeprecatedDOMHTMLTextAreaElement* newElement = new DeprecatedDOMHTMLTextAreaElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else if (e->isHTMLElement()) {
        DeprecatedDOMHTMLElement* newElement = new DeprecatedDOMHTMLElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    } else {
        DeprecatedDOMElement* newElement = new DeprecatedDOMElement(e);
        hr = newElement->QueryInterface(IID_IDeprecatedDOMElement, (void**)&domElement);
    }

    if (FAILED(hr))
        return 0;

    return domElement;
}
