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

#ifndef DOMEventsClasses_H
#define DOMEventsClasses_H

#include "WebKit.h"
#include "DOMCoreClasses.h"
#include <WebCore/EventListener.h>

#include <wtf/RefPtr.h>

// {AC3D1BC3-4976-4431-8A19-4812C5EFE39C}
DEFINE_GUID(IID_DOMEvent, 0xac3d1bc3, 0x4976, 0x4431, 0x8a, 0x19, 0x48, 0x12, 0xc5, 0xef, 0xe3, 0x9c);

namespace WebCore {
    class Event;
}

class DOMUIEvent;

class WebEventListener : public WebCore::EventListener {
public:
    WebEventListener(IDOMEventListener*);
    ~WebEventListener();
    virtual bool operator==(const EventListener&) const;
    virtual void handleEvent(WebCore::ScriptExecutionContext&, WebCore::Event&);
    static Ref<WebEventListener> create(IDOMEventListener*);
private:
    IDOMEventListener* m_iDOMEventListener;
};

class DOMEventListener : public DOMObject, public IDOMEventListener
{
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

    // IDOMEventListener
    virtual HRESULT STDMETHODCALLTYPE handleEvent(_In_opt_ IDOMEvent*);
};

class DOMEvent : public DOMObject, public IDOMEvent
{
public:
    static IDOMEvent* createInstance(RefPtr<WebCore::Event>&&);
protected:
    DOMEvent(RefPtr<WebCore::Event>&&);
    ~DOMEvent();

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR*);
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget**);
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget**);
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result);
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result);
    virtual HRESULT STDMETHODCALLTYPE stopPropagation();
    virtual HRESULT STDMETHODCALLTYPE preventDefault();
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg);

    // DOMEvent methods
    WebCore::Event* coreEvent() { return m_event.get(); }

protected:
    RefPtr<WebCore::Event> m_event;
};

