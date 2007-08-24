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

#ifndef DOMEventsClasses_H
#define DOMEventsClasses_H

#include "DOMEvents.h"
#include "DOMCoreClasses.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>

// {AC3D1BC3-4976-4431-8A19-4812C5EFE39C}
DEFINE_GUID(IID_DeprecatedDOMEvent, 0xac3d1bc3, 0x4976, 0x4431, 0x8a, 0x19, 0x48, 0x12, 0xc5, 0xef, 0xe3, 0x9c);

namespace WebCore {
    class Event;
}

class DeprecatedDOMUIEvent;

class DeprecatedDOMEventListener : public DeprecatedDOMObject, public IDeprecatedDOMEventListener
{
public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEventListener
    virtual HRESULT STDMETHODCALLTYPE handleEvent( 
        /* [in] */ IDeprecatedDOMEvent* evt);
};

class DeprecatedDOMEvent : public DeprecatedDOMObject, public IDeprecatedDOMEvent
{
public:
    static IDeprecatedDOMEvent* createInstance(PassRefPtr<WebCore::Event> e);
protected:
    DeprecatedDOMEvent(PassRefPtr<WebCore::Event> e);
    ~DeprecatedDOMEvent();

public:
    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMObject::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMObject::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result);
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result);
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result);
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result);
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void);
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void);
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg);

    // DeprecatedDOMEvent methods
    WebCore::Event* coreEvent() { return m_event.get(); }

protected:
    RefPtr<WebCore::Event> m_event;
};

class DeprecatedDOMUIEvent : public DeprecatedDOMEvent, public IDeprecatedDOMUIEvent
{
public:
    DeprecatedDOMUIEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMEvent(e) {}

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view( 
        /* [retval][out] */ IDeprecatedDOMWindow** result);
    
    virtual HRESULT STDMETHODCALLTYPE detail( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long detail);
    
    virtual HRESULT STDMETHODCALLTYPE keyCode( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE charCode( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE layerX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE layerY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE pageX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE pageY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE which( 
        /* [retval][out] */ long* result);
};

class DeprecatedDOMKeyboardEvent : public DeprecatedDOMUIEvent, public IDeprecatedDOMKeyboardEvent
{
public:
    DeprecatedDOMKeyboardEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMUIEvent(e) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMUIEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view( 
        /* [retval][out] */ IDeprecatedDOMWindow** result) { return DeprecatedDOMUIEvent::view(result); }
    
    virtual HRESULT STDMETHODCALLTYPE detail( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::detail(result); }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long detail) { return DeprecatedDOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail); }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::keyCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE charCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::charCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE which( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::which(result); }

    // IDeprecatedDOMKeyboardEvent
    virtual HRESULT STDMETHODCALLTYPE keyIdentifier( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE keyLocation( 
        /* [retval][out] */ unsigned long* result);
    
    virtual HRESULT STDMETHODCALLTYPE ctrlKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE shiftKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE altKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE metaKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE altGraphKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE getModifierState( 
        /* [in] */ BSTR keyIdentifierArg,
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE initKeyboardEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ BSTR keyIdentifier,
        /* [in] */ unsigned long keyLocation,
        /* [in] */ BOOL ctrlKey,
        /* [in] */ BOOL altKey,
        /* [in] */ BOOL shiftKey,
        /* [in] */ BOOL metaKey,
        /* [in] */ BOOL graphKey);
};

class DeprecatedDOMMouseEvent : public DeprecatedDOMUIEvent, public IDeprecatedDOMMouseEvent
{
public:
    DeprecatedDOMMouseEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMUIEvent(e) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMUIEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view( 
        /* [retval][out] */ IDeprecatedDOMWindow** result) { return DeprecatedDOMUIEvent::view(result); }
    
