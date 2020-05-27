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

#include "AXObjectCache.h"
#include "Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

AXLogger::AXLogger(const String& methodName)
    : m_methodName(methodName)
{
    if (auto* channel = getLogChannel("Accessibility"))
        channel->level = WTFLogLevel::Debug;

    if (!m_methodName.isEmpty())
        LOG_WITH_STREAM(Accessibility, stream << m_methodName << " {");
}

AXLogger::~AXLogger()
{
    if (!m_methodName.isEmpty())
        LOG_WITH_STREAM(Accessibility, stream << "} " << m_methodName);
}

void AXLogger::log(const String& message)
{
#if !LOG_DISABLED
    LOG(Accessibility, "%s", message.utf8().data());
#else
    UNUSED_PARAM(message);
#endif
}

void AXLogger::log(RefPtr<AXCoreObject> object)
{
    TextStream stream(TextStream::LineMode::MultipleLine);

    if (object)
        stream << *object;
    else
        stream << "null";

    LOG(Accessibility, "%s", stream.release().utf8().data());
}

void AXLogger::add(TextStream& stream, const RefPtr<AXCoreObject>& object, bool recursive)
{
    if (!object)
        return;

    stream.increaseIndent();
    stream << *object;

    if (recursive) {
        for (auto& child : object->children())
            add(stream, child, true);
    }
    stream.decreaseIndent();
}

