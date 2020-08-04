/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "AccessibilityController.h"

#if ENABLE(ACCESSIBILITY)

#include "AccessibilityUIElement.h"
#include "InjectedBundle.h"
#include "InjectedBundlePage.h"
#include "JSAccessibilityController.h"
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include <pal/spi/cocoa/AccessibilitySupportSPI.h>
#include <pal/spi/mac/HIServicesSPI.h>
#endif
#include <WebKit/WKBundle.h>
#include <WebKit/WKBundlePage.h>
#include <WebKit/WKBundlePagePrivate.h>

namespace WTR {

Ref<AccessibilityController> AccessibilityController::create()
{
    return adoptRef(*new AccessibilityController);
}

AccessibilityController::AccessibilityController()
{
}

AccessibilityController::~AccessibilityController()
{
}

void AccessibilityController::setIsolatedTreeMode(bool flag)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_accessibilityIsolatedTreeMode != flag) {
        m_accessibilityIsolatedTreeMode = flag;
        updateIsolatedTreeMode();
    }
#endif
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AccessibilityController::updateIsolatedTreeMode()
{
    // Override to set identifier to VoiceOver so that requests are handled in isolated mode.
    _AXSetClientIdentificationOverride(m_accessibilityIsolatedTreeMode ? (AXClientType)kAXClientTypeWebKitTesting : kAXClientTypeNoActiveRequestFound);
    _AXSSetIsolatedTreeMode(m_accessibilityIsolatedTreeMode ? AXSIsolatedTreeModeMainThread : AXSIsolatedTreeModeOff);
    m_useMockAXThread = WKAccessibilityCanUseSecondaryAXThread(InjectedBundle::singleton().page()->page());
}
#endif

void AccessibilityController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    setProperty(context, windowObject, "accessibilityController", this, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef AccessibilityController::wrapperClass()
{
    return JSAccessibilityController::accessibilityControllerClass();
}

void AccessibilityController::enableEnhancedAccessibility(bool enable)
{
    WKAccessibilityEnableEnhancedAccessibility(enable);
}

bool AccessibilityController::enhancedAccessibilityEnabled()
{
    return WKAccessibilityEnhancedAccessibilityEnabled();
}

#if PLATFORM(COCOA)
Ref<AccessibilityUIElement> AccessibilityController::rootElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    PlatformUIElement root = static_cast<PlatformUIElement>(WKAccessibilityRootObject(page));

    return AccessibilityUIElement::create(root);
}

Ref<AccessibilityUIElement> AccessibilityController::focusedElement()
{
    WKBundlePageRef page = InjectedBundle::singleton().page()->page();
    PlatformUIElement focusedElement = static_cast<PlatformUIElement>(WKAccessibilityFocusedObject(page));
    return AccessibilityUIElement::create(focusedElement);
}

void AccessibilityController::executeOnAXThreadAndWait(Function<void()>&& function)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_useMockAXThread) {
        AXThread::dispatch([&function, this] {
            function();
            m_semaphore.signal();
        });

        m_semaphore.wait();
    } else
#endif
        function();
}

void AccessibilityController::executeOnAXThread(Function<void()>&& function)
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (m_useMockAXThread) {
        AXThread::dispatch([function = WTFMove(function)] {
            function();
        });
    } else
#endif
        function();
}

void AccessibilityController::executeOnMainThread(Function<void()>&& function)
{
    if (isMainThread()) {
        function();
        return;
    }

    AXThread::dispatchBarrier([function = WTFMove(function)] {
        function();
    });
}
#endif // PLATFORM(COCOA)

RefPtr<AccessibilityUIElement> AccessibilityController::elementAtPoint(int x, int y)
{
    auto uiElement = rootElement();
    return uiElement->elementAtPoint(x, y);
}

#if PLATFORM(COCOA)

// AXThread implementation

AXThread::AXThread()
{
}

bool AXThread::isCurrentThread()
{
    return AXThread::singleton().m_thread == &Thread::current();
}

void AXThread::dispatch(Function<void()>&& function)
{
    auto& axThread = AXThread::singleton();
    axThread.createThreadIfNeeded();

    {
        auto locker = holdLock(axThread.m_functionsMutex);
        axThread.m_functions.append(WTFMove(function));
    }

    axThread.wakeUpRunLoop();
}

void AXThread::dispatchBarrier(Function<void()>&& function)
{
    dispatch([function = WTFMove(function)] () mutable {
        callOnMainThread(WTFMove(function));
    });
}

AXThread& AXThread::singleton()
{
    static NeverDestroyed<AXThread> axThread;
    return axThread;
}

void AXThread::createThreadIfNeeded()
{
    // Wait for the thread to initialize the run loop.
    std::unique_lock<Lock> lock(m_initializeRunLoopMutex);

    if (!m_thread) {
        m_thread = Thread::create("WKTR: AccessibilityController", [this] {
            WTF::Thread::setCurrentThreadIsUserInteractive();
            initializeRunLoop();
        });
    }

    m_initializeRunLoopConditionVariable.wait(lock, [this] {
#if PLATFORM(COCOA)
        return m_threadRunLoop;
#else
        return m_runLoop;
#endif
    });
}

void AXThread::dispatchFunctionsFromAXThread()
{
    ASSERT(isCurrentThread());

    Vector<Function<void()>> functions;

    {
        auto locker = holdLock(m_functionsMutex);
        functions = WTFMove(m_functions);
    }

    for (auto& function : functions)
        function();
}

#if !PLATFORM(MAC)
NO_RETURN_DUE_TO_ASSERT void AXThread::initializeRunLoop()
{
    ASSERT_NOT_REACHED();
}

void AXThread::wakeUpRunLoop()
{
}

void AXThread::threadRunLoopSourceCallback(void*)
{
}

void AXThread::threadRunLoopSourceCallback()
{
}
#endif // !PLATFORM(MAC)

#endif // PLATFORM(COCOA)

} // namespace WTR
#endif // ENABLE(ACCESSIBILITY)

