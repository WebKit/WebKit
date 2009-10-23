/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSDOMWindowCustom.h"

#include "AtomicString.h"
#include "Base64.h"
#include "DOMWindow.h"
#include "Document.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "History.h"
#include "JSAudioConstructor.h"
#include "JSDOMWindowShell.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSEventSourceConstructor.h"
#include "JSHTMLCollection.h"
#include "JSHistory.h"
#include "JSImageConstructor.h"
#include "JSLocation.h"
#include "JSMessageChannelConstructor.h"
#include "JSMessagePort.h"
#include "JSMessagePortCustom.h"
#include "JSOptionConstructor.h"

#if ENABLE(SHARED_WORKERS)
#include "JSSharedWorkerConstructor.h"
#endif

#if ENABLE(3D_CANVAS)
#include "JSCanvasArrayBufferConstructor.h"
#include "JSCanvasByteArrayConstructor.h"
#include "JSCanvasUnsignedByteArrayConstructor.h"
#include "JSCanvasIntArrayConstructor.h"
#include "JSCanvasUnsignedIntArrayConstructor.h"
#include "JSCanvasShortArrayConstructor.h"
#include "JSCanvasUnsignedShortArrayConstructor.h"
#include "JSCanvasFloatArrayConstructor.h"
#endif
#include "JSWebKitCSSMatrixConstructor.h"
#include "JSWebKitPointConstructor.h"
#if ENABLE(WEB_SOCKETS)
#include "JSWebSocketConstructor.h"
#endif
#include "JSWorkerConstructor.h"
#include "JSXMLHttpRequestConstructor.h"
#include "JSXSLTProcessorConstructor.h"
#include "Location.h"
#include "MediaPlayer.h"
#include "MessagePort.h"
#include "NotificationCenter.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "RegisteredEventListener.h"
#include "ScheduledAction.h"
#include "ScriptController.h"
#include "SerializedScriptValue.h"
#include "Settings.h"
#include "SharedWorkerRepository.h"
#include "WindowFeatures.h"
#include <runtime/Error.h>
#include <runtime/JSFunction.h>
#include <runtime/JSObject.h>
#include <runtime/PrototypeFunction.h>

using namespace JSC;

namespace WebCore {

void JSDOMWindow::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);

    impl()->markEventListeners(markStack);

    JSGlobalData& globalData = *Heap::heap(this)->globalData();

    markDOMObjectWrapper(markStack, globalData, impl()->optionalConsole());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalHistory());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalLocationbar());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalMenubar());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalNavigator());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalPersonalbar());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalScreen());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalScrollbars());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalSelection());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalStatusbar());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalToolbar());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalLocation());
#if ENABLE(DOM_STORAGE)
    markDOMObjectWrapper(markStack, globalData, impl()->optionalSessionStorage());
    markDOMObjectWrapper(markStack, globalData, impl()->optionalLocalStorage());
#endif
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    markDOMObjectWrapper(markStack, globalData, impl()->optionalApplicationCache());
#endif
}

template<NativeFunction nativeFunction, int length>
JSValue nonCachingStaticFunctionGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot&)
{
    return new (exec) NativeFunctionWrapper(exec, exec->lexicalGlobalObject()->prototypeFunctionStructure(), length, propertyName, nativeFunction);
}

static JSValue childFrameGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindow*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(AtomicString(propertyName))->domWindow());
}

static JSValue indexGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    return toJS(exec, static_cast<JSDOMWindow*>(asObject(slot.slotBase()))->impl()->frame()->tree()->child(slot.index())->domWindow());
}

static JSValue namedItemGetter(ExecState* exec, const Identifier& propertyName, const PropertySlot& slot)
{
    JSDOMWindowBase* thisObj = static_cast<JSDOMWindow*>(asObject(slot.slotBase()));
    Document* document = thisObj->impl()->frame()->document();

    ASSERT(thisObj->allowsAccessFrom(exec));
    ASSERT(document);
    ASSERT(document->isHTMLDocument());

    RefPtr<HTMLCollection> collection = document->windowNamedItems(propertyName);
    if (collection->length() == 1)
        return toJS(exec, collection->firstItem());
    return toJS(exec, collection.get());
}

