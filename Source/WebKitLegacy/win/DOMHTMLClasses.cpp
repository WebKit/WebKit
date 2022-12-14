/*
 * Copyright (C) 2006-2007, 2009, 2015 Apple Inc. All rights reserved.
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
#include "DOMHTMLClasses.h"
#include "WebFrame.h"

#include <WebCore/BString.h>
#include <WebCore/COMPtr.h>
#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLCollection.h>
#include <WebCore/HTMLDocument.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLIFrameElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLOptionsCollection.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderTextControl.h>

using namespace WebCore;
using namespace HTMLNames;

// DOMHTMLCollection

DOMHTMLCollection::DOMHTMLCollection(WebCore::HTMLCollection* c)
: m_collection(c)
{
}

IDOMHTMLCollection* DOMHTMLCollection::createInstance(WebCore::HTMLCollection* c)
{
    if (!c)
        return 0;

    IDOMHTMLCollection* htmlCollection = 0;
    DOMHTMLCollection* newCollection = new DOMHTMLCollection(c);
    if (FAILED(newCollection->QueryInterface(IID_IDOMHTMLCollection, (void**)&htmlCollection))) {
        delete newCollection;
        return 0;
    }

    return htmlCollection;
}

// DOMHTMLCollection - IUnknown -----------------------------------------------

HRESULT DOMHTMLCollection::QueryInterface(_In_ REFIID riid, _COM_Outptr_  void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLCollection))
        *ppvObject = static_cast<IDOMHTMLCollection*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLCollection ----------------------------------------------------------

HRESULT DOMHTMLCollection::length(_Out_ UINT* result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    if (!m_collection)
        return E_POINTER;

    *result = m_collection->length();
    return S_OK;
}

HRESULT DOMHTMLCollection::item(UINT index, _COM_Outptr_opt_ IDOMNode** node)
{
    if (!node)
        return E_POINTER;
    *node = nullptr;
    if (!m_collection)
        return E_POINTER;

    *node = DOMNode::createInstance(m_collection->item(index));
    return *node ? S_OK : E_FAIL;
}

HRESULT DOMHTMLCollection::namedItem(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMNode** node)
{
    ASSERT_NOT_REACHED();
    if (!node)
        return E_POINTER;
    *node = nullptr;
    return E_NOTIMPL;
}

// DOMHTMLOptionsCollection - IUnknown ----------------------------------------

HRESULT DOMHTMLOptionsCollection::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLOptionsCollection))
        *ppvObject = static_cast<IDOMHTMLOptionsCollection*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLOptionsCollection ---------------------------------------------------

DOMHTMLOptionsCollection::DOMHTMLOptionsCollection(WebCore::HTMLOptionsCollection* collection)
    : m_collection(collection)
{
}

IDOMHTMLOptionsCollection* DOMHTMLOptionsCollection::createInstance(WebCore::HTMLOptionsCollection* collection)
{
    if (!collection)
        return nullptr;

    IDOMHTMLOptionsCollection* optionsCollection = nullptr;
    DOMHTMLOptionsCollection* newCollection = new DOMHTMLOptionsCollection(collection);
    if (FAILED(newCollection->QueryInterface(IID_IDOMHTMLOptionsCollection, (void**)&optionsCollection))) {
        delete newCollection;
        return nullptr;
    }

    return optionsCollection;
}

HRESULT DOMHTMLOptionsCollection::length(_Out_ unsigned* result)
{
    if (!result)
        return E_POINTER;

    *result = m_collection->length();
    return S_OK;
}

HRESULT DOMHTMLOptionsCollection::setLength(unsigned /*length*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMHTMLOptionsCollection::item(unsigned index, _COM_Outptr_opt_ IDOMNode** result)
{
    if (!result)
        return E_POINTER;

    *result = DOMNode::createInstance(m_collection->item(index));

    return *result ? S_OK : E_FAIL;
}

HRESULT DOMHTMLOptionsCollection::namedItem(_In_ BSTR /*name*/, _COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

// DOMHTMLDocument - IUnknown -------------------------------------------------

