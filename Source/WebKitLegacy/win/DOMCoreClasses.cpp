/*
 * Copyright (C) 2006-2007, 2009, 2014-2015 Apple Inc.  All rights reserved.
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

#include "WebKitDLL.h"
#include "DOMCoreClasses.h"

#include "DOMCSSClasses.h"
#include "DOMEventsClasses.h"
#include "DOMHTMLClasses.h"
#include "WebKitGraphics.h"
#include <WebCore/Attr.h>
#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Document.h>
#include <WebCore/DragImage.h>
#include <WebCore/Element.h>
#include <WebCore/Event.h>
#include <WebCore/Font.h>
#include <WebCore/FontCascade.h>
#include <WebCore/Frame.h>
#include <WebCore/HTMLCollection.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/NamedNodeMap.h>
#include <WebCore/NodeList.h>
#include <WebCore/Range.h>
#include <WebCore/RenderElement.h>
#include <WebCore/RenderTreeAsText.h>
#include <WebCore/ScrollIntoViewOptions.h>
#include <WebCore/StyledElement.h>
#include <initguid.h>
#include <wtf/text/win/WCharStringExtras.h>

// {3B0C0EFF-478B-4b0b-8290-D2321E08E23E}
DEFINE_GUID(IID_DOMElement, 0x3b0c0eff, 0x478b, 0x4b0b, 0x82, 0x90, 0xd2, 0x32, 0x1e, 0x8, 0xe2, 0x3e);

// Our normal style is just to say "using namespace WebCore" rather than having
// individual using directives for each type from that namespace. But
// "DOMObject" exists both in the WebCore namespace and unnamespaced in this
// file, which leads to ambiguities if we say "using namespace WebCore".
using namespace WebCore::HTMLNames;
using WTF::AtomicString;
using WebCore::BString;
using WebCore::Element;
using WebCore::ExceptionCode;
using WebCore::Frame;
using WebCore::IntRect;
using WTF::String;

// DOMObject - IUnknown -------------------------------------------------------

HRESULT DOMObject::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMObject))
        *ppvObject = static_cast<IDOMObject*>(this);
    else
        return WebScriptObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMNode - IUnknown ---------------------------------------------------------

HRESULT DOMNode::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMNode))
        *ppvObject = static_cast<IDOMNode*>(this);
    else if (IsEqualGUID(riid, __uuidof(DOMNode)))
        *ppvObject = static_cast<DOMNode*>(this);
    else if (IsEqualGUID(riid, __uuidof(IDOMEventTarget)))
        *ppvObject = static_cast<IDOMEventTarget*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMNode --------------------------------------------------------------------

HRESULT DOMNode::nodeName(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;

    *result = BString(m_node->nodeName()).release();
    return S_OK;
}

HRESULT DOMNode::nodeValue(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;
    WTF::String nodeValueStr = m_node->nodeValue();
    *result = BString(nodeValueStr).release();
    if (nodeValueStr.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}

HRESULT DOMNode::setNodeValue(_In_ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::nodeType(_Out_ unsigned short* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::parentNode(_COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node || !m_node->parentNode())
        return E_FAIL;
    *result = DOMNode::createInstance(m_node->parentNode());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNode::childNodes(_COM_Outptr_opt_ IDOMNodeList** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;

    if (!result)
        return E_POINTER;

    *result = DOMNodeList::createInstance(m_node->childNodes().get());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNode::firstChild(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::lastChild(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::previousSibling(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::nextSibling(_COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;
    *result = DOMNode::createInstance(m_node->nextSibling());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNode::attributes(_COM_Outptr_opt_ IDOMNamedNodeMap** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;
    *result = DOMNamedNodeMap::createInstance(m_node->attributes());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNode::ownerDocument(_COM_Outptr_opt_ IDOMDocument** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_node)
        return E_FAIL;
    *result = DOMDocument::createInstance(m_node->ownerDocument());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNode::insertBefore(_In_opt_ IDOMNode* newChild, _In_opt_ IDOMNode* refChild, _COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    if (!m_node)
        return E_FAIL;

    COMPtr<DOMNode> newChildNode(Query, newChild);
    if (!newChildNode)
        return E_FAIL;

    COMPtr<DOMNode> refChildNode(Query, refChild);

    if (m_node->insertBefore(*newChildNode->node(), refChildNode ? refChildNode->node() : nullptr).hasException())
        return E_FAIL;

    *result = newChild;
    (*result)->AddRef();
    return S_OK;
}

HRESULT DOMNode::replaceChild(_In_opt_ IDOMNode* /*newChild*/, _In_opt_ IDOMNode* /*oldChild*/, _COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::removeChild(_In_opt_ IDOMNode* oldChild, _COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    if (!m_node)
        return E_FAIL;

    COMPtr<DOMNode> oldChildNode(Query, oldChild);
    if (!oldChildNode)
        return E_FAIL;

    if (m_node->removeChild(*oldChildNode->node()).hasException())
        return E_FAIL;

    *result = oldChild;
    (*result)->AddRef();
    return S_OK;
}

