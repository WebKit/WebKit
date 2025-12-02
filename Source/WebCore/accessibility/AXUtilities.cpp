/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AXUtilities.h"

#include "AXLoggerBase.h"
#include "AXObjectCache.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSProperty.h"
#include "CSSValueList.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Node.h"
#include "RenderImage.h"
#include "RenderStyleConstants.h"
#include "RenderTreeBuilder.h"
#include "StylePropertiesInlines.h"
#include <wtf/CheckedPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

using namespace HTMLNames;

ContainerNode* composedParentIgnoringDocumentFragments(const Node& node)
{
    RefPtr ancestor = node.parentInComposedTree();
    while (is<DocumentFragment>(ancestor.get()))
        ancestor = ancestor->parentInComposedTree();
    return ancestor.unsafeGet();
}

ContainerNode* composedParentIgnoringDocumentFragments(const Node* node)
{
    return node ? composedParentIgnoringDocumentFragments(*node) : nullptr;
}

NodeName elementName(Node* node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element ? element->elementName() : ElementName::Unknown;
}

NodeName elementName(Node& node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element ? element->elementName() : ElementName::Unknown;
}

const RenderStyle* safeStyleFrom(Element& element)
{
    // We cannot resolve style (as computedStyle() does) if we are downstream of an existing render tree
    // update. Otherwise, a RELEASE_ASSERT preventing re-entrancy will be hit inside RenderTreeBuilder.
    return RenderTreeBuilder::current() ? element.existingComputedStyle() : element.computedStyle();
}

bool hasAccNameAttribute(Element& element)
{
    auto trimmed = [&] (const auto& attribute) {
        const auto& value = element.attributeWithDefaultARIA(attribute);
        if (value.isEmpty())
            return emptyString();
        auto copy = value.string();
        return copy.trim(isASCIIWhitespace);
    };

    // Avoid calculating the actual description here (e.g. resolving aria-labelledby), as it's expensive.
    // The spec is generally permissive in allowing user agents to not ensure complete validity of these attributes.
    // For example, https://w3c.github.io/svg-aam/#include_elements:
    // "It has an ‘aria-labelledby’ attribute or ‘aria-describedby’ attribute containing valid IDREF tokens. User agents MAY include elements with these attributes without checking for validity."
    if (trimmed(aria_labelAttr).length() || trimmed(aria_labelledbyAttr).length() || trimmed(aria_labeledbyAttr).length() || trimmed(aria_descriptionAttr).length() || trimmed(aria_describedbyAttr).length())
        return true;

    return element.attributeWithoutSynchronization(titleAttr).length();
}

RenderImage* toSimpleImage(RenderObject& renderer)
{
    CheckedPtr renderImage = dynamicDowncast<RenderImage>(renderer);
    if (!renderImage)
        return nullptr;

    // Exclude ImageButtons because they are treated as buttons, not as images.
    RefPtr node = renderer.node();
    if (is<HTMLInputElement>(node))
        return nullptr;

    // ImageMaps are not simple images.
    if (renderImage->imageMap())
        return nullptr;

    if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(node); imgElement && imgElement->hasAttributeWithoutSynchronization(usemapAttr))
        return nullptr;

#if ENABLE(VIDEO)
    // Exclude video and audio elements.
    if (is<HTMLMediaElement>(node))
        return nullptr;
#endif // ENABLE(VIDEO)

    return renderImage.unsafeGet();
}

// FIXME: This probably belongs on Element.
bool hasRole(Element& element, StringView role)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (role.isNull())
        return roleValue.isEmpty();
    if (roleValue.isEmpty())
        return false;

    return SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes);
}

bool hasAnyRole(Element& element, Vector<StringView>&& roles)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (roleValue.isEmpty())
        return false;

    for (const auto& role : roles) {
        AX_DEBUG_ASSERT(!role.isEmpty());
        if (SpaceSplitString::spaceSplitStringContainsValue(roleValue, role, SpaceSplitString::ShouldFoldCase::Yes))
            return true;
    }
    return false;
}

bool hasAnyRole(Element* element, Vector<StringView>&& roles)
{
    return element ? hasAnyRole(*element, WTFMove(roles)) : false;
}

bool hasTableRole(Element& element)
{
    return hasAnyRole(element, { "grid"_s, "table"_s, "treegrid"_s });
}

