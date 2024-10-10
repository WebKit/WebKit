/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

#include "DeprecatedGlobalSettings.h"
#include "FrameDestructionObserverInlines.h"
#include "HTMLDocument.h"
#include "HTMLFrameOwnerElement.h"
#include "HTTPParsers.h"
#include "JSDOMBindingSecurity.h"
#include "JSDOMConvertCallbacks.h"
#include "JSDOMConvertNullable.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMMicrotask.h"
#include "JSDatabase.h"
#include "JSDatabaseCallback.h"
#include "JSEvent.h"
#include "JSEventListener.h"
#include "JSHTMLAudioElement.h"
#include "JSHTMLOptionElement.h"
#include "JSIDBFactory.h"
#include "JSWindowProxy.h"
#include "JSWorker.h"
#include "LocalDOMWindowWebDatabase.h"
#include "LocalFrame.h"
#include "Location.h"
#include "ScheduledAction.h"
#include "Settings.h"
#include "WebCoreJSClientData.h"
#include "WebCoreOpaqueRootInlines.h"
#include <JavaScriptCore/BuiltinNames.h>
#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/InternalFunction.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSFunction.h>
#include <JavaScriptCore/Lookup.h>
#include <JavaScriptCore/Structure.h>

#if ENABLE(USER_MESSAGE_HANDLERS)
#include "JSWebKitNamespace.h"
#endif

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {
using namespace JSC;

static JSC_DECLARE_HOST_FUNCTION(jsDOMWindowInstanceFunction_openDatabase);
#if ENABLE(USER_MESSAGE_HANDLERS)
static JSC_DECLARE_CUSTOM_GETTER(jsDOMWindow_webkit);
#endif

template<typename Visitor>
void JSDOMWindow::visitAdditionalChildren(Visitor& visitor)
{
    addWebCoreOpaqueRoot(visitor, wrapped());

    // Normally JSEventTargetCustom.cpp's JSEventTarget::visitAdditionalChildren() would call this. But
    // even though DOMWindow is an EventTarget, JSDOMWindow does not subclass JSEventTarget, so we need
    // to do this here.
    wrapped().visitJSEventListeners(visitor);
}

DEFINE_VISIT_ADDITIONAL_CHILDREN(JSDOMWindow);

#if ENABLE(USER_MESSAGE_HANDLERS)
JSC_DEFINE_CUSTOM_GETTER(jsDOMWindow_webkit, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto* castedThis = toJSDOMGlobalObject<JSDOMWindow>(vm, JSValue::decode(thisValue));
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, castedThis->wrapped()))
        return JSValue::encode(jsUndefined());
    RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(castedThis->wrapped());
    if (!localDOMWindow)
        return JSValue::encode(jsUndefined());
    return JSValue::encode(toJS(lexicalGlobalObject, castedThis->globalObject(), localDOMWindow->webkitNamespace()));
}
#endif

static ALWAYS_INLINE bool allowsLegacyExpandoIndexedProperties()
{
#if PLATFORM(COCOA)
    // Given that WindowProxy is the default |this| value for sloppy mode functions, it's hard to prove
    // that older iOS and macOS apps don't accidentally depend on this behavior, so we keep it for them.
    static bool requiresQuirk = !linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::NoExpandoIndexedPropertiesOnWindow);
    return requiresQuirk;
#else
    return false;
#endif
}