HRESULT DOMNode::appendChild(_In_opt_ IDOMNode* /*oldChild*/, _COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::hasChildNodes(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::cloneNode(BOOL /*deep*/, _COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::normalize()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::isSupported(_In_ BSTR /*feature*/, _In_ BSTR /*version*/, _Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::namespaceURI(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::prefix(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::setPrefix(_In_ BSTR /*prefix*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::localName(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMNode::hasAttributes(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::isSameNode(_In_opt_ IDOMNode* other, _Out_ BOOL* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = FALSE;

    if (!other)
        return E_POINTER;

    COMPtr<DOMNode> domOther;
    HRESULT hr = other->QueryInterface(__uuidof(DOMNode), (void**)&domOther);
    if (FAILED(hr))
        return hr;

    *result = m_node->isSameNode(domOther->node()) ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMNode::isEqualNode(_In_opt_ IDOMNode* /*other*/, _Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMNode::textContent(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(m_node->textContent()).release();

    return S_OK;
}

HRESULT DOMNode::setTextContent(_In_ BSTR /*text*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMNode - IDOMEventTarget --------------------------------------------------

HRESULT DOMNode::addEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener* listener, BOOL useCapture)
{
    auto webListener = WebEventListener::create(listener);
    m_node->addEventListener(ucharFrom(type), WTFMove(webListener), useCapture);

    return S_OK;
}

HRESULT DOMNode::removeEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener* listener, BOOL useCapture)
{
    if (!listener || !type)
        return E_POINTER;
    if (!m_node)
        return E_FAIL;
    auto webListener = WebEventListener::create(listener);
    m_node->removeEventListener(ucharFrom(type), webListener, useCapture);
    return S_OK;
}

HRESULT DOMNode::dispatchEvent(_In_opt_ IDOMEvent* evt, _Out_ BOOL* result)
{
    if (!evt || !result)
        return E_POINTER;
    if (!m_node)
        return E_FAIL;

    COMPtr<DOMEvent> domEvent;
    HRESULT hr = evt->QueryInterface(IID_DOMEvent, (void**) &domEvent);
    if (FAILED(hr))
        return hr;

    if (!domEvent->coreEvent())
        return E_FAIL;

    auto dispatchResult = m_node->dispatchEventForBindings(*domEvent->coreEvent());
    if (dispatchResult.hasException())
        return E_FAIL;

    *result = dispatchResult.releaseReturnValue();
    return S_OK;
}

// DOMNode - DOMNode ----------------------------------------------------------

DOMNode::DOMNode(WebCore::Node* n)
{
    if (n)
        n->ref();

    m_node = n;
}

DOMNode::~DOMNode()
{
    if (m_node)
        m_node->deref();
}

IDOMNode* DOMNode::createInstance(WebCore::Node* n)
{
    if (!n)
        return nullptr;

    HRESULT hr = S_OK;
    IDOMNode* domNode = nullptr;
    WebCore::Node::NodeType nodeType = n->nodeType();

    switch (nodeType) {
        case WebCore::Node::ELEMENT_NODE: 
        {
            IDOMElement* newElement = DOMElement::createInstance(static_cast<WebCore::Element*>(n));
            if (newElement) {
                hr = newElement->QueryInterface(IID_IDOMNode, (void**)&domNode);
                newElement->Release();
            }
        }
        break;
        case WebCore::Node::DOCUMENT_NODE:
        {
            IDOMDocument* newDocument = DOMDocument::createInstance(&n->document());
            if (newDocument) {
                hr = newDocument->QueryInterface(IID_IDOMNode, (void**)&domNode);
                newDocument->Release();
            }
        }
        break;
        default:
        {
            DOMNode* newNode = new DOMNode(n);
            hr = newNode->QueryInterface(IID_IDOMNode, (void**)&domNode);
        }
        break;
    }

    if (FAILED(hr))
        return nullptr;

    return domNode;
}

// DOMNodeList - IUnknown -----------------------------------------------------

HRESULT DOMNodeList::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMNodeList))
        *ppvObject = static_cast<IDOMNodeList*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// IDOMNodeList ---------------------------------------------------------------

HRESULT DOMNodeList::item(UINT index, _COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_nodeList)
        return E_FAIL;

    WebCore::Node* itemNode = m_nodeList->item(index);
    if (!itemNode)
        return E_FAIL;

    *result = DOMNode::createInstance(itemNode);
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNodeList::length(_Out_ UINT* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_nodeList)
        return E_FAIL;
    *result = m_nodeList->length();
    return S_OK;
}

// DOMNodeList - DOMNodeList --------------------------------------------------

DOMNodeList::DOMNodeList(WebCore::NodeList* l)
{
    if (l)
        l->ref();

    m_nodeList = l;
}

DOMNodeList::~DOMNodeList()
{
    if (m_nodeList)
        m_nodeList->deref();
}

IDOMNodeList* DOMNodeList::createInstance(WebCore::NodeList* l)
{
    if (!l)
        return nullptr;

    IDOMNodeList* domNodeList = nullptr;
    DOMNodeList* newNodeList = new DOMNodeList(l);
    if (FAILED(newNodeList->QueryInterface(IID_IDOMNodeList, (void**)&domNodeList)))
        return nullptr;

    return domNodeList;
}

// DOMDocument - IUnknown -----------------------------------------------------

HRESULT DOMDocument::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMDocument))
        *ppvObject = static_cast<IDOMDocument*>(this);
    else if (IsEqualGUID(riid, IID_IDOMViewCSS))
        *ppvObject = static_cast<IDOMViewCSS*>(this);
    else if (IsEqualGUID(riid, IID_IDOMDocumentEvent))
        *ppvObject = static_cast<IDOMDocumentEvent*>(this);
    else
        return DOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMDocument ----------------------------------------------------------------

HRESULT DOMDocument::doctype(_COM_Outptr_opt_ IDOMDocumentType** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::implementation(_COM_Outptr_opt_ IDOMImplementation** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::documentElement(_COM_Outptr_opt_ IDOMElement** result)
{
    if (!result)
        return E_POINTER;
    *result = DOMElement::createInstance(m_document->documentElement());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMDocument::createElement(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMElement** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_document)
        return E_FAIL;

    String tagNameString(tagName);
    auto createElementResult = m_document->createElementForBindings(tagNameString);
    if (createElementResult.hasException())
        return E_FAIL;
    *result = DOMElement::createInstance(createElementResult.releaseReturnValue().ptr());
    return S_OK;
}

HRESULT DOMDocument::createDocumentFragment(_COM_Outptr_opt_ IDOMDocumentFragment** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DOMDocument::createTextNode(_In_ BSTR /*data*/, _COM_Outptr_opt_ IDOMText** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createComment(_In_ BSTR /*data*/, _COM_Outptr_opt_ IDOMComment** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createCDATASection(_In_ BSTR /*data*/, _COM_Outptr_opt_ IDOMCDATASection** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createProcessingInstruction(_In_ BSTR /*target*/, _In_ BSTR /*data*/, _COM_Outptr_opt_ IDOMProcessingInstruction** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createAttribute(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createEntityReference(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMEntityReference** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::getElementsByTagName(_In_ BSTR tagName, _COM_Outptr_opt_ IDOMNodeList** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_document)
        return E_FAIL;

    String tagNameString(tagName);
    RefPtr<WebCore::NodeList> elements;
    if (!tagNameString.isNull())
        elements = m_document->getElementsByTagName(tagNameString);
    *result = DOMNodeList::createInstance(elements.get());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMDocument::importNode(_In_opt_ IDOMNode* /*importedNode*/, BOOL /*deep*/, _COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createElementNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*qualifiedName*/, _COM_Outptr_opt_ IDOMElement** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::createAttributeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*qualifiedName*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMDocument::getElementsByTagNameNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNodeList** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_document)
        return E_FAIL;

    String namespaceURIString(namespaceURI);
    String localNameString(localName);
    RefPtr<WebCore::NodeList> elements;
    if (!localNameString.isNull())
        elements = m_document->getElementsByTagNameNS(namespaceURIString, localNameString);
    *result = DOMNodeList::createInstance(elements.get());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMDocument::getElementById(_In_ BSTR elementId, _COM_Outptr_opt_ IDOMElement** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_document)
        return E_FAIL;

    String idString(elementId);
    *result = DOMElement::createInstance(m_document->getElementById(idString));
    return *result ? S_OK : E_FAIL;
}

// DOMDocument - IDOMViewCSS --------------------------------------------------

HRESULT DOMDocument::getComputedStyle(_In_opt_ IDOMElement* elt, _In_ BSTR pseudoElt, _COM_Outptr_opt_ IDOMCSSStyleDeclaration** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!elt)
        return E_POINTER;

    COMPtr<DOMElement> domEle;
    HRESULT hr = elt->QueryInterface(IID_DOMElement, (void**)&domEle);
    if (FAILED(hr))
        return hr;
    Element* element = domEle->element();
    if (!element)
        return E_FAIL;

    auto* dv = m_document->domWindow();
    String pseudoEltString(pseudoElt);
    
    if (!dv)
        return E_FAIL;
    
    *result = DOMCSSStyleDeclaration::createInstance(dv->getComputedStyle(*element, pseudoEltString.impl()).ptr());
    return *result ? S_OK : E_FAIL;
}

// DOMDocument - IDOMDocumentEvent --------------------------------------------

HRESULT DOMDocument::createEvent(_In_ BSTR eventType, _COM_Outptr_opt_ IDOMEvent** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;

    String eventTypeString(eventType, SysStringLen(eventType));
    auto createEventResult = m_document->createEvent(eventTypeString);
    if (createEventResult.hasException())
        return E_FAIL;
    *result = DOMEvent::createInstance(createEventResult.releaseReturnValue());
    return S_OK;
}

// DOMDocument - DOMDocument --------------------------------------------------

DOMDocument::DOMDocument(WebCore::Document* d)
    : DOMNode(d)
    , m_document(d)
{
}

DOMDocument::~DOMDocument()
{
}

IDOMDocument* DOMDocument::createInstance(WebCore::Document* d)
{
    if (!d)
        return nullptr;

    HRESULT hr;
    IDOMDocument* domDocument = nullptr;

    if (d->isHTMLDocument()) {
        DOMHTMLDocument* newDocument = new DOMHTMLDocument(d);
        hr = newDocument->QueryInterface(IID_IDOMDocument, (void**)&domDocument);
    } else {
        DOMDocument* newDocument = new DOMDocument(d);
        hr = newDocument->QueryInterface(IID_IDOMDocument, (void**)&domDocument);
    }

    if (FAILED(hr))
        return nullptr;

    return domDocument;
}

// DOMWindow - IUnknown ------------------------------------------------------

HRESULT DOMWindow::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMWindow))
        *ppvObject = static_cast<IDOMWindow*>(this);
    else if (IsEqualGUID(riid, IID_IDOMEventTarget))
        *ppvObject = static_cast<IDOMEventTarget*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMWindow - IDOMWindow ------------------------------------------------------

HRESULT DOMWindow::document(_COM_Outptr_opt_ IDOMDocument** result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = DOMDocument::createInstance(m_window->document());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMWindow::getComputedStyle(_In_opt_ IDOMElement* element, _In_ BSTR pseudoElement)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWindow::getMatchedCSSRules(_In_opt_ IDOMElement* element, _In_ BSTR pseudoElement, BOOL authorOnly, _COM_Outptr_opt_ IDOMCSSRuleList** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMWindow::devicePixelRatio(_Out_ double* result)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMWindow - IDOMEventTarget ------------------------------------------------------

HRESULT DOMWindow::addEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener* listener, BOOL useCapture)
{
    if (!type || !listener)
        return E_POINTER;
    if (!m_window)
        return E_FAIL;
    auto webListener = WebEventListener::create(listener);
    m_window->addEventListener(ucharFrom(type), WTFMove(webListener), useCapture);
    return S_OK;
}

HRESULT DOMWindow::removeEventListener(_In_ BSTR type, _In_opt_ IDOMEventListener* listener, BOOL useCapture)
{
    if (!type || !listener)
        return E_POINTER;
    if (!m_window)
        return E_FAIL;
    auto webListener = WebEventListener::create(listener);
    m_window->removeEventListener(ucharFrom(type), webListener, useCapture);
    return S_OK;
}

HRESULT DOMWindow::dispatchEvent(_In_opt_ IDOMEvent* evt, _Out_ BOOL* result)
{
    if (!result || !evt)
        return E_POINTER;
    if (!m_window)
        return E_FAIL;

    COMPtr<DOMEvent> domEvent;
    HRESULT hr = evt->QueryInterface(IID_DOMEvent, (void**) &domEvent);
    if (FAILED(hr))
        return hr;

    if (!domEvent->coreEvent())
        return E_FAIL;

    auto dispatchResult = m_window->dispatchEventForBindings(*domEvent->coreEvent());
    if (dispatchResult.hasException())
        return E_FAIL;

    *result = dispatchResult.releaseReturnValue();
    return S_OK;
}


// DOMWindow - DOMWindow --------------------------------------------------

DOMWindow::DOMWindow(WebCore::DOMWindow* w)
    : m_window(w)
{
}

DOMWindow::~DOMWindow()
{
}

IDOMWindow* DOMWindow::createInstance(WebCore::DOMWindow* w)
{
    if (!w)
        return nullptr;

    DOMWindow* newWindow = new DOMWindow(w);

    IDOMWindow* domWindow = nullptr;
    HRESULT hr = newWindow->QueryInterface(IID_IDOMWindow, reinterpret_cast<void**>(&domWindow));

    if (FAILED(hr))
        return nullptr;

    return domWindow;
}

// DOMElement - IUnknown ------------------------------------------------------

HRESULT DOMElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMElement))
        *ppvObject = static_cast<IDOMElement*>(this);
    else if (IsEqualGUID(riid, IID_DOMElement))
        *ppvObject = static_cast<DOMElement*>(this);
    else if (IsEqualGUID(riid, IID_IDOMElementPrivate))
        *ppvObject = static_cast<IDOMElementPrivate*>(this);
    else if (IsEqualGUID(riid, IID_IDOMNodeExtensions))
        *ppvObject = static_cast<IDOMNodeExtensions*>(this);
    else if (IsEqualGUID(riid, IID_IDOMElementCSSInlineStyle))
        *ppvObject = static_cast<IDOMElementCSSInlineStyle*>(this);
    else if (IsEqualGUID(riid, IID_IDOMElementExtensions))
        *ppvObject = static_cast<IDOMElementExtensions*>(this);
    else
        return DOMNode::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMElement - IDOMNodeExtensions---------------------------------------------

HRESULT DOMElement::boundingBox(_Out_ LPRECT rect)
{
    if (!rect)
        return E_POINTER;

    ::SetRectEmpty(rect);

    if (!m_element)
        return E_FAIL;

    WebCore::RenderElement *renderer = m_element->renderer();
    if (renderer) {
        IntRect boundsIntRect = renderer->absoluteBoundingBoxRect();
        rect->left = boundsIntRect.x();
        rect->top = boundsIntRect.y();
        rect->right = boundsIntRect.x() + boundsIntRect.width();
        rect->bottom = boundsIntRect.y() + boundsIntRect.height();
    }

    return S_OK;
}

HRESULT DOMElement::lineBoxRects(__inout_ecount_full(cRects) RECT* /*rects*/, int cRects)
{
    return E_NOTIMPL;
}

// IDOMElement ----------------------------------------------------------------

HRESULT DOMElement::tagName(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_element)
        return E_FAIL;

    *result = BString(m_element->tagName()).release();
    return S_OK;
}
    
HRESULT DOMElement::getAttribute(_In_ BSTR name, __deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!m_element)
        return E_FAIL;
    WTF::String nameString(name, SysStringLen(name));
    WTF::String& attrValueString = (WTF::String&) m_element->getAttribute(nameString);
    *result = BString(attrValueString).release();
    if (attrValueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT DOMElement::setAttribute(_In_ BSTR name, _In_ BSTR value)
{
    if (!m_element)
        return E_FAIL;

    WTF::String nameString(name, SysStringLen(name));
    WTF::String valueString(value, SysStringLen(value));
    auto result = m_element->setAttribute(nameString, valueString);
    return result.hasException() ? E_FAIL : S_OK;
}
    
HRESULT DOMElement::removeAttribute(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMElement::getAttributeNode(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::setAttributeNode(_In_opt_ IDOMAttr* /*newAttr*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::removeAttributeNode(_In_opt_ IDOMAttr* /*oldAttr*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::getElementsByTagName(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMNodeList** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::getAttributeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*localName*/, __deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::setAttributeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*qualifiedName*/, _In_ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMElement::removeAttributeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*localName*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMElement::getAttributeNodeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*localName*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::setAttributeNodeNS(_In_opt_ IDOMAttr* /*newAttr*/, _COM_Outptr_opt_ IDOMAttr** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::getElementsByTagNameNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*localName*/, _COM_Outptr_opt_ IDOMNodeList** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMElement::hasAttribute(_In_ BSTR /*name*/, _Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMElement::hasAttributeNS(_In_ BSTR /*namespaceURI*/, _In_ BSTR /*localName*/, _Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMElement::focus()
{
    if (!m_element)
        return E_FAIL;
    m_element->focus();
    return S_OK;
}

HRESULT DOMElement::blur()
{
    if (!m_element)
        return E_FAIL;
    m_element->blur();
    return S_OK;
}

// IDOMElementPrivate ---------------------------------------------------------

HRESULT DOMElement::coreElement(__deref_opt_out void **element)
{
    if (!m_element)
        return E_FAIL;
    *element = (void*) m_element;
    return S_OK;
}

HRESULT DOMElement::isEqual(_In_opt_ IDOMElement* other, _Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;

    if (!other)
        return E_POINTER;

    IDOMElementPrivate* otherPriv;
    HRESULT hr = other->QueryInterface(IID_IDOMElementPrivate, (void**) &otherPriv);
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

HRESULT DOMElement::isFocused(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_element)
        return E_FAIL;

    if (m_element->document().focusedElement() == m_element)
        *result = TRUE;
    else
        *result = FALSE;

    return S_OK;
}

HRESULT DOMElement::innerText(__deref_opt_out BSTR* result)
{
    if (!result) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    *result = nullptr;

    if (!m_element) {
        ASSERT_NOT_REACHED();
        return E_FAIL;
    }

    *result = BString(m_element->innerText()).release();
    return S_OK;
}

HRESULT DOMElement::font(_Out_ WebFontDescription* webFontDescription)
{
    if (!webFontDescription) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }

    ASSERT(m_element);

    WebCore::RenderElement* renderer = m_element->renderer();
    if (!renderer)
        return E_FAIL;

    auto fontDescription = renderer->style().fontCascade().fontDescription();
    AtomicString family = fontDescription.firstFamily();

    // FIXME: This leaks. Delete this whole function to get rid of the leak.
    UChar* familyCharactersBuffer = new UChar[family.length()];
    StringView(family.string()).getCharactersWithUpconvert(familyCharactersBuffer);

    webFontDescription->family = wcharFrom(familyCharactersBuffer);
    webFontDescription->familyLength = family.length();
    webFontDescription->size = fontDescription.computedSize();
    webFontDescription->bold = isFontWeightBold(fontDescription.weight());
    webFontDescription->italic = isItalic(fontDescription.italic());

    return S_OK;
}

HRESULT DOMElement::renderedImage(__deref_opt_out HBITMAP* image)
{
    if (!image) {
        ASSERT_NOT_REACHED();
        return E_POINTER;
    }
    *image = nullptr;

    ASSERT(m_element);

    Frame* frame = m_element->document().frame();
    if (!frame)
        return E_FAIL;

    *image = createDragImageForNode(*frame, *m_element);
    if (!*image)
        return E_FAIL;

    return S_OK;
}

HRESULT DOMElement::markerTextForListItem(__deref_opt_out BSTR* markerText)
{
    if (!markerText)
        return E_POINTER;

    ASSERT(m_element);

    *markerText = BString(WebCore::markerTextForListItem(m_element)).release();
    return S_OK;
}

HRESULT DOMElement::shadowPseudoId(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(m_element);

    *result = BString(m_element->shadowPseudoId().string()).release();
    return S_OK;
}

// IDOMElementCSSInlineStyle --------------------------------------------------

HRESULT DOMElement::style(_COM_Outptr_opt_ IDOMCSSStyleDeclaration** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    if (!is<WebCore::StyledElement>(m_element))
        return E_FAIL;

    *result = DOMCSSStyleDeclaration::createInstance(&downcast<WebCore::StyledElement>(*m_element).cssomStyle());
    return *result ? S_OK : E_FAIL;
}

// IDOMElementExtensions ------------------------------------------------------

HRESULT DOMElement::offsetLeft(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetLeft();
    return S_OK;
}

HRESULT DOMElement::offsetTop(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetTop();
    return S_OK;
}

HRESULT DOMElement::offsetWidth(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetWidth();
    return S_OK;
}

HRESULT DOMElement::offsetHeight(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->offsetHeight();
    return S_OK;
}

HRESULT DOMElement::offsetParent(_COM_Outptr_opt_ IDOMElement** result)
{
    // FIXME
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMElement::clientWidth(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->clientWidth();
    return S_OK;
}

HRESULT DOMElement::clientHeight(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->clientHeight();
    return S_OK;
}

HRESULT DOMElement::scrollLeft(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollLeft();
    return S_OK;
}

HRESULT DOMElement::setScrollLeft(int /*newScrollLeft*/)
{
    // FIXME
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMElement::scrollTop(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollTop();
    return S_OK;
}

HRESULT DOMElement::setScrollTop(int /*newScrollTop*/)
{
    // FIXME
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMElement::scrollWidth(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollWidth();
    return S_OK;
}

HRESULT DOMElement::scrollHeight(_Out_ int* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_element)
        return E_FAIL;

    *result = m_element->scrollHeight();
    return S_OK;
}

HRESULT DOMElement::scrollIntoView(BOOL alignWithTop)
{
    if (!m_element)
        return E_FAIL;

    m_element->scrollIntoView(!!alignWithTop);
    return S_OK;
}

HRESULT DOMElement::scrollIntoViewIfNeeded(BOOL centerIfNeeded)
{
    if (!m_element)
        return E_FAIL;

    m_element->scrollIntoViewIfNeeded(!!centerIfNeeded);
    return S_OK;
}

// DOMElement -----------------------------------------------------------------

DOMElement::DOMElement(WebCore::Element* e)
    : DOMNode(e)
    , m_element(e)
{
}

DOMElement::~DOMElement()
{
}

IDOMElement* DOMElement::createInstance(WebCore::Element* e)
{
    if (!e)
        return nullptr;

    HRESULT hr;
    IDOMElement* domElement = nullptr;

    if (is<WebCore::HTMLFormElement>(*e)) {
        DOMHTMLFormElement* newElement = new DOMHTMLFormElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(iframeTag)) {
        DOMHTMLIFrameElement* newElement = new DOMHTMLIFrameElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (is<WebCore::HTMLInputElement>(*e)) {
        DOMHTMLInputElement* newElement = new DOMHTMLInputElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (is<WebCore::HTMLOptionElement>(*e)) {
        DOMHTMLOptionElement* newElement = new DOMHTMLOptionElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->hasTagName(selectTag)) {
        DOMHTMLSelectElement* newElement = new DOMHTMLSelectElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (is<WebCore::HTMLTextAreaElement>(*e)) {
        DOMHTMLTextAreaElement* newElement = new DOMHTMLTextAreaElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else if (e->isHTMLElement()) {
        DOMHTMLElement* newElement = new DOMHTMLElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    } else {
        DOMElement* newElement = new DOMElement(e);
        hr = newElement->QueryInterface(IID_IDOMElement, (void**)&domElement);
    }

    if (FAILED(hr))
        return nullptr;

    return domElement;
}

// DOMRange - IUnknown -----------------------------------------------------

HRESULT DOMRange::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMRange))
        *ppvObject = static_cast<IDOMRange*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMRange ----------------------------------------------------------------- 

DOMRange::DOMRange(WebCore::Range* e)
    : m_range(e)
{
}

DOMRange::~DOMRange()
{
}

IDOMRange* DOMRange::createInstance(WebCore::Range* range)
{
    if (!range)
        return nullptr;

    DOMRange* newRange = new DOMRange(range);

    IDOMRange* domRange = nullptr;
    if (FAILED(newRange->QueryInterface(IID_IDOMRange, reinterpret_cast<void**>(&domRange))))
        return nullptr;

    return newRange;
}

HRESULT DOMRange::startContainer(_COM_Outptr_opt_ IDOMNode** node)
{
    if (!node)
        return E_POINTER;
    *node = nullptr;
    if (!m_range)
        return E_UNEXPECTED;

    *node = DOMNode::createInstance(&m_range->startContainer());

    return S_OK;
}

HRESULT DOMRange::startOffset(_Out_ int* offset)
{
    if (!offset)
        return E_POINTER;
    *offset = 0;
    if (!m_range)
        return E_UNEXPECTED;

    *offset = m_range->startOffset();

    return S_OK;
}

HRESULT DOMRange::endContainer(_COM_Outptr_opt_ IDOMNode** node)
{
    if (!node)
        return E_POINTER;
    *node = nullptr;
    if (!m_range)
        return E_UNEXPECTED;

    *node = DOMNode::createInstance(&m_range->endContainer());

    return S_OK;
}

HRESULT DOMRange::endOffset(_Out_ int* offset)
{
    if (!offset)
        return E_POINTER;
    *offset = 0;
    if (!m_range)
        return E_UNEXPECTED;

    *offset = m_range->endOffset();

    return S_OK;
}

HRESULT DOMRange::collapsed(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_range)
        return E_UNEXPECTED;

    *result = m_range->collapsed();

    return S_OK;
}

HRESULT DOMRange::commonAncestorContainer(_COM_Outptr_opt_ IDOMNode** container)
{
    ASSERT_NOT_REACHED();
    if (!container)
        return E_POINTER;
    *container = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMRange::setStart(_In_opt_ IDOMNode* refNode, int offset)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::setEnd(_In_opt_ IDOMNode* refNode, int offset)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::setStartBefore(_In_opt_ IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::setStartAfter(_In_opt_ IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::setEndBefore(_In_opt_  IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::setEndAfter(_In_opt_  IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::collapse(BOOL toStart)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::selectNode(_In_opt_ IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::selectNodeContents(_In_opt_ IDOMNode* refNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::compareBoundaryPoints(unsigned short how, _In_opt_ IDOMRange* sourceRange)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::deleteContents()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::extractContents(_COM_Outptr_opt_ IDOMDocumentFragment** fragment)
{
    ASSERT_NOT_REACHED();
    if (!fragment)
        return E_POINTER;
    *fragment = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMRange::cloneContents(_COM_Outptr_opt_ IDOMDocumentFragment** fragment)
{
    ASSERT_NOT_REACHED();
    if (!fragment)
        return E_POINTER;
    *fragment = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMRange::insertNode(_In_opt_ IDOMNode* newNode)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::surroundContents(_In_opt_ IDOMNode* newParent)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMRange::cloneRange(_COM_Outptr_opt_ IDOMRange** range)
{
    ASSERT_NOT_REACHED();
    if (!range)
        return E_POINTER;
    *range = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMRange::toString(__deref_opt_out BSTR* str)
{
    if (!str)
        return E_POINTER;
    *str = nullptr;
    if (!m_range)
        return E_UNEXPECTED;

    *str = BString(m_range->toString()).release();

    return S_OK;
}

HRESULT DOMRange::detach()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

DOMNamedNodeMap::DOMNamedNodeMap(WebCore::NamedNodeMap* nodeMap)
    : m_nodeMap(nodeMap)
{
}

DOMNamedNodeMap::~DOMNamedNodeMap()
{
}

IDOMNamedNodeMap* DOMNamedNodeMap::createInstance(WebCore::NamedNodeMap* nodeMap)
{
    if (!nodeMap)
        return nullptr;

    DOMNamedNodeMap* namedNodeMap = new DOMNamedNodeMap(nodeMap);

    IDOMNamedNodeMap* domNamedNodeMap = nullptr;
    if (FAILED(namedNodeMap->QueryInterface(IID_IDOMNamedNodeMap, reinterpret_cast<void**>(&domNamedNodeMap))))
        return nullptr;

    return namedNodeMap;
}

HRESULT DOMNamedNodeMap::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMNamedNodeMap))
        *ppvObject = static_cast<IDOMNamedNodeMap*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMNamedNodeMap::getNamedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}

HRESULT DOMNamedNodeMap::setNamedItem(_In_opt_ IDOMNode* arg, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}

HRESULT DOMNamedNodeMap::removeNamedItem(_In_ BSTR name, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}

HRESULT DOMNamedNodeMap::item(_In_ UINT index, _COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;
    
    if (!m_nodeMap)
        return E_FAIL;

    *result = DOMNode::createInstance(m_nodeMap->item(index).get());
    return *result ? S_OK : E_FAIL;
}

HRESULT DOMNamedNodeMap::length(_Out_opt_ UINT* result)
{
    if (!result)
        return E_POINTER;

    *result = m_nodeMap->length();
    return S_OK;
}

HRESULT DOMNamedNodeMap::getNamedItemNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}

HRESULT DOMNamedNodeMap::setNamedItemNS(_In_opt_ IDOMNode* arg, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}

HRESULT DOMNamedNodeMap::removeNamedItemNS(_In_ BSTR namespaceURI, _In_ BSTR localName, _COM_Outptr_opt_ IDOMNode** result)
{
    return E_NOTIMPL;
}
