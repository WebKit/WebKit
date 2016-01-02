/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "DOMWindowIndexedDatabase.h"
#include "Frame.h"
#include "HTMLCollection.h"
#include "HTMLDocument.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHTMLAudioElement.h"
#include "JSHTMLCollection.h"
#include "JSHTMLOptionElement.h"
#include "JSIDBFactory.h"
#include "JSImageConstructor.h"
#include "JSMessagePortCustom.h"
#include "JSWorker.h"
#include "Location.h"
#include "RuntimeEnabledFeatures.h"
#include "ScheduledAction.h"
#include "Settings.h"

#if ENABLE(IOS_TOUCH_EVENTS)
#include "JSTouchConstructorIOS.h"
#include "JSTouchListConstructorIOS.h"
#endif

#if ENABLE(WEB_AUDIO)
#include "JSAudioContext.h"
#endif

#if ENABLE(WEB_SOCKETS)
#include "JSWebSocket.h"
#endif

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "JSWebKitNamespace.h"
#endif

using namespace JSC;

namespace WebCore {

void JSDOMWindow::visitAdditionalChildren(SlotVisitor& visitor)
{
    if (Frame* frame = wrapped().frame())
        visitor.addOpaqueRoot(frame);
}

static EncodedJSValue childFrameGetter(ExecState* exec, JSObject* slotBase, EncodedJSValue, PropertyName propertyName)
{
    return JSValue::encode(toJS(exec, jsCast<JSDOMWindow*>(slotBase)->wrapped().frame()->tree().scopedChild(propertyNameToAtomicString(propertyName))->document()->domWindow()));
}

static EncodedJSValue namedItemGetter(ExecState* exec, JSObject* slotBase, EncodedJSValue, PropertyName propertyName)
{
    JSDOMWindowBase* thisObj = jsCast<JSDOMWindow*>(slotBase);
    Document* document = thisObj->wrapped().frame()->document();

    ASSERT(BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObj->wrapped()));
    ASSERT(is<HTMLDocument>(document));

    AtomicStringImpl* atomicPropertyName = propertyName.publicName();
    if (!atomicPropertyName || !downcast<HTMLDocument>(*document).hasWindowNamedItem(*atomicPropertyName))
        return JSValue::encode(jsUndefined());

    if (UNLIKELY(downcast<HTMLDocument>(*document).windowNamedItemContainsMultipleElements(*atomicPropertyName))) {
        Ref<HTMLCollection> collection = document->windowNamedItems(atomicPropertyName);
        ASSERT(collection->length() > 1);
        return JSValue::encode(toJS(exec, thisObj->globalObject(), WTF::getPtr(collection)));
    }

    return JSValue::encode(toJS(exec, thisObj->globalObject(), downcast<HTMLDocument>(*document).windowNamedItem(*atomicPropertyName)));
}

#if ENABLE(USER_MESSAGE_HANDLERS)
static EncodedJSValue jsDOMWindowWebKit(ExecState* exec, JSObject*, EncodedJSValue thisValue, PropertyName)
{
    JSDOMWindow* castedThis = toJSDOMWindow(JSValue::decode(thisValue));
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, castedThis->wrapped()))
        return JSValue::encode(jsUndefined());
    return JSValue::encode(toJS(exec, castedThis->globalObject(), castedThis->wrapped().webkitNamespace()));
}
#endif

#if ENABLE(INDEXED_DATABASE)
static EncodedJSValue jsDOMWindowIndexedDB(ExecState* exec, JSObject* slotBase, EncodedJSValue thisValue, PropertyName)
{
    UNUSED_PARAM(exec);
    UNUSED_PARAM(slotBase);
    UNUSED_PARAM(thisValue);
    auto* castedThis = toJSDOMWindow(JSValue::decode(thisValue));
    if (!RuntimeEnabledFeatures::sharedFeatures().indexedDBEnabled())
        return JSValue::encode(jsUndefined());
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, castedThis->wrapped()))
        return JSValue::encode(jsUndefined());
    auto& impl = castedThis->wrapped();
    JSValue result = toJS(exec, castedThis->globalObject(), WTF::getPtr(DOMWindowIndexedDatabase::indexedDB(&impl)));
    return JSValue::encode(result);
}
#endif