bool jsDOMWindowGetOwnPropertySlotRestrictedAccess(JSDOMGlobalObject* thisObject, DOMWindow& window, JSGlobalObject& lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot, const String& errorMessage)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto& builtinNames = WebCore::builtinNames(vm);

    // https://html.spec.whatwg.org/#crossorigingetownpropertyhelper-(-o,-p-)

    // These are the functions we allow access to cross-origin (DoNotCheckSecurity in IDL).
    auto* classInfo = JSDOMWindow::info();
    if (propertyName == builtinNames.closePublicName()
        || propertyName == builtinNames.focusPublicName()
        || propertyName == builtinNames.blurPublicName()
        || propertyName == builtinNames.postMessagePublicName()) {
        auto* entry = classInfo->staticPropHashTable->entry(propertyName);
        auto* jsFunction = thisObject->createCrossOriginFunction(&lexicalGlobalObject, propertyName, entry->function(), entry->functionLength());
        slot.setValue(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, jsFunction);
        return true;
    }

    // When accessing cross-origin known Window properties, we always use the original property getter,
    // even if the property was removed / redefined. As of early 2016, this matches Firefox and Chrome's
    // behavior.
    if (propertyName == builtinNames.windowPublicName()
        || propertyName == builtinNames.selfPublicName()
        || propertyName == builtinNames.locationPublicName()
        || propertyName == builtinNames.closedPublicName()
        || propertyName == builtinNames.framesPublicName()
        || propertyName == vm.propertyNames->length
        || propertyName == builtinNames.topPublicName()
        || propertyName == builtinNames.openerPublicName()
        || propertyName == builtinNames.parentPublicName()) {
        auto* entry = classInfo->staticPropHashTable->entry(propertyName);
        auto setter = propertyName == builtinNames.locationPublicName() ? entry->propertyPutter() : nullptr;
        auto* getterSetter = thisObject->createCrossOriginGetterSetter(&lexicalGlobalObject, propertyName, entry->propertyGetter(), setter);
        slot.setGetterSlot(thisObject, PropertyAttribute::Accessor | PropertyAttribute::DontEnum, getterSetter);
        return true;
    }

    // Check for child frames by name before built-in properties to match Mozilla. This does
    // not match IE, but some sites end up naming frames things that conflict with window
    // properties that are in Moz but not IE. Since we have some of these, we have to do it
    // the Moz way.
    // FIXME: Add support to named attributes on RemoteFrames.
    if (RefPtr frame = window.frame()) {
        if (auto* scopedChild = dynamicDowncast<LocalFrame>(frame->tree().scopedChildBySpecifiedName(propertyNameToAtomString(propertyName)))) {
            slot.setValue(thisObject, PropertyAttribute::ReadOnly | PropertyAttribute::DontEnum, toJS(&lexicalGlobalObject, scopedChild->document()->domWindow()));
            return true;
        }
    }

    if (handleCommonCrossOriginProperties(thisObject, vm, propertyName, slot))
        return true;

    throwSecurityError(lexicalGlobalObject, scope, errorMessage);
    slot.setUndefined();
    return false;
}

// https://html.spec.whatwg.org/#crossorigingetownpropertyhelper-(-o,-p-)
bool handleCommonCrossOriginProperties(JSObject* thisObject, VM& vm, PropertyName propertyName, PropertySlot& slot)
{
    auto& propertyNames =  vm.propertyNames;
    if (propertyName == propertyNames->builtinNames().thenPublicName() || propertyName == propertyNames->toStringTagSymbol || propertyName == propertyNames->hasInstanceSymbol || propertyName == propertyNames->isConcatSpreadableSymbol) {
        slot.setValue(thisObject, JSC::PropertyAttribute::ReadOnly | JSC::PropertyAttribute::DontEnum, jsUndefined());
        return true;
    }
    return false;
}