HRESULT DOMHTMLDocument::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLDocument))
        *ppvObject = static_cast<IDOMHTMLDocument*>(this);
    else
        return DOMDocument::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLDocument ------------------------------------------------------------

HRESULT DOMHTMLDocument::title(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    if (!m_document || !m_document->isHTMLDocument())
        return E_FAIL;

    *result = BString(m_document->title()).release();
    return S_OK;
}
    
HRESULT DOMHTMLDocument::setTitle(_In_ BSTR /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::referrer(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;

    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::domain(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;

    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::URL(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = BString(downcast<HTMLDocument>(*m_document).url()).release();
    return S_OK;
}
    
HRESULT DOMHTMLDocument::body(_COM_Outptr_opt_ IDOMHTMLElement** bodyElement)
{
    if (!bodyElement)
        return E_POINTER;

    *bodyElement = nullptr;
    if (!is<HTMLDocument>(m_document))
        return E_FAIL;

    HTMLDocument& htmlDoc = downcast<HTMLDocument>(*m_document);
    COMPtr<IDOMElement> domElement;
    domElement.adoptRef(DOMHTMLElement::createInstance(htmlDoc.bodyOrFrameset()));
    if (domElement)
        return domElement->QueryInterface(IID_IDOMHTMLElement, (void**) bodyElement);
    return E_FAIL;
}
    
HRESULT DOMHTMLDocument::setBody(_In_opt_ IDOMHTMLElement* /*body*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::images(_COM_Outptr_opt_ IDOMHTMLCollection** collection)
{
    ASSERT_NOT_REACHED();
    if (!collection)
        return E_POINTER;
    *collection = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::applets(_COM_Outptr_opt_ IDOMHTMLCollection** collection)
{
    ASSERT_NOT_REACHED();
    if (!collection)
        return E_POINTER;
    *collection = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::links(_COM_Outptr_opt_ IDOMHTMLCollection** collection)
{
    ASSERT_NOT_REACHED();
    if (!collection)
        return E_POINTER;
    *collection = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::forms(_COM_Outptr_opt_ IDOMHTMLCollection** collection)
{
    if (!collection)
        return E_POINTER;
    *collection = nullptr;
    if (!is<HTMLDocument>(m_document))
        return E_FAIL;

    HTMLDocument& htmlDoc = downcast<HTMLDocument>(*m_document);
    RefPtr<HTMLCollection> forms = htmlDoc.forms();
    *collection = DOMHTMLCollection::createInstance(forms.get());
    return S_OK;
}
    
HRESULT DOMHTMLDocument::anchors(_COM_Outptr_opt_ IDOMHTMLCollection** collection)
{
    ASSERT_NOT_REACHED();
    if (!collection)
        return E_POINTER;
    *collection = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::cookie(__deref_opt_out BSTR* cookie)
{
    ASSERT_NOT_REACHED();
    if (!cookie)
        return E_POINTER;
    *cookie = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::setCookie(_In_ BSTR /*cookie*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::open()
{
    if (!m_document)
        return E_FAIL;

    m_document->open();
    return S_OK;
}
    
HRESULT DOMHTMLDocument::close()
{
    if (!m_document)
        return E_FAIL;
    Ref document { *m_document };
    document->close();
    return S_OK;
}
    
HRESULT DOMHTMLDocument::write(_In_ BSTR text)
{
    if (!m_document)
        return E_FAIL;

    m_document->write(nullptr, { String { text } });
    return S_OK;
}
    
HRESULT DOMHTMLDocument::writeln(_In_ BSTR text)
{
    if (!m_document)
        return E_FAIL;

    m_document->writeln(nullptr, { String { text } });
    return S_OK;
}
    
HRESULT DOMHTMLDocument::getElementById_(_In_ BSTR /*elementId*/, _COM_Outptr_opt_ IDOMElement** element)
{
    ASSERT_NOT_REACHED();
    if (!element)
        return E_POINTER;
    *element = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLDocument::getElementsByName(_In_ BSTR /*elementName*/, _COM_Outptr_opt_ IDOMNodeList** nodeList)
{
    ASSERT_NOT_REACHED();
    if (!nodeList)
        return E_POINTER;
    *nodeList = nullptr;
    return E_NOTIMPL;
}

// DOMHTMLElement - IUnknown --------------------------------------------------

HRESULT DOMHTMLElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLElement))
        *ppvObject = static_cast<IDOMHTMLElement*>(this);
    else
        return DOMElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLElement -------------------------------------------------------------

HRESULT DOMHTMLElement::idName(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLElement>(m_element));
    String idString = downcast<HTMLElement>(m_element)->attributeWithoutSynchronization(idAttr);
    *result = BString(idString).release();
    return S_OK;
}
    
HRESULT DOMHTMLElement::setIdName(_In_ BSTR /*idName*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::title(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::setTitle(_In_ BSTR /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::lang(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::setLang(_In_ BSTR /*lang*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::dir(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::setDir(_In_ BSTR /*dir*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::className(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLElement::setClassName(_In_ BSTR /*className*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMHTMLElement::innerHTML(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    String innerHtmlString = downcast<HTMLElement>(m_element)->innerHTML();
    *result = BString(innerHtmlString).release();
    return S_OK;
}
        
HRESULT DOMHTMLElement::setInnerHTML(_In_ BSTR html)
{
    ASSERT(is<HTMLElement>(m_element));
    HTMLElement* htmlElement = downcast<HTMLElement>(m_element);
    String htmlString(html, SysStringLen(html));
    htmlElement->setInnerHTML(htmlString);
    return S_OK;
}
        
HRESULT DOMHTMLElement::innerText(__deref_opt_out BSTR* result)
{
    ASSERT(is<HTMLElement>(m_element));
    if (!result)
        return E_POINTER;
    WTF::String innerTextString = downcast<HTMLElement>(m_element)->innerText();
    *result = BString(innerTextString).release();
    return S_OK;
}
        
HRESULT DOMHTMLElement::setInnerText(_In_ BSTR text)
{
    ASSERT(is<HTMLElement>(m_element));
    HTMLElement* htmlElement = downcast<HTMLElement>(m_element);
    htmlElement->setInnerText(WTF::String(text, SysStringLen(text)));
    return S_OK;
}

// DOMHTMLFormElement - IUnknown ----------------------------------------------

HRESULT DOMHTMLFormElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLFormElement))
        *ppvObject = static_cast<IDOMHTMLFormElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLFormElement ---------------------------------------------------------

HRESULT DOMHTMLFormElement::elements(_COM_Outptr_opt_ IDOMHTMLCollection** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::length(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::name(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::setName(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::acceptCharset(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::setAcceptCharset(_In_ BSTR /*acceptCharset*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::action(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    ASSERT(is<HTMLFormElement>(m_element));
    WTF::String actionString = downcast<HTMLFormElement>(*m_element).action();
    *result = BString(actionString).release();
    return S_OK;
}
    
HRESULT DOMHTMLFormElement::setAction(_In_ BSTR /*action*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::encType(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::setEnctype(_In_opt_ BSTR* /*encType*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::method(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;
    ASSERT(is<HTMLFormElement>(m_element));
    WTF::String methodString = downcast<HTMLFormElement>(*m_element).method();
    *result = BString(methodString).release();
    return S_OK;
}
    
HRESULT DOMHTMLFormElement::setMethod(_In_ BSTR /*method*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::target(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::setTarget(_In_ BSTR /*target*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::submit()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLFormElement::reset()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMHTMLSelectElement - IUnknown ----------------------------------------------

HRESULT DOMHTMLSelectElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLSelectElement))
        *ppvObject = static_cast<IDOMHTMLSelectElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransitionSelect))
        *ppvObject = static_cast<IFormsAutoFillTransitionSelect*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLSelectElement -------------------------------------------------------

HRESULT DOMHTMLSelectElement::type(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::selectedIndex(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setSelectedIndx(int /*selectedIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::value(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setValue(_In_ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::length(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::form(_COM_Outptr_opt_ IDOMHTMLFormElement** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::options(_COM_Outptr_opt_ IDOMHTMLOptionsCollection** result)
{
    if (!result)
        return E_POINTER;

    ASSERT(m_element);
    HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*m_element);

    *result = nullptr;
    RefPtr<HTMLOptionsCollection> options = selectElement.options();
    *result = DOMHTMLOptionsCollection::createInstance(options.get());
    return S_OK;
}
    
HRESULT DOMHTMLSelectElement::disabled(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setDisabled(BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::multiple(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setMultiple(BOOL /*multiple*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::name(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setName(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::size(_Out_ int* /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setSize(int /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::tabIndex(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::setTabIndex(int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::add(_In_opt_ IDOMHTMLElement* /*element*/, _In_opt_ IDOMHTMLElement* /*before*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLSelectElement::remove(int /*index*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
// DOMHTMLSelectElement - IFormsAutoFillTransitionSelect ----------------------

HRESULT DOMHTMLSelectElement::activateItemAtIndex(int index)
{
    ASSERT(m_element);
    HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*m_element);

    if (index >= selectElement.length())
        return E_FAIL;

    selectElement.setSelectedIndex(index);
    return S_OK;
}

// DOMHTMLOptionElement - IUnknown --------------------------------------------

HRESULT DOMHTMLOptionElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLOptionElement))
        *ppvObject = static_cast<IDOMHTMLOptionElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLOptionElement -------------------------------------------------------

HRESULT DOMHTMLOptionElement::form(_COM_Outptr_opt_ IDOMHTMLFormElement** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::defaultSelected(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::setDefaultSelected(BOOL /*defaultSelected*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::text(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    ASSERT(is<HTMLOptionElement>(m_element));
    HTMLOptionElement& optionElement = downcast<HTMLOptionElement>(*m_element);

    *result = BString(optionElement.text()).release();
    return S_OK;
}
    
HRESULT DOMHTMLOptionElement::index(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::disabled(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::setDisabled(BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::label(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    *result = nullptr;

    ASSERT(is<HTMLOptionElement>(m_element));
    HTMLOptionElement& optionElement = downcast<HTMLOptionElement>(*m_element);

    *result = BString(optionElement.label()).release();
    return S_OK;
}
    
HRESULT DOMHTMLOptionElement::setLabel(_In_ BSTR /*label*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::selected(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::setSelected(BOOL /*selected*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::value(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLOptionElement::setValue(_In_ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMHTMLInputElement - IUnknown ----------------------------------------------

HRESULT DOMHTMLInputElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLInputElement))
        *ppvObject = static_cast<IDOMHTMLInputElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransition))
        *ppvObject = static_cast<IFormsAutoFillTransition*>(this);
    else if (IsEqualGUID(riid, IID_IFormPromptAdditions))
        *ppvObject = static_cast<IFormPromptAdditions*>(this);    
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLInputElement --------------------------------------------------------

HRESULT DOMHTMLInputElement::defaultValue(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setDefaultValue(_In_ BSTR /*val*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::defaultChecked(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setDefaultChecked(_In_ BSTR /*checked*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::form(_COM_Outptr_opt_ IDOMHTMLElement** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    COMPtr<IDOMElement> domElement;
    domElement.adoptRef(DOMHTMLElement::createInstance(inputElement.form()));
    if (domElement)
        return domElement->QueryInterface(IID_IDOMHTMLElement, (void**) result);
    return E_FAIL;
}
    
HRESULT DOMHTMLInputElement::accept(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setAccept(_In_ BSTR /*accept*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::accessKey(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setAccessKey(_In_ BSTR /*key*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::align(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setAlign(_In_ BSTR /*align*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::alt(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setAlt(_In_ BSTR /*alt*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::checked(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setChecked(BOOL /*checked*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::disabled(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *result = inputElement.isDisabledFormControl() ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT DOMHTMLInputElement::setDisabled(BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::maxLength(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setMaxLength(int /*maxLength*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::name(__deref_opt_out BSTR* /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setName(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::readOnly(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *result = inputElement.isReadOnly() ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT DOMHTMLInputElement::setReadOnly(BOOL /*readOnly*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::size(_Out_ unsigned* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setSize(unsigned /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::src(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setSrc(_In_ BSTR /*src*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::tabIndex(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setTabIndex(int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::type(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setType(_In_ BSTR type)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    WTF::AtomString typeString(type, SysStringLen(type));
    inputElement.setType(typeString);
    return S_OK;
}
    
HRESULT DOMHTMLInputElement::useMap(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::setUseMap(_In_ BSTR /*useMap*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLInputElement::value(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    WTF::String valueString = inputElement.value();
    *result = BString(valueString).release();
    if (valueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT DOMHTMLInputElement::setValue(_In_ BSTR value)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.setValue(String((UChar*) value, SysStringLen(value)));
    return S_OK;
}

HRESULT DOMHTMLInputElement::setValueForUser(_In_ BSTR value)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.setValueForUser(String(value, SysStringLen(value)));
    return S_OK;
}

HRESULT DOMHTMLInputElement::select()
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.select();
    return S_OK;
}
    
HRESULT DOMHTMLInputElement::click()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMHTMLInputElement::setSelectionStart(long start)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.setSelectionStart(start);
    return S_OK;
}

HRESULT DOMHTMLInputElement::selectionStart(_Out_ long *start)
{
    if (!start)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *start = inputElement.selectionStart();
    return S_OK;
}

HRESULT DOMHTMLInputElement::setSelectionEnd(long end)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.setSelectionEnd(end);
    return S_OK;
}

HRESULT DOMHTMLInputElement::selectionEnd(_Out_ long *end)
{
    if (!end)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *end = inputElement.selectionEnd();
    return S_OK;
}

// DOMHTMLInputElement -- IFormsAutoFillTransition ----------------------------

HRESULT DOMHTMLInputElement::isTextField(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *result = inputElement.isTextField() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMHTMLInputElement::rectOnScreen(_Out_ LPRECT rect)
{
    if (!rect)
        return E_POINTER;
    ASSERT(is<HTMLInputElement>(m_element));
    rect->left = rect->top = rect->right = rect->bottom = 0;
    RenderObject* renderer = m_element->renderer();
    FrameView* view = m_element->document().view();
    if (!renderer || !view)
        return E_FAIL;

    IntRect coreRect = view->contentsToScreen(renderer->absoluteBoundingBoxRect());
    rect->left = coreRect.x();
    rect->top = coreRect.y();
    rect->right = coreRect.maxX();
    rect->bottom = coreRect.maxY();

    return S_OK;
}

HRESULT DOMHTMLInputElement::replaceCharactersInRange(int startTarget, int endTarget, _In_ BSTR replacementString, int index)
{
    if (!replacementString)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);

    String newValue = inputElement.value();
    String webCoreReplacementString(replacementString, SysStringLen(replacementString));
    newValue = makeStringByReplacing(newValue, startTarget, endTarget - startTarget, webCoreReplacementString);
    inputElement.setValue(newValue);
    inputElement.setSelectionRange(index, newValue.length());

    return S_OK;
}

HRESULT DOMHTMLInputElement::selectedRange(_Out_ int* start, _Out_ int* end)
{
    if (!start || !end)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *start = inputElement.selectionStart();
    *end = inputElement.selectionEnd();
    return S_OK;
}

HRESULT DOMHTMLInputElement::setAutofilled(BOOL filled)
{
    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    inputElement.setAutoFilled(!!filled);
    return S_OK;
}

HRESULT DOMHTMLInputElement::isAutofilled(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLInputElement>(m_element));
    HTMLInputElement& inputElement = downcast<HTMLInputElement>(*m_element);
    *result = inputElement.isAutoFilled() ? TRUE : FALSE;
    return S_OK;
}

// DOMHTMLInputElement -- IFormPromptAdditions ------------------------------------

HRESULT DOMHTMLInputElement::isUserEdited(_Out_ BOOL *result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;
    ASSERT(is<HTMLInputElement>(m_element));
    BOOL textField = FALSE;
    if (FAILED(isTextField(&textField)) || !textField)
        return S_OK;
    if (downcast<HTMLInputElement>(*m_element).lastChangeWasUserEdit())
        *result = TRUE;
    return S_OK;
}

// DOMHTMLTextAreaElement - IUnknown ----------------------------------------------

HRESULT DOMHTMLTextAreaElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLTextAreaElement))
        *ppvObject = static_cast<IDOMHTMLTextAreaElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormPromptAdditions))
        *ppvObject = static_cast<IFormPromptAdditions*>(this);    
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLTextAreaElement -----------------------------------------------------

HRESULT DOMHTMLTextAreaElement::defaultValue(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setDefaultValue(_In_ BSTR /*val*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::form(_COM_Outptr_opt_ IDOMHTMLElement** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    ASSERT(is<HTMLTextAreaElement>(m_element));
    HTMLTextAreaElement& textareaElement = downcast<HTMLTextAreaElement>(*m_element);
    COMPtr<IDOMElement> domElement;
    domElement.adoptRef(DOMHTMLElement::createInstance(textareaElement.form()));
    if (domElement)
        return domElement->QueryInterface(IID_IDOMHTMLElement, (void**) result);
    return E_FAIL;
}
    
HRESULT DOMHTMLTextAreaElement::accessKey(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setAccessKey(_In_ BSTR /*key*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::cols(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setCols(int /*cols*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::disabled(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setDisabled(BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::name(__deref_opt_out BSTR* /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setName(_In_ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::readOnly(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setReadOnly(BOOL /*readOnly*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::rows(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setRows(int /*rows*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::tabIndex(_Out_ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::setTabIndex(int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::type(__deref_opt_out BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT DOMHTMLTextAreaElement::value(__deref_opt_out BSTR* result)
{
    if (!result)
        return E_POINTER;

    ASSERT(is<HTMLTextAreaElement>(m_element));
    HTMLTextAreaElement& textareaElement = downcast<HTMLTextAreaElement>(*m_element);
    WTF::String valueString = textareaElement.value();
    *result = BString(valueString).release();
    if (valueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT DOMHTMLTextAreaElement::setValue(_In_ BSTR value)
{
    ASSERT(is<HTMLTextAreaElement>(m_element));
    HTMLTextAreaElement& textareaElement = downcast<HTMLTextAreaElement>(*m_element);
    textareaElement.setValue(String((UChar*) value, SysStringLen(value)));
    return S_OK;
}
    
HRESULT DOMHTMLTextAreaElement::select()
{
    ASSERT(is<HTMLTextAreaElement>(m_element));
    HTMLTextAreaElement& textareaElement = downcast<HTMLTextAreaElement>(*m_element);
    textareaElement.select();
    return S_OK;
}

// DOMHTMLTextAreaElement -- IFormPromptAdditions ------------------------------------

HRESULT DOMHTMLTextAreaElement::isUserEdited(_Out_ BOOL *result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;
    ASSERT(is<HTMLTextAreaElement>(m_element));
    if (downcast<HTMLTextAreaElement>(*m_element).lastChangeWasUserEdit())
        *result = TRUE;
    return S_OK;
}

// DOMHTMLIFrameElement - IUnknown --------------------------------------------------

HRESULT DOMHTMLIFrameElement::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMHTMLIFrameElement))
        *ppvObject = static_cast<IDOMHTMLIFrameElement*>(this);
    else
        return DOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DOMHTMLIFrameElement -------------------------------------------------------------

HRESULT DOMHTMLIFrameElement::contentFrame(_COM_Outptr_opt_ IWebFrame** result)
{
    if (!result)
        return E_POINTER;
    *result = nullptr;
    ASSERT(m_element);
    HTMLIFrameElement& iFrameElement = downcast<HTMLIFrameElement>(*m_element);
    COMPtr<IWebFrame> webFrame = kit(dynamicDowncast<LocalFrame>(iFrameElement.contentFrame()));
    if (!webFrame)
        return E_FAIL;
    return webFrame.copyRefTo(result);
}
