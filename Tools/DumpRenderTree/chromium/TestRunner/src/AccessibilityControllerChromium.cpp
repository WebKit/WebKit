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
#include "AccessibilityControllerChromium.h"

#include "WebAccessibilityObject.h"
#include "WebElement.h"
#include "WebFrame.h"
#include "WebNode.h"
#include "WebView.h"
#include "platform/WebCString.h"

using namespace WebKit;

namespace WebTestRunner {

AccessibilityController::AccessibilityController()
    : m_logAccessibilityEvents(false)
{

    bindMethod("logAccessibilityEvents", &AccessibilityController::logAccessibilityEventsCallback);
    bindMethod("addNotificationListener", &AccessibilityController::addNotificationListenerCallback);
    bindMethod("removeNotificationListener", &AccessibilityController::removeNotificationListenerCallback);

    bindProperty("focusedElement", &AccessibilityController::focusedElementGetterCallback);
    bindProperty("rootElement", &AccessibilityController::rootElementGetterCallback);

    bindMethod("accessibleElementById", &AccessibilityController::accessibleElementByIdGetterCallback);

    bindFallbackMethod(&AccessibilityController::fallbackCallback);
}

void AccessibilityController::bindToJavascript(WebFrame* frame, const WebString& classname)
{
    WebAccessibilityObject::enableAccessibility();
    CppBoundClass::bindToJavascript(frame, classname);
}

void AccessibilityController::reset()
{
    m_rootElement = WebAccessibilityObject();
    m_focusedElement = WebAccessibilityObject();
    m_elements.clear();

    m_logAccessibilityEvents = false;
}

void AccessibilityController::setFocusedElement(const WebAccessibilityObject& focusedElement)
{
    m_focusedElement = focusedElement;
}

AccessibilityUIElement* AccessibilityController::getFocusedElement()
{
    if (m_focusedElement.isNull())
        m_focusedElement = m_webView->accessibilityObject();
    return m_elements.getOrCreate(m_focusedElement);
}

AccessibilityUIElement* AccessibilityController::getRootElement()
{
    if (m_rootElement.isNull())
        m_rootElement = m_webView->accessibilityObject();
    return m_elements.createRoot(m_rootElement);
}

AccessibilityUIElement* AccessibilityController::findAccessibleElementByIdRecursive(const WebAccessibilityObject& obj, const WebString& id)
{
    if (obj.isNull() || obj.isDetached())
        return 0;

    WebNode node = obj.node();
    if (!node.isNull() && node.isElementNode()) {
        WebElement element = node.to<WebElement>();
        element.getAttribute("id");
        if (element.getAttribute("id") == id) 
            return m_elements.getOrCreate(obj);
    }

    unsigned childCount = obj.childCount();
    for (unsigned i = 0; i < childCount; i++) {
        if (AccessibilityUIElement* result = findAccessibleElementByIdRecursive(obj.childAt(i), id))
            return result;
    }

    return 0;
}

AccessibilityUIElement* AccessibilityController::getAccessibleElementById(const std::string& id)
{
    if (m_rootElement.isNull())
        m_rootElement = m_webView->accessibilityObject();

    if (!m_rootElement.updateBackingStoreAndCheckValidity())
        return 0;

    return findAccessibleElementByIdRecursive(m_rootElement, WebString::fromUTF8(id.c_str()));
}

bool AccessibilityController::shouldLogAccessibilityEvents()
{
    return m_logAccessibilityEvents;
}

void AccessibilityController::notificationReceived(const WebKit::WebAccessibilityObject& target, const char* notificationName)
{
    // Call notification listeners on the element.
    AccessibilityUIElement* element = m_elements.getOrCreate(target);
    element->notificationReceived(notificationName);

    // Call global notification listeners.
    size_t callbackCount = m_notificationCallbacks.size();
    for (size_t i = 0; i < callbackCount; i++) {
        CppVariant arguments[2];
        arguments[0].set(*element->getAsCppVariant());
        arguments[1].set(notificationName);
        CppVariant invokeResult;
        m_notificationCallbacks[i].invokeDefault(arguments, 2, invokeResult);
    }
}

void AccessibilityController::logAccessibilityEventsCallback(const CppArgumentList&, CppVariant* result)
{
    m_logAccessibilityEvents = true;
    result->setNull();
}

void AccessibilityController::addNotificationListenerCallback(const CppArgumentList& arguments, CppVariant* result)
{
    if (arguments.size() < 1 || !arguments[0].isObject()) {
        result->setNull();
        return;
    }

    m_notificationCallbacks.push_back(arguments[0]);
    result->setNull();
}

void AccessibilityController::removeNotificationListenerCallback(const CppArgumentList&, CppVariant* result)
{
    // FIXME: Implement this.
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

void AccessibilityController::accessibleElementByIdGetterCallback(const CppArgumentList& arguments, CppVariant* result)
{
    result->setNull();

    if (arguments.size() < 1 || !arguments[0].isString())
        return;

    std::string id = arguments[0].toString();
    AccessibilityUIElement* foundElement = getAccessibleElementById(id);
    if (!foundElement)
        return;

    result->set(*(foundElement->getAsCppVariant()));
}

void AccessibilityController::fallbackCallback(const CppArgumentList&, CppVariant* result)
{
    printf("CONSOLE MESSAGE: JavaScript ERROR: unknown method called on "
           "AccessibilityController\n");
    result->setNull();
}

}