// Property access sequence is:
// (1) indexed properties,
// (2) regular own properties,
// (3) named properties (in fact, these shouldn't be on the window, should be on the NPO).
bool JSDOMWindow::getOwnPropertySlot(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, PropertySlot& slot)
{
    // (1) First, indexed properties.
    // Hand off all indexed access to getOwnPropertySlotByIndex, which supports the indexed getter.
    if (std::optional<unsigned> index = parseIndex(propertyName))
        return getOwnPropertySlotByIndex(object, lexicalGlobalObject, index.value(), slot);

    auto* thisObject = jsCast<JSDOMWindow*>(object);

    ASSERT(lexicalGlobalObject->vm().currentThreadIsHoldingAPILock());

    // FIXME (rdar://115751655): This should be replaced with a same-origin check between the active and target document.
    if (UNLIKELY(!is<LocalDOMWindow>(thisObject->wrapped()) && propertyName == "$vm"_s))
        return true;

    // Hand off all cross-domain access to jsDOMWindowGetOwnPropertySlotRestrictedAccess.
    String errorMessage;
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(*lexicalGlobalObject, thisObject->wrapped(), errorMessage))
        return jsDOMWindowGetOwnPropertySlotRestrictedAccess(thisObject, thisObject->wrapped(), *lexicalGlobalObject, propertyName, slot, errorMessage);

    if (UNLIKELY(!thisObject->m_windowCloseWatchpoints))
        thisObject->m_windowCloseWatchpoints = WatchpointSet::create(thisObject->wrapped().frame() ? IsWatched : IsInvalidated);
    // We use m_windowCloseWatchpoints to clear any inline caches once the frame is cleared.
    // This is sound because DOMWindow can be associated with at most one frame in its lifetime.
    if (thisObject->m_windowCloseWatchpoints->isStillValid())
        slot.setWatchpointSet(*thisObject->m_windowCloseWatchpoints);

    // (2) Regular own properties.
    if (Base::getOwnPropertySlot(thisObject, lexicalGlobalObject, propertyName, slot))
        return true;
    if (UNLIKELY(slot.isVMInquiry() && slot.isTaintedByOpaqueObject()))
        return false;

#if ENABLE(USER_MESSAGE_HANDLERS)
    RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped());
    if (propertyName == builtinNames(lexicalGlobalObject->vm()).webkitPublicName() && localDOMWindow && localDOMWindow->shouldHaveWebKitNamespaceForWorld(thisObject->world())) {
        slot.setCacheableCustom(thisObject, JSC::PropertyAttribute::DontDelete | JSC::PropertyAttribute::ReadOnly, jsDOMWindow_webkit);
        return true;
    }
#endif

    return false;
}

bool JSDOMWindow::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* lexicalGlobalObject, unsigned index, PropertySlot& slot)
{
    auto* thisObject = jsCast<JSDOMWindow*>(object);
    auto& window = thisObject->wrapped();
    auto* frame = window.frame();

    // Indexed getters take precendence over regular properties, so caching would be invalid.
    slot.disableCaching();

    ASSERT(lexicalGlobalObject->vm().currentThreadIsHoldingAPILock());
    // These are also allowed cross-origin, so come before the access check.
    if (frame) {
        // FIXME: <rdar://118263337> DOMWindow::length needs to include RemoteFrames.
        // FIXME: scopedChild/scopedChildCount and RemoteFrame need to work together well. We're using using child/childCount until then.
        if (is<LocalFrame>(frame)) {
            if (index < frame->tree().scopedChildCount()) {
                if (auto* scopedChild = frame->tree().scopedChild(index)) {
                    slot.setValue(thisObject, enumToUnderlyingType(JSC::PropertyAttribute::ReadOnly), toJS(lexicalGlobalObject, scopedChild->window()));
                    return true;
                }
            }
        } else {
            if (index < frame->tree().childCount()) {
                if (auto* child = frame->tree().child(index)) {
                    slot.setValue(thisObject, enumToUnderlyingType(JSC::PropertyAttribute::ReadOnly), toJS(lexicalGlobalObject, child->window()));
                    return true;
                }
            }
        }
    }

    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, window, ThrowSecurityError))
        return false;
    if (allowsLegacyExpandoIndexedProperties())
        return Base::getOwnPropertySlotByIndex(thisObject, lexicalGlobalObject, index, slot);
    return false;
}

bool JSDOMWindow::put(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsCast<JSDOMWindow*>(cell);

    String errorMessage;
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(*lexicalGlobalObject, thisObject->wrapped(), errorMessage)) {
        // We only allow setting "location" attribute cross-origin.
        if (propertyName == builtinNames(vm).locationPublicName()) {
            auto setter = s_info.staticPropHashTable->entry(propertyName)->propertyPutter();
            scope.release();
            setter(lexicalGlobalObject, JSValue::encode(slot.thisValue()), JSValue::encode(value), propertyName);
            return true;
        }
        throwSecurityError(*lexicalGlobalObject, scope, errorMessage);
        return false;
    }

    if (parseIndex(propertyName) && !allowsLegacyExpandoIndexedProperties())
        return typeError(lexicalGlobalObject, scope, slot.isStrictMode(), makeUnsupportedIndexedSetterErrorMessage("Window"_s));
    RELEASE_AND_RETURN(scope, Base::put(thisObject, lexicalGlobalObject, propertyName, value, slot));
}

