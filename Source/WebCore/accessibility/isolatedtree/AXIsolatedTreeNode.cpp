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

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)

#include "config.h"
#include "AXIsolatedTreeNode.h"

#include "AccessibilityObject.h"

namespace WebCore {

AXIsolatedTreeNode::AXIsolatedTreeNode(const AccessibilityObject& object)
    : m_identifier(object.axObjectID())
    , m_initialized(false)
{
    ASSERT(isMainThread());
    initializeAttributeData(object);
#if !ASSERT_DISABLED
    m_initialized = true;
#endif
}

Ref<AXIsolatedTreeNode> AXIsolatedTreeNode::create(const AccessibilityObject& object)
{
    return adoptRef(*new AXIsolatedTreeNode(object));
}

AXIsolatedTreeNode::~AXIsolatedTreeNode() = default;

void AXIsolatedTreeNode::initializeAttributeData(const AccessibilityObject& object)
{
    setProperty(AXPropertyName::RoleValue, static_cast<int>(object.roleValue()));
    setProperty(AXPropertyName::IsAttachment, object.isAttachment());
    setProperty(AXPropertyName::IsMediaControlLabel, object.isMediaControlLabel());
}

void AXIsolatedTreeNode::setProperty(AXPropertyName propertyName, AttributeValueVariant&& value, bool shouldRemove)
{
    ASSERT(!m_initialized);
    ASSERT(isMainThread());

    if (shouldRemove)
        m_attributeMap.remove(propertyName);
    else
        m_attributeMap.set(propertyName, value);
}

void AXIsolatedTreeNode::appendChild(AXID axID)
{
    ASSERT(isMainThread());
    m_children.append(axID);
}

double AXIsolatedTreeNode::doubleAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value, [&] (double& typedValue) {
        return typedValue;
    }, [&] (auto&) {
        return 0.0;
    });
}

unsigned AXIsolatedTreeNode::unsignedAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value, [&] (unsigned& typedValue) {
        return typedValue;
    }, [&] (auto&) {
        return 0;
    });
}

bool AXIsolatedTreeNode::boolAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value, [&] (bool& typedValue) {
        return typedValue;
    }, [&] (auto&) {
        return false;
    });
}

const String& AXIsolatedTreeNode::stringAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value, [&] (String& typedValue) {
        return typedValue;
    }, [&] (auto&) {
        return nullAtom();
    });
}

int AXIsolatedTreeNode::intAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value, [&] (int& typedValue) {
        return typedValue;
    }, [&] (auto&) {
        return 0;
    });
}
    
} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