bool JSDOMWindow::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // When accessing a Window cross-domain, functions are always the native built-in ones, and they
    // are not affected by properties changed on the Window or anything in its prototype chain.
    // This is consistent with the behavior of Firefox.

    // We don't want any properties other than "close" and "closed" on a frameless window (i.e. one whose page got closed,
    // or whose iframe got removed).
    // FIXME: This doesn't fully match Firefox, which allows at least toString in addition to those.
    if (!thisObject->wrapped().frame()) {
        // The following code is safe for cross-domain and same domain use.
        // It ignores any custom properties that might be set on the DOMWindow (including a custom prototype).
        if (propertyName == exec->propertyNames().closed) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, jsDOMWindowClosed);
            return true;
        }
        if (propertyName == exec->propertyNames().close) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined();
        return true;
    }

    slot.setWatchpointSet(thisObject->m_windowCloseWatchpoints);

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    String errorMessage;
    bool allowsAccess = shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), errorMessage);
    
    // Look for overrides before looking at any of our own properties, but ignore overrides completely
    // if this is cross-domain access.
    if (allowsAccess && JSGlobalObject::getOwnPropertySlot(thisObject, exec, propertyName, slot))
        return true;
    
    // We need this code here because otherwise JSDOMWindowBase will stop the search before we even get to the
    // prototype due to the blanket same origin (shouldAllowAccessToDOMWindow) check at the end of getOwnPropertySlot.
    // Also, it's important to get the implementation straight out of the DOMWindow prototype regardless of
    // what prototype is actually set on this object.
    if (propertyName == exec->propertyNames().blur) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionBlur, 0>);
        return true;
    } else if (propertyName == exec->propertyNames().close) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionClose, 0>);
        return true;
    } else if (propertyName == exec->propertyNames().focus) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionFocus, 0>);
        return true;
    } else if (propertyName == exec->propertyNames().postMessage) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowPrototypeFunctionPostMessage, 2>);
        return true;
    } else if (propertyName == exec->propertyNames().showModalDialog) {
        if (!DOMWindow::canShowModalDialog(thisObject->wrapped().frame())) {
            slot.setUndefined();
            return true;
        }
    } else if (propertyName == exec->propertyNames().toString) {
        // Allow access to toString() cross-domain, but always Object.prototype.toString.
        if (!allowsAccess) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, objectToStringFunctionGetter);
            return true;
        }
    }

#if ENABLE(INDEXED_DATABASE)
    // FIXME: With generated JS bindings built on static property tables there is no way to
    // completely remove a generated property at runtime.
    // So to completely disable IndexedDB at runtime we have to not generate these accessors
    // and have to handle them specially here.
    // Once https://webkit.org/b/145669 is resolved, they can once again be auto generated.
    if (RuntimeEnabledFeatures::sharedFeatures().indexedDBEnabled() && (propertyName == exec->propertyNames().indexedDB || propertyName == exec->propertyNames().webkitIndexedDB)) {
        slot.setCustom(thisObject, allowsAccess ? DontDelete | ReadOnly | CustomAccessor : ReadOnly | DontDelete | DontEnum, jsDOMWindowIndexedDB);
        return true;
    }
#endif

    const HashTableValue* entry = JSDOMWindow::info()->staticPropHashTable->entry(propertyName);
    if (entry) {
        slot.setCacheableCustom(thisObject, allowsAccess ? entry->attributes() : ReadOnly | DontDelete | DontEnum, entry->propertyGetter());
        return true;
    }

#if ENABLE(USER_MESSAGE_HANDLERS)
    if (propertyName == exec->propertyNames().webkit && thisObject->wrapped().shouldHaveWebKitNamespaceForWorld(thisObject->world())) {
        slot.setCacheableCustom(thisObject, allowsAccess ? DontDelete | ReadOnly : ReadOnly | DontDelete | DontEnum, jsDOMWindowWebKit);
        return true;
    }