bool JSDOMWindow::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.

    const HashEntry* entry;

    // We don't want any properties other than "close" and "closed" on a closed window.
    if (!impl()->frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        entry = s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && !(entry->attributes() & Function) && entry->propertyGetter() == jsDOMWindowClosed) {
            slot.setCustom(this, entry->propertyGetter());
            return true;
        }
        entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && (entry->attributes() & Function) && entry->function() == jsDOMWindowPrototypeFunctionClose) {
            slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined();
        return true;
    }

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    String errorMessage;
    bool allowsAccess = allowsAccessFrom(exec, errorMessage);

    // Look for overrides before looking at any of our own properties, but ignore overrides completely
    // if this is cross-domain access.
    if (allowsAccess && JSGlobalObject::getOwnPropertySlot(exec, propertyName, slot))
        return true;

    // We need this code here because otherwise JSDOMWindowBase will stop the search before we even get to the
    // prototype due to the blanket same origin (allowsAccessFrom) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (entry->function() == jsDOMWindowPrototypeFunctionBlur) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionBlur, 0>);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionClose) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionFocus) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionFocus, 0>);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionPostMessage) {
                if (!allowsAccess) {
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionPostMessage, 2>);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionShowModalDialog) {
                if (!DOMWindow::canShowModalDialog(impl()->frame())) {
                    slot.setUndefined();
                    return true;
                }
            }
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.prototype.toString.
        if (propertyName == exec->propertyNames().toString) {
            if (!allowsAccess) {
                slot.setCustom(this, objectToStringFunctionGetter);
                return true;
            }
        }
    }

    entry = JSDOMWindow::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        slot.setCustom(this, entry->propertyGetter());
        return true;
    }

    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (impl()->frame()->tree()->child(propertyName)) {
        slot.setCustom(this, childFrameGetter);
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue proto = prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccess) {
                printErrorMessage(errorMessage);
                slot.setUndefined();
            }
            return true;
        }
    }

    // FIXME: Search the whole frame hierachy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    bool ok;
    unsigned i = propertyName.toArrayIndex(&ok);
    if (ok && i < impl()->frame()->tree()->childCount()) {
        slot.setCustomIndex(this, i, indexGetter);
        return true;
    }

    if (!allowsAccess) {
        printErrorMessage(errorMessage);
        slot.setUndefined();
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = impl()->frame()->document();
    if (document->isHTMLDocument()) {
        AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
        if (atomicPropertyName && (static_cast<HTMLDocument*>(document)->hasNamedItem(atomicPropertyName) || document->hasElementWithId(atomicPropertyName))) {
            slot.setCustom(this, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

bool JSDOMWindow::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
{
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.
    
    const HashEntry* entry;
    
    // We don't want any properties other than "close" and "closed" on a closed window.
    if (!impl()->frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        entry = s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && !(entry->attributes() & Function) && entry->propertyGetter() == jsDOMWindowClosed) {
            descriptor.setDescriptor(jsBoolean(true), ReadOnly | DontDelete | DontEnum);
            return true;
        }
        entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
        if (entry && (entry->attributes() & Function) && entry->function() == jsDOMWindowPrototypeFunctionClose) {
            PropertySlot slot;
            slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
            descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
            return true;
        }
        descriptor.setUndefined();
        return true;
    }

    String errorMessage;
    bool allowsAccess = allowsAccessFrom(exec, errorMessage);
    if (allowsAccess && JSGlobalObject::getOwnPropertyDescriptor(exec, propertyName, descriptor))
        return true;

    // We need this code here because otherwise JSDOMWindowBase will stop the search before we even get to the
    // prototype due to the blanket same origin (allowsAccessFrom) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    entry = JSDOMWindowPrototype::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        if (entry->attributes() & Function) {
            if (entry->function() == jsDOMWindowPrototypeFunctionBlur) {
                if (!allowsAccess) {
                    PropertySlot slot;
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionBlur, 0>);
                    descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionClose) {
                if (!allowsAccess) {
                    PropertySlot slot;
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
                    descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionFocus) {
                if (!allowsAccess) {
                    PropertySlot slot;
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionFocus, 0>);
                    descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionPostMessage) {
                if (!allowsAccess) {
                    PropertySlot slot;
                    slot.setCustom(this, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionPostMessage, 2>);
                    descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
                    return true;
                }
            } else if (entry->function() == jsDOMWindowPrototypeFunctionShowModalDialog) {
                if (!DOMWindow::canShowModalDialog(impl()->frame())) {
                    descriptor.setUndefined();
                    return true;
                }
            }
        }
    } else {
        // Allow access to toString() cross-domain, but always Object.prototype.toString.
        if (propertyName == exec->propertyNames().toString) {
            if (!allowsAccess) {
                PropertySlot slot;
                slot.setCustom(this, objectToStringFunctionGetter);
                descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
                return true;
            }
        }
    }
    
    entry = JSDOMWindow::s_info.propHashTable(exec)->entry(exec, propertyName);
    if (entry) {
        PropertySlot slot;
        slot.setCustom(this, entry->propertyGetter());
        descriptor.setDescriptor(slot.getValue(exec, propertyName), entry->attributes());
        return true;
    }
    
    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (impl()->frame()->tree()->child(propertyName)) {
        PropertySlot slot;
        slot.setCustom(this, childFrameGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
        return true;
    }
    
    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue proto = prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertyDescriptor(exec, propertyName, descriptor)) {
            if (!allowsAccess) {
                printErrorMessage(errorMessage);
                descriptor.setUndefined();
            }
            return true;
        }
    }

    bool ok;
    unsigned i = propertyName.toArrayIndex(&ok);
    if (ok && i < impl()->frame()->tree()->childCount()) {
        PropertySlot slot;
        slot.setCustomIndex(this, i, indexGetter);
        descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = impl()->frame()->document();
    if (document->isHTMLDocument()) {
        AtomicStringImpl* atomicPropertyName = AtomicString::find(propertyName);
        if (atomicPropertyName && (static_cast<HTMLDocument*>(document)->hasNamedItem(atomicPropertyName) || document->hasElementWithId(atomicPropertyName))) {
            PropertySlot slot;
            slot.setCustom(this, namedItemGetter);
            descriptor.setDescriptor(slot.getValue(exec, propertyName), ReadOnly | DontDelete | DontEnum);
            return true;
        }
    }
    
    return Base::getOwnPropertyDescriptor(exec, propertyName, descriptor);
}

void JSDOMWindow::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (!impl()->frame())
        return;

    // Optimization: access JavaScript global variables directly before involving the DOM.
    if (JSGlobalObject::hasOwnPropertyForWrite(exec, propertyName)) {
        if (allowsAccessFrom(exec))
            JSGlobalObject::put(exec, propertyName, value, slot);
        return;
    }

    if (lookupPut<JSDOMWindow>(exec, propertyName, value, s_info.propHashTable(exec), this))
        return;

    if (allowsAccessFrom(exec))
        Base::put(exec, propertyName, value, slot);
}

bool JSDOMWindow::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting properties by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

void JSDOMWindow::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // Only allow the window to enumerated by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;
    Base::getPropertyNames(exec, propertyNames);
}