bool JSDOMWindow::putByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned index, JSValue value, bool shouldThrow)
{
    VM& vm = lexicalGlobalObject->vm();
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    auto scope = DECLARE_THROW_SCOPE(vm);

    String errorMessage;
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(*lexicalGlobalObject, thisObject->wrapped(), errorMessage)) {
        throwSecurityError(*lexicalGlobalObject, scope, errorMessage);
        return false;
    }

    if (allowsLegacyExpandoIndexedProperties()) {
        RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped());
        if (RefPtr document = localDOMWindow ? localDOMWindow->document() : nullptr)
            document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Adding expando indexed properties to 'window' was a non-standard behavior that is now removed."_s);
        RELEASE_AND_RETURN(scope, Base::putByIndex(thisObject, lexicalGlobalObject, index, value, shouldThrow));
    }
    return typeError(lexicalGlobalObject, scope, shouldThrow, makeUnsupportedIndexedSetterErrorMessage("Window"_s));
}

bool JSDOMWindow::deleteProperty(JSCell* cell, JSGlobalObject* lexicalGlobalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    // Only allow deleting properties by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped(), ThrowSecurityError))
        return false;
    if (std::optional<uint32_t> index = parseIndex(propertyName)) {
        if (!allowsLegacyExpandoIndexedProperties()) {
            RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped());
            return localDOMWindow && index.value() >= localDOMWindow->length();
        }
    }
    return Base::deleteProperty(thisObject, lexicalGlobalObject, propertyName, slot);
}

bool JSDOMWindow::deletePropertyByIndex(JSCell* cell, JSGlobalObject* lexicalGlobalObject, unsigned propertyName)
{
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    // Only allow deleting properties by frames in the same origin.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped(), ThrowSecurityError))
        return false;
    if (isIndex(propertyName) && !allowsLegacyExpandoIndexedProperties()) {
        RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped());
        return localDOMWindow && propertyName >= localDOMWindow->length();
    }
    return Base::deletePropertyByIndex(thisObject, lexicalGlobalObject, propertyName);
}

void JSDOMWindow::analyzeHeap(JSCell* cell, HeapAnalyzer& analyzer)
{
    auto* thisObject = jsCast<JSDOMWindow*>(cell);
    auto& location = thisObject->wrapped().location();
    analyzer.setLabelForCell(cell, location.href());

    Base::analyzeHeap(cell, analyzer);
}

// https://html.spec.whatwg.org/#crossoriginproperties-(-o-)
template <CrossOriginObject objectType>
static void addCrossOriginPropertyNames(VM& vm, PropertyNameArray& propertyNames)
{
    auto& builtinNames = WebCore::builtinNames(vm);
    switch (objectType) {
    case CrossOriginObject::Location: {
        static const Identifier* const properties[] = { &builtinNames.hrefPublicName(), &vm.propertyNames->replace };
        for (auto* property : properties)
            propertyNames.add(*property);
        break;
    }
    case CrossOriginObject::Window: {
        static const Identifier* const properties[] = {
            &builtinNames.blurPublicName(), &builtinNames.closePublicName(), &builtinNames.closedPublicName(),
            &builtinNames.focusPublicName(), &builtinNames.framesPublicName(), &vm.propertyNames->length,
            &builtinNames.locationPublicName(), &builtinNames.openerPublicName(), &builtinNames.parentPublicName(),
            &builtinNames.postMessagePublicName(), &builtinNames.selfPublicName(), &builtinNames.topPublicName(),
            &builtinNames.windowPublicName()
        };

        for (auto* property : properties)
            propertyNames.add(*property);
        break;
    }
    }
}

