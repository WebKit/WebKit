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

#include "config.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedTreeNode.h"

#include "AccessibilityObject.h"

extern "C" bool _AXUIElementRequestServicedBySecondaryAXThread(void);

namespace WebCore {

AXIsolatedObject::AXIsolatedObject(AXCoreObject& object)
    : m_id(object.objectID())
{
    ASSERT(isMainThread());
    initializeAttributeData(object);
#if !ASSERT_DISABLED
    m_initialized = true;
#endif
}

Ref<AXIsolatedObject> AXIsolatedObject::create(AXCoreObject& object)
{
    return adoptRef(*new AXIsolatedObject(object));
}

AXIsolatedObject::~AXIsolatedObject() = default;

void AXIsolatedObject::initializeAttributeData(AXCoreObject& object)
{
    setProperty(AXPropertyName::BoundingBoxRect, object.boundingBoxRect());
    setProperty(AXPropertyName::ElementRect, object.elementRect());
    setProperty(AXPropertyName::RoleValue, static_cast<int>(object.roleValue()));
    setProperty(AXPropertyName::RolePlatformString, object.rolePlatformString().isolatedCopy());
    setProperty(AXPropertyName::ARIALandmarkRoleDescription, object.ariaLandmarkRoleDescription().isolatedCopy());
    setProperty(AXPropertyName::RoleDescription, object.roleDescription().isolatedCopy());
    setProperty(AXPropertyName::IsAttachment, object.isAttachment());
    setProperty(AXPropertyName::IsMediaControlLabel, object.isMediaControlLabel());
    setProperty(AXPropertyName::IsLink, object.isLink());
    setProperty(AXPropertyName::IsImageMapLink, object.isImageMapLink());
    setProperty(AXPropertyName::IsImage, object.isImage());
    setProperty(AXPropertyName::IsFileUploadButton, object.isFileUploadButton());
    setProperty(AXPropertyName::IsAccessibilityIgnored, object.accessibilityIsIgnored());
    setProperty(AXPropertyName::IsTree, object.isTree());
    setProperty(AXPropertyName::IsScrollbar, object.isScrollbar());
    setProperty(AXPropertyName::RelativeFrame, object.relativeFrame());
    setProperty(AXPropertyName::SpeechHint, object.speechHintAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::Title, object.titleAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::Description, object.descriptionAttributeValue().isolatedCopy());
    setProperty(AXPropertyName::HelpText, object.helpTextAttributeValue().isolatedCopy());

    setProperty(AXPropertyName::IsChecked, object.isChecked());
    setProperty(AXPropertyName::IsEnabled, object.isEnabled());
    setProperty(AXPropertyName::IsSelected, object.isSelected());
    setProperty(AXPropertyName::IsFocused, object.isFocused());
    setProperty(AXPropertyName::IsHovered, object.isHovered());
    setProperty(AXPropertyName::IsIndeterminate, object.isIndeterminate());
    setProperty(AXPropertyName::IsLoaded, object.isLoaded());
    setProperty(AXPropertyName::IsMultiSelectable, object.isMultiSelectable());
    setProperty(AXPropertyName::IsOnScreen, object.isOnScreen());
    setProperty(AXPropertyName::IsOffScreen, object.isOffScreen());
    setProperty(AXPropertyName::IsPressed, object.isPressed());
    setProperty(AXPropertyName::IsUnvisited, object.isUnvisited());
    setProperty(AXPropertyName::IsVisited, object.isVisited());
    setProperty(AXPropertyName::IsRequired, object.isRequired());
    setProperty(AXPropertyName::SupportsRequiredAttribute, object.supportsRequiredAttribute());
    setProperty(AXPropertyName::IsLinked, object.isLinked());
    setProperty(AXPropertyName::IsExpanded, object.isExpanded());
    setProperty(AXPropertyName::IsVisible, object.isVisible());
    setProperty(AXPropertyName::IsCollapsed, object.isCollapsed());
    setProperty(AXPropertyName::IsSelectedOptionActive, object.isSelectedOptionActive());

    if (bool isMathElement = object.isMathElement()) {
        setProperty(AXPropertyName::IsMathElement, isMathElement);
        setProperty(AXPropertyName::IsAnonymousMathOperator, object.isAnonymousMathOperator());
        setProperty(AXPropertyName::IsMathFraction, object.isMathFraction());
        setProperty(AXPropertyName::IsMathFenced, object.isMathFenced());
        setProperty(AXPropertyName::IsMathSubscriptSuperscript, object.isMathSubscriptSuperscript());
        setProperty(AXPropertyName::IsMathRow, object.isMathRow());
        setProperty(AXPropertyName::IsMathUnderOver, object.isMathUnderOver());
        setProperty(AXPropertyName::IsMathRoot, object.isMathRoot());
        setProperty(AXPropertyName::IsMathSquareRoot, object.isMathSquareRoot());
        setProperty(AXPropertyName::IsMathText, object.isMathText());
        setProperty(AXPropertyName::IsMathNumber, object.isMathNumber());
        setProperty(AXPropertyName::IsMathOperator, object.isMathOperator());
        setProperty(AXPropertyName::IsMathFenceOperator, object.isMathFenceOperator());
        setProperty(AXPropertyName::IsMathSeparatorOperator, object.isMathSeparatorOperator());
        setProperty(AXPropertyName::IsMathIdentifier, object.isMathIdentifier());
        setProperty(AXPropertyName::IsMathTable, object.isMathTable());
        setProperty(AXPropertyName::IsMathTableRow, object.isMathTableRow());
        setProperty(AXPropertyName::IsMathTableCell, object.isMathTableCell());
        setProperty(AXPropertyName::IsMathMultiscript, object.isMathMultiscript());
        setProperty(AXPropertyName::IsMathToken, object.isMathToken());
        setProperty(AXPropertyName::MathFencedOpenString, object.mathFencedOpenString());
        setProperty(AXPropertyName::MathFencedCloseString, object.mathFencedCloseString());
        setProperty(AXPropertyName::MathLineThickness, object.mathLineThickness());
        setObjectProperty(AXPropertyName::MathRadicandObject, object.mathRadicandObject());
        setObjectProperty(AXPropertyName::MathRootIndexObject, object.mathRootIndexObject());
        setObjectProperty(AXPropertyName::MathUnderObject, object.mathUnderObject());
        setObjectProperty(AXPropertyName::MathOverObject, object.mathOverObject());
        setObjectProperty(AXPropertyName::MathNumeratorObject, object.mathNumeratorObject());
        setObjectProperty(AXPropertyName::MathDenominatorObject, object.mathDenominatorObject());
        setObjectProperty(AXPropertyName::MathBaseObject, object.mathBaseObject());
        setObjectProperty(AXPropertyName::MathSubscriptObject, object.mathSubscriptObject());
        setObjectProperty(AXPropertyName::MathSuperscriptObject, object.mathSuperscriptObject());
    }
}

void AXIsolatedObject::setObjectProperty(AXPropertyName propertyName, AXCoreObject* object)
{
    if (object)
        setProperty(propertyName, object->objectID());
    else
        setProperty(propertyName, nullptr, true);
}

void AXIsolatedObject::setProperty(AXPropertyName propertyName, AttributeValueVariant&& value, bool shouldRemove)
{
    ASSERT(!m_initialized);
    ASSERT(isMainThread());

    if (shouldRemove)
        m_attributeMap.remove(propertyName);
    else
        m_attributeMap.set(propertyName, value);
}

void AXIsolatedObject::appendChild(AXID axID)
{
    ASSERT(isMainThread());
    m_childrenIDs.append(axID);
}

void AXIsolatedObject::setParent(AXID parent)
{
    ASSERT(isMainThread());
    m_parent = parent;
}

void AXIsolatedObject::setTreeIdentifier(AXIsolatedTreeID treeIdentifier)
{
    m_treeIdentifier = treeIdentifier;
    if (auto tree = AXIsolatedTree::treeForID(m_treeIdentifier))
        m_cachedTree = tree;
}

const AXCoreObject::AccessibilityChildrenVector& AXIsolatedObject::children(bool)
{
    if (_AXUIElementRequestServicedBySecondaryAXThread()) {
        m_children.clear();
        m_children.reserveInitialCapacity(m_childrenIDs.size());
        auto tree = this->tree();
        for (auto childID : m_childrenIDs)
            m_children.uncheckedAppend(tree->nodeForID(childID));
    }
    return m_children;
}

AXCoreObject* AXIsolatedObject::focusedUIElement() const
{
    if (auto focusedElement = tree()->focusedUIElement())
        return focusedElement.get();
    return nullptr;
}
    
AXCoreObject* AXIsolatedObject::parentObjectUnignored() const
{
    return tree()->nodeForID(parent()).get();
}

AXCoreObject* AXIsolatedObject::accessibilityHitTest(const IntPoint& point) const
{
    if (!relativeFrame().contains(point))
        return nullptr;
    for (const auto& childID : m_childrenIDs) {
        auto child = tree()->nodeForID(childID);
        ASSERT(child);
        if (child && child->relativeFrame().contains(point))
            return child->accessibilityHitTest(point);
    }
    return const_cast<AXIsolatedObject*>(this);
}

AXIsolatedTree* AXIsolatedObject::tree() const
{
    return m_cachedTree.get();
}

AXCoreObject* AXIsolatedObject::objectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    AXID nodeID = WTF::switchOn(value,
        [] (AXID& typedValue) { return typedValue; },
        [] (auto&) { return InvalidAXID; }
    );
    
    return tree()->nodeForID(nodeID).get();
}

template<typename T>
T AXIsolatedObject::rectAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (T& typedValue) { return typedValue; },
        [] (auto&) { return T { }; }
    );
}

double AXIsolatedObject::doubleAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (double& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

unsigned AXIsolatedObject::unsignedAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (unsigned& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

bool AXIsolatedObject::boolAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (bool& typedValue) { return typedValue; },
        [] (auto&) { return false; }
    );
}

const String AXIsolatedObject::stringAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
}

int AXIsolatedObject::intAttributeValue(AXPropertyName propertyName) const
{
    auto value = m_attributeMap.get(propertyName);
    return WTF::switchOn(value,
        [] (int& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

void AXIsolatedObject::updateBackingStore()
{
    if (_AXUIElementRequestServicedBySecondaryAXThread()) {
        RELEASE_ASSERT(!isMainThread());
        if (auto tree = this->tree())
            tree->applyPendingChanges();
    }
}

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