void JSDOMWindow::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // Only allow the window to enumerated by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;
    Base::getOwnPropertyNames(exec, propertyNames);
}

bool JSDOMWindow::getPropertyAttributes(ExecState* exec, const Identifier& propertyName, unsigned& attributes) const
{
    // Only allow getting property attributes properties by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::getPropertyAttributes(exec, propertyName, attributes);
}

void JSDOMWindow::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunction, unsigned attributes)
{
    // Only allow defining getters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;

    // Don't allow shadowing location using defineGetter.
    if (propertyName == "location")
        return;

    Base::defineGetter(exec, propertyName, getterFunction, attributes);
}

void JSDOMWindow::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunction, unsigned attributes)
{
    // Only allow defining setters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return;
    Base::defineSetter(exec, propertyName, setterFunction, attributes);
}

bool JSDOMWindow::defineOwnProperty(JSC::ExecState* exec, const JSC::Identifier& propertyName, JSC::PropertyDescriptor& descriptor, bool shouldThrow)
{
    // Only allow defining properties in this way by frames in the same origin, as it allows setters to be introduced.
    if (!allowsAccessFrom(exec))
        return false;
    return Base::defineOwnProperty(exec, propertyName, descriptor, shouldThrow);
}

JSValue JSDOMWindow::lookupGetter(ExecState* exec, const Identifier& propertyName)
{
    // Only allow looking-up getters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return jsUndefined();
    return Base::lookupGetter(exec, propertyName);
}

