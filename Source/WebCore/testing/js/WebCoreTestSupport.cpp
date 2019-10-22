/*
 * Copyright (C) 2011, 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016-2019 Apple Inc. All rights reserved.
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
#include "WebCoreTestSupport.h"

#include "Frame.h"
#include "InternalSettings.h"
#include "Internals.h"
#include "JSDocument.h"
#include "JSInternals.h"
#include "JSServiceWorkerInternals.h"
#include "JSWorkerGlobalScope.h"
#include "LogInitialization.h"
#include "MockGamepadProvider.h"
#include "Page.h"
#include "SWContextManager.h"
#include "ServiceWorkerGlobalScope.h"
#include "WheelEventTestMonitor.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JSValueRef.h>
#include <wtf/URLParser.h>

#if PLATFORM(COCOA)
#include "UTIRegistry.h"
#endif

namespace WebCoreTestSupport {
using namespace JSC;
using namespace WebCore;

void injectInternalsObject(JSContextRef context)
{
    JSGlobalObject* lexicalGlobalObject = toJS(context);
    VM& vm = lexicalGlobalObject->vm();
    JSLockHolder lock(vm);
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(lexicalGlobalObject);
    ScriptExecutionContext* scriptContext = globalObject->scriptExecutionContext();
    if (is<Document>(*scriptContext)) {
        globalObject->putDirect(vm, Identifier::fromString(vm, Internals::internalsId), toJS(lexicalGlobalObject, globalObject, Internals::create(downcast<Document>(*scriptContext))));
        Options::useDollarVM() = true;
        globalObject->exposeDollarVM(vm);
    }
}

void resetInternalsObject(JSContextRef context)
{
    JSGlobalObject* lexicalGlobalObject = toJS(context);
    JSLockHolder lock(lexicalGlobalObject);
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(lexicalGlobalObject);
    ScriptExecutionContext* scriptContext = globalObject->scriptExecutionContext();
    Page* page = downcast<Document>(scriptContext)->frame()->page();
    Internals::resetToConsistentState(*page);
    InternalSettings::from(page)->resetToConsistentState();
}

void monitorWheelEvents(WebCore::Frame& frame)
{
    Page* page = frame.page();
    if (!page)
        return;

    page->ensureWheelEventTestMonitor();
}

void setTestCallbackAndStartNotificationTimer(WebCore::Frame& frame, JSContextRef context, JSObjectRef jsCallbackFunction)
{
    Page* page = frame.page();
    if (!page || !page->isMonitoringWheelEvents())
        return;

    JSValueProtect(context, jsCallbackFunction);
    
    page->ensureWheelEventTestMonitor().setTestCallbackAndStartNotificationTimer([=](void) {
        JSObjectCallAsFunction(context, jsCallbackFunction, nullptr, 0, nullptr, nullptr);
        JSValueUnprotect(context, jsCallbackFunction);
    });
}

void clearWheelEventTestMonitor(WebCore::Frame& frame)
{
    Page* page = frame.page();
    if (!page)
        return;
    
    page->clearWheelEventTestMonitor();
}

void setLogChannelToAccumulate(const String& name)
{
#if !LOG_DISABLED
    WebCore::setLogChannelToAccumulate(name);
#else
    UNUSED_PARAM(name);
#endif
}

void clearAllLogChannelsToAccumulate()
{
#if !LOG_DISABLED
    WebCore::clearAllLogChannelsToAccumulate();
#endif
}

void initializeLogChannelsIfNecessary()
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    WebCore::initializeLogChannelsIfNecessary();
#endif
}

void setAllowsAnySSLCertificate(bool allowAnySSLCertificate)
{
    InternalSettings::setAllowsAnySSLCertificate(allowAnySSLCertificate);
}

void installMockGamepadProvider()
{
#if ENABLE(GAMEPAD)
    GamepadProvider::setSharedProvider(MockGamepadProvider::singleton());
#endif
}

void connectMockGamepad(unsigned gamepadIndex)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().connectMockGamepad(gamepadIndex);
#else
    UNUSED_PARAM(gamepadIndex);
#endif
}

void disconnectMockGamepad(unsigned gamepadIndex)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().disconnectMockGamepad(gamepadIndex);
#else
    UNUSED_PARAM(gamepadIndex);
#endif
}

void setMockGamepadDetails(unsigned gamepadIndex, const WTF::String& gamepadID, unsigned axisCount, unsigned buttonCount)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadDetails(gamepadIndex, gamepadID, axisCount, buttonCount);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(gamepadID);
    UNUSED_PARAM(axisCount);
    UNUSED_PARAM(buttonCount);
#endif
}

void setMockGamepadAxisValue(unsigned gamepadIndex, unsigned axisIndex, double axisValue)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadAxisValue(gamepadIndex, axisIndex, axisValue);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(axisIndex);
    UNUSED_PARAM(axisValue);
#endif
}

void setMockGamepadButtonValue(unsigned gamepadIndex, unsigned buttonIndex, double buttonValue)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadButtonValue(gamepadIndex, buttonIndex, buttonValue);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(buttonIndex);
    UNUSED_PARAM(buttonValue);
#endif
}

void setupNewlyCreatedServiceWorker(uint64_t serviceWorkerIdentifier)
{
#if ENABLE(SERVICE_WORKER)
    auto identifier = makeObjectIdentifier<ServiceWorkerIdentifierType>(serviceWorkerIdentifier);
    SWContextManager::singleton().postTaskToServiceWorker(identifier, [identifier] (ServiceWorkerGlobalScope& globalScope) {
        auto* script = globalScope.script();
        if (!script)
            return;

        auto& state = *globalScope.execState();
        auto& vm = state.vm();
        JSLockHolder locker(vm);
        auto* contextWrapper = script->workerGlobalScopeWrapper();
        contextWrapper->putDirect(vm, Identifier::fromString(vm, Internals::internalsId), toJS(&state, contextWrapper, ServiceWorkerInternals::create(identifier)));
    });
#else
    UNUSED_PARAM(serviceWorkerIdentifier);
#endif
}

#if PLATFORM(COCOA)
void setAdditionalSupportedImageTypesForTesting(const WTF::String& imageTypes)
{
    WebCore::setAdditionalSupportedImageTypesForTesting(imageTypes);
}
#endif

}