#endif

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue proto = thisObject->prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertySlot(exec, propertyName, slot)) {
            if (!allowsAccess) {
                thisObject->printErrorMessage(errorMessage);
                slot.setUndefined();
            }
            return true;
        }
    }

    // After this point it is no longer valid to cache any results because of
    // the impure nature of the property accesses which follow. We can move this 
    // statement further down when we add ways to mitigate these impurities with, 
    // for example, watchpoints.
    slot.disableCaching();

    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (thisObject->wrapped().frame()->tree().scopedChild(propertyNameToAtomicString(propertyName))) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, childFrameGetter);
        return true;
    }

    // FIXME: Search the whole frame hierarchy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    Optional<uint32_t> index = parseIndex(propertyName);
    if (index && index.value() < thisObject->wrapped().frame()->tree().scopedChildCount()) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum,
            toJS(exec, thisObject->wrapped().frame()->tree().scopedChild(index.value())->document()->domWindow()));
        return true;
    }

    if (!allowsAccess) {
        thisObject->printErrorMessage(errorMessage);
        slot.setUndefined();
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = thisObject->wrapped().frame()->document();
    if (is<HTMLDocument>(*document)) {
        AtomicStringImpl* atomicPropertyName = propertyName.publicName();
        if (atomicPropertyName && downcast<HTMLDocument>(*document).hasWindowNamedItem(*atomicPropertyName)) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlot(thisObject, exec, propertyName, slot);
}

bool JSDOMWindow::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned index, PropertySlot& slot)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    
    if (!thisObject->wrapped().frame()) {
        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined();
        return true;
    }

    // We need to check for cross-domain access here without printing the generic warning message
    // because we always allow access to some function, just different ones depending whether access
    // is allowed.
    String errorMessage;
    bool allowsAccess = shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), errorMessage);

    // Look for overrides before looking at any of our own properties, but ignore overrides completely
    // if this is cross-domain access.
    if (allowsAccess && JSGlobalObject::getOwnPropertySlotByIndex(thisObject, exec, index, slot))
        return true;
    
    Identifier propertyName = Identifier::from(exec, index);
    
    // Check for child frames by name before built-in properties to
    // match Mozilla. This does not match IE, but some sites end up
    // naming frames things that conflict with window properties that
    // are in Moz but not IE. Since we have some of these, we have to do
    // it the Moz way.
    if (thisObject->wrapped().frame()->tree().scopedChild(propertyNameToAtomicString(propertyName))) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, childFrameGetter);
        return true;
    }
    
    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.  
    JSValue proto = thisObject->prototype();
    if (proto.isObject()) {
        if (asObject(proto)->getPropertySlot(exec, index, slot)) {
            if (!allowsAccess) {
                thisObject->printErrorMessage(errorMessage);
                slot.setUndefined();
            }
            return true;
        }
    }

    // FIXME: Search the whole frame hierarchy somewhere around here.
    // We need to test the correct priority order.

    // allow window[1] or parent[1] etc. (#56983)
    if (index < thisObject->wrapped().frame()->tree().scopedChildCount()) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum,
            toJS(exec, thisObject->wrapped().frame()->tree().scopedChild(index)->document()->domWindow()));
        return true;
    }

    if (!allowsAccess) {
        thisObject->printErrorMessage(errorMessage);
        slot.setUndefined();
        return true;
    }

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = thisObject->wrapped().frame()->document();
    if (is<HTMLDocument>(*document)) {
        // This propertyName is generated by Identifier::from.
        // This function generates Identifier from String and always returns the non-symbol Identifier.
        ASSERT(!propertyName.isSymbol());
        auto atomicPropertyName = static_cast<AtomicStringImpl*>(propertyName.impl());
        if (atomicPropertyName && downcast<HTMLDocument>(*document).hasWindowNamedItem(*atomicPropertyName)) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, namedItemGetter);
            return true;
        }
    }

    return Base::getOwnPropertySlotByIndex(thisObject, exec, index, slot);
}

