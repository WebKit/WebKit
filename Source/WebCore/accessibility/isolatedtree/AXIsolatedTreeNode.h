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

#include "AXIsolatedTree.h"
#include "AccessibilityObjectInterface.h"
#include "FloatRect.h"
#include "IntPoint.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AXIsolatedTree;
class AccessibilityObject;

class AXIsolatedTreeNode final : public AccessibilityObjectInterface, public ThreadSafeRefCounted<AXIsolatedTreeNode>, public CanMakeWeakPtr<AXIsolatedTreeNode> {

public:
    enum class AXPropertyName : uint8_t {
        None = 0,
        HelpText,
        IsAccessibilityIgnored,
        IsAttachment,
        IsFileUploadButton,
        IsImage,
        IsImageMapLink,
        IsLink,
        IsMediaControlLabel,
        IsScrollbar,
        IsTree,
        IsTreeItem,
        Description,
        RelativeFrame,
        RoleValue,
        SpeechHint,
        Title,
    };

    static Ref<AXIsolatedTreeNode> create(const AccessibilityObject&);
    virtual ~AXIsolatedTreeNode();

    AXID identifier() const { return m_identifier; }
    
    void setParent(AXID);
    AXID parent() const { return m_parent; }

    void appendChild(AXID);
    const Vector<AXID>& children() const { return m_children; };

    AXIsolatedTree* tree() const;
    AXIsolatedTreeID treeIdentifier() const { return m_treeIdentifier; }
    void setTreeIdentifier(AXIsolatedTreeID);

#if PLATFORM(COCOA)
    AccessibilityObjectWrapper* wrapper() const override { return m_wrapper.get(); }
    void setWrapper(AccessibilityObjectWrapper* wrapper) { m_wrapper = wrapper; }
#endif

protected:
    AXIsolatedTreeNode() = default;

private:
    AXIsolatedTreeNode(const AccessibilityObject&);
    void initializeAttributeData(const AccessibilityObject&);

    AccessibilityObjectInterface* accessibilityHitTest(const IntPoint&) const override;
    void updateChildrenIfNecessary() override { }

    bool isMediaControlLabel() const override { return boolAttributeValue(AXPropertyName::IsMediaControlLabel); }
    bool isAttachment() const override { return boolAttributeValue(AXPropertyName::IsAttachment); }
    AccessibilityRole roleValue() const override { return static_cast<AccessibilityRole>(intAttributeValue(AXPropertyName::RoleValue)); }
    bool isLink() const override { return boolAttributeValue(AXPropertyName::IsLink); }
    bool isImageMapLink() const override { return boolAttributeValue(AXPropertyName::IsImageMapLink); }
    bool isImage() const override { return boolAttributeValue(AXPropertyName::IsImage); }
    bool isFileUploadButton() const override { return boolAttributeValue(AXPropertyName::IsFileUploadButton); }
    bool accessibilityIsIgnored() const override { return boolAttributeValue(AXPropertyName::IsAccessibilityIgnored); }
    AccessibilityObjectInterface* parentObjectInterfaceUnignored() const override;
    bool isTree() const override { return boolAttributeValue(AXPropertyName::IsTree); }
    bool isTreeItem() const override { return boolAttributeValue(AXPropertyName::IsTreeItem); }
    bool isScrollbar() const override { return boolAttributeValue(AXPropertyName::IsScrollbar); }
    FloatRect relativeFrame() const override { return rectAttributeValue(AXPropertyName::RelativeFrame); }
    String speechHintAttributeValue() const override { return stringAttributeValue(AXPropertyName::SpeechHint); }
    String descriptionAttributeValue() const override { return stringAttributeValue(AXPropertyName::Description); }
    String helpTextAttributeValue() const override { return stringAttributeValue(AXPropertyName::HelpText); }
    String titleAttributeValue() const override { return stringAttributeValue(AXPropertyName::Title); }
    AccessibilityObjectInterface* focusedUIElement() const override;

    using AttributeValueVariant = Variant<std::nullptr_t, String, bool, int, unsigned, double, Optional<FloatRect>>;
    void setProperty(AXPropertyName, AttributeValueVariant&&, bool shouldRemove = false);

    bool boolAttributeValue(AXPropertyName) const;
    const String stringAttributeValue(AXPropertyName) const;
    int intAttributeValue(AXPropertyName) const;
    unsigned unsignedAttributeValue(AXPropertyName) const;
    double doubleAttributeValue(AXPropertyName) const;
    FloatRect rectAttributeValue(AXPropertyName) const;

    AXID m_parent;
    AXID m_identifier;
    bool m_initialized { false };
    AXIsolatedTreeID m_treeIdentifier;
    WeakPtr<AXIsolatedTree> m_cachedTree;
    Vector<AXID> m_children;

#if PLATFORM(COCOA)
    RetainPtr<WebAccessibilityObjectWrapper> m_wrapper;
#endif

    HashMap<AXPropertyName, AttributeValueVariant, WTF::IntHash<AXPropertyName>, WTF::StrongEnumHashTraits<AXPropertyName>> m_attributeMap;
};

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE))
