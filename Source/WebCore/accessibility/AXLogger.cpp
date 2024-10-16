/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "AXObjectCache.h"
#include "AXSearchManager.h"
#include "AXTextRun.h"
#include "DocumentInlines.h"
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

void AXLogger::log(const Vector<RefPtr<AXCoreObject>>& objects)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);

        stream << "[";
        for (auto object : objects) {
            if (object)
                stream << *object;
            else
                stream << "null";
        }
        stream << "]";

        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const std::pair<Ref<AccessibilityObject>, AXObjectCache::AXNotification>& notification)
{
    if (shouldLog()) {
        TextStream stream(TextStream::LineMode::MultipleLine);
        stream << "Notification " << notification.second << " for object ";
        stream << notification.first.get();
        LOG(Accessibility, "%s", stream.release().utf8().data());
    }
}

void AXLogger::log(const std::pair<RefPtr<AXCoreObject>, AXObjectCache::AXNotification>& notification)
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
    stream.dumpProperty("ObjectInclusion", inclusion);
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

void AXLogger::log(AXRelationType relationType)
{
    if (!shouldLog())
        return;

    TextStream stream(TextStream::LineMode::SingleLine);
    stream.dumpProperty("RelationType", relationType);
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
        [&size] (const UncheckedKeyHashMap<Element*, String>& typedCollection) { size = typedCollection.size(); },
        [&size] (const HashSet<AXID>& typedCollection) { size = typedCollection.size(); },
        [&size] (const ListHashSet<Node*>& typedCollection) { size = typedCollection.size(); },
        [&size] (const ListHashSet<Ref<AccessibilityObject>>& typedCollection) { size = typedCollection.size(); },
        [&size] (const Vector<AXObjectCache::AttributeChange>& typedCollection) { size = typedCollection.size(); },
        [&size] (const Vector<std::pair<Node*, Node*>>& typedCollection) { size = typedCollection.size(); },
        [&size] (const WeakHashSet<Element, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<HTMLTableElement, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<AccessibilityObject>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<AccessibilityTable>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakHashSet<AccessibilityTableCell>& typedCollection) { size = typedCollection.computeSize(); },
        [&size] (const WeakListHashSet<Node, WeakPtrImplWithEventTargetData>& typedCollection) { size = typedCollection.computeSize(); },
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
    stream << accessibilityRoleToString(role);
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
        stream << objectLabel.characters() << " " << axObject << ", ID " << (axObject && axObject->objectID() ? axObject->objectID()->toUInt64() : 0);
        stream.endGroup();
    };

    stream << "SearchCriteria " << &criteria;
    streamCriteriaObject("anchorObject"_s, criteria.anchorObject);
    streamCriteriaObject("startObject"_s, criteria.startObject);
    stream.dumpProperty("searchDirection", criteria.searchDirection);

    stream.nextLine();
    stream << "(searchKeys [";
    for (auto searchKey : criteria.searchKeys)
        stream << searchKey << ", ";
    stream << "])";

    stream.dumpProperty("searchText", criteria.searchText);
    stream.dumpProperty("resultsLimit", criteria.resultsLimit);
    stream.dumpProperty("visibleOnly", criteria.visibleOnly);
    stream.dumpProperty("immediateDescendantsOnly", criteria.immediateDescendantsOnly);

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