void JSDOMWindow::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(cell);
    if (!thisObject->wrapped().frame())
        return;

    // Optimization: access JavaScript global variables directly before involving the DOM.
    if (thisObject->JSGlobalObject::hasOwnPropertyForWrite(exec, propertyName)) {
        if (BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
            JSGlobalObject::put(thisObject, exec, propertyName, value, slot);
        return;
    }

    if (lookupPut(exec, propertyName, thisObject, value, *s_info.staticPropHashTable, slot))
        return;

    if (BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        Base::put(thisObject, exec, propertyName, value, slot);
}

void JSDOMWindow::putByIndex(JSCell* cell, ExecState* exec, unsigned index, JSValue value, bool shouldThrow)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(cell);
    if (!thisObject->wrapped().frame())
        return;
    
    Identifier propertyName = Identifier::from(exec, index);

    // Optimization: access JavaScript global variables directly before involving the DOM.
    if (thisObject->JSGlobalObject::hasOwnPropertyForWrite(exec, propertyName)) {
        if (BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
            JSGlobalObject::putByIndex(thisObject, exec, index, value, shouldThrow);
        return;
    }
    
    if (BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        Base::putByIndex(thisObject, exec, index, value, shouldThrow);
}

bool JSDOMWindow::deleteProperty(JSCell* cell, ExecState* exec, PropertyName propertyName)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(cell);
    // Only allow deleting properties by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return false;
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSDOMWindow::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(cell);
    // Only allow deleting properties by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return false;
    return Base::deletePropertyByIndex(thisObject, exec, propertyName);
}

uint32_t JSDOMWindow::getEnumerableLength(ExecState* exec, JSObject* object)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow the window to enumerated by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return 0;
    return Base::getEnumerableLength(exec, thisObject);
}

void JSDOMWindow::getStructurePropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow the window to enumerated by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return;
    Base::getStructurePropertyNames(thisObject, exec, propertyNames, mode);
}

void JSDOMWindow::getGenericPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow the window to enumerated by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return;
    Base::getGenericPropertyNames(thisObject, exec, propertyNames, mode);
}

void JSDOMWindow::getPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow the window to enumerated by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return;
    Base::getPropertyNames(thisObject, exec, propertyNames, mode);
}

void JSDOMWindow::getOwnPropertyNames(JSObject* object, ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow the window to enumerated by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return;
    Base::getOwnPropertyNames(thisObject, exec, propertyNames, mode);
}

bool JSDOMWindow::defineOwnProperty(JSC::JSObject* object, JSC::ExecState* exec, JSC::PropertyName propertyName, const JSC::PropertyDescriptor& descriptor, bool shouldThrow)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow defining properties in this way by frames in the same origin, as it allows setters to be introduced.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return false;

    // Don't allow shadowing location using accessor properties.
    if (descriptor.isAccessorDescriptor() && propertyName == Identifier::fromString(exec, "location"))
        return false;

    return Base::defineOwnProperty(thisObject, exec, propertyName, descriptor, shouldThrow);
}

// Custom Attributes

void JSDOMWindow::setLocation(ExecState& state, JSValue value)
{
#if ENABLE(DASHBOARD_SUPPORT)
    // To avoid breaking old widgets, make "var location =" in a top-level frame create
    // a property named "location" instead of performing a navigation (<rdar://problem/5688039>).
    if (Frame* activeFrame = activeDOMWindow(&state).frame()) {
        if (activeFrame->settings().usesDashboardBackwardCompatibilityMode() && !activeFrame->tree().parent()) {
            if (BindingSecurity::shouldAllowAccessToDOMWindow(&state, wrapped()))
                putDirect(state.vm(), Identifier::fromString(&state, "location"), value);
            return;
        }
    }
#endif

    String locationString = value.toString(&state)->value(&state);
    if (state.hadException())
        return;

    if (Location* location = wrapped().location())
        location->setHref(activeDOMWindow(&state), firstDOMWindow(&state), locationString);
}

