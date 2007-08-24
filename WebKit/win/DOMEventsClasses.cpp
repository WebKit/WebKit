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
#include <initguid.h>
#include "DOMEventsClasses.h"

#pragma warning( push, 0 )
#include <WebCore/DOMWindow.h>
#include <WebCore/Event.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseEvent.h>
#pragma warning( pop )

// DeprecatedDOMEventListener -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMEventListener::QueryInterface(const IID &riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMEventListener))
        *ppvObject = static_cast<IDeprecatedDOMEventListener*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEventListener::handleEvent( 
    /* [in] */ IDeprecatedDOMEvent* /*evt*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMEvent -------------------------------------------------------------------

DeprecatedDOMEvent::DeprecatedDOMEvent(PassRefPtr<WebCore::Event> e)
: m_event(0)
{
    m_event = e;
}

DeprecatedDOMEvent::~DeprecatedDOMEvent()
{
}

IDeprecatedDOMEvent* DeprecatedDOMEvent::createInstance(PassRefPtr<WebCore::Event> e)
{
    if (!e)
        return 0;

    HRESULT hr;
    IDeprecatedDOMEvent* domEvent = 0;

    if (e->isKeyboardEvent()) {
        DeprecatedDOMKeyboardEvent* newEvent = new DeprecatedDOMKeyboardEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMKeyboardEvent, (void**)&domEvent);
    } else if (e->isMouseEvent()) {
        DeprecatedDOMMouseEvent* newEvent = new DeprecatedDOMMouseEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMMouseEvent, (void**)&domEvent);
    } else if (e->isMutationEvent()) {
        DeprecatedDOMMutationEvent* newEvent = new DeprecatedDOMMutationEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMMutationEvent, (void**)&domEvent);
    } else if (e->isOverflowEvent()) {
        DeprecatedDOMOverflowEvent* newEvent = new DeprecatedDOMOverflowEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMOverflowEvent, (void**)&domEvent);
    } else if (e->isWheelEvent()) {
        DeprecatedDOMWheelEvent* newEvent = new DeprecatedDOMWheelEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMWheelEvent, (void**)&domEvent);
    } else if (e->isUIEvent()) {
        DeprecatedDOMUIEvent* newEvent = new DeprecatedDOMUIEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMUIEvent, (void**)&domEvent);
    } else {
        DeprecatedDOMEvent* newEvent = new DeprecatedDOMEvent(e);
        hr = newEvent->QueryInterface(IID_IDeprecatedDOMEvent, (void**)&domEvent);
    }

    if (FAILED(hr))
        return 0;

    return domEvent;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::QueryInterface(const IID &riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_DeprecatedDOMEvent))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IDeprecatedDOMEvent))
        *ppvObject = static_cast<IDeprecatedDOMEvent*>(this);
    else
        return DeprecatedDOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::type( 
    /* [retval][out] */ BSTR* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::target( 
    /* [retval][out] */ IDeprecatedDOMEventTarget** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::currentTarget( 
    /* [retval][out] */ IDeprecatedDOMEventTarget** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::eventPhase( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::bubbles( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::cancelable( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::timeStamp( 
    /* [retval][out] */ DOMTimeStamp* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::stopPropagation( void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::preventDefault( void)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMEvent::initEvent( 
    /* [in] */ BSTR /*eventTypeArg*/,
    /* [in] */ BOOL /*canBubbleArg*/,
    /* [in] */ BOOL /*cancelableArg*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMUIEvent -----------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMUIEvent))
        *ppvObject = static_cast<IDeprecatedDOMUIEvent*>(this);
    else
        return DeprecatedDOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::view( 
    /* [retval][out] */ IDeprecatedDOMWindow** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::detail( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::initUIEvent( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ BOOL /*canBubble*/,
    /* [in] */ BOOL /*cancelable*/,
    /* [in] */ IDeprecatedDOMWindow* /*view*/,
    /* [in] */ long /*detail*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::keyCode( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::charCode( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::layerX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::layerY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::pageX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::pageY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMUIEvent::which( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMKeyboardEvent -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMKeyboardEvent))
        *ppvObject = static_cast<IDeprecatedDOMKeyboardEvent*>(this);
    else
        return DeprecatedDOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::keyIdentifier( 
    /* [retval][out] */ BSTR* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::keyLocation( 
    /* [retval][out] */ unsigned long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::ctrlKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->ctrlKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::shiftKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->shiftKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::altKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->altKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::metaKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->metaKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::altGraphKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->altGraphKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::getModifierState( 
    /* [in] */ BSTR /*keyIdentifierArg*/,
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMKeyboardEvent::initKeyboardEvent( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ BOOL /*canBubble*/,
    /* [in] */ BOOL /*cancelable*/,
    /* [in] */ IDeprecatedDOMWindow* /*view*/,
    /* [in] */ BSTR /*keyIdentifier*/,
    /* [in] */ unsigned long /*keyLocation*/,
    /* [in] */ BOOL /*ctrlKey*/,
    /* [in] */ BOOL /*altKey*/,
    /* [in] */ BOOL /*shiftKey*/,
    /* [in] */ BOOL /*metaKey*/,
    /* [in] */ BOOL /*graphKey*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMMouseEvent --------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMMouseEvent))
        *ppvObject = static_cast<IDeprecatedDOMMouseEvent*>(this);
    else
        return DeprecatedDOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::screenX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::screenY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::clientX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::clientY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::ctrlKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->ctrlKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::shiftKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->shiftKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::altKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->altKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::metaKey( 
    /* [retval][out] */ BOOL* result)
{
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->metaKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::button( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::relatedTarget( 
    /* [retval][out] */ IDeprecatedDOMEventTarget** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::initMouseEvent( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ BOOL /*canBubble*/,
    /* [in] */ BOOL /*cancelable*/,
    /* [in] */ IDeprecatedDOMWindow* /*view*/,
    /* [in] */ long /*detail*/,
    /* [in] */ long /*screenX*/,
    /* [in] */ long /*screenY*/,
    /* [in] */ long /*clientX*/,
    /* [in] */ long /*clientY*/,
    /* [in] */ BOOL /*ctrlKey*/,
    /* [in] */ BOOL /*altKey*/,
    /* [in] */ BOOL /*shiftKey*/,
    /* [in] */ BOOL /*metaKey*/,
    /* [in] */ unsigned short /*button*/,
    /* [in] */ IDeprecatedDOMEventTarget* /*relatedTarget*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::offsetX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::offsetY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::x( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::y( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::fromElement( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMouseEvent::toElement( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMMutationEvent -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMMutationEvent))
        *ppvObject = static_cast<IDeprecatedDOMMutationEvent*>(this);
    else
        return DeprecatedDOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::relatedNode( 
    /* [retval][out] */ IDeprecatedDOMNode** /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::prevValue( 
    /* [retval][out] */ BSTR* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::newValue( 
    /* [retval][out] */ BSTR* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::attrName( 
    /* [retval][out] */ BSTR* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::attrChange( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMMutationEvent::initMutationEvent( 
    /* [in] */ BSTR /*type*/,
    /* [in] */ BOOL /*canBubble*/,
    /* [in] */ BOOL /*cancelable*/,
    /* [in] */ IDeprecatedDOMNode* /*relatedNode*/,
    /* [in] */ BSTR /*prevValue*/,
    /* [in] */ BSTR /*newValue*/,
    /* [in] */ BSTR /*attrName*/,
    /* [in] */ unsigned short /*attrChange*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMOverflowEvent -----------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMOverflowEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMOverflowEvent))
        *ppvObject = static_cast<IDeprecatedDOMOverflowEvent*>(this);
    else
        return DeprecatedDOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMOverflowEvent::orient( 
    /* [retval][out] */ unsigned short* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMOverflowEvent::horizontalOverflow( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMOverflowEvent::verticalOverflow( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

// DeprecatedDOMWheelEvent --------------------------------------------------------------

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::QueryInterface(REFIID riid, void** ppvObject)
{
    *ppvObject = 0;
    if (IsEqualGUID(riid, IID_IDeprecatedDOMWheelEvent))
        *ppvObject = static_cast<IDeprecatedDOMWheelEvent*>(this);
    else
        return DeprecatedDOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::screenX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::screenY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::clientX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::clientY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::ctrlKey( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::shiftKey( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::altKey( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::metaKey( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::wheelDelta( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::wheelDeltaX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::wheelDeltaY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::offsetX( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::offsetY( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::x( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::y( 
    /* [retval][out] */ long* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::isHorizontal( 
    /* [retval][out] */ BOOL* /*result*/)
{
    return E_NOTIMPL;
}

HRESULT STDMETHODCALLTYPE DeprecatedDOMWheelEvent::initWheelEvent( 
    /* [in] */ long /*wheelDeltaX*/,
    /* [in] */ long /*wheelDeltaY*/,
    /* [in] */ IDeprecatedDOMWindow* /*view*/,
    /* [in] */ long /*screenX*/,
    /* [in] */ long /*screenY*/,
    /* [in] */ long /*clientX*/,
    /* [in] */ long /*clientY*/,
    /* [in] */ BOOL /*ctrlKey*/,
    /* [in] */ BOOL /*altKey*/,
    /* [in] */ BOOL /*shiftKey*/,
    /* [in] */ BOOL /*metaKey*/)
{
    return E_NOTIMPL;
}
