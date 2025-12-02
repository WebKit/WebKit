/*
 * Copyright (C) 2019-2025 Apple Inc. All rights reserved.
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
#include "AXIsolatedObject.h"

#include "AXGeometryManager.h"
#include "AXIsolatedTree.h"
#include "AXLogger.h"
#include "AXLoggerBase.h"
#include "AXObjectCacheInlines.h"
#include "AXSearchManager.h"
#include "AXTextMarker.h"
#include "AXTextRun.h"
#include "AXUtilities.h"
#include "AccessibilityNodeObject.h"
#include "DateComponents.h"
#include "HTMLNames.h"
#include "Logging.h"
#include "RenderObject.h"
#include <wtf/text/MakeString.h>

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)
#include "ModelPlayerAccessibilityChildren.h"
#endif

#if PLATFORM(MAC)
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if PLATFORM(COCOA)
#include <pal/spi/cocoa/AccessibilitySupportSoftLink.h>
#endif

namespace WebCore {

using namespace HTMLNames;

AXIsolatedObject::AXIsolatedObject(IsolatedObjectData&& data)
    : AXCoreObject(data.axID, data.role, data.getsGeometryFromChildren)
    , m_unresolvedChildrenIDs(WTFMove(data.childrenIDs))
    , m_properties(WTFMove(data.properties))
    , m_tree(WTFMove(data.tree))
    , m_parentID(data.parentID)
    , m_propertyFlags(data.propertyFlags)
{
    ASSERT(!isMainThread());
}

Ref<AXIsolatedObject> AXIsolatedObject::create(IsolatedObjectData&& data)
{
    return adoptRef(*new AXIsolatedObject(WTFMove(data)));
}

AXIsolatedObject::~AXIsolatedObject()
{
    AX_BROKEN_ASSERT(!wrapper());
}

void AXIsolatedObject::updateFromData(IsolatedObjectData&& data)
{
    ASSERT(!isMainThread());

    if (data.axID != objectID() || data.tree->treeID() != treeID()) {
        // Our data should only be updated from the same main-thread equivalent object.
        ASSERT_NOT_REACHED();
        return;
    }

    m_role = data.role;
    m_parentID = data.parentID;
    m_unresolvedChildrenIDs = WTFMove(data.childrenIDs);
    m_childrenDirty = true;
    m_getsGeometryFromChildren = data.getsGeometryFromChildren;

    m_properties = WTFMove(data.properties);
    m_propertyFlags = data.propertyFlags;
}

String AXIsolatedObject::debugDescriptionInternal(bool verbose, std::optional<OptionSet<AXDebugStringOption>> debugOptions) const
{
    StringBuilder result;
    result.append("{"_s);
    result.append("role: "_s, roleToString(role()));
    result.append(", ID "_s, objectID().loggingString());

    if (debugOptions) {
        if (verbose || *debugOptions & AXDebugStringOption::Ignored)
            result.append(isIgnored() ? ", ignored"_s : emptyString());

        if (verbose || *debugOptions & AXDebugStringOption::RelativeFrame) {
            FloatRect frame = relativeFrame();
            result.append(", relativeFrame ((x: "_s, frame.x(), ", y: "_s, frame.y(), "), (w: "_s, frame.width(), ", h: "_s, frame.height(), "))"_s);
        }

        if (verbose || *debugOptions & AXDebugStringOption::RemoteFrameOffset)
            result.append(", remoteFrameOffset ("_s, remoteFrameOffset().x(), ", "_s, remoteFrameOffset().y(), ")"_s);
    }

    result.append("}"_s);
    return result.toString();
}

bool isDefaultValue(AXProperty property, AXPropertyValueVariant& value)
{
    return WTF::switchOn(value,
        [](std::nullptr_t&) { return true; },
        [](Markable<AXID> typedValue) { return !typedValue; },
        [&](String& typedValue) {
#if !ENABLE(AX_THREAD_TEXT_APIS)
            // We use a null stringValue to indicate when the string value is different than the text content.
            if (property == AXProperty::StringValue)
                return typedValue == emptyString(); // Only compares empty, not null
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
            return typedValue.isEmpty(); // null or empty
        },
        [](bool typedValue) { return !typedValue; },
        [](int typedValue) { return !typedValue; },
        [](unsigned typedValue) { return !typedValue; },
        [](double typedValue) { return typedValue == 0.0; },
        [](float typedValue) { return typedValue == 0.0; },
        [](uint64_t typedValue) { return !typedValue; },
        [](AccessibilityButtonState& typedValue) { return typedValue == AccessibilityButtonState::Off; },
        [&](Color& typedValue) {
            if (property == AXProperty::ColorValue)
                return typedValue == Color::black;
            if (property == AXProperty::TextColor)
                return false;
            return typedValue.toColorTypeLossy<SRGBA<uint8_t>>() == Accessibility::defaultColor();
        },
        [](std::unique_ptr<URL>& typedValue) { return !typedValue || *typedValue == URL(); },
        [](LayoutRect& typedValue) { return typedValue == LayoutRect(); },
        [](IntPoint& typedValue) { return typedValue == IntPoint(); },
        [](IntRect& typedValue) { return typedValue == IntRect(); },
        [](FloatPoint& typedValue) { return typedValue == FloatPoint(); },
        [](FloatRect& typedValue) { return typedValue == FloatRect(); },
        [](std::pair<unsigned, unsigned>& typedValue) {
            // (0, 1) is the default for an index range.
            return typedValue == std::pair<unsigned, unsigned>(0, 1);
        },
        [](Vector<AccessibilityText>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<AXID>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<std::pair<Markable<AXID>, Markable<AXID>>>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<String>& typedValue) { return typedValue.isEmpty(); },
        [](std::unique_ptr<Path>& typedValue) { return !typedValue || typedValue->isEmpty(); },
        [](OptionSet<AXAncestorFlag>& typedValue) { return typedValue.isEmpty(); },
#if PLATFORM(COCOA)
        [](RetainPtr<NSAttributedString>& typedValue) { return !typedValue; },
        [](RetainPtr<NSView>& typedValue) { return !typedValue; },
        [](RetainPtr<id>& typedValue) { return !typedValue; },
#endif
        [](InputType::Type&) { return false; },
        [](Vector<Vector<Markable<AXID>>>& typedValue) { return typedValue.isEmpty(); },
        [](Vector<Vector<AXID>>& typedValue) { return typedValue.isEmpty(); },
        [](CharacterRange& typedValue) { return !typedValue.location && !typedValue.length; },
        [](std::unique_ptr<AXIDAndCharacterRange>& typedValue) {
            return !typedValue || (!typedValue->first && !typedValue->second.location && !typedValue->second.length);
        },
#if ENABLE(AX_THREAD_TEXT_APIS)
        [](std::unique_ptr<AXTextRuns>& typedValue) { return !typedValue || !typedValue->size(); },
        [](RetainPtr<CTFontRef>& typedValue) { return !typedValue; },
        [](FontOrientation typedValue) { return typedValue == FontOrientation::Horizontal; },
        [](AXTextRunLineID typedValue) { return !typedValue; },
#endif // ENABLE(AX_THREAD_TEXT_APIS)
        [](WallTime& time) { return !time; },
        [](ElementName& name) { return name == ElementName::Unknown; },
        [](DateComponentsType& typedValue) { return typedValue == DateComponentsType::Invalid; },
        [](AccessibilityOrientation) { return false; },
        [](Style::SpeakAs& typedValue) { return typedValue.isNormal(); },
        [](auto&) {
            ASSERT_NOT_REACHED();
            return false;
        }
    );
}

AccessibilityObject* AXIsolatedObject::associatedAXObject() const
{
    // It is only safe to call this on an AXIsolatedObject when done via a synchronous call from the
    // accessibility thread. Otherwise, |this| could be deleted by the secondary thread (who owns the
    // lifetime of isolated objects) in the middle of this method.
    ASSERT(isMainThread());

    auto* axObjectCache = this->axObjectCache();
    return axObjectCache ? axObjectCache->objectForID(objectID()) : nullptr;
}

void AXIsolatedObject::setMathscripts(AXProperty property, AccessibilityObject& object)
{
    AccessibilityMathMultiscriptPairs pairs;
    if (property == AXProperty::MathPrescripts)
        object.mathPrescripts(pairs);
    else if (property == AXProperty::MathPostscripts)
        object.mathPostscripts(pairs);

    if (pairs.isEmpty())
        return;

    auto idPairs = pairs.map([](auto& mathPair) {
        return std::pair { mathPair.first ? Markable { mathPair.first->objectID() } : std::nullopt, mathPair.second ? Markable { mathPair.second->objectID() } : std::nullopt };
    });
    setProperty(property, WTFMove(idPairs));
}

void AXIsolatedObject::setObjectProperty(AXProperty property, AXCoreObject* object)
{
    setProperty(property, object ? Markable { object->objectID() } : std::nullopt);
}

void AXIsolatedObject::setObjectVectorProperty(AXProperty property, const AccessibilityChildrenVector& objects)
{
    setProperty(property, axIDs(objects));
}

void AXIsolatedObject::setProperty(AXProperty property, AXPropertyValueVariant&& value)
{
    if (const bool* boolValue = std::get_if<bool>(&value)) {
        if (std::optional propertyFlag = convertToPropertyFlag(property)) {
            setPropertyFlag(*propertyFlag, *boolValue);
            return;
        }
    }

    if (isDefaultValue(property, value))
        removePropertyInVector(property);
    else
        setPropertyInVector(property, WTFMove(value));
}

void AXIsolatedObject::detachRemoteParts(AccessibilityDetachmentType)
{
    ASSERT(!isMainThread());

    for (const auto& child : m_children)
        child->detachFromParent();

    for (const auto& childID : m_unresolvedChildrenIDs) {
        // Also loop through unresolved IDs in case they have become resolved.
        if (RefPtr child = tree()->objectForID(childID))
            child->detachFromParent();
    }
    m_unresolvedChildrenIDs.clear();
    m_children.clear();
    m_childrenDirty = false;
}

#if !PLATFORM(MAC)
bool AXIsolatedObject::isDetached() const
{
    ASSERT_NOT_REACHED();
    return false;
}
#endif

void AXIsolatedObject::detachFromParent()
{
    m_parentID = { };
}

void AXIsolatedObject::setChildrenIDs(Vector<AXID>&& ids)
{
    m_unresolvedChildrenIDs = WTFMove(ids);
    m_childrenDirty = true;
}

const AXCoreObject::AccessibilityChildrenVector& AXIsolatedObject::children(bool updateChildrenIfNeeded)
{
#if USE(APPLE_INTERNAL_SDK)
    AX_DEBUG_ASSERT(_AXSIsolatedTreeModeFunctionIsAvailable() && ((_AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeSecondaryThread && !isMainThread())
        || (_AXSIsolatedTreeMode_Soft() == AXSIsolatedTreeModeMainThread && isMainThread())));
#elif USE(ATSPI)
    ASSERT(!isMainThread());
#endif
    if (updateChildrenIfNeeded && m_childrenDirty) {
        unsigned index = 0;
        Vector<AXID> unresolvedIDs;
        m_children = WTF::compactMap(m_unresolvedChildrenIDs, [&] (auto& childID) -> std::optional<Ref<AXCoreObject>> {
            if (RefPtr child = tree()->objectForID(childID)) {
                if (setChildIndexInParent(*child, index))
                    ++index;
                return child.releaseNonNull();
            }
            unresolvedIDs.append(childID);
            return std::nullopt;
        });
        m_childrenDirty = false;
        m_unresolvedChildrenIDs = WTFMove(unresolvedIDs);
        // Having any unresolved children IDs at this point means we should've had a child / children, but they didn't
        // exist in tree()->objectForID(), so we were never able to hydrate it into an object.
        AX_BROKEN_ASSERT(m_unresolvedChildrenIDs.isEmpty());

#ifndef NDEBUG
        verifyChildrenIndexInParent();
#endif
    }
    return m_children;
}

void AXIsolatedObject::setSelectedChildren(const AccessibilityChildrenVector& selectedChildren)
{
    ASSERT(selectedChildren.isEmpty() || selectedChildren[0]->isAXIsolatedObjectInstance());

    auto childrenIDs = axIDs(selectedChildren);
    performFunctionOnMainThread([selectedChildrenIDs = WTFMove(childrenIDs), protectedThis = Ref { *this }] (auto* object) {
        if (selectedChildrenIDs.isEmpty()) {
            object->setSelectedChildren({ });
            return;
        }

        auto* axObjectCache = protectedThis->axObjectCache();
        if (!axObjectCache)
            return;

        object->setSelectedChildren(axObjectCache->objectsForIDs(selectedChildrenIDs));
    });
}

bool AXIsolatedObject::isInDescriptionListTerm() const
{
    return Accessibility::findAncestor<AXIsolatedObject>(*this, false, [&] (const auto& ancestor) {
        return ancestor.role() == AccessibilityRole::DescriptionListTerm;
    });
}

AXIsolatedObject* AXIsolatedObject::cellForColumnAndRow(unsigned columnIndex, unsigned rowIndex)
{
    size_t index = indexOfProperty(AXProperty::CellSlots);
    if (index == notFound)
        return nullptr;

    auto cellID = WTF::switchOn(m_properties[index].second,
        [&] (Vector<Vector<Markable<AXID>>>& cellSlots) -> std::optional<AXID> {
            if (rowIndex >= cellSlots.size() || columnIndex >= cellSlots[rowIndex].size())
                return std::nullopt;
            return cellSlots[rowIndex][columnIndex];
        },
        [] (auto&) -> std::optional<AXID> { return std::nullopt; }
    );
    return tree()->objectForID(cellID);
}

void AXIsolatedObject::accessibilityText(Vector<AccessibilityText>& texts) const
{
    texts = vectorAttributeValue<AccessibilityText>(AXProperty::AccessibilityText);
}

const Vector<Vector<AXID>>* AXIsolatedObject::stitchGroupsView() const
{
    size_t index = indexOfProperty(AXProperty::StitchGroups);
    if (index == notFound)
        return nullptr;

    return WTF::switchOn(m_properties[index].second,
        [] (const Vector<Vector<AXID>>& typedValue) -> const Vector<Vector<AXID>>* { return &typedValue; },
        [] (auto&) -> const Vector<Vector<AXID>>* { return nullptr; }
    );
}

AXIsolatedObject::StitchState AXIsolatedObject::stitchState(IncludeStitchGroup includeStitchGroup) const
{
    if (!AXObjectCache::isAXTextStitchingEnabled())
        return { };

    RefPtr blockFlowAncestor = downcast<AXIsolatedObject>(blockFlowAncestorForStitchable());
    if (!blockFlowAncestor)
        return { };

    return stitchStateFromGroups(blockFlowAncestor->stitchGroupsView(), includeStitchGroup);
}

void AXIsolatedObject::insertMathPairs(Vector<std::pair<Markable<AXID>, Markable<AXID>>>& isolatedPairs, AccessibilityMathMultiscriptPairs& pairs)
{
    for (const auto& pair : isolatedPairs) {
        AccessibilityMathMultiscriptPair prescriptPair;
        if (RefPtr object = tree()->objectForID(pair.first))
            prescriptPair.first = object.get();
        if (RefPtr object = tree()->objectForID(pair.second))
            prescriptPair.second = object.get();
        pairs.append(prescriptPair);
    }
}

void AXIsolatedObject::mathPrescripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<std::pair<Markable<AXID>, Markable<AXID>>>(AXProperty::MathPrescripts);
    insertMathPairs(isolatedPairs, pairs);
}

void AXIsolatedObject::mathPostscripts(AccessibilityMathMultiscriptPairs& pairs)
{
    auto isolatedPairs = vectorAttributeValue<std::pair<Markable<AXID>, Markable<AXID>>>(AXProperty::MathPostscripts);
    insertMathPairs(isolatedPairs, pairs);
}

std::optional<AXCoreObject::AccessibilityChildrenVector> AXIsolatedObject::mathRadicand()
{
    if (indexOfProperty(AXProperty::MathRadicand) != notFound) {
        Vector<Ref<AXCoreObject>> radicand;
        fillChildrenVectorForProperty(AXProperty::MathRadicand, radicand);
        return { radicand };
    }
    return std::nullopt;
}

AXIsolatedObject* AXIsolatedObject::scrollBar(AccessibilityOrientation orientation)
{
    return objectAttributeValue(orientation == AccessibilityOrientation::Vertical ? AXProperty::VerticalScrollBar : AXProperty::HorizontalScrollBar);
}

void AXIsolatedObject::setARIAGrabbed(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setARIAGrabbed(value);
    });
}

void AXIsolatedObject::setIsExpanded(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setIsExpanded(value);
    });
}

bool AXIsolatedObject::performDismissAction()
{
    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (RefPtr axObject = associatedAXObject())
            return axObject->performDismissAction();
        return false;
    });
}

void AXIsolatedObject::performDismissActionIgnoringResult()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->performDismissActionIgnoringResult();
    });
}

void AXIsolatedObject::scrollToMakeVisible() const
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->scrollToMakeVisible();
    });
}

void AXIsolatedObject::scrollToMakeVisibleWithSubFocus(IntRect&& rect) const
{
    performFunctionOnMainThread([rect = WTFMove(rect)] (auto* axObject) mutable {
        axObject->scrollToMakeVisibleWithSubFocus(WTFMove(rect));
    });
}

void AXIsolatedObject::scrollToGlobalPoint(IntPoint&& point) const
{
    performFunctionOnMainThread([point = WTFMove(point)] (auto* axObject) mutable {
        axObject->scrollToGlobalPoint(WTFMove(point));
    });
}

bool AXIsolatedObject::setValue(float value)
{
    return Accessibility::retrieveValueFromMainThread<bool>([&value, this] () -> bool {
        if (RefPtr axObject = associatedAXObject())
            return axObject->setValue(value);
        return false;
    });
}

void AXIsolatedObject::setValueIgnoringResult(float value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setValueIgnoringResult(value);
    });
}

bool AXIsolatedObject::setValue(const String& value)
{
    return Accessibility::retrieveValueFromMainThread<bool>([&value, this] () -> bool {
        if (RefPtr axObject = associatedAXObject())
            return axObject->setValue(value);
        return false;
    });
}

void AXIsolatedObject::setValueIgnoringResult(const String& value)
{
    performFunctionOnMainThread([value = value.isolatedCopy()] (auto* object) {
        object->setValueIgnoringResult(value);
    });
}

void AXIsolatedObject::setSelected(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setSelected(value);
    });
}

void AXIsolatedObject::setSelectedRows(AccessibilityChildrenVector&& selectedRows)
{
    auto rowIDs = axIDs(selectedRows);
    performFunctionOnMainThread([selectedRowIDs = WTFMove(rowIDs), protectedThis = Ref { *this }] (auto* object) {
        if (selectedRowIDs.isEmpty()) {
            object->setSelectedRows({ });
            return;
        }

        auto* axObjectCache = protectedThis->axObjectCache();
        if (!axObjectCache)
            return;

        object->setSelectedRows(axObjectCache->objectsForIDs(selectedRowIDs));
    });
}

void AXIsolatedObject::setFocused(bool value)
{
    performFunctionOnMainThread([value] (auto* object) {
        object->setFocused(value);
    });
}

String AXIsolatedObject::selectedText() const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return selectedTextMarkerRange().toString();
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (RefPtr object = associatedAXObject())
            return object->selectedText().isolatedCopy();
        return { };
    });
}

void AXIsolatedObject::setSelectedText(const String& value)
{
    performFunctionOnMainThread([value = value.isolatedCopy()] (auto* object) {
        object->setSelectedText(value);
    });
}

void AXIsolatedObject::setSelectedTextRange(CharacterRange&& range)
{
    performFunctionOnMainThread([range = WTFMove(range)] (auto* object) mutable {
        object->setSelectedTextRange(WTFMove(range));
    });
}

SRGBA<uint8_t> AXIsolatedObject::colorValue() const
{
    size_t index = indexOfProperty(AXProperty::ColorValue);
    if (index == notFound) {
        // Don't fallback to returning the default Color() value, as that is transparent black,
        // but we want to return opaque black as a default for this property.
        return Color::black;
    }

    return WTF::switchOn(m_properties[index].second,
        [] (const Color& typedValue) -> SRGBA<uint8_t> { return typedValue.toColorTypeLossy<SRGBA<uint8_t>>(); },
        [] (const auto&) -> SRGBA<uint8_t> {
            ASSERT_NOT_REACHED();
            return Color().toColorTypeLossy<SRGBA<uint8_t>>();
        }
    );
}

AXIsolatedObject* AXIsolatedObject::accessibilityHitTest(const IntPoint& point) const
{
    auto axID = Accessibility::retrieveValueFromMainThread<std::optional<AXID>>([&point, this] () -> std::optional<AXID> {
        if (RefPtr object = associatedAXObject()) {
            object->updateChildrenIfNecessary();
            if (auto* axObject = object->accessibilityHitTest(point))
                return axObject->objectID();
        }

        return std::nullopt;
    });

    return tree()->objectForID(axID);
}

TextEmissionBehavior AXIsolatedObject::textEmissionBehavior() const
{
    if (hasPropertyFlag(AXProperty::IsTextEmissionBehaviorNewline))
        return TextEmissionBehavior::Newline;
    if (hasPropertyFlag(AXProperty::IsTextEmissionBehaviorDoubleNewline))
        return TextEmissionBehavior::DoubleNewline;
    if (hasPropertyFlag(AXProperty::IsTextEmissionBehaviorTab))
        return TextEmissionBehavior::Tab;

    return TextEmissionBehavior::None;
}

IntPoint AXIsolatedObject::intPointAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return IntPoint();

    return WTF::switchOn(m_properties[index].second,
        [] (const IntPoint& typedValue) -> IntPoint { return typedValue; },
        [] (auto&) { return IntPoint(); }
    );
}

AXIsolatedObject* AXIsolatedObject::objectAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return nullptr;

    return tree()->objectForID(WTF::switchOn(m_properties[index].second,
        [] (const Markable<AXID>& typedValue) -> std::optional<AXID> { return typedValue; },
        [] (auto&) { return std::optional<AXID> { }; }
    ));
}

template<typename T>
T AXIsolatedObject::rectAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return T { };

    return WTF::switchOn(m_properties[index].second,
        [] (const T& typedValue) -> T { return typedValue; },
        [] (auto&) { return T { }; }
    );
}

template<typename T>
Vector<T> AXIsolatedObject::vectorAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return { };

    return WTF::switchOn(m_properties[index].second,
        [] (const Vector<T>& typedValue) -> Vector<T> { return typedValue; },
        [] (auto&) { return Vector<T>(); }
    );
}

Style::SpeakAs AXIsolatedObject::speakAsAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return CSS::Keyword::Normal { };

    return WTF::switchOn(m_properties[index].second,
        [](const Style::SpeakAs& typedValue) -> Style::SpeakAs { return typedValue; },
        [](auto&) -> Style::SpeakAs { return CSS::Keyword::Normal { }; }
    );
}

std::pair<unsigned, unsigned> AXIsolatedObject::indexRangePairAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return std::pair<unsigned, unsigned>(0, 1);

    return WTF::switchOn(m_properties[index].second,
        [] (const std::pair<unsigned, unsigned>& typedValue) -> std::pair<unsigned, unsigned> { return typedValue; },
        [] (auto&) { return std::pair<unsigned, unsigned>(0, 1); }
    );
}

template<typename T>
std::optional<T> AXIsolatedObject::optionalAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return std::nullopt;

    return WTF::switchOn(m_properties[index].second,
        [] (const T& typedValue) -> std::optional<T> { return typedValue; },
        [] (const auto&) -> std::optional<T> {
            ASSERT_NOT_REACHED();
            return std::nullopt;
        }
    );
}

uint64_t AXIsolatedObject::uint64AttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return 0;

    return WTF::switchOn(m_properties[index].second,
        [] (const uint64_t& typedValue) -> uint64_t { return typedValue; },
        [] (auto&) -> uint64_t { return 0; }
    );
}

URL AXIsolatedObject::urlAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return URL();

    return WTF::switchOn(m_properties[index].second,
        [] (const std::unique_ptr<URL>& typedValue) -> URL {
            ASSERT(typedValue.get());
            return *typedValue.get();
        },
        [] (auto&) { return URL(); }
    );
}

Path AXIsolatedObject::pathAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return Path();

    return WTF::switchOn(m_properties[index].second,
        [] (const std::unique_ptr<Path>& typedValue) -> Path {
            ASSERT(typedValue.get());
            return *typedValue.get();
        },
        [] (auto&) { return Path(); }
    );
}

static Color getColor(const AXPropertyValueVariant& value)
{
    return WTF::switchOn(value,
        [] (const Color& typedValue) -> Color { return typedValue; },
        [] (auto&) { return Color(); }
    );
}

#ifndef NDEBUG
Color AXIsolatedObject::cachedTextColor() const
{
    size_t index = indexOfProperty(AXProperty::TextColor);
    return index == notFound ? Color() : getColor(m_properties[index].second);
}
#endif

static RetainPtr<CTFontRef> getFont(const AXPropertyValueVariant& value)
{
    return WTF::switchOn(value,
        [] (const RetainPtr<CTFontRef>& typedValue) -> RetainPtr<CTFontRef> { return typedValue; },
        [] (auto&) { return RetainPtr<CTFontRef>(); }
    );
}

#ifndef NDEBUG
#if PLATFORM(COCOA)
RetainPtr<CTFontRef> AXIsolatedObject::cachedFont() const
{
    size_t index = indexOfProperty(AXProperty::Font);
    return index == notFound ? RetainPtr<CTFontRef>() : getFont(m_properties[index].second);
}
#endif // PLATFORM(COCOA)
#endif // NDEBUG

Color AXIsolatedObject::colorAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound) {
        if (property == AXProperty::TextColor) {
            if (RefPtr parent = parentObject())
                return parent->textColor();
        }
        return Accessibility::defaultColor();
    }

#ifndef NDEBUG
    if (RefPtr parent = parentObject(); parent && property == AXProperty::TextColor)
        ASSERT(parent->cachedTextColor() != getColor(m_properties[index].second));
#endif

    return getColor(m_properties[index].second);
}

RetainPtr<CTFontRef> AXIsolatedObject::fontAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound) {
        RefPtr parent = parentObject();
        return parent ? parent->font() : nullptr;
    }

#ifndef NDEBUG
    if (RefPtr parent = parentObject(); parent && property == AXProperty::Font)
        ASSERT(parent->cachedFont() != getFont(m_properties[index].second));
#endif

    return getFont(m_properties[index].second);
}

float AXIsolatedObject::floatAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return 0.0f;

    return WTF::switchOn(m_properties[index].second,
        [] (const float& typedValue) -> float { return typedValue; },
        [] (auto&) { return 0.0f; }
    );
}

double AXIsolatedObject::doubleAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return 0.0;

    return WTF::switchOn(m_properties[index].second,
        [] (const double& typedValue) -> double { return typedValue; },
        [] (auto&) { return 0.0; }
    );
}

unsigned AXIsolatedObject::unsignedAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return 0u;

    return WTF::switchOn(m_properties[index].second,
        [] (const unsigned& typedValue) -> unsigned { return typedValue; },
        [] (auto&) { return 0u; }
    );
}

bool AXIsolatedObject::boolAttributeValue(AXProperty property) const
{
    uint16_t propertyIndex = static_cast<uint16_t>(property);
    if (propertyIndex > lastPropertyFlagIndex) {
        size_t index = indexOfProperty(property);
        if (index == notFound)
            return false;
        return WTF::switchOn(m_properties[index].second,
            [] (const bool& typedValue) { return typedValue; },
            [] (auto&) { return false; }
        );
    }
    return hasPropertyFlag(static_cast<AXPropertyFlag>(1 << propertyIndex));
}

String AXIsolatedObject::stringAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return emptyString();

    return WTF::switchOn(m_properties[index].second,
        [] (const String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
}

String AXIsolatedObject::stringAttributeValueNullIfMissing(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return nullString();

    return WTF::switchOn(m_properties[index].second,
        [] (const String& typedValue) { return typedValue; },
        [] (auto&) { return nullString(); }
    );
}

int AXIsolatedObject::intAttributeValue(AXProperty property) const
{
    size_t index = indexOfProperty(property);
    if (index == notFound)
        return 0;

    return WTF::switchOn(m_properties[index].second,
        [] (const int& typedValue) { return typedValue; },
        [] (auto&) { return 0; }
    );
}

#if ENABLE(AX_THREAD_TEXT_APIS)
const AXTextRuns* AXIsolatedObject::textRuns() const
{
    size_t index = indexOfProperty(AXProperty::TextRuns);
    if (index == notFound)
        return nullptr;

    return WTF::switchOn(m_properties[index].second,
        [] (const std::unique_ptr<AXTextRuns>& typedValue) -> const AXTextRuns* { return typedValue.get(); },
        [] (auto&) -> const AXTextRuns* { return nullptr; }
    );
}
#endif

template<typename T>
T AXIsolatedObject::getOrRetrievePropertyValue(AXProperty property)
{
    if (std::optional value = optionalAttributeValue<T>(property))
        return *value;

    Accessibility::performFunctionOnMainThreadAndWait([&property, this] () {
        RefPtr axObject = associatedAXObject();
        if (!axObject)
            return;

        AXPropertyValueVariant value;
        switch (property) {
        case AXProperty::InnerHTML:
            value = axObject->innerHTML().isolatedCopy();
            break;
        case AXProperty::OuterHTML:
            value = axObject->outerHTML().isolatedCopy();
            break;
        default:
            break;
        }

        // Cache value so that there is no need to access the main thread in subsequent calls.
        setPropertyInVector(property, WTFMove(value));
    });

    return propertyValue<T>(property);
}

void AXIsolatedObject::fillChildrenVectorForProperty(AXProperty property, AccessibilityChildrenVector& children) const
{
    Vector<AXID> childIDs = vectorAttributeValue<AXID>(property);
    children.reserveCapacity(childIDs.size());
    for (const auto& childID : childIDs) {
        if (RefPtr object = tree()->objectForID(childID))
            children.append(object.releaseNonNull());
    }
}

void AXIsolatedObject::updateBackingStore()
{
    ASSERT(!isMainThread());

    if (RefPtr tree = this->tree())
        tree->applyPendingChanges();
    // AXIsolatedTree::applyPendingChanges can cause this object and / or the AXIsolatedTree to be destroyed.
    // Make sure to protect `this` with a Ref before adding more logic to this function.
}

std::optional<SimpleRange> AXIsolatedObject::rangeForCharacterRange(const CharacterRange& axRange) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->rangeForCharacterRange(axRange) : std::nullopt;
}

#if PLATFORM(MAC)
AXTextMarkerRange AXIsolatedObject::selectedTextMarkerRange() const
{
    return tree()->selectedTextMarkerRange();
}
#endif // PLATFORM(MAC)

IntRect AXIsolatedObject::boundsForRange(const SimpleRange& range) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->boundsForRange(range) : IntRect();
}

VisiblePosition AXIsolatedObject::visiblePositionForPoint(const IntPoint& point) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForPoint(point) : VisiblePosition();
}

VisiblePosition AXIsolatedObject::nextLineEndPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::previousLineStartPosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return { };
}

VisiblePosition AXIsolatedObject::visiblePositionForIndex(int index) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForIndex(index) : VisiblePosition();
}

int AXIsolatedObject::indexForVisiblePosition(const VisiblePosition&) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

Vector<SimpleRange> AXIsolatedObject::findTextRanges(const AccessibilitySearchTextCriteria& criteria) const
{
    return Accessibility::retrieveValueFromMainThread<Vector<SimpleRange>>([&criteria, this] () -> Vector<SimpleRange> {
        if (RefPtr object = associatedAXObject())
            return object->findTextRanges(criteria);
        return { };
    });
}

Vector<String> AXIsolatedObject::performTextOperation(const AccessibilityTextOperation& textOperation)
{
    return Accessibility::retrieveValueFromMainThread<Vector<String>>([&textOperation, this] () -> Vector<String> {
        if (RefPtr object = associatedAXObject())
            return object->performTextOperation(textOperation);
        return Vector<String>();
    });
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::findMatchingObjects(AccessibilitySearchCriteria&& criteria)
{
    criteria.anchorObject = this;
    return AXSearchManager().findMatchingObjects(WTFMove(criteria));
}

String AXIsolatedObject::textUnderElement(TextUnderElementMode) const
{
    ASSERT_NOT_REACHED();
    return { };
}

std::optional<SimpleRange> AXIsolatedObject::misspellingRange(const SimpleRange& range, AccessibilitySearchDirection direction) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->misspellingRange(range, direction) : std::nullopt;
}

LayoutRect AXIsolatedObject::elementRect() const
{
#if PLATFORM(MAC)
    // It is not expected for elementRect to be called directly or indirectly when serving a request for VoiceOver.
    // If this does happen, we should either see if AXIsolatedObject::relativeFrame can be used instead, or do the
    // work to cache the correct elementRect value.
    ASSERT(_AXGetClientForCurrentRequestUntrusted() != kAXClientTypeVoiceOver);
#endif

    return Accessibility::retrieveValueFromMainThread<LayoutRect>([&, this] () -> LayoutRect {
        if (RefPtr axObject = associatedAXObject())
            return axObject->elementRect();
        return { };
    });
}

IntPoint AXIsolatedObject::remoteFrameOffset() const
{
    RefPtr root = tree()->rootNode();
    return root ? root->propertyValue<IntPoint>(AXProperty::RemoteFrameOffset) : IntPoint();
}

FloatPoint AXIsolatedObject::screenRelativePosition() const
{
    if (auto point = optionalAttributeValue<FloatPoint>(AXProperty::ScreenRelativePosition))
        return *point;
    return convertFrameToSpace(relativeFrame(), AccessibilityConversionSpace::Screen).location();
}

FloatRect AXIsolatedObject::relativeFrame() const
{
    FloatRect relativeFrame;

    if (std::optional cachedRelativeFrame = this->cachedRelativeFrame()) {
        // We should not have cached a relative frame for elements that get their geometry from their children.
        ASSERT(!m_getsGeometryFromChildren);
        relativeFrame = *cachedRelativeFrame;

        if (isStaticText()) {
            auto stitchState = this->stitchState();
            if (stitchState.stitchedIntoID && *stitchState.stitchedIntoID == objectID() && stitchState.group.size()) {
                // |this| is a stitching of multiple objects, so we need to combine all of their frames.

                RefPtr tree = this->tree();
                for (AXID axID : stitchState.group) {
                    if (axID == objectID())
                        continue;

                    if (RefPtr object = tree->objectForID(axID)) {
                        if (std::optional otherCachedFrame = object->cachedRelativeFrame())
                            relativeFrame = unionRect(relativeFrame, *otherCachedFrame);
                    }
                }
            }
        }
    } else if (m_getsGeometryFromChildren) {
        auto frame = enclosingIntRect(relativeFrameFromChildren());
        if (!frame.isEmpty())
            relativeFrame = frame;
        // Either we had no children, or our children had empty frames. The right thing to do would be to return
        // a rect at the position of the nearest render tree ancestor with some made-up size (AccessibilityNodeObject::boundingBoxRect does this).
        // However, we don't have access to the render tree in this context (only the AX isolated tree, which is too sparse for this purpose), so
        // until we cache the necessary information let's go to the main-thread.
    } else if (role() == AccessibilityRole::Column || role() == AccessibilityRole::TableHeaderContainer)
        relativeFrame = exposedTableAncestor() ? relativeFrameFromChildren() : FloatRect();

    // Mock objects and SVG objects need use the main thread since they do not have render nodes and are not painted with layers, respectively.
    // FIXME: Remove isNonLayerSVGObject when LBSE is enabled & SVG frames are cached.
    if (!AXObjectCache::shouldServeInitialCachedFrame() || isNonLayerSVGObject()) {
        return Accessibility::retrieveValueFromMainThread<FloatRect>([this] () -> FloatRect {
            if (RefPtr axObject = associatedAXObject())
                return axObject->relativeFrame();
            return { };
        });
    }

    // Having an empty relative frame at this point means a frame hasn't been cached yet.
    if (relativeFrame.isEmpty()) {
        std::optional<IntRect> rectFromLabels;
        if (isControl()) {
            // For controls, we can try to use the frame of any associated labels.
            auto labels = labeledByObjects();
            for (const auto& label : labels) {
                std::optional frame = downcast<AXIsolatedObject>(label)->cachedRelativeFrame();
                if (!frame)
                    continue;
                if (!rectFromLabels)
                    rectFromLabels = *frame;
                else if (rectFromLabels->intersects(*frame))
                    rectFromLabels->unite(*frame);
            }
        }

        if (rectFromLabels && !rectFromLabels->isEmpty())
            relativeFrame = *rectFromLabels;
        else {
            // InitialLocalRect stores the correct size, but not position, of the element before it is painted.
            // We find the position of the nearest painted ancestor to use as the position until the object's frame
            // is cached during painting.
            relativeFrame = rectAttributeValue<FloatRect>(AXProperty::InitialLocalRect);

            std::optional<IntRect> ancestorRelativeFrame;
            Accessibility::findAncestor<AXIsolatedObject>(*this, false, [&] (const auto& object) {
                ancestorRelativeFrame = object.cachedRelativeFrame();
                return ancestorRelativeFrame;
            });

            if (ancestorRelativeFrame)
                relativeFrame.setLocation(ancestorRelativeFrame->location());
        }

        // If an assistive technology is requesting the frame for something,
        // chances are it's on-screen, so clamp to 0,0 if necessary.
        if (relativeFrame.x() < 0)
            relativeFrame.setX(0);
        if (relativeFrame.y() < 0)
            relativeFrame.setY(0);
    }
    relativeFrame.moveBy({ remoteFrameOffset() });
    return relativeFrame;
}

FloatRect AXIsolatedObject::relativeFrameFromChildren() const
{
    FloatRect rect;
    for (const auto& child : const_cast<AXIsolatedObject*>(this)->unignoredChildren())
        rect.unite(child->relativeFrame());
    return rect;
}

FloatRect AXIsolatedObject::convertFrameToSpace(const FloatRect& rect, AccessibilityConversionSpace space) const
{
    if (space == AccessibilityConversionSpace::Screen) {
        if (RefPtr rootNode = tree()->rootNode()) {
            auto rootPoint = rootNode->propertyValue<FloatPoint>(AXProperty::ScreenRelativePosition);
            auto rootRelativeFrame = rootNode->relativeFrame();
            // Relative frames are top-left origin, but screen relative positions are bottom-left origin.
            FloatPoint position = { rootPoint.x() + rect.x(), rootPoint.y() + (rootRelativeFrame.maxY() - rect.maxY()) };
            return { WTFMove(position), rect.size() };
        }
    }

    return Accessibility::retrieveValueFromMainThread<FloatRect>([&rect, &space, this] () -> FloatRect {
        if (RefPtr axObject = associatedAXObject())
            return axObject->convertFrameToSpace(rect, space);
        return { };
    });
}

bool AXIsolatedObject::replaceTextInRange(const String& replacementText, const CharacterRange& textRange)
{
    return Accessibility::retrieveValueFromMainThread<bool>([text = replacementText.isolatedCopy(), &textRange, this] () -> bool {
        if (RefPtr axObject = associatedAXObject())
            return axObject->replaceTextInRange(text, textRange);
        return false;
    });
}

bool AXIsolatedObject::insertText(const String& text)
{
    AXTRACE(makeString("AXIsolatedObject::insertText text = "_s, text));

    // Dispatch to the main thread without waiting since AXObject::insertText waits for the UI process that can be waiting resulting in a deadlock. That is the case when running LayoutTests.
    // The return value of insertText is not used, so not waiting does not result in any loss of functionality.
    performFunctionOnMainThread([text = text.isolatedCopy()] (auto* axObject) {
        axObject->insertText(text);
    });
    return true;
}

bool AXIsolatedObject::press()
{
    ASSERT(isMainThread());

    if (RefPtr object = associatedAXObject())
        return object->press();
    return false;
}

void AXIsolatedObject::increment()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->increment();
    });
}

void AXIsolatedObject::decrement()
{
    performFunctionOnMainThread([] (auto* axObject) {
        axObject->decrement();
    });
}

bool AXIsolatedObject::isAccessibilityNodeObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isAccessibilityRenderObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

int AXIsolatedObject::insertionPointLineNumber() const
{
    if (!boolAttributeValue(AXProperty::CanBeMultilineTextField))
        return 0;

    auto selectedMarkerRange = selectedTextMarkerRange();
    if (selectedMarkerRange.start().isNull() || !selectedMarkerRange.isCollapsed()) {
        // If the selection is not collapsed, we don't know whether the insertion point is at the start or the end, so return -1.
        return -1;
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr selectionObject = selectedMarkerRange.start().isolatedObject();
        if (isTextControl() && selectionObject && isAncestorOfObject(*selectionObject))
            return selectedMarkerRange.start().lineIndex();
        return -1;
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<int>([this] () -> int {
        if (RefPtr axObject = associatedAXObject())
            return axObject->insertionPointLineNumber();
        return -1;
    });
}

String AXIsolatedObject::identifierAttribute() const
{
#if !LOG_DISABLED
    return stringAttributeValue(AXProperty::IdentifierAttribute);
#else
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (RefPtr object = associatedAXObject())
            return object->identifierAttribute().isolatedCopy();
        return { };
    });
#endif
}

CharacterRange AXIsolatedObject::doAXRangeForLine(unsigned lineIndex) const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return AXTextMarker { *this, 0 }.characterRangeForLine(lineIndex);
#endif

    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&lineIndex, this] () -> CharacterRange {
        if (RefPtr object = associatedAXObject())
            return object->doAXRangeForLine(lineIndex);
        return { };
    });
}

String AXIsolatedObject::doAXStringForRange(const CharacterRange& range) const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return textMarkerRange().toString().substring(range.location, range.length);
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    return Accessibility::retrieveValueFromMainThread<String>([&range, this] () -> String {
        if (RefPtr object = associatedAXObject())
            return object->doAXStringForRange(range).isolatedCopy();
        return { };
    });
}

CharacterRange AXIsolatedObject::characterRangeForPoint(const IntPoint& point) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&point, this] () -> CharacterRange {
        if (RefPtr object = associatedAXObject())
            return object->characterRangeForPoint(point);
        return { };
    });
}

CharacterRange AXIsolatedObject::doAXRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&index, this] () -> CharacterRange {
        if (RefPtr object = associatedAXObject())
            return object->doAXRangeForIndex(index);
        return { };
    });
}

CharacterRange AXIsolatedObject::doAXStyleRangeForIndex(unsigned index) const
{
    return Accessibility::retrieveValueFromMainThread<CharacterRange>([&index, this] () -> CharacterRange {
        if (RefPtr object = associatedAXObject())
            return object->doAXStyleRangeForIndex(index);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRange(const CharacterRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<IntRect>([&axRange, this] () -> IntRect {
        if (RefPtr object = associatedAXObject())
            return object->doAXBoundsForRange(axRange);
        return { };
    });
}

IntRect AXIsolatedObject::doAXBoundsForRangeUsingCharacterOffset(const CharacterRange& axRange) const
{
    return Accessibility::retrieveValueFromMainThread<IntRect>([&axRange, this] () -> IntRect {
        if (RefPtr object = associatedAXObject())
            return object->doAXBoundsForRangeUsingCharacterOffset(axRange);
        return { };
    });
}


unsigned AXIsolatedObject::doAXLineForIndex(unsigned index)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis())
        return AXTextMarker { *this, 0 }.lineNumberForIndex(index);
#endif

    return Accessibility::retrieveValueFromMainThread<unsigned>([&index, this] () -> unsigned {
        if (RefPtr object = associatedAXObject())
            return object->doAXLineForIndex(index);
        return 0;
    });
}

VisibleSelection AXIsolatedObject::selection() const
{
    ASSERT(isMainThread());

    RefPtr object = associatedAXObject();
    return object ? object->selection() : VisibleSelection();
}

void AXIsolatedObject::setSelectedVisiblePositionRange(const VisiblePositionRange& visiblePositionRange) const
{
    ASSERT(isMainThread());

    if (RefPtr object = associatedAXObject())
        object->setSelectedVisiblePositionRange(visiblePositionRange);
}

#if ENABLE(MODEL_ELEMENT_ACCESSIBILITY)

ModelPlayerAccessibilityChildren AXIsolatedObject::modelElementChildren()
{
    return Accessibility::retrieveValueFromMainThread<ModelPlayerAccessibilityChildren>([this] -> ModelPlayerAccessibilityChildren {
        if (RefPtr object = associatedAXObject())
            return object->modelElementChildren();
        return { };
    });
}

#endif

std::optional<SimpleRange> AXIsolatedObject::simpleRange() const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->simpleRange() : std::nullopt;
}

VisiblePositionRange AXIsolatedObject::visiblePositionRange() const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRange() : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForLine(unsigned index) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRangeForLine(index) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::visiblePositionRangeForUnorderedPositions(const VisiblePosition& position1, const VisiblePosition& position2) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionRangeForUnorderedPositions(position1, position2) : visiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::leftLineVisiblePositionRange(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->leftLineVisiblePositionRange(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::rightLineVisiblePositionRange(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->rightLineVisiblePositionRange(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::styleRangeForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->styleRangeForPosition(position) : VisiblePositionRange();
}

VisiblePositionRange AXIsolatedObject::lineRangeForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->lineRangeForPosition(position) : VisiblePositionRange();
}

VisiblePosition AXIsolatedObject::visiblePositionForIndex(unsigned index, bool lastIndexOK) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->visiblePositionForIndex(index, lastIndexOK) : VisiblePosition();
}

int AXIsolatedObject::lineForPosition(const VisiblePosition& position) const
{
    ASSERT(isMainThread());
    RefPtr axObject = associatedAXObject();
    return axObject ? axObject->lineForPosition(position) : -1;
}

bool AXIsolatedObject::isMockObject() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isNonNativeTextControl() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isOnScreen() const
{
    return Accessibility::retrieveValueFromMainThread<bool>([this] () -> bool {
        if (RefPtr object = associatedAXObject())
            return object->isOnScreen();
        return false;
    });
}

bool AXIsolatedObject::isOffScreen() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isPressed() const
{
#if PLATFORM(MAC)
    ASSERT_NOT_REACHED();
#endif
    return boolAttributeValue(AXProperty::IsPressed);
}

bool AXIsolatedObject::isSelectedOptionActive() const
{
    ASSERT_NOT_REACHED();
    return false;
}

Vector<AXTextMarkerRange> AXIsolatedObject::misspellingRanges() const
{
    return Accessibility::retrieveValueFromMainThread<Vector<AXTextMarkerRange>>([this] () -> Vector<AXTextMarkerRange> {
        if (RefPtr axObject = associatedAXObject())
            return axObject->misspellingRanges();
        return { };
    });
}

bool AXIsolatedObject::hasRowGroupTag() const
{
    auto elementName = this->elementName();
    return elementName == ElementName::HTML_thead || elementName == ElementName::HTML_tbody || elementName == ElementName::HTML_tfoot;
}

bool AXIsolatedObject::hasSameFont(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        // Having a font only really makes sense for text, so if this or otherObject isn't text, find the first text descendant to compare.
        RefPtr thisText = selfOrFirstTextDescendant();
        RefPtr otherText = otherObject.selfOrFirstTextDescendant();

        if (!thisText || !otherText) {
            // We can't make a meaningful comparison unless we have two objects to compare, so return false.
            return false;
        }
        return thisText->font() == otherText->font();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (RefPtr axObject = associatedAXObject()) {
            if (RefPtr axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameFont(*axOtherObject);
        }
        return false;
    });
}

bool AXIsolatedObject::hasSameFontColor(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr thisText = downcast<AXIsolatedObject>(selfOrFirstTextDescendant());
        RefPtr otherText = downcast<AXIsolatedObject>(otherObject.selfOrFirstTextDescendant());

        if (!thisText || !otherText)
            return false;
        return thisText->colorAttributeValue(AXProperty::TextColor) == otherText->colorAttributeValue(AXProperty::TextColor);
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (RefPtr axObject = associatedAXObject()) {
            if (RefPtr axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameFontColor(*axOtherObject);
        }
        return false;
    });
}

bool AXIsolatedObject::hasSameStyle(AXCoreObject& otherObject)
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    if (AXObjectCache::useAXThreadTextApis()) {
        RefPtr thisText = selfOrFirstTextDescendant();
        RefPtr otherText = otherObject.selfOrFirstTextDescendant();

        if (!thisText || !otherText)
            return false;
        return thisText->stylesForAttributedString() == otherText->stylesForAttributedString();
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (!is<AXIsolatedObject>(otherObject))
        return false;

    return Accessibility::retrieveValueFromMainThread<bool>([&otherObject, this] () -> bool {
        if (RefPtr axObject = associatedAXObject()) {
            if (RefPtr axOtherObject = downcast<AXIsolatedObject>(otherObject).associatedAXObject())
                return axObject->hasSameStyle(*axOtherObject);
        }
        return false;
    });
}

AXTextMarkerRange AXIsolatedObject::textInputMarkedTextMarkerRange() const
{
    size_t index = indexOfProperty(AXProperty::TextInputMarkedTextMarkerRange);
    if (index == notFound)
        return nullptr;

    return WTF::switchOn(m_properties[index].second,
        [&] (const std::unique_ptr<AXIDAndCharacterRange>& typedValue) -> AXTextMarkerRange {
            auto start = static_cast<unsigned>(typedValue->second.location);
            auto end = start + static_cast<unsigned>(typedValue->second.length);
            return { tree()->treeID(), typedValue->first, start, end };
        },
        [] (auto&) -> AXTextMarkerRange { return { }; }
    );
}

// The attribute this value is exposed as is not used by VoiceOver or any other AX client on macOS, so we intentionally don't cache it.
// Re-visit if ITM expands to more platforms, or if AX clients need to start using this.
String AXIsolatedObject::linkRelValue() const
{
    return Accessibility::retrieveValueFromMainThread<String>([this] () -> String {
        if (RefPtr object = associatedAXObject())
            return object->linkRelValue().isolatedCopy();
        return { };
    });
}

#if ENABLE_ACCESSIBILITY_LOCAL_FRAME

AXIsolatedObject* AXIsolatedObject::crossFrameParentObject() const
{
    if (role() != AccessibilityRole::ScrollArea)
        return nullptr;

    auto parentFrameID = optionalAttributeValue<FrameIdentifier>(AXProperty::CrossFrameParentFrameID);
    if (!parentFrameID)
        return nullptr;

    auto optionalParentObjectID = optionalAttributeValue<Markable<AXID>>(AXProperty::CrossFrameParentAXID);

    // TODO: add helpers to retrieve an AXID directly to clean this up.
    if (!optionalParentObjectID)
        return nullptr;

    auto markableParentObjectID = *optionalParentObjectID;
    if (!markableParentObjectID)
        return nullptr;

    auto parentObjectID = *markableParentObjectID;

    RefPtr parentTree = AXIsolatedTree::treeForFrameIDAlreadyLocked(*parentFrameID);
    if (!parentTree)
        return nullptr;

    return parentTree->objectForID(parentObjectID);
}

AXIsolatedObject* AXIsolatedObject::crossFrameChildObject() const
{
    if (role() != AccessibilityRole::LocalFrame)
        return nullptr;

    auto frameID = optionalAttributeValue<FrameIdentifier>(AXProperty::CrossFrameChildFrameID);
    if (!frameID)
        return nullptr;

    RefPtr<AXIsolatedTree> childTree;
    childTree = AXIsolatedTree::treeForFrameIDAlreadyLocked(*frameID);
    if (!childTree)
        return nullptr;

    childTree->applyPendingChanges();

    return childTree->rootNode();
}

#endif // ENABLE_ACCESSIBILITY_LOCAL_FRAME

Element* AXIsolatedObject::element() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Node* AXIsolatedObject::node() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

RenderObject* AXIsolatedObject::renderer() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::supportsHasPopup() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::supportsChecked() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isModalNode() const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::isTableCell() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::parentTableIfTableCell() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject* AXIsolatedObject::parentTable() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isTableRow() const
{
    ASSERT_NOT_REACHED();
    return false;
}

AXCoreObject* AXIsolatedObject::parentTableIfExposedTableRow() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

bool AXIsolatedObject::isDescendantOfRole(AccessibilityRole) const
{
    ASSERT_NOT_REACHED();
    return false;
}

bool AXIsolatedObject::inheritsPresentationalRole() const
{
    ASSERT_NOT_REACHED();
    return false;
}

void AXIsolatedObject::setAccessibleName(const AtomString&)
{
    ASSERT_NOT_REACHED();
}

String AXIsolatedObject::textContentPrefixFromListMarker() const
{
    return propertyValue<String>(AXProperty::TextContentPrefixFromListMarker);
}

String AXIsolatedObject::stringValue() const
{
#if ENABLE(AX_THREAD_TEXT_APIS)
    size_t index = indexOfProperty(AXProperty::StringValue);
    if (index == notFound) {
        if (hasStitchableRole()) {
            auto stitchState = this->stitchState();
            if (!stitchState.stitchedIntoID)
                return textMarkerRange().toString(IncludeListMarkerText::No);

            AXID thisID = objectID();
            if (*stitchState.stitchedIntoID != thisID) {
                // |this| is stitched into another object, so don't return any string value.
                return emptyString();
            }

            // |this| is the sum of several stitched text-like objects. Our string value should
            // include all of them.
            //
            // We can compute the stringValue of rendered text using AXProperty::TextRuns.
            // See AccessibilityObject::shouldCacheStringValue.
            auto startMarker = AXTextMarker { *this, 0 };
            AXTextMarker endMarker;

            RefPtr tree = std::get<RefPtr<AXIsolatedTree>>(axTreeForID(treeID()));
            if (!tree || stitchState.group.isEmpty())
                return textMarkerRange().toString(IncludeListMarkerText::No);

            for (auto axID = stitchState.group.rbegin(); axID != stitchState.group.rend() && *axID != thisID; ++axID) {
                if (RefPtr object = tree->objectForID(*axID)) {
                    if (const auto* runs = object->textRuns()) {
                        endMarker = AXTextMarker { *object, runs->totalLength() };
                        break;
                    }
                }
            }

            if (!endMarker.isValid())
                return textMarkerRange().toString(IncludeListMarkerText::No);

            return AXTextMarkerRange { WTFMove(startMarker), WTFMove(endMarker) }.toString(IncludeListMarkerText::No);
        }
        return emptyString();
    }

    return WTF::switchOn(m_properties[index].second,
        [] (const String& typedValue) { return typedValue; },
        [] (auto&) { return emptyString(); }
    );
#else
    if (std::optional stringValue = optionalAttributeValue<String>(AXProperty::StringValue))
        return *stringValue;
    if (auto value = platformStringValue())
        return *value;
    return { };
#endif // ENABLE(AX_THREAD_TEXT_APIS)
}

String AXIsolatedObject::text() const
{
    ASSERT_NOT_REACHED();
    return String();
}

#if !PLATFORM(COCOA)
unsigned AXIsolatedObject::textLength() const
{
    ASSERT_NOT_REACHED();
    return 0;
}
#endif

AXObjectCache* AXIsolatedObject::axObjectCache() const
{
    ASSERT(isMainThread());
    return tree()->axObjectCache();
}

Element* AXIsolatedObject::actionElement() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Widget* AXIsolatedObject::widget() const
{
    RefPtr object = associatedAXObject();
    return object ? object->widget() : nullptr;
}

PlatformWidget AXIsolatedObject::platformWidget() const
{
#if PLATFORM(COCOA)
    return propertyValue<RetainPtr<NSView>>(AXProperty::PlatformWidget).unsafeGet();
#else
    return m_platformWidget;
#endif
}

Widget* AXIsolatedObject::widgetForAttachmentView() const
{
    ASSERT_NOT_REACHED();
    return nullptr;
}

Page* AXIsolatedObject::page() const
{
    ASSERT(isMainThread());

    if (RefPtr axObject = associatedAXObject())
        return axObject->page();

    ASSERT_NOT_REACHED();
    return nullptr;
}

Document* AXIsolatedObject::document() const
{
    ASSERT(isMainThread());

    if (RefPtr axObject = associatedAXObject())
        return axObject->document();

    ASSERT_NOT_REACHED();
    return nullptr;
}

LocalFrameView* AXIsolatedObject::documentFrameView() const
{
    ASSERT(isMainThread());

    if (RefPtr axObject = associatedAXObject())
        return axObject->documentFrameView();

    ASSERT_NOT_REACHED();
    return nullptr;
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::relatedObjects(AXRelation relation) const
{
    if (auto relatedObjectIDs = tree()->relatedObjectIDsFor(*this, relation))
        return tree()->objectsForIDs(*relatedObjectIDs);
    return { };
}

String AXIsolatedObject::innerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXProperty::InnerHTML);
}

String AXIsolatedObject::outerHTML() const
{
    return const_cast<AXIsolatedObject*>(this)->getOrRetrievePropertyValue<String>(AXProperty::OuterHTML);
}

AXCoreObject::AccessibilityChildrenVector AXIsolatedObject::rowHeaders()
{
    AccessibilityChildrenVector headers;
    if (isTable()) {
        auto rowsCopy = rows();
        for (const auto& row : rowsCopy) {
            if (RefPtr header = row->rowHeader())
                headers.append(header.releaseNonNull());
        }
    } else if (isExposedTableCell()) {
        RefPtr parent = exposedTableAncestor();
        if (!parent)
            return { };

        auto rowRange = rowIndexRange();
        auto colRange = columnIndexRange();
        for (unsigned column = 0; column < colRange.first; column++) {
            RefPtr tableCell = parent->cellForColumnAndRow(column, rowRange.first);
            if (!tableCell || tableCell == this || headers.contains(Ref { *tableCell }))
                continue;

            if (tableCell->cellScope() == "rowgroup"_s && isTableCellInSameRowGroup(*tableCell))
                headers.append(tableCell.releaseNonNull());
            else if (tableCell->isRowHeader())
                headers.append(tableCell.releaseNonNull());
        }
    }
    return headers;
}

AXIsolatedObject* AXIsolatedObject::tableHeaderContainer()
{
    for (const auto& child : unignoredChildren()) {
        if (child->role() == AccessibilityRole::TableHeaderContainer)
            return downcast<AXIsolatedObject>(child.ptr());
    }
    return nullptr;
}

#if !PLATFORM(MAC)
IntPoint AXIsolatedObject::clickPoint()
{
    ASSERT_NOT_REACHED();
    return { };
}

Vector<String> AXIsolatedObject::determineDropEffects() const
{
    ASSERT_NOT_REACHED();
    return { };
}

bool AXIsolatedObject::pressedIsPresent() const
{
    ASSERT_NOT_REACHED();
    return false;
}

int AXIsolatedObject::layoutCount() const
{
    ASSERT_NOT_REACHED();
    return 0;
}

Vector<String> AXIsolatedObject::classList() const
{
    ASSERT_NOT_REACHED();
    return { };
}

String AXIsolatedObject::computedRoleString() const
{
    ASSERT_NOT_REACHED();
    return { };
}
#endif

} // namespace WebCore

#endif // ENABLE((ACCESSIBILITY_ISOLATED_TREE)