// https://html.spec.whatwg.org/#crossoriginownpropertykeys-(-o-)
template <CrossOriginObject objectType>
void addCrossOriginOwnPropertyNames(JSC::JSGlobalObject& lexicalGlobalObject, JSC::PropertyNameArray& propertyNames)
{
    auto& vm = lexicalGlobalObject.vm();
    addCrossOriginPropertyNames<objectType>(vm, propertyNames);

    static const Identifier* const properties[] = {
        &vm.propertyNames->builtinNames().thenPublicName(), &vm.propertyNames->toStringTagSymbol, &vm.propertyNames->hasInstanceSymbol, &vm.propertyNames->isConcatSpreadableSymbol
    };

    for (auto* property : properties)
        propertyNames.add(*property);

}
template void addCrossOriginOwnPropertyNames<CrossOriginObject::Window>(JSC::JSGlobalObject&, JSC::PropertyNameArray&);
template void addCrossOriginOwnPropertyNames<CrossOriginObject::Location>(JSC::JSGlobalObject&, JSC::PropertyNameArray&);

static void addScopedChildrenIndexes(JSGlobalObject& lexicalGlobalObject, DOMWindow& window, PropertyNameArray& propertyNames)
{
    RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localDOMWindow)
        return;

    auto* document = localDOMWindow->document();
    if (!document)
        return;

    auto* frame = document->frame();
    if (!frame)
        return;

    VM& vm = lexicalGlobalObject.vm();
    unsigned scopedChildCount = frame->tree().scopedChildCount();
    for (unsigned i = 0; i < scopedChildCount; ++i)
        propertyNames.add(Identifier::from(vm, i));
}

// https://html.spec.whatwg.org/#windowproxy-ownpropertykeys
void JSDOMWindow::getOwnPropertyNames(JSObject* object, JSGlobalObject* lexicalGlobalObject, PropertyNameArray& propertyNames, DontEnumPropertiesMode mode)
{
    auto* thisObject = jsCast<JSDOMWindow*>(object);

    addScopedChildrenIndexes(*lexicalGlobalObject, thisObject->wrapped(), propertyNames);

    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped(), DoNotReportSecurityError)) {
        if (mode == DontEnumPropertiesMode::Include)
            addCrossOriginOwnPropertyNames<CrossOriginObject::Window>(*lexicalGlobalObject, propertyNames);
        return;
    }
    Base::getOwnPropertyNames(thisObject, lexicalGlobalObject, propertyNames, mode);
}

bool JSDOMWindow::defineOwnProperty(JSC::JSObject* object, JSC::JSGlobalObject* lexicalGlobalObject, JSC::PropertyName propertyName, const JSC::PropertyDescriptor& descriptor, bool shouldThrow)
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = jsCast<JSDOMWindow*>(object);
    // Only allow defining properties in this way by frames in the same origin, as it allows setters to be introduced.
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped(), ThrowSecurityError))
        return false;
    if (parseIndex(propertyName) && !allowsLegacyExpandoIndexedProperties())
        return typeError(lexicalGlobalObject, scope, shouldThrow, makeUnsupportedIndexedSetterErrorMessage("Window"_s));
    scope.release();

    auto& builtinNames = WebCore::builtinNames(vm);
    if (propertyName == builtinNames.documentPublicName() || propertyName == builtinNames.windowPublicName())
        return JSObject::defineOwnProperty(thisObject, lexicalGlobalObject, propertyName, descriptor, shouldThrow);

    return Base::defineOwnProperty(thisObject, lexicalGlobalObject, propertyName, descriptor, shouldThrow);
}

JSValue JSDOMWindow::getPrototype(JSObject* object, JSGlobalObject* lexicalGlobalObject)
{
    auto* thisObject = jsCast<JSDOMWindow*>(object);
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, thisObject->wrapped(), DoNotReportSecurityError))
        return jsNull();

    return Base::getPrototype(object, lexicalGlobalObject);
}

bool JSDOMWindow::preventExtensions(JSObject*, JSGlobalObject*)
{
    return false;
}

// Custom Attributes