TextStream& operator<<(TextStream& stream, AXRelationType relationType)
{
    switch (relationType) {
    case AXRelationType::None:
        stream << "None";
        break;
    case AXRelationType::ActiveDescendant:
        stream << "ActiveDescendant";
        break;
    case AXRelationType::ActiveDescendantOf:
        stream << "ActiveDescendantOf";
        break;
    case AXRelationType::ControlledBy:
        stream << "ControlledBy";
        break;
    case AXRelationType::ControllerFor:
        stream << "ControllerFor";
        break;
    case AXRelationType::DescribedBy:
        stream << "DescribedBy";
        break;
    case AXRelationType::DescriptionFor:
        stream << "DescriptionFor";
        break;
    case AXRelationType::Details:
        stream << "Details";
        break;
    case AXRelationType::DetailsFor:
        stream << "DetailsFor";
        break;
    case AXRelationType::ErrorMessage:
        stream << "ErrorMessage";
        break;
    case AXRelationType::ErrorMessageFor:
        stream << "ErrorMessageFor";
        break;
    case AXRelationType::FlowsFrom:
        stream << "FlowsFrom";
        break;
    case AXRelationType::FlowsTo:
        stream << "FlowsTo";
        break;
    case AXRelationType::Headers:
        stream << "Headers";
        break;
    case AXRelationType::HeaderFor:
        stream << "HeaderFor";
        break;
    case AXRelationType::LabeledBy:
        stream << "LabeledBy";
        break;
    case AXRelationType::LabelFor:
        stream << "LabelFor";
        break;
    case AXRelationType::OwnedBy:
        stream << "OwnedBy";
        break;
    case AXRelationType::OwnerFor:
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

TextStream& operator<<(TextStream& stream, AXObjectCache::AXNotification notification)
{
    switch (notification) {
#define WEBCORE_LOG_AXNOTIFICATION(name) \
    case AXObjectCache::AXNotification::AX##name: \
        stream << "AX" #name; \
        break;
    WEBCORE_AXNOTIFICATION_KEYS(WEBCORE_LOG_AXNOTIFICATION)
#undef WEBCORE_LOG_AXNOTIFICATION
    }

    return stream;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
WTF::TextStream& operator<<(WTF::TextStream& stream, const AXPropertyMap& map)
{
    stream << "{"_s;
    for (auto iterator = map.begin(); iterator != map.end(); ++iterator) {
        if (iterator != map.begin())
            stream << ", ";
        stream << iterator->key;
    }
    stream << "}"_s;
    return stream;
}

TextStream& operator<<(WTF::TextStream& stream, AXPropertyName property)
{
    switch (property) {
    case AXPropertyName::ARIATreeRows:
        stream << "ARIATreeRows";
        break;
    case AXPropertyName::AttributedText:
        stream << "AttributedText";
        break;
    case AXPropertyName::AXColumnCount:
        stream << "AXColumnCount";
        break;
    case AXPropertyName::AXColumnIndex:
        stream << "AXColumnIndex";
        break;
    case AXPropertyName::AXRowCount:
        stream << "AXRowCount";
        break;
    case AXPropertyName::AXRowIndex:
        stream << "AXRowIndex";
        break;
    case AXPropertyName::AccessKey:
        stream << "AccessKey";
        break;
    case AXPropertyName::AccessibilityText:
        stream << "AccessibilityText";
        break;
    case AXPropertyName::ActionVerb:
        stream << "ActionVerb";
        break;
    case AXPropertyName::AncestorFlags:
        stream << "AncestorFlags";
        break;
    case AXPropertyName::AutoCompleteValue:
        stream << "AutoCompleteValue";
        break;
    case AXPropertyName::BlockquoteLevel:
        stream << "BlockquoteLevel";
        break;
    case AXPropertyName::BrailleLabel:
        stream << "BrailleLabel";
        break;
    case AXPropertyName::BrailleRoleDescription:
        stream << "BrailleRoleDescription";
        break;
    case AXPropertyName::ButtonState:
        stream << "ButtonState";
        break;
    case AXPropertyName::CanBeMultilineTextField:
        stream << "CanBeMultilineTextField";
        break;
    case AXPropertyName::CanSetFocusAttribute:
        stream << "CanSetFocusAttribute";
        break;
    case AXPropertyName::CanSetSelectedAttribute:
        stream << "CanSetSelectedAttribute";
        break;
    case AXPropertyName::CanSetSelectedChildren:
        stream << "CanSetSelectedChildren";
        break;
    case AXPropertyName::CanSetValueAttribute:
        stream << "CanSetValueAttribute";
        break;
#if PLATFORM(MAC)
    case AXPropertyName::CaretBrowsingEnabled:
        stream << "CaretBrowsingEnabled";
        break;
#endif
    case AXPropertyName::Cells:
        stream << "Cells";
        break;
    case AXPropertyName::CellScope:
        stream << "CellScope";
        break;
    case AXPropertyName::CellSlots:
        stream << "CellSlots";
        break;
    case AXPropertyName::ColorValue:
        stream << "ColorValue";
        break;
    case AXPropertyName::Columns:
        stream << "Columns";
        break;
    case AXPropertyName::ColumnHeader:
        stream << "ColumnHeader";
        break;
    case AXPropertyName::ColumnHeaders:
        stream << "ColumnHeaders";
        break;
    case AXPropertyName::ColumnIndex:
        stream << "ColumnIndex";
        break;
    case AXPropertyName::ColumnIndexRange:
        stream << "ColumnIndexRange";
        break;
    case AXPropertyName::CurrentState:
        stream << "CurrentState";
        break;
    case AXPropertyName::DateTimeComponentsType:
        stream << "DateTimeComponentsType";
        break;
    case AXPropertyName::DateTimeValue:
        stream << "DateTimeValue";
        break;
    case AXPropertyName::DatetimeAttributeValue:
        stream << "DatetimeAttributeValue";
        break;
    case AXPropertyName::DecrementButton:
        stream << "DecrementButton";
        break;
    case AXPropertyName::Description:
        stream << "Description";
        break;
    case AXPropertyName::DisclosedByRow:
        stream << "DisclosedByRow";
        break;
    case AXPropertyName::DisclosedRows:
        stream << "DisclosedRows";
        break;
    case AXPropertyName::DocumentEncoding:
        stream << "DocumentEncoding";
        break;
    case AXPropertyName::DocumentLinks:
        stream << "DocumentLinks";
        break;
    case AXPropertyName::DocumentURI:
        stream << "DocumentURI";
        break;
    case AXPropertyName::EmbeddedImageDescription:
        stream << "EmbeddedImageDescription";
        break;
    case AXPropertyName::ExpandedTextValue:
        stream << "ExpandedTextValue";
        break;
    case AXPropertyName::ExtendedDescription:
        stream << "ExtendedDescription";
        break;
    case AXPropertyName::HasApplePDFAnnotationAttribute:
        stream << "HasApplePDFAnnotationAttribute";
        break;
    case AXPropertyName::HasBoldFont:
        stream << "HasBoldFont";
        break;
    case AXPropertyName::HasHighlighting:
        stream << "HasHighlighting";
        break;
    case AXPropertyName::HasItalicFont:
        stream << "HasItalicFont";
        break;
    case AXPropertyName::HasPlainText:
        stream << "HasPlainText";
        break;
    case AXPropertyName::HasRemoteFrameChild:
        stream << "HasRemoteFrameChild";
        break;
    case AXPropertyName::HasUnderline:
        stream << "HasUnderline";
        break;
    case AXPropertyName::HeaderContainer:
        stream << "HeaderContainer";
        break;
    case AXPropertyName::HeadingLevel:
        stream << "HeadingLevel";
        break;
    case AXPropertyName::HierarchicalLevel:
        stream << "HierarchicalLevel";
        break;
    case AXPropertyName::HorizontalScrollBar:
        stream << "HorizontalScrollBar";
        break;
    case AXPropertyName::IdentifierAttribute:
        stream << "IdentifierAttribute";
        break;
    case AXPropertyName::IncrementButton:
        stream << "IncrementButton";
        break;
    case AXPropertyName::InitialFrameRect:
        stream << "InitialFrameRect";
        break;
    case AXPropertyName::InnerHTML:
        stream << "InnerHTML";
        break;
    case AXPropertyName::InternalLinkElement:
        stream << "InternalLinkElement";
        break;
    case AXPropertyName::InsideLink:
        stream << "InsideLink";
        break;
    case AXPropertyName::InvalidStatus:
        stream << "InvalidStatus";
        break;
    case AXPropertyName::IsGrabbed:
        stream << "IsGrabbed";
        break;
    case AXPropertyName::IsARIATreeGridRow:
        stream << "IsARIATreeGridRow";
        break;
    case AXPropertyName::IsAttachment:
        stream << "IsAttachment";
        break;
    case AXPropertyName::IsBusy:
        stream << "IsBusy";
        break;
    case AXPropertyName::IsChecked:
        stream << "IsChecked";
        break;
    case AXPropertyName::IsColumnHeader:
        stream << "IsColumnHeader";
        break;
    case AXPropertyName::IsControl:
        stream << "IsControl";
        break;
    case AXPropertyName::IsEnabled:
        stream << "IsEnabled";
        break;
    case AXPropertyName::IsExpanded:
        stream << "IsExpanded";
        break;
    case AXPropertyName::IsExposable:
        stream << "IsExposable";
        break;
    case AXPropertyName::IsExposedTableCell:
        stream << "IsExposedTableCell";
        break;
    case AXPropertyName::IsFieldset:
        stream << "IsFieldset";
        break;
    case AXPropertyName::IsFileUploadButton:
        stream << "IsFileUploadButton";
        break;
    case AXPropertyName::IsIgnored:
        stream << "IsIgnored";
        break;
    case AXPropertyName::IsIndeterminate:
        stream << "IsIndeterminate";
        break;
    case AXPropertyName::IsInlineText:
        stream << "IsInlineText";
        break;
    case AXPropertyName::IsRadioInput:
        stream << "IsRadioInput";
        break;
    case AXPropertyName::IsInputImage:
        stream << "IsInputImage";
        break;
    case AXPropertyName::IsKeyboardFocusable:
        stream << "IsKeyboardFocusable";
        break;
    case AXPropertyName::IsLink:
        stream << "IsLink";
        break;
    case AXPropertyName::IsList:
        stream << "IsList";
        break;
    case AXPropertyName::IsListBox:
        stream << "IsListBox";
        break;
    case AXPropertyName::IsMathElement:
        stream << "IsMathElement";
        break;
    case AXPropertyName::IsMathFraction:
        stream << "IsMathFraction";
        break;
    case AXPropertyName::IsMathFenced:
        stream << "IsMathFenced";
        break;
    case AXPropertyName::IsMathSubscriptSuperscript:
        stream << "IsMathSubscriptSuperscript";
        break;
    case AXPropertyName::IsMathRow:
        stream << "IsMathRow";
        break;
    case AXPropertyName::IsMathUnderOver:
        stream << "IsMathUnderOver";
        break;
    case AXPropertyName::IsMathRoot:
        stream << "IsMathRoot";
        break;
    case AXPropertyName::IsMathSquareRoot:
        stream << "IsMathSquareRoot";
        break;
    case AXPropertyName::IsMathTable:
        stream << "IsMathTable";
        break;
    case AXPropertyName::IsMathTableRow:
        stream << "IsMathTableRow";
        break;
    case AXPropertyName::IsMathTableCell:
        stream << "IsMathTableCell";
        break;
    case AXPropertyName::IsMathMultiscript:
        stream << "IsMathMultiscript";
        break;
    case AXPropertyName::IsMathToken:
        stream << "IsMathToken";
        break;
    case AXPropertyName::IsMeter:
        stream << "IsMeter";
        break;
    case AXPropertyName::IsMultiSelectable:
        stream << "IsMultiSelectable";
        break;
    case AXPropertyName::IsNonLayerSVGObject:
        stream << "IsNonLayerSVGObject";
        break;
    case AXPropertyName::IsPlugin:
        stream << "IsPlugin";
        break;
    case AXPropertyName::IsPressed:
        stream << "IsPressed";
        break;
    case AXPropertyName::IsRequired:
        stream << "IsRequired";
        break;
    case AXPropertyName::IsRowHeader:
        stream << "IsRowHeader";
        break;
    case AXPropertyName::IsSecureField:
        stream << "IsSecureField";
        break;
    case AXPropertyName::IsSelected:
        stream << "IsSelected";
        break;
    case AXPropertyName::IsSelectedOptionActive:
        stream << "IsSelectedOptionActive";
        break;
    case AXPropertyName::IsTable:
        stream << "IsTable";
        break;
    case AXPropertyName::IsTableColumn:
        stream << "IsTableColumn";
        break;
    case AXPropertyName::IsTableRow:
        stream << "IsTableRow";
        break;
    case AXPropertyName::IsTree:
        stream << "IsTree";
        break;
    case AXPropertyName::IsTreeItem:
        stream << "IsTreeItem";
        break;
    case AXPropertyName::IsValueAutofillAvailable:
        stream << "IsValueAutofillAvailable";
        break;
    case AXPropertyName::IsVisible:
        stream << "IsVisible";
        break;
    case AXPropertyName::IsWidget:
        stream << "IsWidget";
        break;
    case AXPropertyName::KeyShortcuts:
        stream << "KeyShortcuts";
        break;
    case AXPropertyName::Language:
        stream << "Language";
        break;
    case AXPropertyName::LiveRegionAtomic:
        stream << "LiveRegionAtomic";
        break;
    case AXPropertyName::LiveRegionRelevant:
        stream << "LiveRegionRelevant";
        break;
    case AXPropertyName::LiveRegionStatus:
        stream << "LiveRegionStatus";
        break;
    case AXPropertyName::LocalizedActionVerb:
        stream << "LocalizedActionVerb";
        break;
    case AXPropertyName::MathFencedOpenString:
        stream << "MathFencedOpenString";
        break;
    case AXPropertyName::MathFencedCloseString:
        stream << "MathFencedCloseString";
        break;
    case AXPropertyName::MathLineThickness:
        stream << "MathLineThickness";
        break;
    case AXPropertyName::MathPrescripts:
        stream << "MathPrescripts";
        break;
    case AXPropertyName::MathPostscripts:
        stream << "MathPostscripts";
        break;
    case AXPropertyName::MathRadicand:
        stream << "MathRadicand";
        break;
    case AXPropertyName::MathRootIndexObject:
        stream << "MathRootIndexObject";
        break;
    case AXPropertyName::MathUnderObject:
        stream << "MathUnderObject";
        break;
    case AXPropertyName::MathOverObject:
        stream << "MathOverObject";
        break;
    case AXPropertyName::MathNumeratorObject:
        stream << "MathNumeratorObject";
        break;
    case AXPropertyName::MathDenominatorObject:
        stream << "MathDenominatorObject";
        break;
    case AXPropertyName::MathBaseObject:
        stream << "MathBaseObject";
        break;
    case AXPropertyName::MathSubscriptObject:
        stream << "MathSubscriptObject";
        break;
    case AXPropertyName::MathSuperscriptObject:
        stream << "MathSuperscriptObject";
        break;
    case AXPropertyName::MaxValueForRange:
        stream << "MaxValueForRange";
        break;
    case AXPropertyName::MinValueForRange:
        stream << "MinValueForRange";
        break;
    case AXPropertyName::NameAttribute:
        stream << "NameAttribute";
        break;
    case AXPropertyName::Orientation:
        stream << "Orientation";
        break;
    case AXPropertyName::OuterHTML:
        stream << "OuterHTML";
        break;
    case AXPropertyName::Path:
        stream << "Path";
        break;
    case AXPropertyName::PlaceholderValue:
        stream << "PlaceholderValue";
        break;
    case AXPropertyName::PopupValue:
        stream << "PopupValue";
        break;
    case AXPropertyName::PosInSet:
        stream << "PosInSet";
        break;
    case AXPropertyName::PreventKeyboardDOMEventDispatch:
        stream << "PreventKeyboardDOMEventDispatch";
        break;
    case AXPropertyName::RadioButtonGroup:
        stream << "RadioButtonGroup";
        break;
    case AXPropertyName::RelativeFrame:
        stream << "RelativeFrame";
        break;
    case AXPropertyName::RemoteFrameOffset:
        stream << "RemoteFrameOffset";
        break;
    case AXPropertyName::RemoteFramePlatformElement:
        stream << "RemoteFramePlatformElement";
        break;
    case AXPropertyName::RoleValue:
        stream << "RoleValue";
        break;
    case AXPropertyName::RolePlatformString:
        stream << "RolePlatformString";
        break;
    case AXPropertyName::RoleDescription:
        stream << "RoleDescription";
        break;
    case AXPropertyName::Rows:
        stream << "Rows";
        break;
    case AXPropertyName::RowGroupAncestorID:
        stream << "RowGroupAncestorID";
        break;
    case AXPropertyName::RowHeader:
        stream << "RowHeader";
        break;
    case AXPropertyName::RowHeaders:
        stream << "RowHeaders";
        break;
    case AXPropertyName::RowIndex:
        stream << "RowIndex";
        break;
    case AXPropertyName::RowIndexRange:
        stream << "RowIndexRange";
        break;
    case AXPropertyName::ScreenRelativePosition:
        stream << "ScreenRelativePosition";
        break;
    case AXPropertyName::SelectedChildren:
        stream << "SelectedChildren";
        break;
    case AXPropertyName::SelectedTextRange:
        stream << "SelectedTextRange";
        break;
    case AXPropertyName::SetSize:
        stream << "SetSize";
        break;
    case AXPropertyName::ShouldEmitNewlinesBeforeAndAfterNode:
        stream << "ShouldEmitNewlinesBeforeAndAfterNode";
        break;
    case AXPropertyName::SortDirection:
        stream << "SortDirection";
        break;
    case AXPropertyName::SpeechHint:
        stream << "SpeechHint";
        break;
    case AXPropertyName::StringValue:
        stream << "StringValue";
        break;
    case AXPropertyName::SubrolePlatformString:
        stream << "SubrolePlatformString";
        break;
    case AXPropertyName::SupportsDragging:
        stream << "SupportsDragging";
        break;
    case AXPropertyName::SupportsDropping:
        stream << "SupportsDropping";
        break;
    case AXPropertyName::SupportsARIAOwns:
        stream << "SupportsARIAOwns";
        break;
    case AXPropertyName::SupportsCheckedState:
        stream << "SupportsCheckedState";
        break;
    case AXPropertyName::SupportsCurrent:
        stream << "SupportsCurrent";
        break;
    case AXPropertyName::SupportsDatetimeAttribute:
        stream << "SupportsDatetimeAttribute";
        break;
    case AXPropertyName::SupportsExpanded:
        stream << "SupportsExpanded";
        break;
    case AXPropertyName::SupportsExpandedTextValue:
        stream << "SupportsExpandedTextValue";
        break;
    case AXPropertyName::SupportsKeyShortcuts:
        stream << "SupportsKeyShortcuts";
        break;
    case AXPropertyName::SupportsPath:
        stream << "SupportsPath";
        break;
    case AXPropertyName::SupportsPosInSet:
        stream << "SupportsPosInSet";
        break;
    case AXPropertyName::SupportsPressAction:
        stream << "SupportsPressAction";
        break;
    case AXPropertyName::SupportsRangeValue:
        stream << "SupportsRangeValue";
        break;
    case AXPropertyName::SupportsRequiredAttribute:
        stream << "SupportsRequiredAttribute";
        break;
    case AXPropertyName::SupportsSelectedRows:
        stream << "SupportsSelectedRows";
        break;
    case AXPropertyName::SupportsSetSize:
        stream << "SupportsSetSize";
        break;
    case AXPropertyName::TextContent:
        stream << "TextContent";
        break;
    case AXPropertyName::TextInputMarkedTextMarkerRange:
        stream << "TextInputMarkedTextMarkerRange";
        break;
#if ENABLE(AX_THREAD_TEXT_APIS)
    case AXPropertyName::TextRuns:
        stream << "TextRuns";
        break;
#endif
    case AXPropertyName::Title:
        stream << "Title";
        break;
    case AXPropertyName::TitleAttributeValue:
        stream << "TitleAttributeValue";
        break;
    case AXPropertyName::URL:
        stream << "URL";
        break;
    case AXPropertyName::ValueAutofillButtonType:
        stream << "ValueAutofillButtonType";
        break;
    case AXPropertyName::ValueDescription:
        stream << "ValueDescription";
        break;
    case AXPropertyName::ValueForRange:
        stream << "ValueForRange";
        break;
    case AXPropertyName::VerticalScrollBar:
        stream << "VerticalScrollBar";
        break;
    case AXPropertyName::VisibleChildren:
        stream << "VisibleChildren";
        break;
    case AXPropertyName::VisibleRows:
        stream << "VisibleRows";
        break;
    }
    return stream;
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

TextStream& operator<<(TextStream& stream, const AXCoreObject& object)
{
    constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address };
    streamAXCoreObject(stream, object, options);
    return stream;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
TextStream& operator<<(TextStream& stream, AXIsolatedTree& tree)
{
    ASSERT(!isMainThread());
    TextStream::GroupScope groupScope(stream);
    stream << "treeID " << tree.treeID();
    stream.dumpProperty("rootNodeID", tree.rootNode()->objectID());
    stream.dumpProperty("focusedNodeID", tree.m_focusedNodeID);
    constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address };
    streamSubtree(stream, tree.rootNode(), options);
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
        stream.dumpProperty("parentObject", ids.parentID);

    for (auto& childID : ids.childrenIDs)
        streamIsolatedSubtreeOnMainThread(stream, tree, childID, options);

    stream.decreaseIndent();
}
#endif

TextStream& operator<<(TextStream& stream, AXObjectCache& axObjectCache)
{
    TextStream::GroupScope groupScope(stream);
    stream << "AXObjectCache " << &axObjectCache;

    if (auto* root = axObjectCache.get(axObjectCache.document().view())) {
        constexpr OptionSet<AXStreamOptions> options = { AXStreamOptions::ObjectID, AXStreamOptions::Role, AXStreamOptions::ParentID, AXStreamOptions::IdentifierAttribute, AXStreamOptions::OuterHTML, AXStreamOptions::DisplayContents, AXStreamOptions::Address };
        streamSubtree(stream, root, options);
    } else
        stream << "No root!";

    return stream;
}

#if ENABLE(AX_THREAD_TEXT_APIS)
static void streamTextRuns(TextStream& stream, const AXTextRuns& runs)
{
    stream.dumpProperty("textRuns", makeString(interleave(runs.runs, [](auto& builder, auto& run) {
        builder.append(run.lineIndex, ":|"_s, run.text, "|(len: "_s, run.text.length(), ')');
    }, ", "_s)));
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

void streamAXCoreObject(TextStream& stream, const AXCoreObject& object, const OptionSet<AXStreamOptions>& options)
{
    if (options & AXStreamOptions::ObjectID)
        stream << "objectID " << object.objectID();

    if (options & AXStreamOptions::Role)
        stream.dumpProperty("role", object.roleValue());

    if (auto* axObject = dynamicDowncast<AccessibilityObject>(object)) {
        if (auto* renderer = axObject->renderer())
            stream.dumpProperty("renderer", renderer->debugDescription());
        else if (auto* node = axObject->node())
            stream.dumpProperty("node", node->debugDescription());
    }

    if (options & AXStreamOptions::ParentID) {
        auto* parent = object.parentObjectUnignored();
        stream.dumpProperty("parentID", parent && parent->objectID()? parent->objectID()->toUInt64() : 0);
    }

    auto id = options & AXStreamOptions::IdentifierAttribute ? object.identifierAttribute() : emptyString();
    if (!id.isEmpty())
        stream.dumpProperty("identifier", WTFMove(id));

    if (options & AXStreamOptions::OuterHTML) {
        auto role = object.roleValue();
        auto* objectWithInterestingHTML = role == AccessibilityRole::Button ? // Add here other roles of interest.
            &object : nullptr;

        auto* parent = object.parentObjectUnignored();
        if (role == AccessibilityRole::StaticText && parent)
            objectWithInterestingHTML = parent;

        if (objectWithInterestingHTML)
            stream.dumpProperty("outerHTML", objectWithInterestingHTML->outerHTML().left(150));
    }

#if ENABLE(AX_THREAD_TEXT_APIS)
    if (options & AXStreamOptions::TextRuns) {
        if (auto* isolatedObject = dynamicDowncast<AXIsolatedObject>(&object)) {
            if (auto* runs = isolatedObject->textRuns(); runs && runs->size())
                streamTextRuns(stream, *runs);
        } else if (auto* liveObject = dynamicDowncast<AccessibilityObject>(&object)) {
            if (auto runs = const_cast<AccessibilityObject*>(liveObject)->textRuns(); runs.size())
                streamTextRuns(stream, runs);
        }
    }
#endif // ENABLE(AX_THREAD_TEXT_APIS)

    if (options & AXStreamOptions::DisplayContents) {
        if (auto* axObject = dynamicDowncast<AccessibilityObject>(&object); axObject && axObject->hasDisplayContents())
            stream.dumpProperty("hasDisplayContents", true);
    }

    if (options & AXStreamOptions::Address) {
        stream.dumpProperty("address", &object);
        stream.dumpProperty("wrapper", object.wrapper());
    }
}

void streamSubtree(TextStream& stream, const RefPtr<AXCoreObject>& object, const OptionSet<AXStreamOptions>& options)
{
    if (!object)
        return;

    stream.increaseIndent();

    TextStream::GroupScope groupScope(stream);
    streamAXCoreObject(stream, *object, options);
    for (auto& child : object->unignoredChildren(/* updateChildrenIfNeeded */ false))
        streamSubtree(stream, child, options);

    stream.decreaseIndent();
}

} // namespace WebCore