bool hasCellARIARole(Element& element)
{
    return hasAnyRole(element, { "gridcell"_s, "cell"_s, "columnheader"_s, "rowheader"_s });
}

bool hasPresentationRole(Element& element)
{
    return hasAnyRole(element, { "presentation"_s, "none"_s });
}

bool isRowGroup(Element& element)
{
    auto name = element.elementName();
    return name == ElementName::HTML_thead || name == ElementName::HTML_tbody || name == ElementName::HTML_tfoot || hasRole(element, "rowgroup"_s);
}

bool isRowGroup(Node* node)
{
    auto* element = dynamicDowncast<Element>(node);
    return element && isRowGroup(*element);
}

void dumpAccessibilityTreeToStderr(Document& document)
{
    if (CheckedPtr cache = document.existingAXObjectCache()) {
        AXTreeData data = cache->treeData();
        SAFE_FPRINTF(stderr, "==AX Trees==\n%s\n%s\n", data.liveTree.utf8(), data.isolatedTree.utf8());
    }
}

String roleToString(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Application:
        return "Application"_s;
    case AccessibilityRole::ApplicationAlert:
        return "ApplicationAlert"_s;
    case AccessibilityRole::ApplicationAlertDialog:
        return "ApplicationAlertDialog"_s;
    case AccessibilityRole::ApplicationDialog:
        return "ApplicationDialog"_s;
    case AccessibilityRole::ApplicationLog:
        return "ApplicationLog"_s;
    case AccessibilityRole::ApplicationMarquee:
        return "ApplicationMarquee"_s;
    case AccessibilityRole::ApplicationStatus:
        return "ApplicationStatus"_s;
    case AccessibilityRole::ApplicationTimer:
        return "ApplicationTimer"_s;
    case AccessibilityRole::Audio:
        return "Audio"_s;
    case AccessibilityRole::Blockquote:
        return "Blockquote"_s;
    case AccessibilityRole::Button:
        return "Button"_s;
    case AccessibilityRole::Canvas:
        return "Canvas"_s;
    case AccessibilityRole::Caption:
        return "Caption"_s;
    case AccessibilityRole::Cell:
        return "Cell"_s;
    case AccessibilityRole::Checkbox:
        return "Checkbox"_s;
    case AccessibilityRole::Code:
        return "Code"_s;
    case AccessibilityRole::ColorWell:
        return "ColorWell"_s;
    case AccessibilityRole::Column:
        return "Column"_s;
    case AccessibilityRole::ColumnHeader:
        return "ColumnHeader"_s;
    case AccessibilityRole::ComboBox:
        return "ComboBox"_s;
    case AccessibilityRole::DateTime:
        return "DateTime"_s;
    case AccessibilityRole::Definition:
        return "Definition"_s;
    case AccessibilityRole::Deletion:
        return "Deletion"_s;
    case AccessibilityRole::DescriptionList:
        return "DescriptionList"_s;
    case AccessibilityRole::DescriptionListTerm:
        return "DescriptionListTerm"_s;
    case AccessibilityRole::DescriptionListDetail:
        return "DescriptionListDetail"_s;
    case AccessibilityRole::Details:
        return "Details"_s;
    case AccessibilityRole::Directory:
        return "Directory"_s;
    case AccessibilityRole::Document:
        return "Document"_s;
    case AccessibilityRole::DocumentArticle:
        return "DocumentArticle"_s;
    case AccessibilityRole::DocumentMath:
        return "DocumentMath"_s;
    case AccessibilityRole::DocumentNote:
        return "DocumentNote"_s;
    case AccessibilityRole::Emphasis:
        return "Emphasis"_s;
    case AccessibilityRole::Feed:
        return "Feed"_s;
    case AccessibilityRole::Figure:
        return "Figure"_s;
    case AccessibilityRole::Footnote:
        return "Footnote"_s;
    case AccessibilityRole::Form:
        return "Form"_s;
    case AccessibilityRole::FrameHost:
        return "FrameHost"_s;
    case AccessibilityRole::Generic:
        return "Generic"_s;
    case AccessibilityRole::GraphicsDocument:
        return "GraphicsDocument"_s;
    case AccessibilityRole::GraphicsObject:
        return "GraphicsObject"_s;
    case AccessibilityRole::GraphicsSymbol:
        return "GraphicsSymbol"_s;
    case AccessibilityRole::Grid:
        return "Grid"_s;
    case AccessibilityRole::GridCell:
        return "GridCell"_s;
    case AccessibilityRole::Group:
        return "Group"_s;
    case AccessibilityRole::Heading:
        return "Heading"_s;
    case AccessibilityRole::HorizontalRule:
        return "HorizontalRule"_s;
    case AccessibilityRole::Ignored:
        return "Ignored"_s;
    case AccessibilityRole::Inline:
        return "Inline"_s;
    case AccessibilityRole::Image:
        return "Image"_s;
    case AccessibilityRole::ImageMap:
        return "ImageMap"_s;
    case AccessibilityRole::Insertion:
        return "Insertion"_s;
    case AccessibilityRole::Label:
        return "Label"_s;
    case AccessibilityRole::LandmarkBanner:
        return "LandmarkBanner"_s;
    case AccessibilityRole::LandmarkComplementary:
        return "LandmarkComplementary"_s;
    case AccessibilityRole::LandmarkContentInfo:
        return "LandmarkContentInfo"_s;
    case AccessibilityRole::LandmarkDocRegion:
        return "LandmarkDocRegion"_s;
    case AccessibilityRole::LandmarkMain:
        return "LandmarkMain"_s;
    case AccessibilityRole::LandmarkNavigation:
        return "LandmarkNavigation"_s;
    case AccessibilityRole::LandmarkRegion:
        return "LandmarkRegion"_s;
    case AccessibilityRole::LandmarkSearch:
        return "LandmarkSearch"_s;
    case AccessibilityRole::Legend:
        return "Legend"_s;
    case AccessibilityRole::Link:
        return "Link"_s;
    case AccessibilityRole::LineBreak:
        return "LineBreak"_s;
    case AccessibilityRole::List:
        return "List"_s;
    case AccessibilityRole::ListBox:
        return "ListBox"_s;
    case AccessibilityRole::ListBoxOption:
        return "ListBoxOption"_s;
    case AccessibilityRole::ListItem:
        return "ListItem"_s;
    case AccessibilityRole::ListMarker:
        return "ListMarker"_s;
    case AccessibilityRole::LocalFrame:
        return "LocalFrame"_s;
    case AccessibilityRole::Mark:
        return "Mark"_s;
    case AccessibilityRole::MathElement:
        return "MathElement"_s;
    case AccessibilityRole::Menu:
        return "Menu"_s;
    case AccessibilityRole::MenuBar:
        return "MenuBar"_s;
    case AccessibilityRole::MenuItem:
        return "MenuItem"_s;
    case AccessibilityRole::MenuItemCheckbox:
        return "MenuItemCheckbox"_s;
    case AccessibilityRole::MenuItemRadio:
        return "MenuItemRadio"_s;
    case AccessibilityRole::MenuListPopup:
        return "MenuListPopup"_s;
    case AccessibilityRole::MenuListOption:
        return "MenuListOption"_s;
    case AccessibilityRole::Meter:
        return "Meter"_s;
    case AccessibilityRole::Model:
        return "Model"_s;
    case AccessibilityRole::Paragraph:
        return "Paragraph"_s;
    case AccessibilityRole::PopUpButton:
        return "PopUpButton"_s;
    case AccessibilityRole::Pre:
        return "Pre"_s;
    case AccessibilityRole::Presentational:
        return "Presentational"_s;
    case AccessibilityRole::ProgressIndicator:
        return "ProgressIndicator"_s;
    case AccessibilityRole::RadioButton:
        return "RadioButton"_s;
    case AccessibilityRole::RadioGroup:
        return "RadioGroup"_s;
    case AccessibilityRole::RemoteFrame:
        return "RemoteFrame"_s;
    case AccessibilityRole::RowHeader:
        return "RowHeader"_s;
    case AccessibilityRole::Row:
        return "Row"_s;
    case AccessibilityRole::RowGroup:
        return "RowGroup"_s;
    case AccessibilityRole::RubyInline:
        return "RubyInline"_s;
    case AccessibilityRole::RubyText:
        return "RubyText"_s;
    case AccessibilityRole::ScrollArea:
        return "ScrollArea"_s;
    case AccessibilityRole::ScrollBar:
        return "ScrollBar"_s;
    case AccessibilityRole::SearchField:
        return "SearchField"_s;
    case AccessibilityRole::SectionFooter:
        return "SectionFooter"_s;
    case AccessibilityRole::SectionHeader:
        return "SectionHeader"_s;
    case AccessibilityRole::Slider:
        return "Slider"_s;
    case AccessibilityRole::SliderThumb:
        return "SliderThumb"_s;
    case AccessibilityRole::SpinButton:
        return "SpinButton"_s;
    case AccessibilityRole::SpinButtonPart:
        return "SpinButtonPart"_s;
    case AccessibilityRole::Splitter:
        return "Splitter"_s;
    case AccessibilityRole::StaticText:
        return "StaticText"_s;
    case AccessibilityRole::Strong:
        return "Strong"_s;
    case AccessibilityRole::Subscript:
        return "Subscript"_s;
    case AccessibilityRole::Suggestion:
        return "Suggestion"_s;
    case AccessibilityRole::Summary:
        return "Summary"_s;
    case AccessibilityRole::Superscript:
        return "Superscript"_s;
    case AccessibilityRole::Switch:
        return "Switch"_s;
    case AccessibilityRole::SVGRoot:
        return "SVGRoot"_s;
    case AccessibilityRole::SVGText:
        return "SVGText"_s;
    case AccessibilityRole::SVGTSpan:
        return "SVGTSpan"_s;
    case AccessibilityRole::SVGTextPath:
        return "SVGTextPath"_s;
    case AccessibilityRole::TabGroup:
        return "TabGroup"_s;
    case AccessibilityRole::TabList:
        return "TabList"_s;
    case AccessibilityRole::TabPanel:
        return "TabPanel"_s;
    case AccessibilityRole::Tab:
        return "Tab"_s;
    case AccessibilityRole::Table:
        return "Table"_s;
    case AccessibilityRole::TableHeaderContainer:
        return "TableHeaderContainer"_s;
    case AccessibilityRole::Term:
        return "Term"_s;
    case AccessibilityRole::TextArea:
        return "TextArea"_s;
    case AccessibilityRole::TextField:
        return "TextField"_s;
    case AccessibilityRole::TextGroup:
        return "TextGroup"_s;
    case AccessibilityRole::Time:
        return "Time"_s;
    case AccessibilityRole::Tree:
        return "Tree"_s;
    case AccessibilityRole::TreeGrid:
        return "TreeGrid"_s;
    case AccessibilityRole::TreeItem:
        return "TreeItem"_s;
    case AccessibilityRole::ToggleButton:
        return "ToggleButton"_s;
    case AccessibilityRole::Toolbar:
        return "Toolbar"_s;
    case AccessibilityRole::Unknown:
        return "Unknown"_s;
    case AccessibilityRole::UserInterfaceTooltip:
        return "UserInterfaceTooltip"_s;
    case AccessibilityRole::Video:
        return "Video"_s;
    case AccessibilityRole::WebApplication:
        return "WebApplication"_s;
    case AccessibilityRole::WebArea:
        return "WebArea"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

bool needsLayoutOrStyleRecalc(const Document& document)
{
    if (RefPtr frameView = document.view()) {
        if (frameView->needsLayout() || frameView->checkedLayoutContext()->isLayoutPending())
            return true;
    }
    return document.hasPendingStyleRecalc();
}

std::optional<CursorType> cursorTypeFrom(const StyleProperties& properties)
{
    for (auto property : properties) {
        if (property.id() == CSSPropertyCursor) {
            if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>(property.value()))
                return fromCSSValue<CursorType>(*primitiveValue);
            if (RefPtr valueList = dynamicDowncast<CSSValueList>(property.value()); valueList && valueList->size() >= 2) {
                if (RefPtr primitiveValue = dynamicDowncast<CSSPrimitiveValue>((*valueList)[valueList->size() - 1]))
                    return fromCSSValue<CursorType>(*primitiveValue);
            }
        }
    }
    return std::nullopt;
}

RefPtr<Node> lastNode(const Vector<AXID>& group, AXObjectCache& cache)
{
    ASSERT(isMainThread());

    for (auto axID = group.rbegin(); axID != group.rend(); ++axID) {
        if (RefPtr object = cache.objectForID(*axID)) {
            if (RefPtr node = object->node())
                return node;
        }
    }

    return nullptr;
}

RefPtr<AccessibilityObject> lastObject(const Vector<AXID>& group, AXObjectCache& cache)
{
    for (auto axID = group.rbegin(); axID != group.rend(); ++axID) {
        if (RefPtr object = cache.objectForID(*axID))
            return object;
    }
    return nullptr;
}

} // namespace WebCore