JSValue JSDOMWindow::event(JSGlobalObject& lexicalGlobalObject) const
{
    Event* event = currentEvent();
    if (!event)
        return jsUndefined();
    return toJS(&lexicalGlobalObject, const_cast<JSDOMWindow*>(this), event);
}

// Custom functions

class DialogHandler {
public:
    explicit DialogHandler(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
        : m_globalObject(lexicalGlobalObject)
        , m_callFrame(callFrame)
    {
    }

    void dialogCreated(DOMWindow&);
    JSValue returnValue() const;

private:
    JSGlobalObject& m_globalObject;
    CallFrame& m_callFrame;
    RefPtr<LocalFrame> m_frame;
};

inline void DialogHandler::dialogCreated(DOMWindow& dialog)
{
    RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(dialog);
    if (!localDOMWindow)
        return;
    VM& vm = m_globalObject.vm();
    m_frame = localDOMWindow->frame();

    // FIXME: This looks like a leak between the normal world and an isolated
    //        world if dialogArguments comes from an isolated world.
    auto* globalObject = toJSDOMWindow(m_frame.get(), normalWorld(vm));
    if (JSValue dialogArguments = m_callFrame.argument(1))
        globalObject->putDirect(vm, Identifier::fromString(vm, "dialogArguments"_s), dialogArguments);
}

inline JSValue DialogHandler::returnValue() const
{
    VM& vm = m_globalObject.vm();
    auto* globalObject = toJSDOMWindow(m_frame.get(), normalWorld(vm));
    if (!globalObject)
        return jsUndefined();
    Identifier identifier = Identifier::fromString(vm, "returnValue"_s);
    PropertySlot slot(globalObject, PropertySlot::InternalMethodType::Get);
    if (!JSGlobalObject::getOwnPropertySlot(globalObject, &m_globalObject, identifier, slot))
        return jsUndefined();
    return slot.getValue(&m_globalObject, identifier);
}

static JSC_DECLARE_HOST_FUNCTION(showModalDialog);

JSC_DEFINE_CUSTOM_GETTER(showModalDialogGetter, (JSGlobalObject* lexicalGlobalObject, EncodedJSValue thisValue, PropertyName propertyName))
{
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = castThisValue<JSDOMWindow>(*lexicalGlobalObject, JSValue::decode(thisValue));
    if (UNLIKELY(!thisObject))
        return throwVMDOMAttributeGetterTypeError(lexicalGlobalObject, scope, JSDOMWindow::info(), propertyName);

    RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped());
    if (RefPtr document = localDOMWindow ? localDOMWindow->document() : nullptr)
        document->addConsoleMessage(MessageSource::JS, MessageLevel::Warning, "Window 'showModalDialog' function is deprecated and will be removed soon."_s);

    if (RefPtr frame = thisObject->wrapped().frame()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (frame->settings().showModalDialogEnabled() && localFrame && LocalDOMWindow::canShowModalDialog(*localFrame)) {
            auto* jsFunction = JSFunction::create(vm, lexicalGlobalObject, 1, "showModalDialog"_s, showModalDialog, ImplementationVisibility::Public);
            thisObject->putDirect(vm, propertyName, jsFunction);
            return JSValue::encode(jsFunction);
        }
    }

    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(showModalDialog, (JSGlobalObject* lexicalGlobalObjectPtr, CallFrame* callFramePtr))
{
    auto& lexicalGlobalObject = *lexicalGlobalObjectPtr;
    auto& callFrame = *callFramePtr;

    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* thisObject = IDLOperation<JSDOMWindow>::cast(lexicalGlobalObject, callFrame);
    if (UNLIKELY(!thisObject))
        return throwThisTypeError(lexicalGlobalObject, scope, "Window"_s, "showModalDialog"_s);

    bool shouldAllowAccess = BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObjectPtr, thisObject->wrapped(), ThrowSecurityError);
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception() || !shouldAllowAccess);
    if (!shouldAllowAccess)
        return JSValue::encode(jsUndefined());

    if (UNLIKELY(callFrame.argumentCount() < 1))
        return throwVMException(&lexicalGlobalObject, scope, createNotEnoughArgumentsError(&lexicalGlobalObject));

    auto urlString = convert<IDLNullable<IDLDOMString>>(lexicalGlobalObject, callFrame.argument(0));
    if (UNLIKELY(urlString.hasException(scope)))
        return { };
    auto dialogFeaturesString = convert<IDLNullable<IDLDOMString>>(lexicalGlobalObject, callFrame.argument(2));
    if (UNLIKELY(dialogFeaturesString.hasException(scope)))
        return { };

    DialogHandler handler(lexicalGlobalObject, callFrame);

    if (RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(thisObject->wrapped())) {
        localDOMWindow->showModalDialog(urlString.releaseReturnValue(), dialogFeaturesString.releaseReturnValue(), activeDOMWindow(lexicalGlobalObject), firstDOMWindow(lexicalGlobalObject), [&handler](DOMWindow& dialog) {
            handler.dialogCreated(dialog);
        });
    }

    return JSValue::encode(handler.returnValue());
}

