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
#include "DOMHTMLClasses.h"
#include "COMPtr.h"

#pragma warning(push, 0)
#include <WebCore/BString.h>
#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLDocument.h>
#include <WebCore/HTMLFormElement.h>
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/HTMLOptionElement.h>
#include <WebCore/HTMLSelectElement.h>
#include <WebCore/HTMLTextAreaElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/RenderObject.h>
#include <WebCore/RenderTextControl.h>
#pragma warning(pop)

using namespace WebCore;
using namespace HTMLNames;

// DeprecatedDOMHTMLCollection

DeprecatedDOMHTMLCollection::DeprecatedDOMHTMLCollection(WebCore::HTMLCollection* c)
: m_collection(c)
{
}

IDeprecatedDOMHTMLCollection* DeprecatedDOMHTMLCollection::createInstance(WebCore::HTMLCollection* c)
{
    if (!c)
        return 0;

    IDeprecatedDOMHTMLCollection* htmlCollection = 0;
    DeprecatedDOMHTMLCollection* newCollection = new DeprecatedDOMHTMLCollection(c);
    if (FAILED(newCollection->QueryInterface(IID_IDeprecatedDOMHTMLCollection, (void**)&htmlCollection))) {
        delete newCollection;
        return 0;
    }

    return htmlCollection;
}

