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

#include "WebKitDLL.h"
#include <initguid.h>
#include "DOMEventsClasses.h"

#include <WebCore/COMPtr.h>
#include <WebCore/DOMWindow.h>
#include <WebCore/Event.h>
#include <WebCore/EventNames.h>
#include <WebCore/KeyboardEvent.h>
#include <WebCore/MouseEvent.h>
#include <WebCore/ScriptExecutionContext.h>

// DOMEventListener -----------------------------------------------------------

HRESULT DOMEventListener::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMEventListener))
        *ppvObject = static_cast<IDOMEventListener*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMEventListener::handleEvent(_In_opt_ IDOMEvent* /*evt*/)
{
    return E_NOTIMPL;
}

WebEventListener::WebEventListener(IDOMEventListener* i)
    : EventListener(CPPEventListenerType)
    , m_iDOMEventListener(i)
{
    m_iDOMEventListener->AddRef();
}

WebEventListener::~WebEventListener()
{
    m_iDOMEventListener->Release();
}

bool WebEventListener::operator==(const WebCore::EventListener& other) const
{
    return (other.type() == CPPEventListenerType 
        && reinterpret_cast<const WebEventListener*>(&other)->m_iDOMEventListener == m_iDOMEventListener);
}

void WebEventListener::handleEvent(WebCore::ScriptExecutionContext& s, WebCore::Event& e)
{
    RefPtr<WebCore::Event> ePtr(&e);
    COMPtr<IDOMEvent> domEvent = DOMEvent::createInstance(WTFMove(ePtr));
    m_iDOMEventListener->handleEvent(domEvent.get());
}

Ref<WebEventListener> WebEventListener::create(IDOMEventListener* d)
{
    return adoptRef(*new WebEventListener(d));
}

// DOMEvent -------------------------------------------------------------------

DOMEvent::DOMEvent(RefPtr<WebCore::Event>&& e)
{
    m_event = WTFMove(e);
}

DOMEvent::~DOMEvent()
{
}

IDOMEvent* DOMEvent::createInstance(RefPtr<WebCore::Event>&& e)
{
    if (!e)
        return nullptr;

    HRESULT hr;
    IDOMEvent* domEvent = nullptr;

    switch (e->eventInterface()) {
    case WebCore::KeyboardEventInterfaceType: {
        DOMKeyboardEvent* newEvent = new DOMKeyboardEvent(WTFMove(e));
        hr = newEvent->QueryInterface(IID_IDOMKeyboardEvent, (void**)&domEvent);
        break;
    }
    case WebCore::MouseEventInterfaceType: {
        DOMMouseEvent* newEvent = new DOMMouseEvent(WTFMove(e));
        hr = newEvent->QueryInterface(IID_IDOMMouseEvent, (void**)&domEvent);
        break;
    }
    case WebCore::MutationEventInterfaceType: {
        DOMMutationEvent* newEvent = new DOMMutationEvent(WTFMove(e));
        hr = newEvent->QueryInterface(IID_IDOMMutationEvent, (void**)&domEvent);
        break;
    }
    case WebCore::OverflowEventInterfaceType: {
        DOMOverflowEvent* newEvent = new DOMOverflowEvent(WTFMove(e));
        hr = newEvent->QueryInterface(IID_IDOMOverflowEvent, (void**)&domEvent);
        break;
    }
    case WebCore::WheelEventInterfaceType: {
        DOMWheelEvent* newEvent = new DOMWheelEvent(WTFMove(e));
        hr = newEvent->QueryInterface(IID_IDOMWheelEvent, (void**)&domEvent);
        break;
    }
    default:
        if (e->isUIEvent()) {
            DOMUIEvent* newEvent = new DOMUIEvent(WTFMove(e));
            hr = newEvent->QueryInterface(IID_IDOMUIEvent, (void**)&domEvent);
        } else {
            DOMEvent* newEvent = new DOMEvent(WTFMove(e));
            hr = newEvent->QueryInterface(IID_IDOMEvent, (void**)&domEvent);
        }
    }

    if (FAILED(hr))
        return nullptr;

    return domEvent;
}

