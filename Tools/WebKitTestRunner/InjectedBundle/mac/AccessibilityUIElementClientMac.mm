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
    auto dictionary = adoptWK(WKMutableDictionaryCreate());
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

static WKRetainPtr<WKArrayRef> axCopyAttributeValueAsElementArray(uint64_t elementToken, const char* attributeName)
{
    auto dictionary = adoptWK(WKMutableDictionaryCreate());
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

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::role()
{
    if (!isValid())
        return nullptr;

    auto value = axCopyAttributeValueAsString(m_elementToken, "AXRole");
    if (!value)
        return nullptr;

    auto roleString = toWTFString(value.get());
    auto result = makeString("AXRole: "_s, roleString);
    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(result).leakRef());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::title()
{
    if (!isValid())
        return nullptr;

    auto value = axCopyAttributeValueAsString(m_elementToken, "AXTitle");
    if (!value)
        return nullptr;

    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(toWTFString(value.get())).leakRef());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::description()
{
    if (!isValid())
        return nullptr;

    auto value = axCopyAttributeValueAsString(m_elementToken, "AXDescription");
    if (!value)
        return nullptr;

    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(toWTFString(value.get())).leakRef());
}

JSRetainPtr<JSStringRef> AccessibilityUIElementClientMac::stringValue()
{
    if (!isValid())
        return nullptr;

    auto value = axCopyAttributeValueAsString(m_elementToken, "AXValue");
    if (!value)
        return nullptr;

    return JSRetainPtr<JSStringRef>(Adopt, OpaqueJSString::tryCreate(toWTFString(value.get())).leakRef());
}

Vector<RefPtr<AccessibilityUIElement>> AccessibilityUIElementClientMac::getChildren() const
{
    Vector<RefPtr<AccessibilityUIElement>> children;
    if (!isValid())
        return children;

    auto value = axCopyAttributeValueAsElementArray(m_elementToken, "AXChildren");
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
    auto allChildren = getChildren();

    if (location >= allChildren.size())
        return { };

    unsigned end = std::min(location + length, static_cast<unsigned>(allChildren.size()));
    Vector<RefPtr<AccessibilityUIElement>> result;
    result.reserveInitialCapacity(end - location);

    for (unsigned i = location; i < end; i++)
        result.append(allChildren[i]);

    return result;
}

unsigned AccessibilityUIElementClientMac::childrenCount()
{
    return getChildren().size();
}

RefPtr<AccessibilityUIElement> AccessibilityUIElementClientMac::childAtIndex(unsigned index)
{
    auto children = getChildrenInRange(index, 1);
    return children.size() == 1 ? children[0] : nullptr;
}

} // namespace WTR

#endif // PLATFORM(MAC)
