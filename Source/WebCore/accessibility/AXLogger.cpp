/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXLogger.h"

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
#include "AXIsolatedObject.h"
#endif
#include "AXNotifications.h"
#include "AXObjectCache.h"
#include "AXSearchManager.h"
#include "AXTextRun.h"
#include "AXUtilities.h"
#include "DocumentView.h"
#include "LocalFrameView.h"
#include "LogInitialization.h"
#include "Logging.h"
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/OptionSet.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

#if !LOG_DISABLED

bool AXLogger::shouldLog()
{
    // Modify the initializer list below to choose what thread you want to log messages from.
    static constexpr OptionSet<AXLoggingOptions> loggingOptions { AXLoggingOptions::MainThread, AXLoggingOptions::OffMainThread };

    // Add strings to the Vector below to just log from instances whose m_methodName includes any of the strings.
    // For instance, if you want to just log from the wrapper and the AXIsolatedTree class:
    // static NeverDestroyed nameFilter = Vector<String> { "WebAccessibilityObjectWrapper"_s, "AXIsolatedTree"_s };
    // The default string "log nothing", prevents any output. An empty Vector or an empty string in the Vector will log everything.
    static NeverDestroyed nameFilter = Vector<String> { "log nothing"_s };

    if (!nameFilter->isEmpty()) {
        auto it = std::find_if(nameFilter->begin(), nameFilter->end(), [this] (const auto& name) {
            return m_methodName.contains(name);
        });
        if (it == nameFilter->end())
            return false;
    }

    if (isMainThread())
        return loggingOptions.contains(AXLoggingOptions::MainThread);
    return loggingOptions.contains(AXLoggingOptions::OffMainThread);
}

AXLogger::AXLogger(const String& methodName)
    : m_methodName(methodName)
{
    auto initializeAXChannel = [] () {
        static std::atomic<bool> initialized = false;
        if (initialized)
            return;

        if (auto* channel = getLogChannel("Accessibility"_s)) {
            LogAccessibility.state = WTFLogChannelState::On;
            channel->level = WTFLogLevel::Debug;
            initialized = true;
        }
    };
    initializeAXChannel();

    if (shouldLog()) {
        if (!m_methodName.isEmpty())
            LOG_WITH_STREAM(Accessibility, stream << m_methodName << " {");
    }
    m_startTime = MonotonicTime::now();
}

AXLogger::~AXLogger()
{
    static const Seconds ExecutionTimeThreshold { 1_s };
    auto elapsedTime = MonotonicTime::now() - m_startTime;
    if (shouldLog() && !m_methodName.isEmpty()) {
        if (elapsedTime > ExecutionTimeThreshold)
            LOG_WITH_STREAM(Accessibility, stream << "} " << m_methodName << " exceeded ExecutionTimeThreshold " << elapsedTime);
        else
            LOG_WITH_STREAM(Accessibility, stream << "} " << m_methodName << " took " << elapsedTime);
    }
}

void AXLogger::log(const String& message)
{
    if (shouldLog())
        LOG(Accessibility, "%s", message.utf8().data());
}

void AXLogger::log(const char* message)
{
    if (shouldLog())
        LOG(Accessibility, "%s", message);
}

void AXLogger::log(const AXCoreObject& object)
{
    log(const_cast<AXCoreObject*>(&object));
}

