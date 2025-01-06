/*
 * Copyright (C) 2008-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSWindowProxy.h"

#include "CommonVM.h"
#include "Frame.h"
#include "FrameLoaderTypes.h"
#include "GCController.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowProperties.h"
#include "JSEventTarget.h"
#include "Location.h"
#include "Logging.h"
#include "ScriptController.h"
#include "WebCoreJSClientData.h"
#include <JavaScriptCore/Debugger.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/text/MakeString.h>

#if PLATFORM(COCOA)
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCore {

using namespace JSC;

const ClassInfo JSWindowProxy::s_info = { "JSWindowProxy"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSWindowProxy) };

inline JSWindowProxy::JSWindowProxy(VM& vm, Structure& structure, DOMWrapperWorld& world)
    : Base(vm, &structure, nullptr)
    , m_world(world)
{
}

JSWindowProxy::~JSWindowProxy() = default;

void JSWindowProxy::finishCreation(VM& vm, DOMWindow& window)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));
    setWindow(window);
}

JSWindowProxy& JSWindowProxy::create(VM& vm, DOMWindow& window, DOMWrapperWorld& world)
{
    auto& structure = *Structure::create(vm, 0, jsNull(), TypeInfo(GlobalProxyType, StructureFlags), info());
    auto& proxy = *new (NotNull, allocateCell<JSWindowProxy>(vm)) JSWindowProxy(vm, structure, world);
    proxy.finishCreation(vm, window);
    return proxy;
}

void JSWindowProxy::destroy(JSCell* cell)
{
    static_cast<JSWindowProxy*>(cell)->JSWindowProxy::~JSWindowProxy();
}

DOMWrapperWorld& JSWindowProxy::world()
{
    return m_world;
}

void JSWindowProxy::setWindow(VM& vm, JSDOMGlobalObject& window)
{
    ASSERT(window.classInfo() == JSDOMWindow::info());
    setTarget(vm, &window);
    GCController::singleton().garbageCollectSoon();
}

void JSWindowProxy::setWindow(DOMWindow& domWindow)
{
    // Replacing JSDOMWindow via telling JSWindowProxy to use the same LocalDOMWindow it already uses makes no sense,
    // so we'd better never try to.
    ASSERT(!window() || &domWindow != &wrapped());

    auto* localWindow = dynamicDowncast<LocalDOMWindow>(domWindow);

    VM& vm = commonVM();
    auto& prototypeStructure = *JSDOMWindowPrototype::createStructure(vm, nullptr, jsNull());

    // Explicitly protect the prototype so it isn't collected when we allocate the global object.
    // (Once the global object is fully constructed, it will mark its own prototype.)
    JSNonFinalObject* prototype = static_cast<JSNonFinalObject*>(JSDOMWindowPrototype::create(vm, nullptr, &prototypeStructure));
    JSC::EnsureStillAliveScope protectedPrototype(prototype);

    JSDOMGlobalObject* window = nullptr;
    auto& windowStructure = *JSDOMWindow::createStructure(vm, nullptr, prototype);
    window = JSDOMWindow::create(vm, &windowStructure, domWindow, this);
    if (localWindow) {
        bool linkedWithNewSDK = true;
#if PLATFORM(COCOA)
        linkedWithNewSDK = linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::DOMWindowReuseRestriction);
#endif
        if (!localWindow->document()->haveInitializedSecurityOrigin() && linkedWithNewSDK)
            localWindow->setAsWrappedWithoutInitializedSecurityOrigin();
    }

    prototype->structure()->setGlobalObject(vm, window);

    auto& propertiesStructure = *JSDOMWindowProperties::createStructure(vm, window, JSEventTarget::prototype(vm, *window));
    auto& properties = *JSDOMWindowProperties::create(&propertiesStructure, *window);
    properties.didBecomePrototype(vm);
    prototype->structure()->setPrototypeWithoutTransition(vm, &properties);

    setWindow(vm, *window);

    ASSERT(window->globalObject() == window);
    ASSERT(prototype->globalObject() == window);
}

WindowProxy* JSWindowProxy::windowProxy() const
{
    auto& window = wrapped();
    return window.frame() ? &window.frame()->windowProxy() : nullptr;
}

void JSWindowProxy::attachDebugger(JSC::Debugger* debugger)
{
    auto* globalObject = window();
    JSLockHolder lock(globalObject->vm());

    if (debugger)
        debugger->attach(globalObject);
    else if (auto* currentDebugger = globalObject->debugger())
        currentDebugger->detach(globalObject, JSC::Debugger::TerminatingDebuggingSession);
}

DOMWindow& JSWindowProxy::wrapped() const
{
    auto* window = this->window();
    return jsCast<JSDOMWindowBase*>(window)->wrapped();
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, WindowProxy& windowProxy)
{
    auto* jsWindowProxy = windowProxy.jsWindowProxy(currentWorld(*lexicalGlobalObject));
    return jsWindowProxy ? JSValue(jsWindowProxy) : jsNull();
}

JSWindowProxy* toJSWindowProxy(WindowProxy& windowProxy, DOMWrapperWorld& world)
{
    return windowProxy.jsWindowProxy(world);
}

WindowProxy* JSWindowProxy::toWrapped(VM&, JSValue value)
{
    if (!value.isObject())
        return nullptr;
    JSObject* object = asObject(value);
    if (object->inherits<JSWindowProxy>())
        return jsCast<JSWindowProxy*>(object)->windowProxy();
    return nullptr;
}

JSC::GCClient::IsoSubspace* JSWindowProxy::subspaceForImpl(JSC::VM& vm)
{
    return &static_cast<JSVMClientData*>(vm.clientData)->windowProxySpace();
}

Ref<DOMWrapperWorld> JSWindowProxy::protectedWorld()
{
    return m_world;
}

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

struct FrameInfo {
    Ref<Frame> frame;
    Ref<Frame> mainFrame;
};

static std::optional<FrameInfo> frameInfo(JSGlobalObject* globalObject)
{
    auto* domGlobalObject = jsDynamicCast<JSDOMGlobalObject*>(globalObject);
    if (!domGlobalObject)
        return std::nullopt;

    RefPtr document { dynamicDowncast<Document>(domGlobalObject->scriptExecutionContext()) };
    if (!document)
        return std::nullopt;

    RefPtr frame { document->frame() };
    if (!frame)
        return std::nullopt;

    Ref mainFrame { frame->mainFrame() };
    return FrameInfo { frame.releaseNonNull(), WTFMove(mainFrame) };
}

static bool hasSameMainFrame(const Frame* a, const FrameInfo& b)
{
    return a && (&a->mainFrame() == b.mainFrame.ptr());
}

static void logCrossTabPropertyAccess(Frame& childFrame, const std::variant<PropertyName, unsigned>& propertyName)
{
#if LOG_DISABLED
    UNUSED_PARAM(childFrame);
    UNUSED_PARAM(propertyName);
#else
    if (!childFrame.opener())
        return;

    RefPtr parentWindow { childFrame.opener()->window() };
    RefPtr childWindow { childFrame.window() };
    if (!parentWindow || !childWindow)
        return;

    String propertyNameDescription;
    if (std::holds_alternative<PropertyName>(propertyName))
        propertyNameDescription = std::get<PropertyName>(propertyName).uid();
    else
        propertyNameDescription = makeString(std::get<unsigned>(propertyName));

    LOG(Loading, "Detected cross-tab WindowProxy property access of %s between parent window (origin = %s) and child window (origin = %s)", propertyNameDescription.utf8().data(), parentWindow->location().origin().utf8().data(), childWindow->location().origin().utf8().data());
#endif // #if LOG_DISABLED
}

static void checkCrossTabWindowProxyUsage(JSWindowProxy* proxy, JSGlobalObject* lexicalGlobalObject, const std::variant<PropertyName, unsigned>& propertyName)
{
    if (!proxy || !lexicalGlobalObject)
        return;

    auto target = proxy->target();
    if (!target)
        return;

    // If the caller is just trying to access their own window, we don't need to log anything.
    if (target == lexicalGlobalObject)
        return;

    auto lexicalInfo = frameInfo(lexicalGlobalObject);
    auto targetInfo = frameInfo(target);
    if (!lexicalInfo || !targetInfo)
        return;

    // If the caller is trying to access a window within the same tab, then we don't need to log anything.
    if (lexicalInfo->mainFrame.ptr() == targetInfo->mainFrame.ptr())
        return;

    WindowProxyProperty property = WindowProxyProperty::Other;
    auto& builtinNames = WebCore::builtinNames(lexicalGlobalObject->vm());

    if (std::holds_alternative<PropertyName>(propertyName)) {
        auto name = std::get<PropertyName>(propertyName);
        if (name == builtinNames.closedPublicName())
            property = WindowProxyProperty::Closed;
        else if (name == builtinNames.postMessagePublicName())
            property = WindowProxyProperty::PostMessage;
    }

    // For the following scenarios, assume window A calls window.open to create window B.
    // We'll call A the "parent" and B the "child".
    //
    // Scenario 1: some script in the parent tab tries to access the child window via a WindowProxy object.
    // In this case, the child is the target of the WindowProxy object, and we check that the child's opener
    // points to some tab in the parent.
    {
        auto& parent = *lexicalInfo;
        auto& child = *targetInfo;

        if (hasSameMainFrame(child.frame->opener(), parent)) {
            logCrossTabPropertyAccess(child.frame, propertyName);

            if (auto childFrame = dynamicDowncast<LocalFrame>(child.frame))
                childFrame->didAccessWindowProxyPropertyViaOpener(property);
        }
    }

    // Scenario 2: some script in the child's tab tries to access the parent window via a WindowProxy object.
    // In this case, the parent is the target of the WindowProxy object, and we check that the child's main
    // frame's opener points to some tab in the parent.
    {
        auto& parent = *targetInfo;
        auto& child = *lexicalInfo;

        if (hasSameMainFrame(child.mainFrame->opener(), parent)) {
            logCrossTabPropertyAccess(child.mainFrame, propertyName);

            if (auto childMainFrame = dynamicDowncast<LocalFrame>(child.mainFrame))
                childMainFrame->didAccessWindowProxyPropertyViaOpener(property);
        }
    }
}

bool JSWindowProxy::getOwnPropertySlot(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, PropertySlot& slot)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(object), globalObject, propertyName);
    return Base::getOwnPropertySlot(object, globalObject, propertyName, slot);
}

bool JSWindowProxy::getOwnPropertySlotByIndex(JSObject* object, JSGlobalObject* globalObject, unsigned propertyName, PropertySlot& slot)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(object), globalObject, propertyName);
    return Base::getOwnPropertySlotByIndex(object, globalObject, propertyName, slot);
}

bool JSWindowProxy::put(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, JSValue value, PutPropertySlot& slot)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(cell), globalObject, propertyName);
    return Base::put(cell, globalObject, propertyName, value, slot);
}

bool JSWindowProxy::putByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName, JSValue value, bool shouldThrow)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(cell), globalObject, propertyName);
    return Base::putByIndex(cell, globalObject, propertyName, value, shouldThrow);
}

bool JSWindowProxy::deleteProperty(JSCell* cell, JSGlobalObject* globalObject, PropertyName propertyName, DeletePropertySlot& slot)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(cell), globalObject, propertyName);
    return Base::deleteProperty(cell, globalObject, propertyName, slot);
}

bool JSWindowProxy::deletePropertyByIndex(JSCell* cell, JSGlobalObject* globalObject, unsigned propertyName)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(cell), globalObject, propertyName);
    return Base::deletePropertyByIndex(cell, globalObject, propertyName);
}

bool JSWindowProxy::defineOwnProperty(JSObject* object, JSGlobalObject* globalObject, PropertyName propertyName, const PropertyDescriptor& descriptor, bool shouldThrow)
{
    checkCrossTabWindowProxyUsage(jsCast<JSWindowProxy*>(object), globalObject, propertyName);
    return Base::defineOwnProperty(object, globalObject, propertyName, descriptor, shouldThrow);
}

#endif // #if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)

} // namespace WebCore
