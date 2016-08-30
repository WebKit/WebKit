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

EncodedJSValue JSC_HOST_CALL jsDOMWindowInstanceFunctionShowModalDialog(ExecState*);

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

static bool jsDOMWindowGetOwnPropertySlotRestrictedAccess(JSDOMWindow* thisObject, Frame* frame, ExecState* exec, PropertyName propertyName, PropertySlot& slot, String& errorMessage)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
        throwSecurityError(*exec, scope, errorMessage);
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

    throwSecurityError(*exec, scope, errorMessage);
    slot.setUndefined();
    return true;
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

    // (2) Regular own properties.
    PropertySlot slotCopy = slot;
    if (Base::getOwnPropertySlot(thisObject, exec, propertyName, slot)) {
        // Detect when we're getting the property 'showModalDialog', this is disabled, and has its original value.
        bool isShowModalDialogAndShouldHide = propertyName == exec->propertyNames().showModalDialog
            && !DOMWindow::canShowModalDialog(frame)
            && slot.isValue() && isHostFunction(slot.getValue(exec, propertyName), jsDOMWindowInstanceFunctionShowModalDialog);
        // Unless we're in the showModalDialog special case, we're done.
        if (!isShowModalDialogAndShouldHide)
            return true;
        slot = slotCopy;
    }

#if ENABLE(USER_MESSAGE_HANDLERS)
    if (propertyName == exec->propertyNames().webkit && thisObject->wrapped().shouldHaveWebKitNamespaceForWorld(thisObject->world())) {
        slot.setCacheableCustom(thisObject, DontDelete | ReadOnly, jsDOMWindowWebKit);
        return true;
    }
#endif

    return false;
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
    return Base::getOwnPropertySlotByIndex(thisObject, exec, index, slot);
}

bool JSDOMWindow::put(JSCell* cell, ExecState* exec, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = exec->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

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
        throwSecurityError(*exec, scope, errorMessage);
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
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), ThrowSecurityError))
        return false;
    return Base::deleteProperty(thisObject, exec, propertyName);
}

bool JSDOMWindow::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned propertyName)
{
    JSDOMWindow* thisObject = jsCast<JSDOMWindow*>(cell);
    // Only allow deleting properties by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(exec, thisObject->wrapped(), ThrowSecurityError))
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

// Custom functions

JSValue JSDOMWindow::open(ExecState& state)
{
    String urlString = valueToUSVStringWithUndefinedOrNullCheck(&state, state.argument(0));
    if (state.hadException())
        return jsUndefined();
    JSValue targetValue = state.argument(1);
    AtomicString target = targetValue.isUndefinedOrNull() ? AtomicString("_blank", AtomicString::ConstructFromLiteral) : targetValue.toString(&state)->toAtomicString(&state);
    if (state.hadException())
        return jsUndefined();
    String windowFeaturesString = valueToStringWithUndefinedOrNullCheck(&state, state.argument(2));
    if (state.hadException())
        return jsUndefined();

    RefPtr<DOMWindow> openedWindow = wrapped().open(urlString, target, windowFeaturesString, activeDOMWindow(&state), firstDOMWindow(&state));
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
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

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
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 2))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

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
        if (state.uncheckedArgument(2).isString()) {
            targetOriginArgIndex = 2;
            transferablesArgIndex = 1;
        }
        fillMessagePortArray(state, state.argument(transferablesArgIndex), messagePorts, arrayBuffers);
    }
    if (state.hadException())
        return jsUndefined();

    auto message = SerializedScriptValue::create(&state, state.uncheckedArgument(0), &messagePorts, &arrayBuffers);

    if (state.hadException())
        return jsUndefined();

    String targetOrigin = valueToUSVStringWithUndefinedOrNullCheck(&state, state.uncheckedArgument(targetOriginArgIndex));
    if (state.hadException())
        return jsUndefined();

    ExceptionCode ec = 0;
    impl.postMessage(WTFMove(message), &messagePorts, targetOrigin, callerDOMWindow(&state), ec);
    setDOMException(&state, ec);

    return jsUndefined();
}

JSValue JSDOMWindow::postMessage(ExecState& state)
{
    return handlePostMessage(wrapped(), state);
}

JSValue JSDOMWindow::setTimeout(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

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
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(state.argumentCount() < 1))
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

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

DOMWindow* JSDOMWindow::toWrapped(ExecState&, JSValue value)
{
    if (!value.isObject())
        return nullptr;
    JSObject* object = asObject(value);
    if (object->inherits(JSDOMWindow::info()))
        return &jsCast<JSDOMWindow*>(object)->wrapped();
    if (object->inherits(JSDOMWindowShell::info()))
        return &jsCast<JSDOMWindowShell*>(object)->wrapped();
    return nullptr;
}

} // namespace WebCore