void AXLogger::log(const std::pair<RefPtr<AXCoreObject>, AXObjectCache::AXNotification>& notification)
{
    TextStream stream(TextStream::LineMode::MultipleLine);
    stream << "Notification " << notification.second << " for object ";
    if (notification.first)
        stream << *notification.first;
    else
        stream << "null";
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

void AXLogger::log(AccessibilityObjectInclusion inclusion)
{
    TextStream stream(TextStream::LineMode::SingleLine);
    stream.dumpProperty("ObjectInclusion", inclusion);
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
void AXLogger::log(AXIsolatedTree& tree)
{
    TextStream stream(TextStream::LineMode::MultipleLine);
    stream << tree;
    LOG(Accessibility, "%s", stream.release().utf8().data());
}
#endif

void AXLogger::log(AXObjectCache& axObjectCache)
{
    TextStream stream(TextStream::LineMode::MultipleLine);
    stream << axObjectCache;
    LOG(Accessibility, "%s", stream.release().utf8().data());
}

TextStream& operator<<(TextStream& stream, AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Annotation:
        stream << "Annotation";
        break;
    case AccessibilityRole::Application:
        stream << "Application";
        break;
    case AccessibilityRole::ApplicationAlert:
        stream << "ApplicationAlert";
        break;
    case AccessibilityRole::ApplicationAlertDialog:
        stream << "ApplicationAlertDialog";
        break;
    case AccessibilityRole::ApplicationDialog:
        stream << "ApplicationDialog";
        break;
    case AccessibilityRole::ApplicationGroup:
        stream << "ApplicationGroup";
        break;
    case AccessibilityRole::ApplicationLog:
        stream << "ApplicationLog";
        break;
    case AccessibilityRole::ApplicationMarquee:
        stream << "ApplicationMarquee";
        break;
    case AccessibilityRole::ApplicationStatus:
        stream << "ApplicationStatus";
        break;
    case AccessibilityRole::ApplicationTextGroup:
        stream << "ApplicationTextGroup";
        break;
    case AccessibilityRole::ApplicationTimer:
        stream << "ApplicationTimer";
        break;
    case AccessibilityRole::Audio:
        stream << "Audio";
        break;
    case AccessibilityRole::Blockquote:
        stream << "Blockquote";
        break;
    case AccessibilityRole::Browser:
        stream << "Browser";
        break;
    case AccessibilityRole::BusyIndicator:
        stream << "BusyIndicator";
        break;
    case AccessibilityRole::Button:
        stream << "Button";
        break;
    case AccessibilityRole::Canvas:
        stream << "Canvas";
        break;
    case AccessibilityRole::Caption:
        stream << "Caption";
        break;
    case AccessibilityRole::Cell:
        stream << "Cell";
        break;
    case AccessibilityRole::CheckBox:
        stream << "CheckBox";
        break;
    case AccessibilityRole::ColorWell:
        stream << "ColorWell";
        break;
    case AccessibilityRole::Column:
        stream << "Column";
        break;
    case AccessibilityRole::ColumnHeader:
        stream << "ColumnHeader";
        break;
    case AccessibilityRole::ComboBox:
        stream << "ComboBox";
        break;
    case AccessibilityRole::Definition:
        stream << "Definition";
        break;
    case AccessibilityRole::Deletion:
        stream << "Deletion";
        break;
    case AccessibilityRole::DescriptionList:
        stream << "DescriptionList";
        break;
    case AccessibilityRole::DescriptionListTerm:
        stream << "DescriptionListTerm";
        break;
    case AccessibilityRole::DescriptionListDetail:
        stream << "DescriptionListDetail";
        break;
    case AccessibilityRole::Details:
        stream << "Details";
        break;
    case AccessibilityRole::Directory:
        stream << "Directory";
        break;
    case AccessibilityRole::DisclosureTriangle:
        stream << "DisclosureTriangle";
        break;
    case AccessibilityRole::Div:
        stream << "Div";
        break;
    case AccessibilityRole::Document:
        stream << "Document";
        break;
    case AccessibilityRole::DocumentArticle:
        stream << "DocumentArticle";
        break;
    case AccessibilityRole::DocumentMath:
        stream << "DocumentMath";
        break;
    case AccessibilityRole::DocumentNote:
        stream << "DocumentNote";
        break;
    case AccessibilityRole::Drawer:
        stream << "Drawer";
        break;
    case AccessibilityRole::EditableText:
        stream << "EditableText";
        break;
    case AccessibilityRole::Feed:
        stream << "Feed";
        break;
    case AccessibilityRole::Figure:
        stream << "Figure";
        break;
    case AccessibilityRole::Footer:
        stream << "Footer";
        break;
    case AccessibilityRole::Footnote:
        stream << "Footnote";
        break;
    case AccessibilityRole::Form:
        stream << "Form";
        break;
    case AccessibilityRole::GraphicsDocument:
        stream << "GraphicsDocument";
        break;
    case AccessibilityRole::GraphicsObject:
        stream << "GraphicsObject";
        break;
    case AccessibilityRole::GraphicsSymbol:
        stream << "GraphicsSymbol";
        break;
    case AccessibilityRole::Grid:
        stream << "Grid";
        break;
    case AccessibilityRole::GridCell:
        stream << "GridCell";
        break;
    case AccessibilityRole::Group:
        stream << "Group";
        break;
    case AccessibilityRole::GrowArea:
        stream << "GrowArea";
        break;
    case AccessibilityRole::Heading:
        stream << "Heading";
        break;
    case AccessibilityRole::HelpTag:
        stream << "HelpTag";
        break;
    case AccessibilityRole::HorizontalRule:
        stream << "HorizontalRule";
        break;
    case AccessibilityRole::Ignored:
        stream << "Ignored";
        break;
    case AccessibilityRole::Inline:
        stream << "Inline";
        break;
    case AccessibilityRole::Image:
        stream << "Image";
        break;
    case AccessibilityRole::ImageMap:
        stream << "ImageMap";
        break;
    case AccessibilityRole::ImageMapLink:
        stream << "ImageMapLink";
        break;
    case AccessibilityRole::Incrementor:
        stream << "Incrementor";
        break;
    case AccessibilityRole::Insertion:
        stream << "Insertion";
        break;
    case AccessibilityRole::Label:
        stream << "Label";
        break;
    case AccessibilityRole::LandmarkBanner:
        stream << "LandmarkBanner";
        break;
    case AccessibilityRole::LandmarkComplementary:
        stream << "LandmarkComplementary";
        break;
    case AccessibilityRole::LandmarkContentInfo:
        stream << "LandmarkContentInfo";
        break;
    case AccessibilityRole::LandmarkDocRegion:
        stream << "LandmarkDocRegion";
        break;
    case AccessibilityRole::LandmarkMain:
        stream << "LandmarkMain";
        break;
    case AccessibilityRole::LandmarkNavigation:
        stream << "LandmarkNavigation";
        break;
    case AccessibilityRole::LandmarkRegion:
        stream << "LandmarkRegion";
        break;
    case AccessibilityRole::LandmarkSearch:
        stream << "LandmarkSearch";
        break;
    case AccessibilityRole::Legend:
        stream << "Legend";
        break;
    case AccessibilityRole::Link:
        stream << "Link";
        break;
    case AccessibilityRole::List:
        stream << "List";
        break;
    case AccessibilityRole::ListBox:
        stream << "ListBox";
        break;
    case AccessibilityRole::ListBoxOption:
        stream << "ListBoxOption";
        break;
    case AccessibilityRole::ListItem:
        stream << "ListItem";
        break;
    case AccessibilityRole::ListMarker:
        stream << "ListMarker";
        break;
    case AccessibilityRole::Mark:
        stream << "Mark";
        break;
    case AccessibilityRole::MathElement:
        stream << "MathElement";
        break;
    case AccessibilityRole::Matte:
        stream << "Matte";
        break;
    case AccessibilityRole::Menu:
        stream << "Menu";
        break;
    case AccessibilityRole::MenuBar:
        stream << "MenuBar";
        break;
    case AccessibilityRole::MenuButton:
        stream << "MenuButton";
        break;
    case AccessibilityRole::MenuItem:
        stream << "MenuItem";
        break;
    case AccessibilityRole::MenuItemCheckbox:
        stream << "MenuItemCheckbox";
        break;
    case AccessibilityRole::MenuItemRadio:
        stream << "MenuItemRadio";
        break;
    case AccessibilityRole::MenuListPopup:
        stream << "MenuListPopup";
        break;
    case AccessibilityRole::MenuListOption:
        stream << "MenuListOption";
        break;
    case AccessibilityRole::Meter:
        stream << "Meter";
        break;
    case AccessibilityRole::Outline:
        stream << "Outline";
        break;
    case AccessibilityRole::Paragraph:
        stream << "Paragraph";
        break;
    case AccessibilityRole::PopUpButton:
        stream << "PopUpButton";
        break;
    case AccessibilityRole::Pre:
        stream << "Pre";
        break;
    case AccessibilityRole::Presentational:
        stream << "Presentational";
        break;
    case AccessibilityRole::ProgressIndicator:
        stream << "ProgressIndicator";
        break;
    case AccessibilityRole::RadioButton:
        stream << "RadioButton";
        break;
    case AccessibilityRole::RadioGroup:
        stream << "RadioGroup";
        break;
    case AccessibilityRole::RowHeader:
        stream << "RowHeader";
        break;
    case AccessibilityRole::Row:
        stream << "Row";
        break;
    case AccessibilityRole::RowGroup:
        stream << "RowGroup";
        break;
    case AccessibilityRole::RubyBase:
        stream << "RubyBase";
        break;
    case AccessibilityRole::RubyBlock:
        stream << "RubyBlock";
        break;
    case AccessibilityRole::RubyInline:
        stream << "RubyInline";
        break;
    case AccessibilityRole::RubyRun:
        stream << "RubyRun";
        break;
    case AccessibilityRole::RubyText:
        stream << "RubyText";
        break;
    case AccessibilityRole::Ruler:
        stream << "Ruler";
        break;
    case AccessibilityRole::RulerMarker:
        stream << "RulerMarker";
        break;
    case AccessibilityRole::ScrollArea:
        stream << "ScrollArea";
        break;
    case AccessibilityRole::ScrollBar:
        stream << "ScrollBar";
        break;
    case AccessibilityRole::SearchField:
        stream << "SearchField";
        break;
    case AccessibilityRole::Sheet:
        stream << "Sheet";
        break;
    case AccessibilityRole::Slider:
        stream << "Slider";
        break;
    case AccessibilityRole::SliderThumb:
        stream << "SliderThumb";
        break;
    case AccessibilityRole::SpinButton:
        stream << "SpinButton";
        break;
    case AccessibilityRole::SpinButtonPart:
        stream << "SpinButtonPart";
        break;
    case AccessibilityRole::SplitGroup:
        stream << "SplitGroup";
        break;
    case AccessibilityRole::Splitter:
        stream << "Splitter";
        break;
    case AccessibilityRole::StaticText:
        stream << "StaticText";
        break;
    case AccessibilityRole::Subscript:
        stream << "Subscript";
        break;
    case AccessibilityRole::Summary:
        stream << "Summary";
        break;
    case AccessibilityRole::Superscript:
        stream << "Superscript";
        break;
    case AccessibilityRole::Switch:
        stream << "Switch";
        break;
    case AccessibilityRole::SystemWide:
        stream << "SystemWide";
        break;
    case AccessibilityRole::SVGRoot:
        stream << "SVGRoot";
        break;
    case AccessibilityRole::SVGText:
        stream << "SVGText";
        break;
    case AccessibilityRole::SVGTSpan:
        stream << "SVGTSpan";
        break;
    case AccessibilityRole::SVGTextPath:
        stream << "SVGTextPath";
        break;
    case AccessibilityRole::TabGroup:
        stream << "TabGroup";
        break;
    case AccessibilityRole::TabList:
        stream << "TabList";
        break;
    case AccessibilityRole::TabPanel:
        stream << "TabPanel";
        break;
    case AccessibilityRole::Tab:
        stream << "Tab";
        break;
    case AccessibilityRole::Table:
        stream << "Table";
        break;
    case AccessibilityRole::TableHeaderContainer:
        stream << "TableHeaderContainer";
        break;
    case AccessibilityRole::Term:
        stream << "Term";
        break;
    case AccessibilityRole::TextArea:
        stream << "TextArea";
        break;
    case AccessibilityRole::TextField:
        stream << "TextField";
        break;
    case AccessibilityRole::TextGroup:
        stream << "TextGroup";
        break;
    case AccessibilityRole::Time:
        stream << "Time";
        break;
    case AccessibilityRole::Tree:
        stream << "Tree";
        break;
    case AccessibilityRole::TreeGrid:
        stream << "TreeGrid";
        break;
    case AccessibilityRole::TreeItem:
        stream << "TreeItem";
        break;
    case AccessibilityRole::ToggleButton:
        stream << "ToggleButton";
        break;
    case AccessibilityRole::Toolbar:
        stream << "Toolbar";
        break;
    case AccessibilityRole::Unknown:
        stream << "Unknown";
        break;
    case AccessibilityRole::UserInterfaceTooltip:
        stream << "UserInterfaceTooltip";
        break;
    case AccessibilityRole::ValueIndicator:
        stream << "ValueIndicator";
        break;
    case AccessibilityRole::Video:
        stream << "Video";
        break;
    case AccessibilityRole::WebApplication:
        stream << "WebApplication";
        break;
    case AccessibilityRole::WebArea:
        stream << "WebArea";
        break;
    case AccessibilityRole::WebCoreLink:
        stream << "WebCoreLink";
        break;
    case AccessibilityRole::Window:
        stream << "Window";
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
    default:
        break;
    }

    return stream;
}

TextStream& operator<<(TextStream& stream, AXObjectCache::AXNotification notification)
{
    switch (notification) {
    case AXObjectCache::AXNotification::AXActiveDescendantChanged:
        stream << "AXActiveDescendantChanged";
        break;
    case AXObjectCache::AXNotification::AXAutocorrectionOccured:
        stream << "AXAutocorrectionOccured";
        break;
    case AXObjectCache::AXNotification::AXCheckedStateChanged:
        stream << "AXCheckedStateChanged";
        break;
    case AXObjectCache::AXNotification::AXChildrenChanged:
        stream << "AXChildrenChanged";
        break;
    case AXObjectCache::AXNotification::AXCurrentChanged:
        stream << "AXCurrentChanged";
        break;
    case AXObjectCache::AXNotification::AXDisabledStateChanged:
        stream << "AXDisabledStateChanged";
        break;
    case AXObjectCache::AXNotification::AXFocusedUIElementChanged:
        stream << "AXFocusedUIElementChanged";
        break;
    case AXObjectCache::AXNotification::AXLayoutComplete:
        stream << "AXLayoutComplete";
        break;
    case AXObjectCache::AXNotification::AXLoadComplete:
        stream << "AXLoadComplete";
        break;
    case AXObjectCache::AXNotification::AXNewDocumentLoadComplete:
        stream << "AXNewDocumentLoadComplete";
        break;
    case AXObjectCache::AXNotification::AXSelectedChildrenChanged:
        stream << "AXSelectedChildrenChanged";
        break;
    case AXObjectCache::AXNotification::AXSelectedTextChanged:
        stream << "AXSelectedTextChanged";
        break;
    case AXObjectCache::AXNotification::AXValueChanged:
        stream << "AXValueChanged";
        break;
    case AXObjectCache::AXNotification::AXScrolledToAnchor:
        stream << "AXScrolledToAnchor";
        break;
    case AXObjectCache::AXNotification::AXLiveRegionCreated:
        stream << "AXLiveRegionCreated";
        break;
    case AXObjectCache::AXNotification::AXLiveRegionChanged:
        stream << "AXLiveRegionChanged";
        break;
    case AXObjectCache::AXNotification::AXMenuListItemSelected:
        stream << "AXMenuListItemSelected";
        break;
    case AXObjectCache::AXNotification::AXMenuListValueChanged:
        stream << "AXMenuListValueChanged";
        break;
    case AXObjectCache::AXNotification::AXMenuClosed:
        stream << "AXMenuClosed";
        break;
    case AXObjectCache::AXNotification::AXMenuOpened:
        stream << "AXMenuOpened";
        break;
    case AXObjectCache::AXNotification::AXRowCountChanged:
        stream << "AXRowCountChanged";
        break;
    case AXObjectCache::AXNotification::AXRowCollapsed:
        stream << "AXRowCollapsed";
        break;
    case AXObjectCache::AXNotification::AXRowExpanded:
        stream << "AXRowExpanded";
        break;
    case AXObjectCache::AXNotification::AXExpandedChanged:
        stream << "AXExpandedChanged";
        break;
    case AXObjectCache::AXNotification::AXInvalidStatusChanged:
        stream << "AXInvalidStatusChanged";
        break;
    case AXObjectCache::AXNotification::AXPressDidSucceed:
        stream << "AXPressDidSucceed";
        break;
    case AXObjectCache::AXNotification::AXPressDidFail:
        stream << "AXPressDidFail";
        break;
    case AXObjectCache::AXNotification::AXPressedStateChanged:
        stream << "AXPressedStateChanged";
        break;
    case AXObjectCache::AXNotification::AXReadOnlyStatusChanged:
        stream << "AXReadOnlyStatusChanged";
        break;
    case AXObjectCache::AXNotification::AXRequiredStatusChanged:
        stream << "AXRequiredStatusChanged";
        break;
    case AXObjectCache::AXNotification::AXTextChanged:
        stream << "AXTextChanged";
        break;
    case AXObjectCache::AXNotification::AXAriaAttributeChanged:
        stream << "AXAriaAttributeChanged";
        break;
    case AXObjectCache::AXNotification::AXElementBusyChanged:
        stream << "AXElementBusyChanged";
        break;
    default:
        break;
    }

    return stream;
}

TextStream& operator<<(TextStream& stream, const AXCoreObject& object)
{
    TextStream::GroupScope groupScope(stream);
    stream << "objectID " << object.objectID();
    stream.dumpProperty("identifierAttribute", object.identifierAttribute());
    stream.dumpProperty("roleValue", object.roleValue());
    stream.dumpProperty("address", &object);

    auto* parent = object.parentObject();
    stream.dumpProperty("parentObject", parent ? parent->objectID() : 0);
#if PLATFORM(COCOA)
    stream.dumpProperty("remoteParentObject", object.remoteParentObject());
#endif

    return stream;
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
TextStream& operator<<(TextStream& stream, AXIsolatedTree& tree)
{
    TextStream::GroupScope groupScope(stream);
    stream << "treeID " << tree.treeID();
    stream.dumpProperty("rootNodeID", tree.m_rootNodeID);
    stream.dumpProperty("focusedNodeID", tree.m_focusedNodeID);
    AXLogger::add(stream, tree.rootNode(), true);
    return stream;
}
#endif

TextStream& operator<<(TextStream& stream, AXObjectCache& axObjectCache)
{
    TextStream::GroupScope groupScope(stream);
    stream << "AXObjectCache " << &axObjectCache;

    if (auto* root = axObjectCache.get(axObjectCache.document().view()))
        AXLogger::add(stream, root, true);
    else
        stream << "No root!";

    return stream;
}

} // namespace WebCore
