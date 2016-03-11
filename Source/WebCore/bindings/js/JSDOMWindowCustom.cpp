/*
 * Copyright (C) 2007-2010, 2016 Apple Inc. All rights reserved.
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

#if ENABLE(USER_MESSAGE_HANDLERS)
static EncodedJSValue jsDOMWindowWebKit(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    JSDOMWindow* castedThis = toJSDOMWindow(JSValue::decode(thisValue));
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, castedThis->wrapped()))
        return JSValue::encode(jsUndefined());
    return JSValue::encode(toJS(exec, castedThis->globalObject(), castedThis->wrapped().webkitNamespace()));
}
#endif

#if ENABLE(INDEXED_DATABASE)
static EncodedJSValue jsDOMWindowIndexedDB(ExecState* exec, EncodedJSValue thisValue, PropertyName)
{
    UNUSED_PARAM(exec);
    auto* castedThis = toJSDOMWindow(JSValue::decode(thisValue));
    if (!RuntimeEnabledFeatures::sharedFeatures().indexedDBEnabled())
        return JSValue::encode(jsUndefined());
    if (!castedThis || !BindingSecurity::shouldAllowAccessToDOMWindow(exec, castedThis->wrapped()))
        return JSValue::encode(jsUndefined());
    auto& impl = castedThis->wrapped();
    JSValue result = toJS(exec, castedThis->globalObject(), WTF::getPtr(DOMWindowIndexedDatabase::indexedDB(&impl)));
    return JSValue::encode(result);
}
#endif

static bool jsDOMWindowGetOwnPropertySlotRestrictedAccess(JSDOMWindow* thisObject, Frame* frame, ExecState* exec, PropertyName propertyName, PropertySlot& slot, String& errorMessage)
{
    // Allow access to toString() cross-domain, but always Object.prototype.toString.
    if (propertyName == exec->propertyNames().toString) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<objectProtoFuncToString, 0>);
        return true;
    }

    // We don't want any properties other than "close" and "closed" on a frameless window
    // (i.e. one whose page got closed, or whose iframe got removed).
    // FIXME: This handling for frameless windows duplicates similar behaviour for cross-origin
    // access below; we should try to find a way to merge the two.
    if (!frame) {
        if (propertyName == exec->propertyNames().closed) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, jsDOMWindowClosed);
            return true;
        }
        if (propertyName == exec->propertyNames().close) {
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionClose, 0>);
            return true;
        }

        // FIXME: We should have a message here that explains why the property access/function call was
        // not allowed. 
        slot.setUndefined();
        return true;
    }

    // These are the functions we allow access to cross-origin (DoNotCheckSecurity in IDL).
    // Always provide the original function, on a fresh uncached function object.
    if (propertyName == exec->propertyNames().blur) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionBlur, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().close) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionClose, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().focus) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionFocus, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().postMessage) {
        slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionPostMessage, 2>);
        return true;
    }

    // When accessing cross-origin known Window properties, we always use the original property getter,
    // even if the property was removed / redefined. As of early 2016, this matches Firefox and Chrome's
    // behavior.
    if (auto* entry = JSDOMWindow::info()->staticPropHashTable->entry(propertyName)) {
        // Only allow access to these specific properties.
        if (propertyName == exec->propertyNames().location
            || propertyName == exec->propertyNames().closed
            || propertyName == exec->propertyNames().length
            || propertyName == exec->propertyNames().self
            || propertyName == exec->propertyNames().window
            || propertyName == exec->propertyNames().frames
            || propertyName == exec->propertyNames().opener
            || propertyName == exec->propertyNames().parent
            || propertyName == exec->propertyNames().top) {
            slot.setCacheableCustom(thisObject, ReadOnly | DontDelete | DontEnum, entry->propertyGetter());
            return true;
        }

        // For any other entries in the static property table, deny access. (Early return also prevents
        // named getter from returning frames with matching names - this seems a little questionable, see
        // FIXME comment on prototype search below.)
        thisObject->printErrorMessage(errorMessage);
        slot.setUndefined();
        return true;
    }

    // Do prototype lookup early so that functions and attributes in the prototype can have
    // precedence over the index and name getters.
    // FIXME: This seems like a silly idea. It only serves to suppress named property access
    // to frames that happen to have names corresponding to properties on the prototype.
    // This seems to only serve to leak some information cross-origin.
    JSValue proto = thisObject->getPrototypeDirect();
    if (proto.isObject() && asObject(proto)->getPropertySlot(exec, propertyName, slot)) {
        thisObject->printErrorMessage(errorMessage);
        slot.setUndefined();
        return true;
    }

    // Check for child frames by name before built-in properties to match Mozilla. This does
    // not match IE, but some sites end up naming frames things that conflict with window
    // properties that are in Moz but not IE. Since we have some of these, we have to do it
    // the Moz way.
    if (auto* scopedChild = frame->tree().scopedChild(propertyNameToAtomicString(propertyName))) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum, toJS(exec, scopedChild->document()->domWindow()));
        return true;
    }

    thisObject->printErrorMessage(errorMessage);
    slot.setUndefined();
    return true;
}

static bool jsDOMWindowGetOwnPropertySlotNamedItemGetter(JSDOMWindow* thisObject, Frame& frame, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    JSValue proto = thisObject->getPrototypeDirect();
    if (proto.isObject() && asObject(proto)->hasProperty(exec, propertyName))
        return false;

    // Check for child frames by name before built-in properties to match Mozilla. This does
    // not match IE, but some sites end up naming frames things that conflict with window
    // properties that are in Moz but not IE. Since we have some of these, we have to do it
    // the Moz way.
    if (auto* scopedChild = frame.tree().scopedChild(propertyNameToAtomicString(propertyName))) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum, toJS(exec, scopedChild->document()->domWindow()));
        return true;
    }

    // FIXME: Search the whole frame hierarchy somewhere around here.
    // We need to test the correct priority order.

    // Allow shortcuts like 'Image1' instead of document.images.Image1
    Document* document = frame.document();
    if (is<HTMLDocument>(*document)) {
        auto& htmlDocument = downcast<HTMLDocument>(*document);
        auto* atomicPropertyName = propertyName.publicName();
        if (atomicPropertyName && htmlDocument.hasWindowNamedItem(*atomicPropertyName)) {
            JSValue namedItem;
            if (UNLIKELY(htmlDocument.windowNamedItemContainsMultipleElements(*atomicPropertyName))) {
                Ref<HTMLCollection> collection = document->windowNamedItems(atomicPropertyName);
                ASSERT(collection->length() > 1);
                namedItem = toJS(exec, thisObject->globalObject(), collection.ptr());
            } else
                namedItem = toJS(exec, thisObject->globalObject(), htmlDocument.windowNamedItem(*atomicPropertyName));
            slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum, namedItem);
            return true;
        }
    }

    return false;
}

// Property access sequence is:
// (1) indexed properties,
// (2) regular own properties,
// (3) named properties (in fact, these shouldn't be on the window, should be on the NPO).
bool JSDOMWindow::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    // (1) First, indexed properties.
    // Hand off all indexed access to getOwnPropertySlotByIndex, which supports the indexed getter.
    if (Optional<unsigned> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, exec, index.value(), slot);

    auto* thisObject = jsCast<JSDOMWindow*>(object);
    auto* frame = thisObject->wrapped().frame();

    // Hand off all cross-domain/frameless access to jsDOMWindowGetOwnPropertySlotRestrictedAccess.
    String errorMessage;
    if (!frame || !shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), errorMessage))
        return jsDOMWindowGetOwnPropertySlotRestrictedAccess(thisObject, frame, exec, propertyName, slot, errorMessage);
    
    // FIXME: this need more explanation.
    // (Particularly, is it correct that this exists here but not in getOwnPropertySlotByIndex?)
    slot.setWatchpointSet(thisObject->m_windowCloseWatchpoints);

    // FIXME: These are all bogus. Keeping these here make some tests pass that check these properties
    // are own properties of the window, but introduces other problems instead (e.g. if you overwrite
    // & delete then the original value is restored!) Should be removed.
    if (propertyName == exec->propertyNames().blur) {
        if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionBlur, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().close) {
        if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionClose, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().focus) {
        if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionFocus, 0>);
        return true;
    }
    if (propertyName == exec->propertyNames().postMessage) {
        if (!Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            slot.setCustom(thisObject, ReadOnly | DontDelete | DontEnum, nonCachingStaticFunctionGetter<jsDOMWindowInstanceFunctionPostMessage, 2>);
        return true;
    }

    if (propertyName == exec->propertyNames().showModalDialog) {
        if (Base::getOwnPropertySlot(thisObject, exec, propertyName, slot))
            return true;
        if (!DOMWindow::canShowModalDialog(frame))
            return jsDOMWindowGetOwnPropertySlotNamedItemGetter(thisObject, *frame, exec, propertyName, slot);
    }

    // (2) Regular own properties.
    if (getStaticPropertySlot<JSDOMWindow, Base>(exec, *JSDOMWindow::info()->staticPropHashTable, thisObject, propertyName, slot))
        return true;

#if ENABLE(INDEXED_DATABASE)
    // FIXME: With generated JS bindings built on static property tables there is no way to
    // completely remove a generated property at runtime. So to completely disable IndexedDB
    // at runtime we have to not generate these accessors and have to handle them specially here.
    // Once https://webkit.org/b/145669 is resolved, they can once again be auto generated.
    if (RuntimeEnabledFeatures::sharedFeatures().indexedDBEnabled() && (propertyName == exec->propertyNames().indexedDB || propertyName == exec->propertyNames().webkitIndexedDB)) {
        slot.setCustom(thisObject, DontDelete | ReadOnly | CustomAccessor, jsDOMWindowIndexedDB);
        return true;
    }
#endif
#if ENABLE(USER_MESSAGE_HANDLERS)
    if (propertyName == exec->propertyNames().webkit && thisObject->wrapped().shouldHaveWebKitNamespaceForWorld(thisObject->world())) {
        slot.setCacheableCustom(thisObject, DontDelete | ReadOnly, jsDOMWindowWebKit);
        return true;
    }
#endif

    // (3) Finally, named properties.
    // Really, this should just be 'return false;' - these should all be on the NPO.
    return jsDOMWindowGetOwnPropertySlotNamedItemGetter(thisObject, *frame, exec, propertyName, slot);
}

// Property access sequence is:
// (1) indexed properties,
// (2) regular own properties,
// (3) named properties (in fact, these shouldn't be on the window, should be on the NPO).
bool JSDOMWindow::getOwnPropertySlotByIndex(JSObject* object, ExecState* exec, unsigned index, PropertySlot& slot)
{
    auto* thisObject = jsCast<JSDOMWindow*>(object);
    auto* frame = thisObject->wrapped().frame();

    // Indexed getters take precendence over regular properties, so caching would be invalid.
    slot.disableCaching();

    // (1) First, indexed properties.
    // These are also allowed cross-orgin, so come before the access check.
    if (frame && index < frame->tree().scopedChildCount()) {
        slot.setValue(thisObject, ReadOnly | DontDelete | DontEnum, toJS(exec, frame->tree().scopedChild(index)->document()->domWindow()));
        return true;
    }

    // Hand off all cross-domain/frameless access to jsDOMWindowGetOwnPropertySlotRestrictedAccess.
    String errorMessage;
    if (!frame || !shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), errorMessage))
        return jsDOMWindowGetOwnPropertySlotRestrictedAccess(thisObject, frame, exec, Identifier::from(exec, index), slot, errorMessage);

    // (2) Regular own properties.
    if (Base::getOwnPropertySlotByIndex(thisObject, exec, index, slot))
        return true;

    // (3) Finally, named properties.
    // Really, this should just be 'return false;' - these should all be on the NPO.
    return jsDOMWindowGetOwnPropertySlotNamedItemGetter(thisObject, *frame, exec, Identifier::from(exec, index), slot);
}

bool JSDOMWindow::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    if (!thisObject->wrapped().frame())
        return false;

    String errorMessage;
    if (!shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), errorMessage)) {
        // We only allow setting "location" attribute cross-origin.
        if (propertyName == exec->propertyNames().location) {
            bool putResult = false;
            if (lookupPut(exec, propertyName, thisObject, value, *s_info.staticPropHashTable, slot, putResult))
                return putResult;
            return false;
        }
        thisObject->printErrorMessage(errorMessage);
        return false;
    }

    return Base::put(thisObject, exec, propertyName, value, slot);
}

bool JSDOMWindow::putByIndex(JSCell* cell, ExecState* exec, unsigned index, JSValue value, bool shouldThrow)
{
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    if (!thisObject->wrapped().frame() || !BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped()))
        return false;
    
    return Base::putByIndex(thisObject, exec, index, value, shouldThrow);
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
    PropertySlot slot(globalObject, PropertySlot::InternalMethodType::Get);
    if (!JSGlobalObject::getOwnPropertySlot(globalObject, &m_exec, identifier, slot))
        return jsUndefined();
    return slot.getValue(&m_exec, identifier);
}

JSValue JSDOMWindow::showModalDialog(ExecState& state)
{
    if (UNLIKELY(state.argumentCount() < 1))
        return state.vm().throwException(&state, createNotEnoughArgumentsError(&state));

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