JSValue JSDOMWindow::lookupSetter(ExecState* exec, const Identifier& propertyName)
{
    // Only allow looking-up setters by frames in the same origin.
    if (!allowsAccessFrom(exec))
        return jsUndefined();
    return Base::lookupSetter(exec, propertyName);
}

// Custom Attributes

JSValue JSDOMWindow::history(ExecState* exec) const
{
    History* history = impl()->history();
    if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), history))
        return wrapper;

    JSDOMWindow* window = const_cast<JSDOMWindow*>(this);
    JSHistory* jsHistory = new (exec) JSHistory(getDOMStructure<JSHistory>(exec, window), window, history);
    cacheDOMObjectWrapper(exec->globalData(), history, jsHistory);
    return jsHistory;
}

JSValue JSDOMWindow::location(ExecState* exec) const
{
    Location* location = impl()->location();
    if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), location))
        return wrapper;

    JSDOMWindow* window = const_cast<JSDOMWindow*>(this);
    JSLocation* jsLocation = new (exec) JSLocation(getDOMStructure<JSLocation>(exec, window), window, location);
    cacheDOMObjectWrapper(exec->globalData(), location, jsLocation);
    return jsLocation;
}

void JSDOMWindow::setLocation(ExecState* exec, JSValue value)
{
    Frame* lexicalFrame = toLexicalFrame(exec);
    if (!lexicalFrame)
        return;

#if ENABLE(DASHBOARD_SUPPORT)
    // To avoid breaking old widgets, make "var location =" in a top-level frame create
    // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
    if (Settings* settings = lexicalFrame->settings()) {
        if (settings->usesDashboardBackwardCompatibilityMode() && !lexicalFrame->tree()->parent()) {
            if (allowsAccessFrom(exec))
                putDirect(Identifier(exec, "location"), value);
            return;
        }
    }
#endif

    Frame* frame = impl()->frame();
    ASSERT(frame);

    KURL url = completeURL(exec, value.toString(exec));
    if (url.isNull())
        return;

    if (!shouldAllowNavigation(exec, frame))
        return;

    if (!protocolIsJavaScript(url) || allowsAccessFrom(exec)) {
        // We want a new history item if this JS was called via a user gesture
        frame->redirectScheduler()->scheduleLocationChange(url, lexicalFrame->loader()->outgoingReferrer(), !lexicalFrame->script()->anyPageIsProcessingUserGesture(), false, processingUserGesture(exec));
    }
}

JSValue JSDOMWindow::crypto(ExecState*) const
{
    return jsUndefined();
}

JSValue JSDOMWindow::event(ExecState* exec) const
{
    Event* event = currentEvent();
    if (!event)
        return jsUndefined();
    return toJS(exec, event);
}

#if ENABLE(EVENTSOURCE)
JSValue JSDOMWindow::eventSource(ExecState* exec) const
{
    return getDOMConstructor<JSEventSourceConstructor>(exec, this);
}
#endif

JSValue JSDOMWindow::image(ExecState* exec) const
{
    return getDOMConstructor<JSImageConstructor>(exec, this);
}

JSValue JSDOMWindow::option(ExecState* exec) const
{
    return getDOMConstructor<JSOptionConstructor>(exec, this);
}