class DOMUIEvent : public DOMEvent, public IDOMUIEvent
{
public:
    DOMUIEvent(RefPtr<WebCore::Event>&& e)
        : DOMEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    {
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault()
    {
        return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view(_COM_Outptr_opt_ IDOMWindow**);
    virtual HRESULT STDMETHODCALLTYPE detail(_Out_ long* result);
    virtual HRESULT STDMETHODCALLTYPE initUIEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable, _In_opt_ IDOMWindow* view, long detail);
    virtual HRESULT STDMETHODCALLTYPE keyCode(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE charCode(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE unused1(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE unused2(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE pageX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE pageY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE which(_Out_ long*);
};

class DOMKeyboardEvent : public DOMUIEvent, public IDOMKeyboardEvent
{
public:
    DOMKeyboardEvent(RefPtr<WebCore::Event>&& e)
        : DOMUIEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMUIEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    {
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault()
    {
        return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view(_COM_Outptr_opt_ IDOMWindow** result)
    {
        return DOMUIEvent::view(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE detail( 
        /* [retval][out] */ long* result) { return DOMUIEvent::detail(result); }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable, _In_opt_ IDOMWindow* view, long detail)
    {
        return DOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail);
    }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode(_Out_ long* result)
    {
        return DOMUIEvent::keyCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE charCode(_Out_ long* result)
    {
        return DOMUIEvent::charCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused1(_Out_ long* result)
    {
        return DOMUIEvent::unused1(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused2(_Out_ long* result)
    {
        return DOMUIEvent::unused2(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageX(_Out_ long* result)
    {
        return DOMUIEvent::pageX(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageY(_Out_ long* result)
    {
        return DOMUIEvent::pageY(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE which(_Out_ long* result)
    {
        return DOMUIEvent::which(result);
    }

    // IDOMKeyboardEvent
    virtual HRESULT STDMETHODCALLTYPE keyIdentifier(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE location(_Out_ unsigned long*);
    virtual HRESULT STDMETHODCALLTYPE keyLocation(_Out_ unsigned long*);
    virtual HRESULT STDMETHODCALLTYPE ctrlKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE shiftKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE altKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE metaKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE altGraphKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE getModifierState(_In_ BSTR keyIdentifierArg, _Out_ BOOL* result);
    virtual HRESULT STDMETHODCALLTYPE initKeyboardEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable,
        _In_opt_ IDOMWindow* view, _In_ BSTR keyIdentifier, unsigned long keyLocation, BOOL ctrlKey,
        BOOL altKey, BOOL shiftKey, BOOL metaKey, BOOL graphKey);
};

class DOMMouseEvent : public DOMUIEvent, public IDOMMouseEvent
{
public:
    DOMMouseEvent(RefPtr<WebCore::Event>&& e)
        : DOMUIEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMUIEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    {
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault()
    {
        return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view(_COM_Outptr_opt_ IDOMWindow** result)
    {
        return DOMUIEvent::view(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE detail(_Out_ long* result)
    {
        return DOMUIEvent::detail(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable, _In_opt_ IDOMWindow* view, long detail)
    {
        return DOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail);
    }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode(_Out_ long* result)
    {
        return DOMUIEvent::keyCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE charCode(_Out_ long* result)
    {
        return DOMUIEvent::charCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused1(_Out_ long* result)
    {
        return DOMUIEvent::unused1(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused2(_Out_ long* result)
    {
        return DOMUIEvent::unused2(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageX(_Out_ long* result)
    {
        return DOMUIEvent::pageX(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageY(_Out_ long* result)
    {
        return DOMUIEvent::pageY(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE which(_Out_ long* result)
    {
        return DOMUIEvent::which(result);
    }

    // IDOMMouseEvent
    virtual HRESULT STDMETHODCALLTYPE screenX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE screenY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE clientX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE clientY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE ctrlKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE shiftKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE altKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE metaKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE button(_Out_ unsigned short*);
    virtual HRESULT STDMETHODCALLTYPE relatedTarget(_COM_Outptr_opt_ IDOMEventTarget**);
    virtual HRESULT STDMETHODCALLTYPE initMouseEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable, _In_opt_ IDOMWindow* view,
        long detail, long screenX, long screenY, long clientX, long clientY, BOOL ctrlKey, BOOL altKey, BOOL shiftKey, BOOL metaKey,
        unsigned short button, _In_opt_ IDOMEventTarget* relatedTarget);
    virtual HRESULT STDMETHODCALLTYPE offsetX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE offsetY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE x(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE y(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE fromElement(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE toElement(_COM_Outptr_opt_ IDOMNode**);
};

class DOMMutationEvent : public DOMEvent, public IDOMMutationEvent
{
public:
    DOMMutationEvent(RefPtr<WebCore::Event>&& e)
        : DOMEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    { 
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault()
    {
        return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMMutationEvent
    virtual HRESULT STDMETHODCALLTYPE relatedNode(_COM_Outptr_opt_ IDOMNode**);
    virtual HRESULT STDMETHODCALLTYPE prevValue(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE newValue(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE attrName(__deref_opt_out BSTR* result);
    virtual HRESULT STDMETHODCALLTYPE attrChange(_Out_ unsigned short* result);
    virtual HRESULT STDMETHODCALLTYPE initMutationEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable,
        _In_opt_ IDOMNode* relatedNode, _In_ BSTR prevValue, _In_ BSTR newValue, _In_ BSTR attrName,
        unsigned short attrChange);
};

class DOMOverflowEvent : public DOMEvent, public IDOMOverflowEvent
{
public:
    DOMOverflowEvent(RefPtr<WebCore::Event>&& e)
        : DOMEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    {
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault() 
    {
        return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMOverflowEvent
    virtual HRESULT STDMETHODCALLTYPE orient(_Out_ unsigned short*);
    virtual HRESULT STDMETHODCALLTYPE horizontalOverflow(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE verticalOverflow(_Out_ BOOL*);
};

class DOMWheelEvent : public DOMUIEvent, public IDOMWheelEvent
{
public:
    DOMWheelEvent(RefPtr<WebCore::Event>&& e)
        : DOMUIEvent(WTFMove(e))
    { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(_In_ REFIID riid, _COM_Outptr_ void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef() { return DOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release() { return DOMUIEvent::Release(); }

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

    // IDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type(__deref_opt_out BSTR* result)
    {
        return DOMEvent::type(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE target(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::target(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget(_COM_Outptr_opt_ IDOMEventTarget** result)
    {
        return DOMEvent::currentTarget(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase(_Out_ unsigned short* result)
    {
        return DOMEvent::eventPhase(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles(_Out_ BOOL* result)
    {
        return DOMEvent::bubbles(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable(_Out_ BOOL* result)
    { 
        return DOMEvent::cancelable(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp(_Out_ DOMTimeStamp* result)
    {
        return DOMEvent::timeStamp(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation()
    {
        return DOMEvent::stopPropagation();
    }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault()
    { return DOMEvent::preventDefault();
    }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent(_In_ BSTR eventTypeArg, BOOL canBubbleArg, BOOL cancelableArg)
    {
        return DOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg);
    }

    // IDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view(_COM_Outptr_opt_ IDOMWindow** result)
    {
        return DOMUIEvent::view(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE detail(_Out_ long* result)
    {
        return DOMUIEvent::detail(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent(_In_ BSTR type, BOOL canBubble, BOOL cancelable, _In_opt_ IDOMWindow* view, long detail)
    {
        return DOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail);
    }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode(_Out_ long* result)
    {
        return DOMUIEvent::keyCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE charCode(_Out_ long* result)
    {
        return DOMUIEvent::charCode(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused1(_Out_ long* result)
    {
        return DOMUIEvent::unused1(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE unused2(_Out_ long* result)
    {
        return DOMUIEvent::unused2(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageX(_Out_ long* result)
    {
        return DOMUIEvent::pageX(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE pageY(_Out_ long* result)
    {
        return DOMUIEvent::pageY(result);
    }
    
    virtual HRESULT STDMETHODCALLTYPE which(_Out_ long* result)
    {
        return DOMUIEvent::which(result);
    }

    // IDOMWheelEvent
    virtual HRESULT STDMETHODCALLTYPE screenX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE screenY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE clientX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE clientY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE ctrlKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE shiftKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE altKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE metaKey(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE wheelDelta(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE wheelDeltaX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE wheelDeltaY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE offsetX(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE offsetY(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE x(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE y(_Out_ long*);
    virtual HRESULT STDMETHODCALLTYPE isHorizontal(_Out_ BOOL*);
    virtual HRESULT STDMETHODCALLTYPE initWheelEvent(long wheelDeltaX, long wheelDeltaY,
        _In_opt_ IDOMWindow* view, long screenX, long screenY, long clientX, long clientY,
        BOOL ctrlKey, BOOL altKey, BOOL shiftKey, BOOL metaKey);
};

#endif