HRESULT DOMEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_DOMEvent))
        *ppvObject = this;
    else if (IsEqualGUID(riid, IID_IDOMEvent))
        *ppvObject = static_cast<IDOMEvent*>(this);
    else
        return DOMObject::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMEvent::type(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMEvent::target(_COM_Outptr_opt_ IDOMEventTarget** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMEvent::currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMEvent::eventPhase(_Out_ unsigned short* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::bubbles(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::cancelable(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::timeStamp(_Out_ DOMTimeStamp* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::stopPropagation()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::preventDefault()
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMEvent::initEvent(_In_ BSTR /*eventTypeArg*/, BOOL /*canBubbleArg*/, BOOL /*cancelableArg*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMUIEvent -----------------------------------------------------------------

HRESULT DOMUIEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMUIEvent))
        *ppvObject = static_cast<IDOMUIEvent*>(this);
    else
        return DOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMUIEvent::view(_COM_Outptr_opt_ IDOMWindow** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::detail(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::initUIEvent(_In_ BSTR /*type*/, BOOL /*canBubble*/, BOOL /*cancelable*/, _In_opt_ IDOMWindow* /*view*/, long /*detail*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::keyCode(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::charCode(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::unused1(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::unused2(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::pageX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::pageY(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMUIEvent::which(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMKeyboardEvent -----------------------------------------------------------

HRESULT DOMKeyboardEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMKeyboardEvent))
        *ppvObject = static_cast<IDOMKeyboardEvent*>(this);
    else
        return DOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMKeyboardEvent::keyIdentifier(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMKeyboardEvent::location(_Out_ unsigned long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMKeyboardEvent::keyLocation(_Out_ unsigned long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMKeyboardEvent::ctrlKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->ctrlKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMKeyboardEvent::shiftKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->shiftKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMKeyboardEvent::altKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->altKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMKeyboardEvent::metaKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->metaKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMKeyboardEvent::altGraphKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isKeyboardEvent())
        return E_FAIL;
    WebCore::KeyboardEvent* keyEvent = static_cast<WebCore::KeyboardEvent*>(m_event.get());

    *result = keyEvent->altGraphKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMKeyboardEvent::getModifierState(_In_ BSTR /*keyIdentifierArg*/, _Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMKeyboardEvent::initKeyboardEvent(_In_ BSTR /*type*/, BOOL /*canBubble*/, BOOL /*cancelable*/,
    _In_opt_ IDOMWindow* /*view*/, _In_ BSTR /*keyIdentifier*/, unsigned long /*keyLocation*/,
    BOOL /*ctrlKey*/, BOOL /*altKey*/, BOOL /*shiftKey*/, BOOL /*metaKey*/, BOOL /*graphKey*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMMouseEvent --------------------------------------------------------------

HRESULT DOMMouseEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMMouseEvent))
        *ppvObject = static_cast<IDOMMouseEvent*>(this);
    else
        return DOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMMouseEvent::screenX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::screenY(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::clientX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::clientY(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::ctrlKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->ctrlKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMMouseEvent::shiftKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->shiftKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMMouseEvent::altKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->altKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMMouseEvent::metaKey(_Out_ BOOL* result)
{
    if (!result)
        return E_POINTER;
    *result = FALSE;
    if (!m_event || !m_event->isMouseEvent())
        return E_FAIL;
    WebCore::MouseEvent* mouseEvent = static_cast<WebCore::MouseEvent*>(m_event.get());

    *result = mouseEvent->metaKey() ? TRUE : FALSE;
    return S_OK;
}

HRESULT DOMMouseEvent::button(_Out_ unsigned short* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::relatedTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::initMouseEvent(_In_ BSTR /*type*/, BOOL /*canBubble*/, BOOL /*cancelable*/,
    _In_opt_ IDOMWindow* /*view*/, long /*detail*/, long /*screenX*/, long /*screenY*/, long /*clientX*/, long /*clientY*/,
    BOOL /*ctrlKey*/, BOOL /*altKey*/, BOOL /*shiftKey*/, BOOL /*metaKey*/, unsigned short /*button*/,
    _In_opt_ IDOMEventTarget* /*relatedTarget*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::offsetX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::offsetY(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::x(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::y(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::fromElement(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMouseEvent::toElement(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

// DOMMutationEvent -----------------------------------------------------------

HRESULT DOMMutationEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMMutationEvent))
        *ppvObject = static_cast<IDOMMutationEvent*>(this);
    else
        return DOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMMutationEvent::relatedNode(_COM_Outptr_opt_ IDOMNode** result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMutationEvent::prevValue(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMutationEvent::newValue(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMutationEvent::attrName(__deref_opt_out BSTR* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = nullptr;
    return E_NOTIMPL;
}

HRESULT DOMMutationEvent::attrChange(_Out_ unsigned short* result)
{
    ASSERT_NOT_REACHED();
    if (!result)
        return E_POINTER;
    *result = 0;
    return E_NOTIMPL;
}

HRESULT DOMMutationEvent::initMutationEvent(_In_ BSTR /*type*/, BOOL /*canBubble*/, BOOL /*cancelable*/,
    _In_opt_ IDOMNode* /*relatedNode*/, _In_ BSTR /*prevValue*/, _In_ BSTR /*newValue*/, _In_ BSTR /*attrName*/,
    unsigned short /*attrChange*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMOverflowEvent -----------------------------------------------------------

HRESULT DOMOverflowEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMOverflowEvent))
        *ppvObject = static_cast<IDOMOverflowEvent*>(this);
    else
        return DOMEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMOverflowEvent::orient(_Out_ unsigned short* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMOverflowEvent::horizontalOverflow(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMOverflowEvent::verticalOverflow(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

// DOMWheelEvent --------------------------------------------------------------

HRESULT DOMWheelEvent::QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject)
{
    if (!ppvObject)
        return E_POINTER;
    *ppvObject = nullptr;
    if (IsEqualGUID(riid, IID_IDOMWheelEvent))
        *ppvObject = static_cast<IDOMWheelEvent*>(this);
    else
        return DOMUIEvent::QueryInterface(riid, ppvObject);

    AddRef();
    return S_OK;
}

HRESULT DOMWheelEvent::screenX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::screenY(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::clientX(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::clientY(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::ctrlKey(_Out_  BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::shiftKey(_Out_  BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::altKey(_Out_  BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::metaKey(_Out_  BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::wheelDelta(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::wheelDeltaX(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::wheelDeltaY(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::offsetX(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::offsetY(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::x(_Out_  long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::y(_Out_ long* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::isHorizontal(_Out_ BOOL* /*result*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}

HRESULT DOMWheelEvent::initWheelEvent(long /*wheelDeltaX*/, long /*wheelDeltaY*/,
    _In_opt_ IDOMWindow* /*view*/, long /*screenX*/, long /*screenY*/, long /*clientX*/, long /*clientY*/,
    BOOL /*ctrlKey*/, BOOL /*altKey*/, BOOL /*shiftKey*/, BOOL /*metaKey*/)
{
    ASSERT_NOT_REACHED();
    return E_NOTIMPL;
}