#if ENABLE(VIDEO)
JSValue JSDOMWindow::audio(ExecState* exec) const
{
    if (!MediaPlayer::isAvailable())
        return jsUndefined();
    return getDOMConstructor<JSAudioConstructor>(exec, this);
}
#endif

JSValue JSDOMWindow::webKitPoint(ExecState* exec) const
{
    return getDOMConstructor<JSWebKitPointConstructor>(exec, this);
}

JSValue JSDOMWindow::webKitCSSMatrix(ExecState* exec) const
{
    return getDOMConstructor<JSWebKitCSSMatrixConstructor>(exec, this);
}
 
#if ENABLE(3D_CANVAS)
JSValue JSDOMWindow::canvasArrayBuffer(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasArrayBufferConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasByteArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasByteArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasUnsignedByteArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasUnsignedByteArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasIntArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasIntArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasUnsignedIntArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasUnsignedIntArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasShortArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasShortArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasUnsignedShortArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasUnsignedShortArrayConstructor>(exec, this);
}
 
JSValue JSDOMWindow::canvasFloatArray(ExecState* exec) const
{
    return getDOMConstructor<JSCanvasFloatArrayConstructor>(exec, this);
}
#endif
 
JSValue JSDOMWindow::xmlHttpRequest(ExecState* exec) const
{
    return getDOMConstructor<JSXMLHttpRequestConstructor>(exec, this);
}

#if ENABLE(XSLT)
JSValue JSDOMWindow::xsltProcessor(ExecState* exec) const
{
    return getDOMConstructor<JSXSLTProcessorConstructor>(exec, this);
}
#endif

#if ENABLE(CHANNEL_MESSAGING)
JSValue JSDOMWindow::messageChannel(ExecState* exec) const
{
    return getDOMConstructor<JSMessageChannelConstructor>(exec, this);
}
#endif

#if ENABLE(WORKERS)
JSValue JSDOMWindow::worker(ExecState* exec) const
{
    return getDOMConstructor<JSWorkerConstructor>(exec, this);
}
#endif

#if ENABLE(SHARED_WORKERS)
JSValue JSDOMWindow::sharedWorker(ExecState* exec) const
{
    if (SharedWorkerRepository::isAvailable())
        return getDOMConstructor<JSSharedWorkerConstructor>(exec, this);
    return jsUndefined();
}
#endif

#if ENABLE(WEB_SOCKETS)
JSValue JSDOMWindow::webSocket(ExecState* exec) const
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    Settings* settings = frame->settings();
    if (!settings)
        return jsUndefined();
    return getDOMConstructor<JSWebSocketConstructor>(exec, this);
}
#endif

// Custom functions

