/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AccessibilityController.h"

#include "TestShell.h"
#include "WebAccessibilityCache.h"
#include "WebAccessibilityObject.h"
#include "WebFrame.h"
#include "WebString.h"
#include "WebView.h"

using namespace WebKit;

AccessibilityController::AccessibilityController(TestShell* shell)
    : m_shell(shell)
{

    bindMethod("dumpAccessibilityNotifications", &AccessibilityController::dumpAccessibilityNotifications);
    bindMethod("logFocusEvents", &AccessibilityController::logFocusEventsCallback);
    bindMethod("logScrollingStartEvents", &AccessibilityController::logScrollingStartEventsCallback);

    bindProperty("focusedElement", &AccessibilityController::focusedElementGetterCallback);
    bindProperty("rootElement", &AccessibilityController::rootElementGetterCallback);

    bindFallbackMethod(&AccessibilityController::fallbackCallback);
}

void AccessibilityController::bindToJavascript(WebFrame* frame, const WebString& classname)
{
    WebAccessibilityCache::enableAccessibility();
    CppBoundClass::bindToJavascript(frame, classname);
}

void AccessibilityController::reset()
{
    m_rootElement = WebAccessibilityObject();
    m_focusedElement = WebAccessibilityObject();
    m_elements.clear();
    
    m_dumpAccessibilityNotifications = false;
}

void AccessibilityController::setFocusedElement(const WebAccessibilityObject& focusedElement)
{
    m_focusedElement = focusedElement;
}

AccessibilityUIElement* AccessibilityController::getFocusedElement()
{
    if (m_focusedElement.isNull())
        m_focusedElement = m_shell->webView()->accessibilityObject();
    return m_elements.create(m_focusedElement);
}

AccessibilityUIElement* AccessibilityController::getRootElement()
{
    if (m_rootElement.isNull())
        m_rootElement = m_shell->webView()->accessibilityObject();
    return m_elements.createRoot(m_rootElement);
}

void AccessibilityController::dumpAccessibilityNotifications(const CppArgumentList&, CppVariant* result)
{
    m_dumpAccessibilityNotifications = true;
    result->setNull();
}

void AccessibilityController::logFocusEventsCallback(const CppArgumentList&, CppVariant* result)
{
    // As of r49031, this is not being used upstream.
    result->setNull();
}

void AccessibilityController::logScrollingStartEventsCallback(const CppArgumentList&, CppVariant* result)
{
    // As of r49031, this is not being used upstream.
    result->setNull();
}

void AccessibilityController::focusedElementGetterCallback(CppVariant* result)
{
    result->set(*(getFocusedElement()->getAsCppVariant()));
}

void AccessibilityController::rootElementGetterCallback(CppVariant* result)
{
    result->set(*(getRootElement()->getAsCppVariant()));
}

void AccessibilityController::fallbackCallback(const CppArgumentList&, CppVariant* result)
{
    printf("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on "
           "AccessibilityController\n");
    result->setNull();
}
