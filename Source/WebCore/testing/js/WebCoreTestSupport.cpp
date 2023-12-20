/*
 * Copyright (C) 2011, 2015 Google Inc. All rights reserved.
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "DeprecatedGlobalSettings.h"
#include "DocumentFragment.h"
#include "FrameDestructionObserverInlines.h"
#include "InternalSettings.h"
#include "Internals.h"
#include "JSDocument.h"
#include "JSInternals.h"
#include "JSServiceWorkerInternals.h"
#include "JSWorkerGlobalScope.h"
#include "LocalFrame.h"
#include "LogInitialization.h"
#include "Logging.h"
#include "MockGamepadProvider.h"
#include "Page.h"
#include "ProcessWarming.h"
#include "SWContextManager.h"
#include "ServiceWorkerGlobalScope.h"
#include "SincResampler.h"
#include "WheelEventTestMonitor.h"
#include "XMLDocument.h"
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/CallFrame.h>
#include <JavaScriptCore/IdentifierInlines.h>
#include <JavaScriptCore/JITOperationList.h>
#include <JavaScriptCore/JSValueRef.h>
#include <wtf/URLParser.h>

#if PLATFORM(COCOA)
#include "UTIRegistry.h"
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#endif

namespace WebCoreTestSupport {
using namespace JSC;
using namespace WebCore;

void initializeNames()
{
    ProcessWarming::initializeNames();
}

void injectInternalsObject(JSContextRef context)
{
    JSGlobalObject* lexicalGlobalObject = toJS(context);
    VM& vm = lexicalGlobalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);
    JSLockHolder lock(vm);
    JSDOMGlobalObject* globalObject = jsCast<JSDOMGlobalObject*>(lexicalGlobalObject);
    ScriptExecutionContext* scriptContext = globalObject->scriptExecutionContext();
    if (is<Document>(*scriptContext)) {
        globalObject->putDirect(vm, Identifier::fromString(vm, Internals::internalsId), toJS(lexicalGlobalObject, globalObject, Internals::create(downcast<Document>(*scriptContext))));
        Options::useDollarVM() = true;
        globalObject->exposeDollarVM(vm);
    }
    EXCEPTION_ASSERT_UNUSED(scope, !scope.exception());
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

void monitorWheelEvents(WebCore::LocalFrame& frame, bool clearLatchingState)
{
    Page* page = frame.page();
    if (!page)
        return;

    page->startMonitoringWheelEvents(clearLatchingState);
}

void setWheelEventMonitorTestCallbackAndStartMonitoring(bool expectWheelEndOrCancel, bool expectMomentumEnd, WebCore::LocalFrame& frame, JSContextRef context, JSObjectRef jsCallbackFunction)
{
    Page* page = frame.page();
    if (!page || !page->isMonitoringWheelEvents())
        return;

    JSValueProtect(context, jsCallbackFunction);

    if (auto wheelEventTestMonitor = page->wheelEventTestMonitor()) {
        wheelEventTestMonitor->setTestCallbackAndStartMonitoring(expectWheelEndOrCancel, expectMomentumEnd, [=](void) {
            JSObjectCallAsFunction(context, jsCallbackFunction, nullptr, 0, nullptr, nullptr);
            JSValueUnprotect(context, jsCallbackFunction);
        });
    }
}

void clearWheelEventTestMonitor(WebCore::LocalFrame& frame)
{
    Page* page = frame.page();
    if (!page)
        return;

    page->clearWheelEventTestMonitor();
}

void setLogChannelToAccumulate(const String& name)
{
#if !LOG_DISABLED
    logChannels().setLogChannelToAccumulate(name);
#else
    UNUSED_PARAM(name);
#endif
}

void clearAllLogChannelsToAccumulate()
{
#if !LOG_DISABLED
    logChannels().clearAllLogChannelsToAccumulate();
#endif
}

void initializeLogChannelsIfNecessary()
{
#if !LOG_DISABLED || !RELEASE_LOG_DISABLED
    logChannels().initializeLogChannelsIfNecessary();
#endif
}

void setAllowsAnySSLCertificate(bool allowAnySSLCertificate)
{
    DeprecatedGlobalSettings::setAllowsAnySSLCertificate(allowAnySSLCertificate);
}

bool allowsAnySSLCertificate()
{
    return DeprecatedGlobalSettings::allowsAnySSLCertificate();
}

void setLinkedOnOrAfterEverythingForTesting()
{
#if PLATFORM(COCOA)
    enableAllSDKAlignedBehaviors();
#endif
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

void setMockGamepadDetails(unsigned gamepadIndex, const String& gamepadID, const String& mapping, unsigned axisCount, unsigned buttonCount, bool supportsDualRumble)
{
#if ENABLE(GAMEPAD)
    MockGamepadProvider::singleton().setMockGamepadDetails(gamepadIndex, gamepadID, mapping, axisCount, buttonCount, supportsDualRumble);
#else
    UNUSED_PARAM(gamepadIndex);
    UNUSED_PARAM(gamepadID);
    UNUSED_PARAM(mapping);
    UNUSED_PARAM(axisCount);
    UNUSED_PARAM(buttonCount);
    UNUSED_PARAM(supportsDualRumble);
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
    auto identifier = AtomicObjectIdentifier<ServiceWorkerIdentifierType>(serviceWorkerIdentifier);
    SWContextManager::singleton().postTaskToServiceWorker(identifier, [identifier] (ServiceWorkerGlobalScope& globalScope) {
        auto* script = globalScope.script();
        if (!script)
            return;

        auto& globalObject = *globalScope.globalObject();
        auto& vm = globalObject.vm();
        JSLockHolder locker(vm);
        auto* contextWrapper = script->globalScopeWrapper();
        contextWrapper->putDirect(vm, Identifier::fromString(vm, Internals::internalsId), toJS(&globalObject, contextWrapper, ServiceWorkerInternals::create(globalScope, identifier)));
    });
}

#if PLATFORM(COCOA)
void setAdditionalSupportedImageTypesForTesting(const String& imageTypes)
{
    WebCore::setAdditionalSupportedImageTypesForTesting(imageTypes);
}
#endif

#if ENABLE(JIT_OPERATION_VALIDATION) || ENABLE(JIT_OPERATION_DISASSEMBLY)

extern const JSC::JITOperationAnnotation startOfJITOperationsInWebCoreTestSupport __asm("section$start$__DATA_CONST$__jsc_ops");
extern const JSC::JITOperationAnnotation endOfJITOperationsInWebCoreTestSupport __asm("section$end$__DATA_CONST$__jsc_ops");

#if ENABLE(JIT_OPERATION_VALIDATION)

void populateJITOperations()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        JSC::JITOperationList::populatePointersInEmbedder(&startOfJITOperationsInWebCoreTestSupport, &endOfJITOperationsInWebCoreTestSupport);
    });
#if ENABLE(JIT_OPERATION_DISASSEMBLY)
    if (UNLIKELY(JSC::Options::needDisassemblySupport()))
        populateDisassemblyLabels();
#endif
}
#endif // ENABLE(JIT_OPERATION_VALIDATION)

#if ENABLE(JIT_OPERATION_DISASSEMBLY)
void populateDisassemblyLabels()
{
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        JSC::JITOperationList::populateDisassemblyLabelsInEmbedder(&startOfJITOperationsInWebCoreTestSupport, &endOfJITOperationsInWebCoreTestSupport);
    });
}
#endif // ENABLE(JIT_OPERATION_DISASSEMBLY)

#endif // ENABLE(JIT_OPERATION_VALIDATION) || ENABLE(JIT_OPERATION_DISASSEMBLY)

#if ENABLE(WEB_AUDIO)
void testSincResamplerProcessBuffer(std::span<const float> source, std::span<float> destination, double scaleFactor)
{
    SincResampler::processBuffer(source, destination, scaleFactor);
}
#endif // ENABLE(WEB_AUDIO)

bool testDocumentFragmentParseXML(const String& chunk, OptionSet<ParserContentPolicy> parserContentPolicy)
{
    ProcessWarming::prewarmGlobally();

    auto settings = Settings::create(nullptr);
    auto document = WebCore::XMLDocument::createXHTML(nullptr, settings, URL());
    auto fragment = document->createDocumentFragment();

    return fragment->parseXML(chunk, nullptr, parserContentPolicy);
}

} // namespace WebCoreTestSupport