// Helper for window.open() and window.showModalDialog()
static Frame* createWindow(ExecState* exec, Frame* lexicalFrame, Frame* dynamicFrame,
                           Frame* openerFrame, const String& url, const String& frameName, 
                           const WindowFeatures& windowFeatures, JSValue dialogArgs)
{
    ASSERT(lexicalFrame);
    ASSERT(dynamicFrame);

    ResourceRequest request;

    // For whatever reason, Firefox uses the dynamicGlobalObject to determine
    // the outgoingReferrer.  We replicate that behavior here.
    String referrer = dynamicFrame->loader()->outgoingReferrer();
    request.setHTTPReferrer(referrer);
    FrameLoader::addHTTPOriginIfNeeded(request, dynamicFrame->loader()->outgoingOrigin());
    FrameLoadRequest frameRequest(request, frameName);

    // FIXME: It's much better for client API if a new window starts with a URL, here where we
    // know what URL we are going to open. Unfortunately, this code passes the empty string
    // for the URL, but there's a reason for that. Before loading we have to set up the opener,
    // openedByDOM, and dialogArguments values. Also, to decide whether to use the URL we currently
    // do an allowsAccessFrom call using the window we create, which can't be done before creating it.
    // We'd have to resolve all those issues to pass the URL instead of "".

    bool created;
    // We pass in the opener frame here so it can be used for looking up the frame name, in case the active frame
    // is different from the opener frame, and the name references a frame relative to the opener frame, for example
    // "_self" or "_parent".
    Frame* newFrame = lexicalFrame->loader()->createWindow(openerFrame->loader(), frameRequest, windowFeatures, created);
    if (!newFrame)
        return 0;

    newFrame->loader()->setOpener(openerFrame);
    newFrame->page()->setOpenedByDOM();

    // FIXME: If a window is created from an isolated world, what are the consequences of this? 'dialogArguments' only appears back in the normal world?
    JSDOMWindow* newWindow = toJSDOMWindow(newFrame, normalWorld(exec->globalData()));

    if (dialogArgs)
        newWindow->putDirect(Identifier(exec, "dialogArguments"), dialogArgs);

    if (!protocolIsJavaScript(url) || newWindow->allowsAccessFrom(exec)) {
        KURL completedURL = url.isEmpty() ? KURL(ParsedURLString, "") : completeURL(exec, url);
        bool userGesture = processingUserGesture(exec);

        if (created)
            newFrame->loader()->changeLocation(completedURL, referrer, false, false, userGesture);
        else if (!url.isEmpty())
            newFrame->redirectScheduler()->scheduleLocationChange(completedURL.string(), referrer, !lexicalFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
    }

    return newFrame;
}

JSValue JSDOMWindow::open(ExecState* exec, const ArgList& args)
{
    String urlString = valueToStringWithUndefinedOrNullCheck(exec, args.at(0));
    AtomicString frameName = args.at(1).isUndefinedOrNull() ? "_blank" : AtomicString(args.at(1).toString(exec));
    WindowFeatures windowFeatures(valueToStringWithUndefinedOrNullCheck(exec, args.at(2)));

    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* lexicalFrame = toLexicalFrame(exec);
    if (!lexicalFrame)
        return jsUndefined();
    Frame* dynamicFrame = toDynamicFrame(exec);
    if (!dynamicFrame)
        return jsUndefined();

    Page* page = frame->page();

    // Because FrameTree::find() returns true for empty strings, we must check for empty framenames.
    // Otherwise, illegitimate window.open() calls with no name will pass right through the popup blocker.
    if (!DOMWindow::allowPopUp(dynamicFrame) && (frameName.isEmpty() || !frame->tree()->find(frameName)))
        return jsUndefined();

    // Get the target frame for the special cases of _top and _parent.  In those
    // cases, we can schedule a location change right now and return early.
    bool topOrParent = false;
    if (frameName == "_top") {
        frame = frame->tree()->top();
        topOrParent = true;
    } else if (frameName == "_parent") {
        if (Frame* parent = frame->tree()->parent())
            frame = parent;
        topOrParent = true;
    }
    if (topOrParent) {
        String completedURL;
        if (!urlString.isEmpty())
            completedURL = completeURL(exec, urlString).string();

        if (!shouldAllowNavigation(exec, frame))
            return jsUndefined();

        const JSDOMWindow* targetedWindow = toJSDOMWindow(frame, currentWorld(exec));
        if (!completedURL.isEmpty() && (!protocolIsJavaScript(completedURL) || (targetedWindow && targetedWindow->allowsAccessFrom(exec)))) {
            bool userGesture = processingUserGesture(exec);

            // For whatever reason, Firefox uses the dynamicGlobalObject to
            // determine the outgoingReferrer.  We replicate that behavior
            // here.
            String referrer = dynamicFrame->loader()->outgoingReferrer();

            frame->redirectScheduler()->scheduleLocationChange(completedURL, referrer, !lexicalFrame->script()->anyPageIsProcessingUserGesture(), false, userGesture);
        }
        return toJS(exec, frame->domWindow());
    }

    // In the case of a named frame or a new window, we'll use the createWindow() helper
    FloatRect windowRect(windowFeatures.xSet ? windowFeatures.x : 0, windowFeatures.ySet ? windowFeatures.y : 0,
                         windowFeatures.widthSet ? windowFeatures.width : 0, windowFeatures.heightSet ? windowFeatures.height : 0);
    DOMWindow::adjustWindowRect(screenAvailableRect(page ? page->mainFrame()->view() : 0), windowRect, windowRect);

    windowFeatures.x = windowRect.x();
    windowFeatures.y = windowRect.y();
    windowFeatures.height = windowRect.height();
    windowFeatures.width = windowRect.width();

    frame = createWindow(exec, lexicalFrame, dynamicFrame, frame, urlString, frameName, windowFeatures, JSValue());

    if (!frame)
        return jsUndefined();

    return toJS(exec, frame->domWindow());
}

JSValue JSDOMWindow::showModalDialog(ExecState* exec, const ArgList& args)
{
    String url = valueToStringWithUndefinedOrNullCheck(exec, args.at(0));
    JSValue dialogArgs = args.at(1);
    String featureArgs = valueToStringWithUndefinedOrNullCheck(exec, args.at(2));

    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();
    Frame* lexicalFrame = toLexicalFrame(exec);
    if (!lexicalFrame)
        return jsUndefined();
    Frame* dynamicFrame = toDynamicFrame(exec);
    if (!dynamicFrame)
        return jsUndefined();

    if (!DOMWindow::canShowModalDialogNow(frame) || !DOMWindow::allowPopUp(dynamicFrame))
        return jsUndefined();

    HashMap<String, String> features;
    DOMWindow::parseModalDialogFeatures(featureArgs, features);

    const bool trusted = false;

    // The following features from Microsoft's documentation are not implemented:
    // - default font settings
    // - width, height, left, and top specified in units other than "px"
    // - edge (sunken or raised, default is raised)
    // - dialogHide: trusted && boolFeature(features, "dialoghide"), makes dialog hide when you print
    // - help: boolFeature(features, "help", true), makes help icon appear in dialog (what does it do on Windows?)
    // - unadorned: trusted && boolFeature(features, "unadorned");

    FloatRect screenRect = screenAvailableRect(frame->view());

    WindowFeatures wargs;
    wargs.width = WindowFeatures::floatFeature(features, "dialogwidth", 100, screenRect.width(), 620); // default here came from frame size of dialog in MacIE
    wargs.widthSet = true;
    wargs.height = WindowFeatures::floatFeature(features, "dialogheight", 100, screenRect.height(), 450); // default here came from frame size of dialog in MacIE
    wargs.heightSet = true;

    wargs.x = WindowFeatures::floatFeature(features, "dialogleft", screenRect.x(), screenRect.right() - wargs.width, -1);
    wargs.xSet = wargs.x > 0;
    wargs.y = WindowFeatures::floatFeature(features, "dialogtop", screenRect.y(), screenRect.bottom() - wargs.height, -1);
    wargs.ySet = wargs.y > 0;

    if (WindowFeatures::boolFeature(features, "center", true)) {
        if (!wargs.xSet) {
            wargs.x = screenRect.x() + (screenRect.width() - wargs.width) / 2;
            wargs.xSet = true;
        }
        if (!wargs.ySet) {
            wargs.y = screenRect.y() + (screenRect.height() - wargs.height) / 2;
            wargs.ySet = true;
        }
    }

    wargs.dialog = true;
    wargs.resizable = WindowFeatures::boolFeature(features, "resizable");
    wargs.scrollbarsVisible = WindowFeatures::boolFeature(features, "scroll", true);
    wargs.statusBarVisible = WindowFeatures::boolFeature(features, "status", !trusted);
    wargs.menuBarVisible = false;
    wargs.toolBarVisible = false;
    wargs.locationBarVisible = false;
    wargs.fullscreen = false;

    Frame* dialogFrame = createWindow(exec, lexicalFrame, dynamicFrame, frame, url, "", wargs, dialogArgs);
    if (!dialogFrame)
        return jsUndefined();

    JSDOMWindow* dialogWindow = toJSDOMWindow(dialogFrame, currentWorld(exec));
    dialogFrame->page()->chrome()->runModal();

    Identifier returnValue(exec, "returnValue");
    if (dialogWindow->allowsAccessFromNoErrorMessage(exec)) {
        PropertySlot slot;
        // This is safe, we have already performed the origin security check and we are
        // not interested in any of the DOM properties of the window.
        if (dialogWindow->JSGlobalObject::getOwnPropertySlot(exec, returnValue, slot))
            return slot.getValue(exec, returnValue);
    }
    return jsUndefined();
}

JSValue JSDOMWindow::postMessage(ExecState* exec, const ArgList& args)
{
    DOMWindow* window = impl();

    DOMWindow* source = asJSDOMWindow(exec->lexicalGlobalObject())->impl();
    PassRefPtr<SerializedScriptValue> message = SerializedScriptValue::create(exec, args.at(0));

    if (exec->hadException())
        return jsUndefined();

    MessagePortArray messagePorts;
    if (args.size() > 2)
        fillMessagePortArray(exec, args.at(1), messagePorts);
    if (exec->hadException())
        return jsUndefined();

    String targetOrigin = valueToStringWithUndefinedOrNullCheck(exec, args.at((args.size() == 2) ? 1 : 2));
    if (exec->hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    window->postMessage(message, &messagePorts, targetOrigin, source, ec);
    setDOMException(exec, ec);

    return jsUndefined();
}

JSValue JSDOMWindow::setTimeout(ExecState* exec, const ArgList& args)
{
    ScheduledAction* action = ScheduledAction::create(exec, args, currentWorld(exec));
    if (exec->hadException())
        return jsUndefined();
    int delay = args.at(1).toInt32(exec);
    return jsNumber(exec, impl()->setTimeout(action, delay));
}

JSValue JSDOMWindow::setInterval(ExecState* exec, const ArgList& args)
{
    ScheduledAction* action = ScheduledAction::create(exec, args, currentWorld(exec));
    if (exec->hadException())
        return jsUndefined();
    int delay = args.at(1).toInt32(exec);
    return jsNumber(exec, impl()->setInterval(action, delay));
}

JSValue JSDOMWindow::atob(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue v = args.at(0);
    if (v.isNull())
        return jsEmptyString(exec);

    UString s = v.toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    if (!base64Decode(in, out))
        return throwError(exec, GeneralError, "Cannot decode base64");

    return jsString(exec, String(out.data(), out.size()));
}

JSValue JSDOMWindow::btoa(ExecState* exec, const ArgList& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError, "Not enough arguments");

    JSValue v = args.at(0);
    if (v.isNull())
        return jsEmptyString(exec);

    UString s = v.toString(exec);
    if (!s.is8Bit()) {
        setDOMException(exec, INVALID_CHARACTER_ERR);
        return jsUndefined();
    }

    Vector<char> in(s.size());
    for (int i = 0; i < s.size(); ++i)
        in[i] = static_cast<char>(s.data()[i]);
    Vector<char> out;

    base64Encode(in, out);

    return jsString(exec, String(out.data(), out.size()));
}

JSValue JSDOMWindow::addEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue listener = args.at(1);
    if (!listener.isObject())
        return jsUndefined();

    impl()->addEventListener(args.at(0).toString(exec), JSEventListener::create(asObject(listener), false, currentWorld(exec)), args.at(2).toBoolean(exec));
    return jsUndefined();
}

JSValue JSDOMWindow::removeEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->frame();
    if (!frame)
        return jsUndefined();

    JSValue listener = args.at(1);
    if (!listener.isObject())
        return jsUndefined();

    impl()->removeEventListener(args.at(0).toString(exec), JSEventListener::create(asObject(listener), false, currentWorld(exec)).get(), args.at(2).toBoolean(exec));
    return jsUndefined();
}

DOMWindow* toDOMWindow(JSValue value)
{
    if (!value.isObject())
        return 0;
    JSObject* object = asObject(value);
    if (object->inherits(&JSDOMWindow::s_info))
        return static_cast<JSDOMWindow*>(object)->impl();
    if (object->inherits(&JSDOMWindowShell::s_info))
        return static_cast<JSDOMWindowShell*>(object)->impl();
    return 0;
}

} // namespace WebCore