void AXLogger::log(RefPtr<AXCoreObject> object)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);

        if (object)
            stream << *object;
        else
            stream << "null";

        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const Vector<Ref<AXCoreObject>>& objects)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);

        stream << "[";
        for (auto object : objects)
            stream << object.get();
        stream << "]";

        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const std::pair<Ref<AccessibilityObject>, AXNotification>& notification)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);
        stream << "Notification " << notification.second << " for object ";
        stream << notification.first.get();
        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const std::pair<RefPtr<AXCoreObject>, AXNotification>& notification)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);
        stream << "Notification " << notification.second << " for object ";
        if (notification.first)
            stream << *notification.first;
        else
            stream << "null";
        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const AccessibilitySearchCriteria& criteria)
{
    if (!shouldLog())
        return;

    TextStream stream(TextStream::LineMode::MultipleLine);
    stream << criteria;
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

void AXLogger::log(AccessibilityObjectInclusion inclusion)
{
    if (!shouldLog())
        return;

    TextStream stream(TextStream::LineMode::SingleLine);
    stream.dumpProperty("ObjectInclusion"_s, inclusion);
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

void AXLogger::log(AXRelation relation)
{
    if (!shouldLog())
        return;

    TextStream stream(TextStream::LineMode::SingleLine);
    stream.dumpProperty("RelationType"_s, relation);
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXLogger::log(AXIsolatedTree& tree)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);
        stream << tree;
        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}
#endif

void AXLogger::log(AXObjectCache& axObjectCache)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);
        stream << axObjectCache;
        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const String& collectionName, const AXObjectCache::DeferredCollection& collection)
{
    unsigned size = 0;
    WTF::switchOn(collection,
        [&size] (const HashMap<Element*, String>& typedCollection) { size = typedCollection.size(); },
        [&size] (const HashSet<AXID>& typedCollection) { size = typedCollection.size(); },
        [&size] (const ListHashSet<Node*>& typedCollection) { size = typedCollection.size(); },
        [&size] (const ListHashSet<Ref<AccessibilityObject>>& typedCollection) { size = typedCollection.size(); },
        [&size] (const Vector<AXObjectCache::AttributeChange>& typedCollection) { size = typedCollection.size(); },
        [&size] (const Vector<std::pair<Node*, Node*>>& typedCollection) { size = typedCollection.size(); },
        [&size] (const WeakHashSet<Element, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<AccessibilityObject>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<AccessibilityNodeObject>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakListHashSet<Node, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakListHashSet<Element, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashMap<Element, String, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [] (auto&) {
            ASSERT_NOT_REACHED();
            return;
        });
    if (size)
        log(makeString(collectionName, " size: "_s, size));
}

#endif // !LOG_DISABLED

TextStream& operator<<(TextStream& stream, AccessibilityRole role)
{
    stream << roleToString(role);
    return stream;
}

TextStream& operator<<(TextStream& stream, AccessibilitySearchDirection direction)
{
    switch (direction) {
    case AccessibilitySearchDirection::Next:
        stream << "Next";
        break;
    case AccessibilitySearchDirection::Previous:
        stream << "Previous";
        break;
    };

    return stream;
}

TextStream& operator<<(TextStream& stream, AccessibilitySearchKey searchKey)
{
    switch (searchKey) {
    case AccessibilitySearchKey::AnyType:
        stream << "AnyType";
        break;
    case AccessibilitySearchKey::Article:
        stream << "Article";
        break;
    case AccessibilitySearchKey::BlockquoteSameLevel:
        stream << "BlockquoteSameLevel";
        break;
    case AccessibilitySearchKey::Blockquote:
        stream << "Blockquote";
        break;
    case AccessibilitySearchKey::BoldFont:
        stream << "BoldFont";
        break;
    case AccessibilitySearchKey::Button:
        stream << "Button";
        break;
    case AccessibilitySearchKey::Checkbox:
        stream << "Checkbox";
        break;
    case AccessibilitySearchKey::Control:
        stream << "Control";
        break;
    case AccessibilitySearchKey::DifferentType:
        stream << "DifferentType";
        break;
    case AccessibilitySearchKey::FontChange:
        stream << "FontChange";
        break;
    case AccessibilitySearchKey::FontColorChange:
        stream << "FontColorChange";
        break;
    case AccessibilitySearchKey::Frame:
        stream << "Frame";
        break;
    case AccessibilitySearchKey::Graphic:
        stream << "Graphic";
        break;
    case AccessibilitySearchKey::HeadingLevel1:
        stream << "HeadingLevel1";
        break;
    case AccessibilitySearchKey::HeadingLevel2:
        stream << "HeadingLevel2";
        break;
    case AccessibilitySearchKey::HeadingLevel3:
        stream << "HeadingLevel3";
        break;
    case AccessibilitySearchKey::HeadingLevel4:
        stream << "HeadingLevel4";
        break;
    case AccessibilitySearchKey::HeadingLevel5:
        stream << "HeadingLevel5";
        break;
    case AccessibilitySearchKey::HeadingLevel6:
        stream << "HeadingLevel6";
        break;
    case AccessibilitySearchKey::HeadingSameLevel:
        stream << "HeadingSameLevel";
        break;
    case AccessibilitySearchKey::Heading:
        stream << "Heading";
        break;
    case AccessibilitySearchKey::Highlighted:
        stream << "Highlighted";
        break;
    case AccessibilitySearchKey::ItalicFont:
        stream << "ItalicFont";
        break;
    case AccessibilitySearchKey::KeyboardFocusable:
        stream << "KeyboardFocusable";
        break;
    case AccessibilitySearchKey::Landmark:
        stream << "Landmark";
        break;
    case AccessibilitySearchKey::Link:
        stream << "Link";
        break;
    case AccessibilitySearchKey::List:
        stream << "List";
        break;
    case AccessibilitySearchKey::LiveRegion:
        stream << "LiveRegion";
        break;
    case AccessibilitySearchKey::MisspelledWord:
        stream << "MisspelledWord";
        break;
    case AccessibilitySearchKey::Outline:
        stream << "Outline";
        break;
    case AccessibilitySearchKey::PlainText:
        stream << "PlainText";
        break;
    case AccessibilitySearchKey::RadioGroup:
        stream << "RadioGroup";
        break;
    case AccessibilitySearchKey::SameType:
        stream << "SameType";
        break;
    case AccessibilitySearchKey::StaticText:
        stream << "StaticText";
        break;
    case AccessibilitySearchKey::StyleChange:
        stream << "StyleChange";
        break;
    case AccessibilitySearchKey::TableSameLevel:
        stream << "TableSameLevel";
        break;
    case AccessibilitySearchKey::Table:
        stream << "Table";
        break;
    case AccessibilitySearchKey::TextField:
        stream << "TextField";
        break;
    case AccessibilitySearchKey::Underline:
        stream << "Underline";
        break;
    case AccessibilitySearchKey::UnvisitedLink:
        stream << "UnvisitedLink";
        break;
    case AccessibilitySearchKey::VisitedLink:
        stream << "VisitedLink";
        break;
    };

    return stream;
}

TextStream& operator<<(TextStream& stream, const AccessibilitySearchCriteria& criteria)
{
    TextStream::GroupScope groupScope(stream);
    auto streamCriteriaObject = [&stream] (ASCIILiteral objectLabel, auto* axObject) {
        stream.startGroup();
        stream << objectLabel.characters() << " " << axObject << ", ID " << (axObject ? axObject->objectID().toUInt64() : 0);
        stream.endGroup();
    };

    stream << "SearchCriteria " << &criteria;
    streamCriteriaObject("anchorObject"_s, criteria.anchorObject);
    streamCriteriaObject("startObject"_s, criteria.startObject);
    stream.dumpProperty("searchDirection"_s, criteria.searchDirection);

    stream.nextLine();
    stream << "(searchKeys [";
    for (auto searchKey : criteria.searchKeys)
        stream << searchKey << ", ";
    stream << "])";

    stream.dumpProperty("searchText"_s, criteria.searchText);
    stream.dumpProperty("resultsLimit"_s, criteria.resultsLimit);
    stream.dumpProperty("visibleOnly"_s, criteria.visibleOnly);
    stream.dumpProperty("immediateDescendantsOnly"_s, criteria.immediateDescendantsOnly);

    return stream;
}

TextStream& operator<<(TextStream& stream, Vector<AccessibilityText>& texts)
{
    stream << "{"_s;
    for (unsigned i = 0; i < texts.size(); i++) {
        stream << texts[i];
        if (i != texts.size() - 1)
            stream << " ";
    }
    stream << "}"_s;
    return stream;
}

TextStream& operator<<(TextStream& stream, AccessibilityText text)
{
    stream << text.textSource << ": " << text.text;
    return stream;
}

TextStream& operator<<(TextStream& stream, AccessibilityTextSource source)
{
    switch (source) {
    case AccessibilityTextSource::Alternative:
        stream << "Alternative";
        break;
    case AccessibilityTextSource::Children:
        stream << "Children";
        break;
    case AccessibilityTextSource::Summary:
        stream << "Summary";
        break;
    case AccessibilityTextSource::Help:
        stream << "Help";
        break;
    case AccessibilityTextSource::Visible:
        stream << "Visible";
        break;
    case AccessibilityTextSource::TitleTag:
        stream << "TitleTag";
        break;
    case AccessibilityTextSource::Placeholder:
        stream << "Placeholder";
        break;
    case AccessibilityTextSource::LabelByElement:
        stream << "LabelByElement";
        break;
    case AccessibilityTextSource::Title:
        stream << "Title";
        break;
    case AccessibilityTextSource::Subtitle:
        stream << "Subtitle";
        break;
    case AccessibilityTextSource::Action:
        stream << "Action";
        break;
    case AccessibilityTextSource::Heading:
        stream << "Heading";
        break;
    }
    return stream;
}

TextStream& operator<<(TextStream& stream, AccessibilityObjectInclusion inclusion)
{
    switch (inclusion) {
    case AccessibilityObjectInclusion::IncludeObject:
        stream << "IncludeObject";
        break;
    case AccessibilityObjectInclusion::IgnoreObject:
        stream << "IgnoreObject";
        break;
    case AccessibilityObjectInclusion::DefaultBehavior:
        stream << "DefaultBehavior";
        break;
    }

    return stream;
}

TextStream& operator<<(TextStream& stream, AXRelation relation)
{
    switch (relation) {
    case AXRelation::None:
        stream << "None";
        break;
    case AXRelation::ActiveDescendant:
        stream << "ActiveDescendant";
        break;
    case AXRelation::ActiveDescendantOf:
        stream << "ActiveDescendantOf";
        break;
    case AXRelation::ControlledBy:
        stream << "ControlledBy";
        break;
    case AXRelation::ControllerFor:
        stream << "ControllerFor";
        break;
    case AXRelation::DescribedBy:
        stream << "DescribedBy";
        break;
    case AXRelation::DescriptionFor:
        stream << "DescriptionFor";
        break;
    case AXRelation::Details:
        stream << "Details";
        break;
    case AXRelation::DetailsFor:
        stream << "DetailsFor";
        break;
    case AXRelation::ErrorMessage:
        stream << "ErrorMessage";
        break;
    case AXRelation::ErrorMessageFor:
        stream << "ErrorMessageFor";
        break;
    case AXRelation::FlowsFrom:
        stream << "FlowsFrom";
        break;
    case AXRelation::FlowsTo:
        stream << "FlowsTo";
        break;
    case AXRelation::Headers:
        stream << "Headers";
        break;
    case AXRelation::HeaderFor:
        stream << "HeaderFor";
        break;
    case AXRelation::LabeledBy:
        stream << "LabeledBy";
        break;
    case AXRelation::LabelFor:
        stream << "LabelFor";
        break;
    case AXRelation::OwnedBy:
        stream << "OwnedBy";
        break;
    case AXRelation::OwnerFor:
        stream << "OwnerFor";
        break;
    }

    return stream;
}

TextStream& operator<<(WTF::TextStream& stream, const TextUnderElementMode& mode)
{
    String childrenInclusion;
    switch (mode.childrenInclusion) {
    case TextUnderElementMode::Children::SkipIgnoredChildren:
        childrenInclusion = "SkipIgnoredChildren"_s;
        break;
    case TextUnderElementMode::Children::IncludeAllChildren:
        childrenInclusion = "IncludeAllChildren"_s;
        break;
    case TextUnderElementMode::Children::IncludeNameFromContentsChildren:
        childrenInclusion = "IncludeNameFromContentsChildren"_s;
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    stream << childrenInclusion;
    // Only log non-default values to avoid noise.
    if (mode.includeFocusableContent)
        stream << ", includeFocusableContent: 1";
    if (mode.inHiddenSubtree)
        stream << ", inHiddenSubtree: 1";
    if (!mode.considerHiddenState)
        stream << ", considerHiddenState: 0";
    if (mode.ignoredChildNode)
        stream << ", ignoredChildNode: " << mode.ignoredChildNode;
    if (mode.trimWhitespace == TrimWhitespace::No)
        stream << ", trimWhitespace: 0";
    return stream;
}

TextStream& operator<<(TextStream& stream, AXNotification notification)
{
    switch (notification) {
#define WEBCORE_LOG_AXNOTIFICATION(name) \
    case AXNotification::name: \
        stream << #name; \
        break;

        WEBCORE_AXNOTIFICATION_KEYS(WEBCORE_LOG_AXNOTIFICATION)
#undef WEBCORE_LOG_AXNOTIFICATION
    }

    return stream;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
WTF::TextStream& operator<<(WTF::TextStream& stream, const AXPropertyVector& properties)
{
    stream << "{"_s;
    for (size_t i = 0; i < properties.size(); i++) {
        if (i)
            stream << ", ";
        stream << properties[i].first;
    }
    stream << "}"_s;
    return stream;
}

TextStream& operator<<(WTF::TextStream& stream, AXProperty property)
{
    switch (property) {
#if !ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::AttributedText:
        stream << "AttributedText";
        break;
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::AXColumnCount:
        stream << "AXColumnCount";
        break;
    case AXProperty::AXColumnIndex:
        stream << "AXColumnIndex";
        break;
    case AXProperty::AXColumnIndexText:
        stream << "AXColumnIndexText";
        break;
    case AXProperty::AXRowCount:
        stream << "AXRowCount";
        break;
    case AXProperty::AXRowIndex:
        stream << "AXRowIndex";
        break;
    case AXProperty::AXRowIndexText:
        stream << "AXRowIndexText";
        break;
    case AXProperty::Abbreviation:
        stream << "Abbreviation";
        break;
    case AXProperty::AccessKey:
        stream << "AccessKey";
        break;
    case AXProperty::AccessibilityText:
        stream << "AccessibilityText";
        break;
    case AXProperty::ActionVerb:
        stream << "ActionVerb";
        break;
    case AXProperty::ARIARoleDescription:
        stream << "ARIARoleDescription";
        break;
    case AXProperty::ARIALevel:
        stream << "ARIALevel";
        break;
    case AXProperty::BackgroundColor:
        stream << "BackgroundColor";
        break;
    case AXProperty::BrailleLabel:
        stream << "BrailleLabel";
        break;
    case AXProperty::BrailleRoleDescription:
        stream << "BrailleRoleDescription";
        break;
    case AXProperty::ButtonState:
        stream << "ButtonState";
        break;
    case AXProperty::CanBeMultilineTextField:
        stream << "CanBeMultilineTextField";
        break;
    case AXProperty::CanSetFocusAttribute:
        stream << "CanSetFocusAttribute";
        break;
    case AXProperty::CanSetSelectedAttribute:
        stream << "CanSetSelectedAttribute";
        break;
    case AXProperty::CanSetValueAttribute:
        stream << "CanSetValueAttribute";
        break;
#if PLATFORM(MAC)
    case AXProperty::CaretBrowsingEnabled:
        stream << "CaretBrowsingEnabled";
        break;
#endif
    case AXProperty::Cells:
        stream << "Cells";
        break;
    case AXProperty::CellScope:
        stream << "CellScope";
        break;
    case AXProperty::CellSlots:
        stream << "CellSlots";
        break;
    case AXProperty::ColorValue:
        stream << "ColorValue";
        break;
    case AXProperty::Columns:
        stream << "Columns";
        break;
    case AXProperty::ColumnIndex:
        stream << "ColumnIndex";
        break;
    case AXProperty::ColumnIndexRange:
        stream << "ColumnIndexRange";
        break;
    case AXProperty::CrossFrameChildFrameID:
        stream << "CrossFrameChildFrameID";
        break;
    case AXProperty::CrossFrameParentFrameID:
        stream << "CrossFrameParentFrameID";
        break;
    case AXProperty::CrossFrameParentAXID:
        stream << "CrossFrameParentAXID";
        break;
    case AXProperty::CurrentState:
        stream << "CurrentState";
        break;
    case AXProperty::DateTimeComponentsType:
        stream << "DateTimeComponentsType";
        break;
    case AXProperty::DateTimeValue:
        stream << "DateTimeValue";
        break;
    case AXProperty::DatetimeAttributeValue:
        stream << "DatetimeAttributeValue";
        break;
    case AXProperty::DecrementButton:
        stream << "DecrementButton";
        break;
    case AXProperty::Description:
        stream << "Description";
        break;
    case AXProperty::DisclosedByRow:
        stream << "DisclosedByRow";
        break;
    case AXProperty::DisclosedRows:
        stream << "DisclosedRows";
        break;
    case AXProperty::DocumentEncoding:
        stream << "DocumentEncoding";
        break;
    case AXProperty::DocumentLinks:
        stream << "DocumentLinks";
        break;
    case AXProperty::DocumentURI:
        stream << "DocumentURI";
        break;
    case AXProperty::ElementName:
        stream << "ElementName";
        break;
    case AXProperty::EmbeddedImageDescription:
        stream << "EmbeddedImageDescription";
        break;
    case AXProperty::ExplicitAutoCompleteValue:
        stream << "ExplicitAutoCompleteValue";
        break;
    case AXProperty::ExplicitInvalidStatus:
        stream << "ExplicitInvalidStatus";
        break;
    case AXProperty::ExplicitLiveRegionRelevant:
        stream << "ExplicitLiveRegionRelevant";
        break;
    case AXProperty::ExplicitLiveRegionStatus:
        stream << "ExplicitLiveRegionStatus";
        break;
    case AXProperty::ExplicitOrientation:
        stream << "ExplicitOrientation";
        break;
    case AXProperty::ExplicitPopupValue:
        stream << "ExplicitPopupValue";
        break;
    case AXProperty::ExtendedDescription:
        stream << "ExtendedDescription";
        break;
#if PLATFORM(COCOA)
    case AXProperty::Font:
        stream << "Font";
        break;
    case AXProperty::FontOrientation:
        stream << "FontOrientation";
        break;
#endif // PLATFORM(COCOA)
    case AXProperty::TextColor:
        stream << "TextColor";
        break;
    case AXProperty::HasApplePDFAnnotationAttribute:
        stream << "HasApplePDFAnnotationAttribute";
        break;
    case AXProperty::HasBoldFont:
        stream << "HasBoldFont";
        break;
    case AXProperty::HasClickHandler:
        stream << "HasClickHandler";
        break;
    case AXProperty::HasCursorPointer:
        stream << "HasCursorPointer";
        break;
    case AXProperty::HasItalicFont:
        stream << "HasItalicFont";
        break;
    case AXProperty::HasLinethrough:
        stream << "HasLinethrough";
        break;
    case AXProperty::HasPlainText:
        stream << "HasPlainText";
        break;
    case AXProperty::HasPointerEventsNone:
        stream << "HasPointerEventsNone";
        break;
    case AXProperty::HasRemoteFrameChild:
        stream << "HasRemoteFrameChild";
        break;
    case AXProperty::IsBlockFlow:
        stream << "IsBlockFlow";
        break;
    case AXProperty::IsEditableWebArea:
        stream << "IsEditableWebArea";
        break;
    case AXProperty::IsHiddenUntilFoundContainer:
        stream << "IsHiddenUntilFoundContainer";
        break;
    case AXProperty::IsSubscript:
        stream << "IsSubscript";
        break;
    case AXProperty::IsSuperscript:
        stream << "IsSuperscript";
        break;
    case AXProperty::HasTextShadow:
        stream << "HasTextShadow";
        break;
    case AXProperty::HorizontalScrollBar:
        stream << "HorizontalScrollBar";
        break;
    case AXProperty::IdentifierAttribute:
        stream << "IdentifierAttribute";
        break;
    case AXProperty::IncrementButton:
        stream << "IncrementButton";
        break;
    case AXProperty::InitialLocalRect:
        stream << "InitialLocalRect";
        break;
    case AXProperty::InnerHTML:
        stream << "InnerHTML";
        break;
    case AXProperty::InputType:
        stream << "InputType";
        break;
    case AXProperty::InternalLinkElement:
        stream << "InternalLinkElement";
        break;
    case AXProperty::IsGrabbed:
        stream << "IsGrabbed";
        break;
    case AXProperty::IsARIAGridRow:
        stream << "IsARIAGridRow";
        break;
    case AXProperty::IsARIATreeGridRow:
        stream << "IsARIATreeGridRow";
        break;
    case AXProperty::IsAnonymousMathOperator:
        stream << "IsAnonymousMathOperator";
        break;
    case AXProperty::IsAttachment:
        stream << "IsAttachment";
        break;
    case AXProperty::IsBusy:
        stream << "IsBusy";
        break;
    case AXProperty::IsChecked:
        stream << "IsChecked";
        break;
    case AXProperty::IsColumnHeader:
        stream << "IsColumnHeader";
        break;
    case AXProperty::IsEnabled:
        stream << "IsEnabled";
        break;
    case AXProperty::IsExpanded:
        stream << "IsExpanded";
        break;
    case AXProperty::IsExposableTable:
        stream << "IsExposableTable";
        break;
    case AXProperty::IsExposedTableCell:
        stream << "IsExposedTableCell";
        break;
    case AXProperty::IsFieldset:
        stream << "IsFieldset";
        break;
    case AXProperty::IsIgnored:
        stream << "IsIgnored";
        break;
    case AXProperty::IsIndeterminate:
        stream << "IsIndeterminate";
        break;
    case AXProperty::IsInlineText:
        stream << "IsInlineText";
        break;
    case AXProperty::IsKeyboardFocusable:
        stream << "IsKeyboardFocusable";
        break;
    case AXProperty::IsMathElement:
        stream << "IsMathElement";
        break;
    case AXProperty::IsMathFraction:
        stream << "IsMathFraction";
        break;
    case AXProperty::IsMathFenced:
        stream << "IsMathFenced";
        break;
    case AXProperty::IsMathSubscriptSuperscript:
        stream << "IsMathSubscriptSuperscript";
        break;
    case AXProperty::IsMathRow:
        stream << "IsMathRow";
        break;
    case AXProperty::IsMathUnderOver:
        stream << "IsMathUnderOver";
        break;
    case AXProperty::IsMathRoot:
        stream << "IsMathRoot";
        break;
    case AXProperty::IsMathSquareRoot:
        stream << "IsMathSquareRoot";
        break;
    case AXProperty::IsMathTable:
        stream << "IsMathTable";
        break;
    case AXProperty::IsMathTableRow:
        stream << "IsMathTableRow";
        break;
    case AXProperty::IsMathTableCell:
        stream << "IsMathTableCell";
        break;
    case AXProperty::IsMathMultiscript:
        stream << "IsMathMultiscript";
        break;
    case AXProperty::IsMathToken:
        stream << "IsMathToken";
        break;
    case AXProperty::IsMultiSelectable:
        stream << "IsMultiSelectable";
        break;
    case AXProperty::IsNonLayerSVGObject:
        stream << "IsNonLayerSVGObject";
        break;
    case AXProperty::IsPlugin:
        stream << "IsPlugin";
        break;
    case AXProperty::IsPressed:
        stream << "IsPressed";
        break;
    case AXProperty::IsRequired:
        stream << "IsRequired";
        break;
    case AXProperty::IsRowHeader:
        stream << "IsRowHeader";
        break;
    case AXProperty::IsSecureField:
        stream << "IsSecureField";
        break;
    case AXProperty::IsSelected:
        stream << "IsSelected";
        break;
    case AXProperty::IsSelectedOptionActive:
        stream << "IsSelectedOptionActive";
        break;
    case AXProperty::IsTable:
        stream << "IsTable";
        break;
    case AXProperty::IsExposedTableRow:
        stream << "IsExposedTableRow";
        break;
    case AXProperty::IsTextEmissionBehaviorDoubleNewline:
        stream << "IsTextEmissionBehaviorDoubleNewline";
        break;
    case AXProperty::IsTextEmissionBehaviorNewline:
        stream << "IsTextEmissionBehaviorNewline";
        break;
    case AXProperty::IsTextEmissionBehaviorTab:
        stream << "IsTextEmissionBehaviorTab";
        break;
    case AXProperty::IsTree:
        stream << "IsTree";
        break;
    case AXProperty::IsTreeItem:
        stream << "IsTreeItem";
        break;
    case AXProperty::IsValueAutofillAvailable:
        stream << "IsValueAutofillAvailable";
        break;
    case AXProperty::IsVisible:
        stream << "IsVisible";
        break;
    case AXProperty::IsVisited:
        stream << "IsVisited";
        break;
    case AXProperty::IsWidget:
        stream << "IsWidget";
        break;
    case AXProperty::KeyShortcuts:
        stream << "KeyShortcuts";
        break;
    case AXProperty::Language:
        stream << "Language";
        break;
    case AXProperty::LinethroughColor:
        stream << "LinethroughColor";
        break;
#if ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::ListMarkerLineID:
        stream << "ListMarkerLineID";
        break;
    case AXProperty::ListMarkerText:
        stream << "ListMarkerText";
        break;
#endif // ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::LiveRegionAtomic:
        stream << "LiveRegionAtomic";
        break;
    case AXProperty::LocalizedActionVerb:
        stream << "LocalizedActionVerb";
        break;
    case AXProperty::MathFencedOpenString:
        stream << "MathFencedOpenString";
        break;
    case AXProperty::MathFencedCloseString:
        stream << "MathFencedCloseString";
        break;
    case AXProperty::MathLineThickness:
        stream << "MathLineThickness";
        break;
    case AXProperty::MathPrescripts:
        stream << "MathPrescripts";
        break;
    case AXProperty::MathPostscripts:
        stream << "MathPostscripts";
        break;
    case AXProperty::MathRadicand:
        stream << "MathRadicand";
        break;
    case AXProperty::MathRootIndexObject:
        stream << "MathRootIndexObject";
        break;
    case AXProperty::MathUnderObject:
        stream << "MathUnderObject";
        break;
    case AXProperty::MathOverObject:
        stream << "MathOverObject";
        break;
    case AXProperty::MathNumeratorObject:
        stream << "MathNumeratorObject";
        break;
    case AXProperty::MathDenominatorObject:
        stream << "MathDenominatorObject";
        break;
    case AXProperty::MathBaseObject:
        stream << "MathBaseObject";
        break;
    case AXProperty::MathSubscriptObject:
        stream << "MathSubscriptObject";
        break;
    case AXProperty::MathSuperscriptObject:
        stream << "MathSuperscriptObject";
        break;
    case AXProperty::MaxValueForRange:
        stream << "MaxValueForRange";
        break;
    case AXProperty::MinValueForRange:
        stream << "MinValueForRange";
        break;
    case AXProperty::NameAttribute:
        stream << "NameAttribute";
        break;
    case AXProperty::OuterHTML:
        stream << "OuterHTML";
        break;
    case AXProperty::Path:
        stream << "Path";
        break;
    case AXProperty::PlaceholderValue:
        stream << "PlaceholderValue";
        break;
#if PLATFORM(COCOA)
    case AXProperty::PlatformWidget:
        stream << "PlatformWidget";
        break;
#endif
    case AXProperty::PosInSet:
        stream << "PosInSet";
        break;
    case AXProperty::PreventKeyboardDOMEventDispatch:
        stream << "PreventKeyboardDOMEventDispatch";
        break;
    case AXProperty::RadioButtonGroupMembers:
        stream << "RadioButtonGroupMembers";
        break;
    case AXProperty::RelativeFrame:
        stream << "RelativeFrame";
        break;
    case AXProperty::RemoteFrameOffset:
        stream << "RemoteFrameOffset";
        break;
    case AXProperty::RemoteFramePlatformElement:
        stream << "RemoteFramePlatformElement";
        break;
#if PLATFORM(COCOA)
    case AXProperty::RemoteParent:
        stream << "RemoteParent";
        break;
#endif
    case AXProperty::RevealableText:
        stream << "RevealableText";
        break;
    case AXProperty::RolePlatformString:
        stream << "RolePlatformString";
        break;
    case AXProperty::Rows:
        stream << "Rows";
        break;
    case AXProperty::RowHeaders:
        stream << "RowHeaders";
        break;
    case AXProperty::RowIndex:
        stream << "RowIndex";
        break;
    case AXProperty::RowIndexRange:
        stream << "RowIndexRange";
        break;
    case AXProperty::ScreenRelativePosition:
        stream << "ScreenRelativePosition";
        break;
    case AXProperty::SelectedTextRange:
        stream << "SelectedTextRange";
        break;
    case AXProperty::SetSize:
        stream << "SetSize";
        break;
    case AXProperty::ShowsCursorOnHover:
        stream << "ShowsCursorOnHover";
        break;
    case AXProperty::SortDirection:
        stream << "SortDirection";
        break;
    case AXProperty::SpeakAs:
        stream << "SpeakAs";
        break;
    case AXProperty::StringValue:
        stream << "StringValue";
        break;
    case AXProperty::StitchGroups:
        stream << "StitchGroups";
        break;
    case AXProperty::SubrolePlatformString:
        stream << "SubrolePlatformString";
        break;
    case AXProperty::SupportsDragging:
        stream << "SupportsDragging";
        break;
    case AXProperty::SupportsDropping:
        stream << "SupportsDropping";
        break;
    case AXProperty::SupportsARIAOwns:
        stream << "SupportsARIAOwns";
        break;
    case AXProperty::SupportsCheckedState:
        stream << "SupportsCheckedState";
        break;
    case AXProperty::SupportsCurrent:
        stream << "SupportsCurrent";
        break;
    case AXProperty::SupportsExpanded:
        stream << "SupportsExpanded";
        break;
    case AXProperty::SupportsKeyShortcuts:
        stream << "SupportsKeyShortcuts";
        break;
    case AXProperty::SupportsPath:
        stream << "SupportsPath";
        break;
    case AXProperty::SupportsPosInSet:
        stream << "SupportsPosInSet";
        break;
    case AXProperty::SupportsSetSize:
        stream << "SupportsSetSize";
        break;
    case AXProperty::TextContentPrefixFromListMarker:
        stream << "TextContentPrefixFromListMarker";
        break;
#if !ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::TextContent:
        stream << "TextContent";
        break;
#endif // !ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::TextInputMarkedTextMarkerRange:
        stream << "TextInputMarkedTextMarkerRange";
        break;
#if ENABLE(AX_THREAD_TEXT_APIS)
    case AXProperty::TextRuns:
        stream << "TextRuns";
        break;
#endif
    case AXProperty::TitleAttribute:
        stream << "TitleAttribute";
        break;
    case AXProperty::URL:
        stream << "URL";
        break;
    case AXProperty::UnderlineColor:
        stream << "UnderlineColor";
        break;
    case AXProperty::ValueAutofillButtonType:
        stream << "ValueAutofillButtonType";
        break;
    case AXProperty::ValueDescription:
        stream << "ValueDescription";
        break;
    case AXProperty::ValueForRange:
        stream << "ValueForRange";
        break;
    case AXProperty::VerticalScrollBar:
        stream << "VerticalScrollBar";
        break;
    case AXProperty::VisibleChildren:
        stream << "VisibleChildren";
        break;
    case AXProperty::VisibleRows:
        stream << "VisibleRows";
        break;
    case AXProperty::WebAreaTitle:
        stream << "WebAreaTitle";
        break;
    }
    return stream;
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

TextStream& operator<<(TextStream& stream, const AXCoreObject& object)
{
    constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address, AXStreamOptions::RendererOrNode };
    streamAXCoreObject(stream, object, options);
    return stream;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
TextStream& operator<<(TextStream& stream, AXIsolatedTree& tree)
{
    ASSERT(!isMainThread());
    TextStream::GroupScope groupScope(stream);
    stream << "treeID " << tree.treeID();
    stream.dumpProperty("rootNodeID"_s, tree.rootNode()->objectID());
    stream.dumpProperty("focusedNodeID"_s, tree.m_focusedNodeID);
    constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address, AXStreamOptions::RendererOrNode };
    if (RefPtr root = tree.rootNode())
        streamSubtree(stream, root.releaseNonNull(), options);
    return stream;
}

void streamIsolatedSubtreeOnMainThread(TextStream& stream, const AXIsolatedTree& tree, AXID objectID, const OptionSet<AXStreamOptions>& options)
{
    ASSERT(isMainThread());

    stream.increaseIndent();
    TextStream::GroupScope groupScope(stream);

    if (options & AXStreamOptions::ObjectID)
        stream << "objectID " << objectID;

    auto ids = tree.m_nodeMap.get(objectID);
    if (options & AXStreamOptions::ParentID)
        stream.dumpProperty("parentObject"_s, ids.parentID);

    for (auto& childID : ids.childrenIDs)
        streamIsolatedSubtreeOnMainThread(stream, tree, childID, options);

    stream.decreaseIndent();
}
#endif

TextStream& operator<<(TextStream& stream, AXObjectCache& axObjectCache)
{
    TextStream::GroupScope groupScope(stream);
    stream << "AXObjectCache " << &axObjectCache;

    RefPtr document = axObjectCache.document();
    if (!document)
        stream << "No document!";
    else if (RefPtr root = axObjectCache.get(document->view())) {
        constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address, AXStreamOptions::RendererOrNode };
        streamSubtree(stream, root.releaseNonNull(), options);
    } else
        stream << "No root!";

    return stream;
}

#if ENABLE(AX_THREAD_TEXT_APIS)
static void streamTextRuns(TextStream& stream, const AXTextRuns& runs)
{
    stream.dumpProperty("textRuns"_s, runs.debugDescription());
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

void streamAXCoreObject(TextStream& stream, const AXCoreObject& object, const OptionSet<AXStreamOptions>& options)
{
    if (options & AXStreamOptions::ObjectID)
        stream << "objectID " << object.objectID();

    if (options & AXStreamOptions::Role)
        stream.dumpProperty("role"_s, object.role());

    auto* axObject = dynamicDowncast<AccessibilityObject>(object);
    if (axObject) {
        if (options & AXStreamOptions::RendererOrNode) {
            if (auto* renderer = axObject->renderer())
                stream.dumpProperty("renderer"_s, renderer->debugDescription());
            else if (auto* node = axObject->node())
                stream.dumpProperty("node"_s, node->debugDescription());
        }
    }

    if (options & AXStreamOptions::ParentID) {
        RefPtr parent = object.parentObjectUnignored();
        stream.dumpProperty("parentID"_s, parent ? parent->objectID().toUInt64() : 0);
    }

    auto id = options & AXStreamOptions::IdentifierAttribute ? object.identifierAttribute() : emptyString();
    if (!id.isEmpty())
        stream.dumpProperty("identifier"_s, WTFMove(id));

    if (options & AXStreamOptions::OuterHTML) {
        auto role = object.role();
        auto* objectWithInterestingHTML = role == AccessibilityRole::Button ? // Add here other roles of interest.
            &object : nullptr;

        RefPtr parent = object.parentObjectUnignored();
        if (role == AccessibilityRole::StaticText && parent)
            objectWithInterestingHTML = parent.get();

        if (objectWithInterestingHTML)
            stream.dumpProperty("outerHTML"_s, objectWithInterestingHTML->outerHTML().left(150));
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (options & AXStreamOptions::TextRuns) {
        if (auto* isolatedObject = dynamicDowncast<AXIsolatedObject>(object)) {
            if (auto* runs = isolatedObject->textRuns(); runs && runs->size())
                streamTextRuns(stream, *runs);
        } else if (axObject) {
            if (auto runs = const_cast<AccessibilityObject*>(axObject)->textRuns(); runs.size())
                streamTextRuns(stream, runs);
        }
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (options & AXStreamOptions::DisplayContents) {
        if (axObject && axObject->hasDisplayContents())
            stream.dumpProperty("hasDisplayContents"_s, true);
    }

    if (options & AXStreamOptions::Address) {
        stream.dumpProperty("address"_s, &object);
        stream.dumpProperty("wrapper"_s, object.wrapper());
    }
}

void streamSubtree(TextStream& stream, const Ref<AXCoreObject>& object, const OptionSet<AXStreamOptions>& options)
{
    stream.increaseIndent();

    TextStream::GroupScope groupScope(stream);
    streamAXCoreObject(stream, object, options);
    for (auto& child : object->children())
        streamSubtree(stream, child, options);

    stream.decreaseIndent();
}

} // namespace WebCore