    virtual HRESULT STDMETHODCALLTYPE detail( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::detail(result); }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long detail) { return DeprecatedDOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail); }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::keyCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE charCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::charCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE which( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::which(result); }

    // IDeprecatedDOMMouseEvent
    virtual HRESULT STDMETHODCALLTYPE screenX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE screenY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE clientX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE clientY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE ctrlKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE shiftKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE altKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE metaKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE button( 
        /* [retval][out] */ unsigned short* result);
    
    virtual HRESULT STDMETHODCALLTYPE relatedTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result);
    
    virtual HRESULT STDMETHODCALLTYPE initMouseEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long detail,
        /* [in] */ long screenX,
        /* [in] */ long screenY,
        /* [in] */ long clientX,
        /* [in] */ long clientY,
        /* [in] */ BOOL ctrlKey,
        /* [in] */ BOOL altKey,
        /* [in] */ BOOL shiftKey,
        /* [in] */ BOOL metaKey,
        /* [in] */ unsigned short button,
        /* [in] */ IDeprecatedDOMEventTarget* relatedTarget);
    
    virtual HRESULT STDMETHODCALLTYPE offsetX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE x( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE y( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE fromElement( 
        /* [retval][out] */ IDeprecatedDOMNode** result);
    
    virtual HRESULT STDMETHODCALLTYPE toElement( 
        /* [retval][out] */ IDeprecatedDOMNode** result);
};

class DeprecatedDOMMutationEvent : public DeprecatedDOMEvent, public IDeprecatedDOMMutationEvent
{
public:
    DeprecatedDOMMutationEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMEvent(e) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMMutationEvent
    virtual HRESULT STDMETHODCALLTYPE relatedNode( 
        /* [retval][out] */ IDeprecatedDOMNode** result);
    
    virtual HRESULT STDMETHODCALLTYPE prevValue( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE newValue( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE attrName( 
        /* [retval][out] */ BSTR* result);
    
    virtual HRESULT STDMETHODCALLTYPE attrChange( 
        /* [retval][out] */ unsigned short* result);
    
    virtual HRESULT STDMETHODCALLTYPE initMutationEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMNode* relatedNode,
        /* [in] */ BSTR prevValue,
        /* [in] */ BSTR newValue,
        /* [in] */ BSTR attrName,
        /* [in] */ unsigned short attrChange);
};

class DeprecatedDOMOverflowEvent : public DeprecatedDOMEvent, public IDeprecatedDOMOverflowEvent
{
public:
    DeprecatedDOMOverflowEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMEvent(e) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMOverflowEvent
    virtual HRESULT STDMETHODCALLTYPE orient( 
        /* [retval][out] */ unsigned short* result);
    
    virtual HRESULT STDMETHODCALLTYPE horizontalOverflow( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE verticalOverflow( 
        /* [retval][out] */ BOOL* result);
};

class DeprecatedDOMWheelEvent : public DeprecatedDOMUIEvent, public IDeprecatedDOMWheelEvent
{
public:
    DeprecatedDOMWheelEvent(PassRefPtr<WebCore::Event> e) : DeprecatedDOMUIEvent(e) { }

    // IUnknown
    virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject);
    virtual ULONG STDMETHODCALLTYPE AddRef(void) { return DeprecatedDOMUIEvent::AddRef(); }
    virtual ULONG STDMETHODCALLTYPE Release(void) { return DeprecatedDOMUIEvent::Release(); }

    // IWebScriptObject
    virtual HRESULT STDMETHODCALLTYPE throwException( 
        /* [in] */ BSTR exceptionMessage,
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMObject::throwException(exceptionMessage, result); }
    
    virtual HRESULT STDMETHODCALLTYPE callWebScriptMethod( 
        /* [in] */ BSTR name,
        /* [size_is][in] */ const VARIANT args[  ],
        /* [in] */ int cArgs,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::callWebScriptMethod(name, args, cArgs, result); }
    
    virtual HRESULT STDMETHODCALLTYPE evaluateWebScript( 
        /* [in] */ BSTR script,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::evaluateWebScript(script, result); }
    
    virtual HRESULT STDMETHODCALLTYPE removeWebScriptKey( 
        /* [in] */ BSTR name) { return DeprecatedDOMObject::removeWebScriptKey(name); }
    
    virtual HRESULT STDMETHODCALLTYPE stringRepresentation( 
        /* [retval][out] */ BSTR* stringRepresentation) { return DeprecatedDOMObject::stringRepresentation(stringRepresentation); }
    
    virtual HRESULT STDMETHODCALLTYPE webScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [retval][out] */ VARIANT* result) { return DeprecatedDOMObject::webScriptValueAtIndex(index, result); }
    
    virtual HRESULT STDMETHODCALLTYPE setWebScriptValueAtIndex( 
        /* [in] */ unsigned int index,
        /* [in] */ VARIANT val) { return DeprecatedDOMObject::setWebScriptValueAtIndex(index, val); }
    
    virtual HRESULT STDMETHODCALLTYPE setException( 
        /* [in] */ BSTR description) { return DeprecatedDOMObject::setException(description); }

    // IDeprecatedDOMEvent
    virtual HRESULT STDMETHODCALLTYPE type( 
        /* [retval][out] */ BSTR* result) { return DeprecatedDOMEvent::type(result); }
    
    virtual HRESULT STDMETHODCALLTYPE target( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::target(result); }
    
    virtual HRESULT STDMETHODCALLTYPE currentTarget( 
        /* [retval][out] */ IDeprecatedDOMEventTarget** result) { return DeprecatedDOMEvent::currentTarget(result); }
    
    virtual HRESULT STDMETHODCALLTYPE eventPhase( 
        /* [retval][out] */ unsigned short* result) { return DeprecatedDOMEvent::eventPhase(result); }
    
    virtual HRESULT STDMETHODCALLTYPE bubbles( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::bubbles(result); }
    
    virtual HRESULT STDMETHODCALLTYPE cancelable( 
        /* [retval][out] */ BOOL* result) { return DeprecatedDOMEvent::cancelable(result); }
    
    virtual HRESULT STDMETHODCALLTYPE timeStamp( 
        /* [retval][out] */ DOMTimeStamp* result) { return DeprecatedDOMEvent::timeStamp(result); }
    
    virtual HRESULT STDMETHODCALLTYPE stopPropagation( void) { return DeprecatedDOMEvent::stopPropagation(); }
    
    virtual HRESULT STDMETHODCALLTYPE preventDefault( void) { return DeprecatedDOMEvent::preventDefault(); }
    
    virtual HRESULT STDMETHODCALLTYPE initEvent( 
        /* [in] */ BSTR eventTypeArg,
        /* [in] */ BOOL canBubbleArg,
        /* [in] */ BOOL cancelableArg) { return DeprecatedDOMEvent::initEvent(eventTypeArg, canBubbleArg, cancelableArg); }

    // IDeprecatedDOMUIEvent
    virtual HRESULT STDMETHODCALLTYPE view( 
        /* [retval][out] */ IDeprecatedDOMWindow** result) { return DeprecatedDOMUIEvent::view(result); }
    
    virtual HRESULT STDMETHODCALLTYPE detail( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::detail(result); }
    
    virtual HRESULT STDMETHODCALLTYPE initUIEvent( 
        /* [in] */ BSTR type,
        /* [in] */ BOOL canBubble,
        /* [in] */ BOOL cancelable,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long detail) { return DeprecatedDOMUIEvent::initUIEvent(type, canBubble, cancelable, view, detail); }
    
    virtual HRESULT STDMETHODCALLTYPE keyCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::keyCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE charCode( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::charCode(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE layerY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::layerY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageX( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageX(result); }
    
    virtual HRESULT STDMETHODCALLTYPE pageY( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::pageY(result); }
    
    virtual HRESULT STDMETHODCALLTYPE which( 
        /* [retval][out] */ long* result) { return DeprecatedDOMUIEvent::which(result); }

    // IDeprecatedDOMWheelEvent
    virtual HRESULT STDMETHODCALLTYPE screenX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE screenY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE clientX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE clientY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE ctrlKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE shiftKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE altKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE metaKey( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE wheelDelta( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE wheelDeltaX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE wheelDeltaY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetX( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE offsetY( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE x( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE y( 
        /* [retval][out] */ long* result);
    
    virtual HRESULT STDMETHODCALLTYPE isHorizontal( 
        /* [retval][out] */ BOOL* result);
    
    virtual HRESULT STDMETHODCALLTYPE initWheelEvent( 
        /* [in] */ long wheelDeltaX,
        /* [in] */ long wheelDeltaY,
        /* [in] */ IDeprecatedDOMWindow* view,
        /* [in] */ long screenX,
        /* [in] */ long screenY,
        /* [in] */ long clientX,
        /* [in] */ long clientY,
        /* [in] */ BOOL ctrlKey,
        /* [in] */ BOOL altKey,
        /* [in] */ BOOL shiftKey,
        /* [in] */ BOOL metaKey);
};

#endif
