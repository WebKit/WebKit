/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "AccessibilityUIElementClientMac.h"

#if PLATFORM(MAC)

#import "DictionaryFunctions.h"
#import "InjectedBundle.h"
#import <JavaScriptCore/JSRetainPtr.h>
#import <JavaScriptCore/JSStringRef.h>
#import <JavaScriptCore/JSValueRef.h>
#import <JavaScriptCore/OpaqueJSString.h>
#import <WebKit/WKBundle.h>
#import <WebKit/WKBundlePrivate.h>
#import <wtf/RetainPtr.h>

namespace WTR {

// IPC helper functions for client accessibility
static uint64_t axGetRoot()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXGetRoot").get(), nullptr, &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKUInt64GetTypeID())
        return 0;

    uint64_t token = WKUInt64GetValue(static_cast<WKUInt64Ref>(returnData));
    WKRelease(returnData);
    return token;
}

static WKRetainPtr<WKStringRef> axCopyAttributeValueAsString(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsString").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKStringGetTypeID())
        return nullptr;

    return adoptWK(static_cast<WKStringRef>(returnData));
}

static std::optional<uint64_t> axCopyAttributeValueAsElementToken(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsElement").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKUInt64GetTypeID())
        return std::nullopt;

    uint64_t token = WKUInt64GetValue(static_cast<WKUInt64Ref>(returnData));
    WKRelease(returnData);
    return token;
}

static WKRetainPtr<WKArrayRef> axCopyAttributeValueAsElementArray(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsElementArray").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKArrayGetTypeID())
        return nullptr;

    return adoptWK(static_cast<WKArrayRef>(returnData));
}

static std::optional<double> axCopyAttributeValueAsNumber(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsNumber").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKDoubleGetTypeID())
        return std::nullopt;

    double value = WKDoubleGetValue(static_cast<WKDoubleRef>(returnData));
    WKRelease(returnData);
    return value;
}

static std::pair<double, double> axCopyAttributeValueAsPoint(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsPoint").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKDictionaryGetTypeID())
        return { 0, 0 };

    auto resultDict = static_cast<WKDictionaryRef>(returnData);
    double x = doubleValue(resultDict, "x");
    double y = doubleValue(resultDict, "y");
    WKRelease(returnData);
    return { x, y };
}

static std::pair<double, double> axCopyAttributeValueAsSize(uint64_t elementToken, const char* attributeName)
{
    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    setValue(dictionary, "elementToken", elementToken);
    setValue(dictionary, "attributeName", attributeName);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXCopyAttributeValueAsSize").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKDictionaryGetTypeID())
        return { 0, 0 };

    auto resultDict = static_cast<WKDictionaryRef>(returnData);
    double width = doubleValue(resultDict, "width");
    double height = doubleValue(resultDict, "height");
    WKRelease(returnData);
    return { width, height };
}

Ref<AccessibilityUIElementClientMac> AccessibilityUIElementClientMac::create(uint64_t elementToken)
{
    return adoptRef(*new AccessibilityUIElementClientMac(elementToken));
}

Ref<AccessibilityUIElementClientMac> AccessibilityUIElementClientMac::create(const AccessibilityUIElementClientMac& other)
{
    return adoptRef(*new AccessibilityUIElementClientMac(other));
}

Ref<AccessibilityUIElementClientMac> AccessibilityUIElementClientMac::createForUIProcess()
{
    return create(axGetRoot());
}

AccessibilityUIElementClientMac::AccessibilityUIElementClientMac(uint64_t elementToken)
    : AccessibilityUIElement(nullptr)
    , m_elementToken(elementToken)
{
}

AccessibilityUIElementClientMac::AccessibilityUIElementClientMac(const AccessibilityUIElementClientMac& other)
    : AccessibilityUIElement(other)
    , m_elementToken(other.m_elementToken)
{
}

AccessibilityUIElementClientMac::~AccessibilityUIElementClientMac()
{
}

PlatformUIElement AccessibilityUIElementClientMac::platformUIElement()
{
    // Client elements don't have a local platform element
    return nullptr;
}