JSValue JSDOMWindow::queueMicrotask(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (UNLIKELY(callFrame.argumentCount() < 1))
        return throwException(&lexicalGlobalObject, scope, createNotEnoughArgumentsError(&lexicalGlobalObject));

    JSValue functionValue = callFrame.uncheckedArgument(0);
    if (UNLIKELY(!functionValue.isCallable()))
        return JSValue::decode(throwArgumentMustBeFunctionError(lexicalGlobalObject, scope, 0, "callback"_s, "Window"_s, "queueMicrotask"_s));

    scope.release();
    Base::queueMicrotask(createJSDOMMicrotask(vm, asObject(functionValue)));
    return jsUndefined();
}

DOMWindow* JSDOMWindow::toWrapped(VM&, JSValue value)
{
    if (!value.isObject())
        return nullptr;
    JSObject* object = asObject(value);
    if (object->inherits<JSDOMWindow>())
        return &jsCast<JSDOMWindow*>(object)->wrapped();
    if (object->inherits<JSWindowProxy>()) {
        if (auto* jsDOMWindow = jsDynamicCast<JSDOMWindow*>(jsCast<JSWindowProxy*>(object)->window()))
            return &jsDOMWindow->wrapped();
    }
    return nullptr;
}

void JSDOMWindow::setOpener(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(&lexicalGlobalObject, wrapped(), ThrowSecurityError))
        return;

    if (value.isNull()) {
        if (RefPtr localDOMWindow = dynamicDowncast<LocalDOMWindow>(wrapped()))
            localDOMWindow->disownOpener();
        return;
    }

    bool shouldThrow = true;
    createDataProperty(&lexicalGlobalObject, builtinNames(lexicalGlobalObject.vm()).openerPublicName(), value, shouldThrow);
}

JSValue JSDOMWindow::self(JSC::JSGlobalObject&) const
{
    return globalThis();
}

JSValue JSDOMWindow::window(JSC::JSGlobalObject&) const
{
    return globalThis();
}

JSValue JSDOMWindow::frames(JSC::JSGlobalObject&) const
{
    return globalThis();
}