JSValue JSDOMWindow::event(ExecState& state) const
{
    Event* event = currentEvent();
    if (!event)
        return jsUndefined();
    return toJS(&state, const_cast<JSDOMWindow*>(this), event);
}

JSValue JSDOMWindow::image(ExecState& state) const
{
    return createImageConstructor(state.vm(), *this);
}

#if ENABLE(IOS_TOUCH_EVENTS)
JSValue JSDOMWindow::touch(ExecState& state) const
{
    return getDOMConstructor<JSTouchConstructor>(state.vm(), *this);
}

JSValue JSDOMWindow::touchList(ExecState& state) const
{
    return getDOMConstructor<JSTouchListConstructor>(state.vm(), *this);
}
#endif

// Custom functions

JSValue JSDOMWindow::open(ExecState& state)
{
    String urlString = valueToStringWithUndefinedOrNullCheck(&state, state.argument(0));
    if (state.hadException())
        return jsUndefined();
    AtomicString frameName = state.argument(1).isUndefinedOrNull() ? "_blank" : state.argument(1).toString(&state)->value(&state);
    if (state.hadException())
        return jsUndefined();
    String windowFeaturesString = valueToStringWithUndefinedOrNullCheck(&state, state.argument(2));
    if (state.hadException())
        return jsUndefined();

    RefPtr<DOMWindow> openedWindow = wrapped().open(urlString, frameName, windowFeaturesString, activeDOMWindow(&state), firstDOMWindow(&state));
    if (!openedWindow)
        return jsUndefined();
    return toJS(&state, openedWindow.get());
}

class DialogHandler {
public:
    explicit DialogHandler(ExecState& exec)
        : m_exec(exec)
    {
    }

    void dialogCreated(DOMWindow&);
    JSValue returnValue() const;

private:
    ExecState& m_exec;
    RefPtr<Frame> m_frame;
};

inline void DialogHandler::dialogCreated(DOMWindow& dialog)
{
    m_frame = dialog.frame();
    
    // FIXME: This looks like a leak between the normal world and an isolated
    //        world if dialogArguments comes from an isolated world.
    JSDOMWindow* globalObject = toJSDOMWindow(m_frame.get(), normalWorld(m_exec.vm()));
    if (JSValue dialogArguments = m_exec.argument(1))
        globalObject->putDirect(m_exec.vm(), Identifier::fromString(&m_exec, "dialogArguments"), dialogArguments);
}

inline JSValue DialogHandler::returnValue() const
{
    JSDOMWindow* globalObject = toJSDOMWindow(m_frame.get(), normalWorld(m_exec.vm()));
    if (!globalObject)
        return jsUndefined();
    Identifier identifier = Identifier::fromString(&m_exec, "returnValue");
    PropertySlot slot(globalObject);
    if (!JSGlobalObject::getOwnPropertySlot(globalObject, &m_exec, identifier, slot))
        return jsUndefined();
    return slot.getValue(&m_exec, identifier);
}

JSValue JSDOMWindow::showModalDialog(ExecState& state)
{
    String urlString = valueToStringWithUndefinedOrNullCheck(&state, state.argument(0));
    if (state.hadException())
        return jsUndefined();
    String dialogFeaturesString = valueToStringWithUndefinedOrNullCheck(&state, state.argument(2));
    if (state.hadException())
        return jsUndefined();

    DialogHandler handler(state);

    wrapped().showModalDialog(urlString, dialogFeaturesString, activeDOMWindow(&state), firstDOMWindow(&state), [&handler](DOMWindow& dialog) {
        handler.dialogCreated(dialog);
    });

    return handler.returnValue();
}