bool AccessibilityUIElementClientMac::isValid() const
{
    return m_elementToken;
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::getStringAttribute(const char* attributeName) const
{
    if (!isValid())
        return nullptr;

    WKRetainPtr value = axCopyAttributeValueAsString(m_elementToken, attributeName);
    if (!value)
        return nullptr;

    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(toWTFString(value.get())).leakRef());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::role()
{
    if (!isValid())
        return nullptr;

    WKRetainPtr value = axCopyAttributeValueAsString(m_elementToken, "AXRole");
    if (!value)
        return nullptr;

    String roleString = toWTFString(value.get());
    String result = makeString("AXRole: "_s, roleString);
    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(result).leakRef());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::title()
{
    return getStringAttribute("AXTitle");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::description()
{
    return getStringAttribute("AXDescription");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::debugDescription()
{
    return getStringAttribute("_AXDebugDescription");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::rawRoleForTesting()
{
    return getStringAttribute("_AXRawRoleForTesting");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::stringValue()
{
    return getStringAttribute("AXValue");
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::domIdentifier() const
{
    return getStringAttribute("AXDOMIdentifier");
}

double AccessibilityUIElementClientMac::getNumberAttribute(const char* attributeName) const
{
    if (!isValid())
        return 0;

    return axCopyAttributeValueAsNumber(m_elementToken, attributeName).value_or(0);
}

int AccessibilityUIElementClientMac::hierarchicalLevel() const
{
    return static_cast<int>(getNumberAttribute("AXDisclosureLevel"));
}

double AccessibilityUIElementClientMac::minValue()
{
    return getNumberAttribute("AXMinValue");
}

double AccessibilityUIElementClientMac::maxValue()
{
    return getNumberAttribute("AXMaxValue");
}

double AccessibilityUIElementClientMac::x()
{
    if (!isValid())
        return 0;
    auto [x, y] = axCopyAttributeValueAsPoint(m_elementToken, "AXPosition");
    return x;
}

double AccessibilityUIElementClientMac::y()
{
    if (!isValid())
        return 0;
    auto [x, y] = axCopyAttributeValueAsPoint(m_elementToken, "AXPosition");
    return y;
}

double AccessibilityUIElementClientMac::width()
{
    if (!isValid())
        return 0;
    auto [width, height] = axCopyAttributeValueAsSize(m_elementToken, "AXSize");
    return width;
}

double AccessibilityUIElementClientMac::height()
{
    if (!isValid())
        return 0;
    auto [width, height] = axCopyAttributeValueAsSize(m_elementToken, "AXSize");
    return height;
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementClientMac::getChildren() const
{
    Vector<RefPtr<AccessibilityUIElement>> children;
    if (!isValid())
        return children;

    WKRetainPtr value = axCopyAttributeValueAsElementArray(m_elementToken, "AXChildren");
    if (!value)
        return children;

    size_t count = WKArrayGetSize(value.get());
    for (size_t i = 0; i < count; i++) {
        WKTypeRef item = WKArrayGetItemAtIndex(value.get(), i);
        if (WKGetTypeID(item) == WKUInt64GetTypeID()) {
            uint64_t childToken = WKUInt64GetValue(static_cast<WKUInt64Ref>(item));
            children.append(AccessibilityUIElementClientMac::create(childToken));
        }
    }

    return children;
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementClientMac::getChildrenInRange(unsigned location, unsigned length) const
{
    Vector allChildren = getChildren();

    if (location >= allChildren.size())
        return { };

    unsigned end = std::min(location + length, static_cast<unsigned>(allChildren.size()));
    Vector<RefPtr<AccessibilityUIElement>> result;
    result.reserveInitialCapacity(end - location);

    for (unsigned i = location; i < end; i++)
        result.append(allChildren[i]);

    return result;
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementClientMac::parentElement()
{
    std::optional token = axCopyAttributeValueAsElementToken(m_elementToken, "AXParent");
    return token ? create(*token).ptr() : nullptr;
}

unsigned AccessibilityUIElementClientMac::childrenCount()
{
    return getChildren().size();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementClientMac::childAtIndex(unsigned index)
{
    Vector children = getChildrenInRange(index, 1);
    return children.size() == 1 ? children[0] : nullptr;
}

JSValueRef AccessibilityUIElementClientMac::uiElementsForSearchPredicate(JSContextRef context, AccessibilityUIElement* startElement, bool isDirectionNext, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly, unsigned resultsLimit)
{
    if (!isValid())
        return nullptr;

    WKRetainPtr dictionary = adoptWK(WKMutableDictionaryCreate());
    WTR::setValue(dictionary, "elementToken", m_elementToken);

    uint64_t startElementToken = 0;
    if (startElement) {
        // In client mode, all elements are AccessibilityUIElementClientMac.
        startElementToken = static_cast<AccessibilityUIElementClientMac*>(startElement)->m_elementToken;
    }
    WTR::setValue(dictionary, "startElementToken", startElementToken);

    WTR::setValue(dictionary, "isDirectionNext", isDirectionNext);
    WTR::setValue(dictionary, "resultsLimit", static_cast<uint64_t>(resultsLimit));
    WTR::setValue(dictionary, "visibleOnly", visibleOnly);
    WTR::setValue(dictionary, "immediateDescendantsOnly", immediateDescendantsOnly);

    if (searchKey && JSValueIsString(context, searchKey)) {
        JSRetainPtr<JSStringRef> jsStr(Adopt, JSValueToStringCopy(context, searchKey, nullptr));
        WTR::setValue(dictionary, "searchKey", jsStr.get());
    }

    if (searchText && JSStringGetLength(searchText))
        WTR::setValue(dictionary, "searchText", searchText);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    WKTypeRef returnData = nullptr;
    WKBundlePostSynchronousMessage(InjectedBundle::singleton().bundle(), toWK("AXSearchPredicate").get(), dictionary.get(), &returnData);
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!returnData || WKGetTypeID(returnData) != WKArrayGetTypeID())
        return nullptr;

    WKArrayRef resultArray = static_cast<WKArrayRef>(returnData);
    size_t count = WKArrayGetSize(resultArray);

    Vector<RefPtr<AccessibilityUIElement>> elements;
    elements.reserveInitialCapacity(count);
    for (size_t i = 0; i < count; i++) {
        WKTypeRef item = WKArrayGetItemAtIndex(resultArray, i);
        if (WKGetTypeID(item) == WKUInt64GetTypeID()) {
            uint64_t childToken = WKUInt64GetValue(static_cast<WKUInt64Ref>(item));
            elements.append(AccessibilityUIElementClientMac::create(childToken));
        }
    }

    WKRelease(returnData);
    return makeJSArray(context, elements);
}

} // namespace WTR

#endif // PLATFORM(MAC)