// DeprecatedDOMHTMLCollection - IUnknown -----------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLCollection::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLCollection))
        *ppvObject = static_cast<IDeprecatedDOMHTMLCollection*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLCollection ----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLCollection::length( 
    /* [retval][out] */ UINT* result)
{
    *result = 0;
    if (!m_collection)
        return E_POINTER;

    *result = m_collection->length();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLCollection::item( 
    /* [in] */ UINT index,
    /* [retval][out] */ IDeprecatedDOMNode** node)
{
    *node = 0;
    if (!m_collection)
        return E_POINTER;

    *node = DeprecatedDOMNode::createInstance(m_collection->item(index));
    return *node ? S_OK : E_FAIL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLCollection::namedItem( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*node*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMHTMLOptionsCollection - IUnknown ----------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionsCollection::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLOptionsCollection))
        *ppvObject = static_cast<IDeprecatedDOMHTMLOptionsCollection*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLOptionsCollection ---------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionsCollection::length( 
    /* [retval][out] */ unsigned int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionsCollection::setLength( 
    /* [in] */ unsigned int /*length*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionsCollection::item( 
    /* [in] */ unsigned int /*index*/,
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionsCollection::namedItem( 
    /* [in] */ BSTR /*name*/,
    /* [retval][out] */ IDeprecatedDOMNode* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMHTMLDocument - IUnknown -------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLDocument))
        *ppvObject = static_cast<IDeprecatedDOMHTMLDocument*>(this);
    else
        return DeprecatedDOMDocument::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLDocument ------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::title( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::setTitle( 
        /* [in] */ BSTR /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::referrer( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::domain( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::URL( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::body( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement** bodyElement)
{
    *bodyElement = 0;
    if (!m_document || !m_document->isHTMLDocument())
        return E_FAIL;

    HTMLDocument* htmlDoc = static_cast<HTMLDocument*>(m_document);
    COMPtr<IDeprecatedDOMElement> domElement;
    domElement.adoptRef(DeprecatedDOMHTMLElement::createInstance(htmlDoc->body()));
    if (domElement)
        return domElement->QueryInterface(IID_IDeprecatedDOMHTMLElement, (void**) bodyElement);
    return E_FAIL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::setBody( 
        /* [in] */ IDeprecatedDOMHTMLElement* /*body*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::images( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** /*collection*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::applets( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** /*collection*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::links( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** /*collection*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::forms( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** collection)
{
    *collection = 0;
    if (!m_document || !m_document->isHTMLDocument())
        return E_FAIL;

    HTMLDocument* htmlDoc = static_cast<HTMLDocument*>(m_document);
    *collection = DeprecatedDOMHTMLCollection::createInstance(htmlDoc->forms().get());
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::anchors( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** /*collection*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::cookie( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::setCookie( 
        /* [in] */ BSTR /*cookie*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::open( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::close( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::write( 
        /* [in] */ BSTR /*text*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::writeln( 
        /* [in] */ BSTR /*text*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::getElementById_( 
        /* [in] */ BSTR /*elementId*/,
        /* [retval][out] */ IDeprecatedDOMElement** /*element*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLDocument::getElementsByName( 
        /* [in] */ BSTR /*elementName*/,
        /* [retval][out] */ IDeprecatedDOMNodeList** /*nodeList*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMHTMLElement - IUnknown --------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLElement*>(this);
    else
        return DeprecatedDOMElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLElement -------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::idName( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setIdName( 
        /* [in] */ BSTR /*idName*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::title( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setTitle( 
        /* [in] */ BSTR /*title*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::lang( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setLang( 
        /* [in] */ BSTR /*lang*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::dir( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setDir( 
        /* [in] */ BSTR /*dir*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::className( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setClassName( 
        /* [in] */ BSTR /*className*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::innerHTML( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setInnerHTML( 
        /* [in] */ BSTR /*html*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
        
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::innerText( 
        /* [retval][out] */ BSTR* result)
{
    ASSERT(m_element && m_element->isHTMLElement());
    WebCore::String innerTextString = static_cast<HTMLElement*>(m_element)->innerText();
    *result = BString(innerTextString.characters(), innerTextString.length()).release();
    return S_OK;
}
        
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLElement::setInnerText( 
        /* [in] */ BSTR text)
{
    ASSERT(m_element && m_element->isHTMLElement());
    HTMLElement* htmlEle = static_cast<HTMLElement*>(m_element);
    WebCore::String textString(text, SysStringLen(text));
    WebCore::ExceptionCode ec = 0;
    htmlEle->setInnerText(textString, ec);
    return S_OK;
}

// DeprecatedDOMHTMLFormElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLFormElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLFormElement*>(this);
    else
        return DeprecatedDOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLFormElement ---------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::elements( 
        /* [retval][out] */ IDeprecatedDOMHTMLCollection** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::length( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::name( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::acceptCharset( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setAcceptCharset( 
        /* [in] */ BSTR /*acceptCharset*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::action( 
        /* [retval][out] */ BSTR* result)
{
    ASSERT(m_element && m_element->hasTagName(formTag));
    WebCore::String actionString = static_cast<HTMLFormElement*>(m_element)->action();
    *result = BString(actionString.characters(), actionString.length()).release();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setAction( 
        /* [in] */ BSTR /*action*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::encType( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setEnctype( 
        /* [retval][out] */ BSTR* /*encType*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::method( 
        /* [retval][out] */ BSTR* result)
{
    ASSERT(m_element && m_element->hasTagName(formTag));
    WebCore::String methodString = static_cast<HTMLFormElement*>(m_element)->method();
    *result = BString(methodString.characters(), methodString.length()).release();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setMethod( 
        /* [in] */ BSTR /*method*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::target( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::setTarget( 
        /* [in] */ BSTR /*target*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::submit( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLFormElement::reset( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMHTMLSelectElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLSelectElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLSelectElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransitionSelect))
        *ppvObject = static_cast<IFormsAutoFillTransitionSelect*>(this);
    else
        return DeprecatedDOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLSelectElement -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::selectedIndex( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setSelectedIndx( 
        /* [in] */ int /*selectedIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::value( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setValue( 
        /* [in] */ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::length( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::form( 
        /* [retval][out] */ IDeprecatedDOMHTMLFormElement** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::options( 
        /* [retval][out] */ IDeprecatedDOMHTMLOptionsCollection** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::multiple( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setMultiple( 
        /* [in] */ BOOL /*multiple*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::name( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::size( 
        /* [retval][out] */ int* /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setSize( 
        /* [in] */ int /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::add( 
        /* [in] */ IDeprecatedDOMHTMLElement* /*element*/,
        /* [in] */ IDeprecatedDOMHTMLElement* /*before*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::remove( 
        /* [in] */ int /*index*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
// DeprecatedDOMHTMLSelectElement - IFormsAutoFillTransitionSelect ----------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLSelectElement::activateItemAtIndex( 
    /* [in] */ int /*index*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;    
}

// DeprecatedDOMHTMLOptionElement - IUnknown --------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLOptionElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLOptionElement*>(this);
    else
        return DeprecatedDOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLOptionElement -------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::form( 
        /* [retval][out] */ IDeprecatedDOMHTMLFormElement** /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::defaultSelected( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::setDefaultSelected( 
        /* [in] */ BOOL /*defaultSelected*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::text( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::index( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::label( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::setLabel( 
        /* [in] */ BSTR /*label*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::selected( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::setSelected( 
        /* [in] */ BOOL /*selected*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::value( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLOptionElement::setValue( 
        /* [in] */ BSTR /*value*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DeprecatedDOMHTMLInputElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLInputElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLInputElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormsAutoFillTransition))
        *ppvObject = static_cast<IFormsAutoFillTransition*>(this);
    else if (IsEqualGUID(riid, IID_IFormPromptAdditions))
        *ppvObject = static_cast<IFormPromptAdditions*>(this);    
    else
        return DeprecatedDOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLInputElement --------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::defaultValue( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setDefaultValue( 
        /* [in] */ BSTR /*val*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::defaultChecked( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setDefaultChecked( 
        /* [in] */ BSTR /*checked*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::form( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement** result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    COMPtr<IDeprecatedDOMElement> domElement;
    domElement.adoptRef(DeprecatedDOMHTMLElement::createInstance(inputElement->form()));
    if (domElement)
        return domElement->QueryInterface(IID_IDeprecatedDOMHTMLElement, (void**) result);
    return E_FAIL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::accept( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setAccept( 
        /* [in] */ BSTR /*accept*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::accessKey( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setAccessKey( 
        /* [in] */ BSTR /*key*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::align( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setAlign( 
        /* [in] */ BSTR /*align*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::alt( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setAlt( 
        /* [in] */ BSTR /*alt*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::checked( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setChecked( 
        /* [in] */ BOOL /*checked*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::disabled( 
        /* [retval][out] */ BOOL* result)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *result = inputElement->disabled() ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::maxLength( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setMaxLength( 
        /* [in] */ int /*maxLength*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::name( 
        /* [retval][out] */ BSTR* /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::readOnly( 
        /* [retval][out] */ BOOL* result)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *result = inputElement->readOnly() ? TRUE : FALSE;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setReadOnly( 
        /* [in] */ BOOL /*readOnly*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::size( 
        /* [retval][out] */ unsigned int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setSize( 
        /* [in] */ unsigned int /*size*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::src( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setSrc( 
        /* [in] */ BSTR /*src*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setType( 
        /* [in] */ BSTR type)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    WebCore::String typeString(type, SysStringLen(type));
    inputElement->setType(typeString);
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::useMap( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setUseMap( 
        /* [in] */ BSTR /*useMap*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::value( 
        /* [retval][out] */ BSTR* result)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    WebCore::String valueString = inputElement->value();
    *result = BString(valueString.characters(), valueString.length()).release();
    if (valueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setValue( 
        /* [in] */ BSTR value)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setValue(String((UChar*) value, SysStringLen(value)));
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::select( void)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->select();
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::click( void)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setSelectionStart( 
    /* [in] */ long start)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setSelectionStart(start);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::selectionStart( 
    /* [retval][out] */ long *start)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *start = inputElement->selectionStart();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setSelectionEnd( 
    /* [in] */ long end)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setSelectionEnd(end);
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::selectionEnd( 
    /* [retval][out] */ long *end)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *end = inputElement->selectionEnd();
    return S_OK;
}

// DeprecatedDOMHTMLInputElement -- IFormsAutoFillTransition ----------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::isTextField(
    /* [retval][out] */ BOOL* result)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *result = inputElement->isTextField() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::rectOnScreen( 
    /* [retval][out] */ LPRECT rect)
{
    rect->left = rect->top = rect->right = rect->bottom = 0;
    RenderObject* renderer = m_element->renderer();
    FrameView* view = m_element->document()->view();
    if (!renderer || !view)
        return E_FAIL;

    IntRect coreRect = renderer->absoluteBoundingBoxRect();
    coreRect.setLocation(view->contentsToWindow(coreRect.location()));
    rect->left = coreRect.x();
    rect->top = coreRect.y();
    rect->right = coreRect.right();
    rect->bottom = coreRect.bottom();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::replaceCharactersInRange( 
    /* [in] */ int /*startTarget*/,
    /* [in] */ int /*endTarget*/,
    /* [in] */ BSTR /*replacementString*/,
    /* [in] */ int /*index*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::selectedRange( 
    /* [out] */ int* start,
    /* [out] */ int* end)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    *start = inputElement->selectionStart();
    *end = inputElement->selectionEnd();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::setAutofilled( 
    /* [in] */ BOOL filled)
{
    ASSERT(m_element && m_element->hasTagName(inputTag));
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(m_element);
    inputElement->setAutofilled(!!filled);
    return S_OK;
}

// DeprecatedDOMHTMLInputElement -- IFormPromptAdditions ------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLInputElement::isUserEdited( 
    /* [retval][out] */ BOOL *result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;
    ASSERT(m_element);
    BOOL textField = FALSE;
    if (FAILED(isTextField(&textField)) || !textField)
        return S_OK;
    RenderObject* renderer = m_element->renderer();
    if (renderer && static_cast<WebCore::RenderTextControl*>(renderer)->isUserEdited())
        *result = TRUE;
    return S_OK;
}

// DeprecatedDOMHTMLTextAreaElement - IUnknown ----------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMHTMLTextAreaElement))
        *ppvObject = static_cast<IDeprecatedDOMHTMLTextAreaElement*>(this);
    else if (IsEqualGUID(riid, IID_IFormPromptAdditions))
        *ppvObject = static_cast<IFormPromptAdditions*>(this);    
    else
        return DeprecatedDOMHTMLElement::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

// DeprecatedDOMHTMLTextAreaElement -----------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::defaultValue( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setDefaultValue( 
        /* [in] */ BSTR /*val*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::form( 
        /* [retval][out] */ IDeprecatedDOMHTMLElement** result)
{
    if (!result)
        return E_POINTER;
    *result = 0;
    ASSERT(m_element && m_element->hasTagName(textareaTag));
    HTMLTextAreaElement* textareaElement = static_cast<HTMLTextAreaElement*>(m_element);
    COMPtr<IDeprecatedDOMElement> domElement;
    domElement.adoptRef(DeprecatedDOMHTMLElement::createInstance(textareaElement->form()));
    if (domElement)
        return domElement->QueryInterface(IID_IDeprecatedDOMHTMLElement, (void**) result);
    return E_FAIL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::accessKey( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setAccessKey( 
        /* [in] */ BSTR /*key*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::cols( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setCols( 
        /* [in] */ int /*cols*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::disabled( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setDisabled( 
        /* [in] */ BOOL /*disabled*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::name( 
        /* [retval][out] */ BSTR* /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setName( 
        /* [in] */ BSTR /*name*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::readOnly( 
        /* [retval][out] */ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setReadOnly( 
        /* [in] */ BOOL /*readOnly*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::rows( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setRows( 
        /* [in] */ int /*rows*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::tabIndex( 
        /* [retval][out] */ int* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setTabIndex( 
        /* [in] */ int /*tabIndex*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::type( 
        /* [retval][out] */ BSTR* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::value( 
        /* [retval][out] */ BSTR* result)
{
    ASSERT(m_element && m_element->hasTagName(textareaTag));
    HTMLTextAreaElement* textareaElement = static_cast<HTMLTextAreaElement*>(m_element);
    WebCore::String valueString = textareaElement->value();
    *result = BString(valueString.characters(), valueString.length()).release();
    if (valueString.length() && !*result)
        return E_OUTOFMEMORY;
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::setValue( 
        /* [in] */ BSTR value)
{
    ASSERT(m_element && m_element->hasTagName(textareaTag));
    HTMLTextAreaElement* textareaElement = static_cast<HTMLTextAreaElement*>(m_element);
    textareaElement->setValue(String((UChar*) value, SysStringLen(value)));
    return S_OK;
}
    
HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::select( void)
{
    ASSERT(m_element && m_element->hasTagName(textareaTag));
    HTMLTextAreaElement* textareaElement = static_cast<HTMLTextAreaElement*>(m_element);
    textareaElement->select();
    return S_OK;
}

// DeprecatedDOMHTMLTextAreaElement -- IFormPromptAdditions ------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMHTMLTextAreaElement::isUserEdited( 
    /* [retval][out] */ BOOL *result)
{
    if (!result)
        return E_POINTER;

    *result = FALSE;
    ASSERT(m_element);
    RenderObject* renderer = m_element->renderer();
    if (renderer && static_cast<WebCore::RenderTextControl*>(renderer)->isUserEdited())
        *result = TRUE;
    return S_OK;
}
