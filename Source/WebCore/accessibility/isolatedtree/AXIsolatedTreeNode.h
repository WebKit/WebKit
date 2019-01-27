/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#pragma once

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "AccessibilityObjectInterface.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
OBJC_CLASS WebAccessibilityObjectWrapper;
#endif

namespace WebCore {

class AccessibilityObject;

class AXIsolatedTreeNode final : public AccessibilityObjectInterface, public ThreadSafeRefCounted<AXIsolatedTreeNode>, public CanMakeWeakPtr<AXIsolatedTreeNode> {

public:
    enum class AXPropertyName : uint8_t {
        None = 0,
        RoleValue = 1,
        IsAttachment,
        IsMediaControlLabel,
    };

    static Ref<AXIsolatedTreeNode> create(const AccessibilityObject&);
    virtual ~AXIsolatedTreeNode();

    AXID identifier() const { return m_identifier; }

    bool isRootNode() const { return m_isRootNode; }
    void setIsRootNode(bool value)
    {
        ASSERT(isMainThread());
        m_isRootNode = value;
    }

    void setParent(AXID parent)
    {
        ASSERT(isMainThread());
        m_parent = parent;
    }
    AXID parent() const { return m_parent; }

    void appendChild(AXID);
    const Vector<AXID>& children() const { return m_children; };

#if PLATFORM(COCOA)
    WebAccessibilityObjectWrapper* wrapper() const { return m_wrapper.get(); }
    void setWrapper(WebAccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }
#endif

protected:
    AXIsolatedTreeNode() = default;

private:
    AXIsolatedTreeNode(const AccessibilityObject&);
    void initializeAttributeData(const AccessibilityObject&);

    bool isMediaControlLabel() const override { return boolAttributeValue(AXPropertyName::IsMediaControlLabel); }
    bool isAttachment() const override { return boolAttributeValue(AXPropertyName::IsAttachment); }
    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }

    using AttributeValueVariant = Variant<std::nullptr_t, String, bool, int, unsigned, double>;
    void setProperty(AXPropertyName, AttributeValueVariant&&, bool shouldRemove = false);

    bool boolAttributeValue(AXPropertyName) const;
    const String& stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;

    AXID m_parent;
    AXID m_identifier;
    bool m_isRootNode;
    bool m_initialized;
    Vector<AXID> m_children;

#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#endif

    HashMap<AXPropertyName, AttributeValueVariant, WTF::IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>> m_attributeMap;
};

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