static JSValue handlePostMessage(DOMWindow& impl, ExecState& state)
{
    MessagePortArray messagePorts;
    ArrayBufferArray arrayBuffers;

    // This function has variable arguments and can be:
    // Per current spec:
    //   postMessage(message, targetOrigin)
    //   postMessage(message, targetOrigin, {sequence of transferrables})
    // Legacy non-standard implementations in webkit allowed:
    //   postMessage(message, {sequence of transferrables}, targetOrigin);
    int targetOriginArgIndex = 1;
    if (state.argumentCount() > 2) {
        int transferablesArgIndex = 2;
        if (state.argument(2).isString()) {
            targetOriginArgIndex = 2;
            transferablesArgIndex = 1;
        }
        fillMessagePortArray(state, state.argument(transferablesArgIndex), messagePorts, arrayBuffers);
    }
    if (state.hadException())
        return jsUndefined();

    RefPtr<SerializedScriptValue> message = SerializedScriptValue::create(&state, state.argument(0),
                                                                         &messagePorts,
                                                                         &arrayBuffers);

    if (state.hadException())
        return jsUndefined();

    String targetOrigin = valueToStringWithUndefinedOrNullCheck(&state, state.argument(targetOriginArgIndex));
    if (state.hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    impl.postMessage(message.release(), &messagePorts, targetOrigin, activeDOMWindow(&state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSDOMWindow::postMessage(ExecState& state)
{
    return handlePostMessage(wrapped(), state);
}

JSValue JSDOMWindow::setTimeout(ExecState& state)
{
    ContentSecurityPolicy* contentSecurityPolicy = wrapped().document() ? wrapped().document()->contentSecurityPolicy() : nullptr;
    std::unique_ptr<ScheduledAction> action = ScheduledAction::create(&state, globalObject()->world(), contentSecurityPolicy);
    if (state.hadException())
        return jsUndefined();

    if (!action)
        return jsNumber(0);

    int delay = state.argument(1).toInt32(&state);

    ExceptionCode ec = 0;
    int result = wrapped().setTimeout(WTFMove(action), delay, ec);
    setDOMException(&state, ec);

    return jsNumber(result);
}

JSValue JSDOMWindow::setInterval(ExecState& state)
{
    ContentSecurityPolicy* contentSecurityPolicy = wrapped().document() ? wrapped().document()->contentSecurityPolicy() : nullptr;
    std::unique_ptr<ScheduledAction> action = ScheduledAction::create(&state, globalObject()->world(), contentSecurityPolicy);
    if (state.hadException())
        return jsUndefined();
    int delay = state.argument(1).toInt32(&state);

    if (!action)
        return jsNumber(0);

    ExceptionCode ec = 0;
    int result = wrapped().setInterval(WTFMove(action), delay, ec);
    setDOMException(&state, ec);

    return jsNumber(result);
}

JSValue JSDOMWindow::addEventListener(ExecState& state)
{
    Frame* frame = wrapped().frame();
    if (!frame)
        return jsUndefined();

    JSValue listener = state.argument(1);
    if (!listener.isObject())
        return jsUndefined();

    wrapped().addEventListener(state.argument(0).toString(&state)->toAtomicString(&state), JSEventListener::create(asObject(listener), this, false, globalObject()->world()), state.argument(2).toBoolean(&state));
    return jsUndefined();
}

JSValue JSDOMWindow::removeEventListener(ExecState& state)
{
    Frame* frame = wrapped().frame();
    if (!frame)
        return jsUndefined();

    JSValue listener = state.argument(1);
    if (!listener.isObject())
        return jsUndefined();

    wrapped().removeEventListener(state.argument(0).toString(&state)->toAtomicString(&state), JSEventListener::create(asObject(listener), this, false, globalObject()->world()).ptr(), state.argument(2).toBoolean(&state));
    return jsUndefined();
}

DOMWindow* JSDOMWindow::toWrapped(JSValue value)
{
    if (!value.isObject())
        return 0;
    JSObject* object = asObject(value);
    if (object->inherits(JSDOMWindow::info()))
        return &jsCast<JSDOMWindow*>(object)->wrapped();
    if (object->inherits(JSDOMWindowShell::info()))
        return &jsCast<JSDOMWindowShell*>(object)->wrapped();
    return 0;
}

} // namespace WebCore
