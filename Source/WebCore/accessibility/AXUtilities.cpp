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
#include "CSSProperty.h"
#include "CSSValueList.h"
#include "Document.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "ElementInlines.h"
#include "HTMLImageElement.h"
#include "HTMLMapElement.h"
#include "HTMLMediaElement.h"
#include "HTMLNames.h"
#include "Node.h"
#include "RenderImage.h"
#include "RenderObject.h"
#include "RenderObjectNode.h"
#include "RenderStyleConstants.h"
#include "RenderTreeBuilder.h"
#include "SpaceSplitString.h"
#include "StyleKeyword+Mappings.h"
#include "StylePropertiesInlines.h"
#include <wtf/CheckedPtr.h>
#include <wtf/RefPtr.h>

namespace WebCore {

using namespace HTMLNames;

RefPtr<ContainerNode> composedParentIgnoringDocumentFragments(const Node& node)
{
    auto* ancestor = node.parentInComposedTree();
    while (is<DocumentFragment>(ancestor))
        ancestor = ancestor->parentInComposedTree();
    return ancestor;
}

RefPtr<ContainerNode> composedParentIgnoringDocumentFragments(const Node* node)
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

const Style::ComputedStyle* safeStyleFrom(Element& element)
{
    // We cannot resolve style (as computedStyle() does) if we are downstream of an existing render tree
    // update. Otherwise, a RELEASE_ASSERT preventing re-entrancy will be hit inside RenderTreeBuilder.
    return RenderTreeBuilder::current() ? element.existingComputedStyle() : element.computedStyle();
}

AffineTransform localToAbsoluteLinearTransform(const RenderObject& renderer)
{
    auto origin = renderer.localToAbsolute(FloatPoint(), MapCoordinatesMode::UseTransforms);
    auto xAxis = renderer.localToAbsolute(FloatPoint(1, 0), MapCoordinatesMode::UseTransforms) - origin;
    auto yAxis = renderer.localToAbsolute(FloatPoint(0, 1), MapCoordinatesMode::UseTransforms) - origin;
    return { xAxis.width(), xAxis.height(), yAxis.width(), yAxis.height(), 0, 0 };
}

bool hasARIAAccNameAttribute(Element& element)
{
    auto trimmed = [&] (const auto& attribute) -> RefPtr<StringImpl> {
        const auto& value = element.attributeWithDefaultARIA(attribute);
        if (isEmpty(value))
            return emptyString();
        return trim(value, isASCIIWhitespace);
    };

    // Check aria-label first - it's the simplest check.
    if (length(trimmed(aria_labelAttr)))
        return true;

    // Check aria-description - just need non-empty, non-whitespace.
    if (length(trimmed(aria_descriptionAttr)))
        return true;

    // For aria-labelledby/aria-describedby, we need to validate that the referenced IDs
    // actually exist and have text content. Per HTML-AAM, if aria-labelledby references
    // non-existing elements, empty elements, or elements with only whitespace text,
    // it should not provide an accessible name.
    auto hasValidIdRef = [&] (const auto& attribute) {
        const auto& value = element.attributeWithDefaultARIA(attribute);
        if (isEmpty(value))
            return false;

        SpaceSplitString ids(value, SpaceSplitString::ShouldFoldCase::No);
        if (ids.isEmpty())
            return false;

        Ref document = element.document();
        for (auto& id : ids) {
            if (RefPtr referencedElement = document->getElementById(id)) {
                RefPtr<StringImpl> elementText = referencedElement->textContent();
                if (!isEmpty(elementText) && !containsOnly<isASCIIWhitespace>(elementText))
                    return true;
            }
        }
        return false;
    };

    if (hasValidIdRef(aria_labelledbyAttr) || hasValidIdRef(aria_labeledbyAttr) || hasValidIdRef(aria_describedbyAttr))
        return true;

    return false;
}

bool hasAccNameAttribute(Element& element)
{
    if (hasARIAAccNameAttribute(element))
        return true;

    // For title, check that it's not whitespace-only.
    const auto& titleValue = element.attributeWithoutSynchronization(titleAttr);
    if (!containsOnly<isASCIIWhitespace>(titleValue))
        return true;

    return false;
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
        return isEmpty(roleValue);
    if (isEmpty(roleValue))
        return false;

    // Lowercase the role value ourselves (allocation-free when it's already lowercase, the common case for
    // ARIA roles) and match without folding, so spaceSplitStringContainsValue doesn't allocate a lowercased copy.
    return SpaceSplitString::spaceSplitStringContainsValue(convertToASCIILowercase(roleValue), role, SpaceSplitString::ShouldFoldCase::No);
}

bool hasAnyRole(Element& element, std::initializer_list<StringView> roles)
{
    auto roleValue = element.attributeWithDefaultARIA(roleAttr);
    if (isEmpty(roleValue))
        return false;

    // Lowercase the role value once (allocation-free when it's already lowercase, the common case for ARIA
    // roles) and match each candidate without folding, rather than lowercasing it once per candidate role.
    RefPtr<AtomStringImpl> lowercasedRoleValue = convertToASCIILowercase(roleValue);
    for (const auto& role : roles) {
        AX_ASSERT(!role.isEmpty());
        if (SpaceSplitString::spaceSplitStringContainsValue(lowercasedRoleValue, role, SpaceSplitString::ShouldFoldCase::No))
            return true;
    }
    return false;
}

bool hasAnyRole(Element* element, std::initializer_list<StringView> roles)
{
    return element ? hasAnyRole(*element, roles) : false;
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
    if (CheckedPtr cache = document.existingAXObjectCache())
        cache->treeData().dumpToStderr();
}

RefPtr<StringImpl> roleToString(AccessibilityRole role)
{
    switch (role) {
    case AccessibilityRole::Application:
        return "Application"_si;
    case AccessibilityRole::ApplicationAlert:
        return "ApplicationAlert"_si;
    case AccessibilityRole::ApplicationAlertDialog:
        return "ApplicationAlertDialog"_si;
    case AccessibilityRole::ApplicationDialog:
        return "ApplicationDialog"_si;
    case AccessibilityRole::ApplicationLog:
        return "ApplicationLog"_si;
    case AccessibilityRole::ApplicationMarquee:
        return "ApplicationMarquee"_si;
    case AccessibilityRole::ApplicationStatus:
        return "ApplicationStatus"_si;
    case AccessibilityRole::ApplicationTimer:
        return "ApplicationTimer"_si;
    case AccessibilityRole::Audio:
        return "Audio"_si;
    case AccessibilityRole::Blockquote:
        return "Blockquote"_si;
    case AccessibilityRole::Button:
        return "Button"_si;
    case AccessibilityRole::Canvas:
        return "Canvas"_si;
    case AccessibilityRole::Caption:
        return "Caption"_si;
    case AccessibilityRole::Cell:
        return "Cell"_si;
    case AccessibilityRole::Checkbox:
        return "Checkbox"_si;
    case AccessibilityRole::Code:
        return "Code"_si;
    case AccessibilityRole::ColorWell:
        return "ColorWell"_si;
    case AccessibilityRole::Column:
        return "Column"_si;
    case AccessibilityRole::ColumnHeader:
        return "ColumnHeader"_si;
    case AccessibilityRole::ComboBox:
        return "ComboBox"_si;
    case AccessibilityRole::DateTime:
        return "DateTime"_si;
    case AccessibilityRole::Definition:
        return "Definition"_si;
    case AccessibilityRole::Deletion:
        return "Deletion"_si;
    case AccessibilityRole::DescriptionList:
        return "DescriptionList"_si;
    case AccessibilityRole::DescriptionListTerm:
        return "DescriptionListTerm"_si;
    case AccessibilityRole::DescriptionListDetail:
        return "DescriptionListDetail"_si;
    case AccessibilityRole::Details:
        return "Details"_si;
    case AccessibilityRole::Directory:
        return "Directory"_si;
    case AccessibilityRole::Document:
        return "Document"_si;
    case AccessibilityRole::DocumentArticle:
        return "DocumentArticle"_si;
    case AccessibilityRole::DocumentMath:
        return "DocumentMath"_si;
    case AccessibilityRole::DocumentNote:
        return "DocumentNote"_si;
    case AccessibilityRole::Emphasis:
        return "Emphasis"_si;
    case AccessibilityRole::Feed:
        return "Feed"_si;
    case AccessibilityRole::Figure:
        return "Figure"_si;
    case AccessibilityRole::Footnote:
        return "Footnote"_si;
    case AccessibilityRole::Form:
        return "Form"_si;
    case AccessibilityRole::FrameHost:
        return "FrameHost"_si;
    case AccessibilityRole::Generic:
        return "Generic"_si;
    case AccessibilityRole::GraphicsDocument:
        return "GraphicsDocument"_si;
    case AccessibilityRole::GraphicsObject:
        return "GraphicsObject"_si;
    case AccessibilityRole::GraphicsSymbol:
        return "GraphicsSymbol"_si;
    case AccessibilityRole::Grid:
        return "Grid"_si;
    case AccessibilityRole::GridCell:
        return "GridCell"_si;
    case AccessibilityRole::Group:
        return "Group"_si;
    case AccessibilityRole::Heading:
        return "Heading"_si;
    case AccessibilityRole::HorizontalRule:
        return "HorizontalRule"_si;
    case AccessibilityRole::Ignored:
        return "Ignored"_si;
    case AccessibilityRole::Inline:
        return "Inline"_si;
    case AccessibilityRole::Image:
        return "Image"_si;
    case AccessibilityRole::ImageMap:
        return "ImageMap"_si;
    case AccessibilityRole::Insertion:
        return "Insertion"_si;
    case AccessibilityRole::Label:
        return "Label"_si;
    case AccessibilityRole::LandmarkBanner:
        return "LandmarkBanner"_si;
    case AccessibilityRole::LandmarkComplementary:
        return "LandmarkComplementary"_si;
    case AccessibilityRole::LandmarkContentInfo:
        return "LandmarkContentInfo"_si;
    case AccessibilityRole::LandmarkDocRegion:
        return "LandmarkDocRegion"_si;
    case AccessibilityRole::LandmarkMain:
        return "LandmarkMain"_si;
    case AccessibilityRole::LandmarkNavigation:
        return "LandmarkNavigation"_si;
    case AccessibilityRole::LandmarkRegion:
        return "LandmarkRegion"_si;
    case AccessibilityRole::LandmarkSearch:
        return "LandmarkSearch"_si;
    case AccessibilityRole::Legend:
        return "Legend"_si;
    case AccessibilityRole::Link:
        return "Link"_si;
    case AccessibilityRole::LineBreak:
        return "LineBreak"_si;
    case AccessibilityRole::List:
        return "List"_si;
    case AccessibilityRole::ListBox:
        return "ListBox"_si;
    case AccessibilityRole::ListBoxOption:
        return "ListBoxOption"_si;
    case AccessibilityRole::ListItem:
        return "ListItem"_si;
    case AccessibilityRole::ListMarker:
        return "ListMarker"_si;
    case AccessibilityRole::LocalFrame:
        return "LocalFrame"_si;
    case AccessibilityRole::Mark:
        return "Mark"_si;
    case AccessibilityRole::MathElement:
        return "MathElement"_si;
    case AccessibilityRole::Menu:
        return "Menu"_si;
    case AccessibilityRole::MenuBar:
        return "MenuBar"_si;
    case AccessibilityRole::MenuItem:
        return "MenuItem"_si;
    case AccessibilityRole::MenuItemCheckbox:
        return "MenuItemCheckbox"_si;
    case AccessibilityRole::MenuItemRadio:
        return "MenuItemRadio"_si;
    case AccessibilityRole::MenuListPopup:
        return "MenuListPopup"_si;
    case AccessibilityRole::MenuListOption:
        return "MenuListOption"_si;
    case AccessibilityRole::Meter:
        return "Meter"_si;
    case AccessibilityRole::Model:
        return "Model"_si;
    case AccessibilityRole::Paragraph:
        return "Paragraph"_si;
    case AccessibilityRole::PopUpButton:
        return "PopUpButton"_si;
    case AccessibilityRole::Pre:
        return "Pre"_si;
    case AccessibilityRole::Presentational:
        return "Presentational"_si;
    case AccessibilityRole::ProgressIndicator:
        return "ProgressIndicator"_si;
    case AccessibilityRole::RadioButton:
        return "RadioButton"_si;
    case AccessibilityRole::RadioGroup:
        return "RadioGroup"_si;
    case AccessibilityRole::RemoteFrame:
        return "RemoteFrame"_si;
    case AccessibilityRole::RowHeader:
        return "RowHeader"_si;
    case AccessibilityRole::Row:
        return "Row"_si;
    case AccessibilityRole::RowGroup:
        return "RowGroup"_si;
    case AccessibilityRole::RubyInline:
        return "RubyInline"_si;
    case AccessibilityRole::RubyText:
        return "RubyText"_si;
    case AccessibilityRole::ScrollArea:
        return "ScrollArea"_si;
    case AccessibilityRole::ScrollBar:
        return "ScrollBar"_si;
    case AccessibilityRole::SearchField:
        return "SearchField"_si;
    case AccessibilityRole::SectionFooter:
        return "SectionFooter"_si;
    case AccessibilityRole::SectionHeader:
        return "SectionHeader"_si;
    case AccessibilityRole::Slider:
        return "Slider"_si;
    case AccessibilityRole::SliderThumb:
        return "SliderThumb"_si;
    case AccessibilityRole::SpinButton:
        return "SpinButton"_si;
    case AccessibilityRole::SpinButtonPart:
        return "SpinButtonPart"_si;
    case AccessibilityRole::Splitter:
        return "Splitter"_si;
    case AccessibilityRole::StaticText:
        return "StaticText"_si;
    case AccessibilityRole::Strong:
        return "Strong"_si;
    case AccessibilityRole::Subscript:
        return "Subscript"_si;
    case AccessibilityRole::Suggestion:
        return "Suggestion"_si;
    case AccessibilityRole::Summary:
        return "Summary"_si;
    case AccessibilityRole::Superscript:
        return "Superscript"_si;
    case AccessibilityRole::Switch:
        return "Switch"_si;
    case AccessibilityRole::SVGRoot:
        return "SVGRoot"_si;
    case AccessibilityRole::SVGText:
        return "SVGText"_si;
    case AccessibilityRole::SVGTSpan:
        return "SVGTSpan"_si;
    case AccessibilityRole::SVGTextPath:
        return "SVGTextPath"_si;
    case AccessibilityRole::TabGroup:
        return "TabGroup"_si;
    case AccessibilityRole::TabList:
        return "TabList"_si;
    case AccessibilityRole::TabPanel:
        return "TabPanel"_si;
    case AccessibilityRole::Tab:
        return "Tab"_si;
    case AccessibilityRole::Table:
        return "Table"_si;
    case AccessibilityRole::TableHeaderContainer:
        return "TableHeaderContainer"_si;
    case AccessibilityRole::Term:
        return "Term"_si;
    case AccessibilityRole::TextArea:
        return "TextArea"_si;
    case AccessibilityRole::TextField:
        return "TextField"_si;
    case AccessibilityRole::TextGroup:
        return "TextGroup"_si;
    case AccessibilityRole::Time:
        return "Time"_si;
    case AccessibilityRole::Tree:
        return "Tree"_si;
    case AccessibilityRole::TreeGrid:
        return "TreeGrid"_si;
    case AccessibilityRole::TreeItem:
        return "TreeItem"_si;
    case AccessibilityRole::ToggleButton:
        return "ToggleButton"_si;
    case AccessibilityRole::Toolbar:
        return "Toolbar"_si;
    case AccessibilityRole::Unknown:
        return "Unknown"_si;
    case AccessibilityRole::UserInterfaceTooltip:
        return "UserInterfaceTooltip"_si;
    case AccessibilityRole::Video:
        return "Video"_si;
    case AccessibilityRole::WebApplication:
        return "WebApplication"_si;
    case AccessibilityRole::WebArea:
        return "WebArea"_si;
    }
    AX_ASSERT_NOT_REACHED();
    return ""_si;
}

OptionSet<DocumentNeeds> needsLayoutOrStyleRecalc(const Document& document)
{
    OptionSet<DocumentNeeds> needs;
    if (RefPtr frameView = document.view()) {
        if (frameView->needsLayout() || frameView->layoutContext().isLayoutPending())
            needs.add(DocumentNeeds::Layout);
    }
    if (document.hasPendingStyleRecalc())
        needs.add(DocumentNeeds::StyleRecalc);
    return needs;
}

std::optional<CursorType> cursorTypeFrom(const StyleProperties& properties)
{
    for (auto property : properties) {
        if (property.id() == CSSPropertyID::Cursor) {
            if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>(property.value()))
                return fromCSSValue<CursorType>(*keywordValue);
            if (RefPtr valueList = dynamicDowncast<CSSValueList>(property.value()); valueList && valueList->size() >= 2) {
                if (RefPtr keywordValue = dynamicDowncast<CSSKeywordValue>((*valueList)[valueList->size() - 1]))
                    return fromCSSValue<CursorType>(*keywordValue);
            }
        }
    }
    return std::nullopt;
}

RefPtr<Node> lastNonAriaHiddenNode(const FixedVector<AXID>& axIDs, AXObjectCache& cache)
{
    AX_ASSERT(isMainThread());

    for (auto axID = axIDs.rbegin(); axID != axIDs.rend(); ++axID) {
        RefPtr object = cache.objectForID(*axID);
        RefPtr node = object ? object->node() : nullptr;
        if (node && !object->isAXHidden())
            return node;
    }
    return nullptr;
}

} // namespace WebCore