static inline JSC::EncodedJSValue jsDOMWindowInstanceFunctionOpenDatabaseBody(JSC::JSGlobalObject* lexicalGlobalObject, CallFrame* callFrame, typename IDLOperation<JSDOMWindow>::ClassParameter castedThis)
{
    auto& vm = lexicalGlobalObject->vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);

    if (!BindingSecurity::shouldAllowAccessToDOMWindow(lexicalGlobalObject, castedThis->wrapped(), ThrowSecurityError))
        return JSValue::encode(jsUndefined());
    RefPtr impl = dynamicDowncast<LocalDOMWindow>(castedThis->wrapped());
    if (!impl)
        return JSValue::encode(jsUndefined());
    if (UNLIKELY(callFrame->argumentCount() < 4))
        return throwVMError(lexicalGlobalObject, throwScope, createNotEnoughArgumentsError(lexicalGlobalObject));
    auto name = convert<IDLDOMString>(*lexicalGlobalObject, callFrame->uncheckedArgument(0));
    if (UNLIKELY(name.hasException(throwScope)))
        return encodedJSValue();
    auto version = convert<IDLDOMString>(*lexicalGlobalObject, callFrame->uncheckedArgument(1));
    if (UNLIKELY(version.hasException(throwScope)))
        return encodedJSValue();
    auto displayName = convert<IDLDOMString>(*lexicalGlobalObject, callFrame->uncheckedArgument(2));
    if (UNLIKELY(displayName.hasException(throwScope)))
        return encodedJSValue();
    auto estimatedSize = convert<IDLUnsignedLong>(*lexicalGlobalObject, callFrame->uncheckedArgument(3));
    if (UNLIKELY(estimatedSize.hasException(throwScope)))
        return encodedJSValue();

    if (!DeprecatedGlobalSettings::webSQLEnabled()) {
        if (name.returnValue() != "null"_s || version.returnValue() != "null"_s || displayName.returnValue() != "null"_s || estimatedSize.returnValue())
            propagateException(*lexicalGlobalObject, throwScope, Exception(ExceptionCode::UnknownError, "Web SQL is deprecated"_s));
        return JSValue::encode(constructEmptyObject(lexicalGlobalObject, castedThis->globalObject()->objectPrototype()));
    }

    auto creationCallback = convert<IDLNullable<IDLCallbackFunction<JSDatabaseCallback>>>(*lexicalGlobalObject, callFrame->argument(4), *castedThis->globalObject(), [](JSC::JSGlobalObject& lexicalGlobalObject, JSC::ThrowScope& scope) {
        throwArgumentMustBeFunctionError(lexicalGlobalObject, scope, 4, "creationCallback"_s, "Window"_s, "openDatabase"_s);
    });
    if (UNLIKELY(creationCallback.hasException(throwScope)))
        return encodedJSValue();

    return JSValue::encode(toJS<IDLNullable<IDLInterface<Database>>>(*lexicalGlobalObject, *castedThis->globalObject(), throwScope, WebCore::LocalDOMWindowWebDatabase::openDatabase(*impl, name.releaseReturnValue(), version.releaseReturnValue(), displayName.releaseReturnValue(), estimatedSize.releaseReturnValue(), creationCallback.releaseReturnValue())));
}

JSC_DEFINE_HOST_FUNCTION(jsDOMWindowInstanceFunction_openDatabase, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    return IDLOperation<JSDOMWindow>::call<jsDOMWindowInstanceFunctionOpenDatabaseBody>(*globalObject, *callFrame, "openDatabase");
}

JSValue JSDOMWindow::openDatabase(JSC::JSGlobalObject& lexicalGlobalObject) const
{
    VM& vm = lexicalGlobalObject.vm();
    StringImpl* name = PropertyName(builtinNames(vm).openDatabasePublicName()).publicName();
    if (DeprecatedGlobalSettings::webSQLEnabled())
        return JSFunction::create(vm, &lexicalGlobalObject, 4, name, jsDOMWindowInstanceFunction_openDatabase, ImplementationVisibility::Public);

    return InternalFunction::createFunctionThatMasqueradesAsUndefined(vm, &lexicalGlobalObject, 4, name, jsDOMWindowInstanceFunction_openDatabase);
}

void JSDOMWindow::setOpenDatabase(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    if (!BindingSecurity::shouldAllowAccessToDOMWindow(&lexicalGlobalObject, wrapped(), ThrowSecurityError))
        return;

    bool shouldThrow = true;
    createDataProperty(&lexicalGlobalObject, builtinNames(lexicalGlobalObject.vm()).openDatabasePublicName(), value, shouldThrow);
}

JSDOMWindow& mainWorldGlobalObject(LocalFrame& frame)
{
    // FIXME: What guarantees the result of jsWindowProxy() is non-null?
    // FIXME: What guarantees the result of window() is non-null?
    // FIXME: What guarantees the result of window() a JSDOMWindow?
    return *jsCast<JSDOMWindow*>(frame.windowProxy().jsWindowProxy(mainThreadNormalWorld())->window());
}

} // namespace WebCore
