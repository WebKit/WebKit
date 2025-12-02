/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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
#include "AccessibilityNodeObject.h"

#include "AXAttachmentHelpers.h"
#include "AXImageMapHelpers.h"
#include "AXListHelpers.h"
#include "AXLogger.h"
#include "AXLoggerBase.h"
#include "AXNotifications.h"
#include "AXObjectCacheInlines.h"
#include "AXStitchUtilities.h"
#include "AXTableHelpers.h"
#include "AXTreeStore.h"
#include "AXUtilities.h"
#include "AccessibilityMediaHelpers.h"
#include "AccessibilityObjectInlines.h"
#include "AccessibilityRenderObject.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTableColumn.h"
#include "CSSSelector.h"
#include "ComposedTreeIterator.h"
#include "ContainerNodeInlines.h"
#include "DateComponents.h"
#include "EditingInlines.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "ElementRuleCollector.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FindRevealAlgorithms.h"
#include "FloatRect.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAreaElement.h"
#include "HTMLAttachmentElement.h"
#include "HTMLAudioElement.h"
#include "HTMLButtonElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLParagraphElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLSlotElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTableCaptionElement.h"
#include "HTMLTableCellElement.h"
#include "HTMLTableElement.h"
#include "HTMLTableRowElement.h"
#include "HTMLTableSectionElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "HTMLVideoElement.h"
#include "HitTestSource.h"
#include "InlineIteratorLineBoxInlines.h"
#include "KeyboardEvent.h"
#include "LayoutIntegrationLineLayout.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "ProgressTracker.h"
#include "PseudoClassChangeInvalidation.h"
#include "RenderAncestorIterator.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderImage.h"
#include "RenderListBox.h"
#include "RenderListItem.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "RuleFeature.h"
#include "SVGElement.h"
#include "ShadowRoot.h"
#include "StyleListStyleType.h"
#include "StyleResolver.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "TypedElementDescendantIteratorInlines.h"
#include "UserGestureIndicator.h"
#include "VisibleUnits.h"
#include <numeric>
#include <wtf/Scope.h>
#include <wtf/SetForScope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

static String accessibleNameForNode(Node&, Node* labelledbyNode = nullptr);
static void appendNameToStringBuilder(StringBuilder&, String&&, bool prependSpace = true);

AccessibilityNodeObject::AccessibilityNodeObject(AXID axID, Node* node, AXObjectCache& cache)
    : AccessibilityObject(axID, cache)
    , m_node(node)
{
}

Ref<AccessibilityNodeObject> AccessibilityNodeObject::create(AXID axID, Node* node, AXObjectCache& cache)
{
    return adoptRef(*new AccessibilityNodeObject(axID, node, cache));
}

AccessibilityNodeObject::~AccessibilityNodeObject()
{
    ASSERT(isDetached());
}

void AccessibilityNodeObject::init()
{
#ifndef NDEBUG
    ASSERT(!m_initialized);
    m_initialized = true;
#endif
    m_ariaRole = determineAriaRoleAttribute();
    // m_ariaRole must be setup before calling isTable() because isTable() depends on an object's ARIA role.
    if (isTable())
        ensureRareData().setIsExposableTable(computeIsTableExposableThroughAccessibility());
    AccessibilityObject::init();
}

void AccessibilityNodeObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    // AccessibilityObject calls clearChildren.
    AccessibilityObject::detachRemoteParts(detachmentType);
    m_node = nullptr;
}

AccessibilityObject* AccessibilityNodeObject::firstChild() const
{
    RefPtr currentChild = node() ? node()->firstChild() : nullptr;
    if (!currentChild)
        return nullptr;

    auto* cache = axObjectCache();
    if (!cache)
        return nullptr;

    RefPtr axCurrentChild = cache->getOrCreate(*currentChild);
    while (!axCurrentChild && currentChild) {
        currentChild = currentChild->nextSibling();
        axCurrentChild = cache->getOrCreate(currentChild.get());
    }
    return axCurrentChild.unsafeGet();
}

AccessibilityObject* AccessibilityNodeObject::lastChild() const
{
    if (!node())
        return nullptr;

    RefPtr lastChild = node()->lastChild();
    if (!lastChild)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(*lastChild) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::previousSibling() const
{
    if (!node())
        return nullptr;

    RefPtr previousSibling = node()->previousSibling();
    if (!previousSibling)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(*previousSibling) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::nextSibling() const
{
    if (!node())
        return nullptr;

    RefPtr nextSibling = node()->nextSibling();
    if (!nextSibling)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(*nextSibling) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::ownerParentObject() const
{
    auto owners = this->owners();
    AX_DEBUG_ASSERT(owners.size() <= 1);
    return owners.size() ? dynamicDowncast<AccessibilityObject>(owners.first().get()) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::parentObject() const
{
    RefPtr node = this->node();
    if (!node)
        return nullptr;

#if USE(ATSPI)
    // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
    RefPtr domParent = node->parentNode();
#else
    RefPtr domParent = composedParentIgnoringDocumentFragments(*node);
#endif // USE(ATSPI)

    if (!domParent) {
        // This null-check is especially important because Node::parentNode will return nullptr
        // when |node| is in the midst of being destroyed. If we try to call
        // dynamicDowncast<HTMLAreaElement>(*node) on any mid-destruction node, we will crash
        // because m_tagName has already been cleared.
        return nullptr;
    }

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(*node)) {
        RefPtr map = ancestorsOfType<HTMLMapElement>(*areaElement).first();
        return map ? cache->getOrCreate(map->imageElement().get()) : nullptr;
    }

    if (RefPtr ownerParent = ownerParentObject()) [[unlikely]]
        return ownerParent.unsafeGet();

    return cache->getOrCreate(*domParent);
}

#if PLATFORM(IOS_FAMILY)
HTMLMediaElement* AccessibilityNodeObject::mediaElement() const
{
    return dynamicDowncast<HTMLMediaElement>(node());
}

HTMLVideoElement* AccessibilityNodeObject::videoElement() const
{
    return dynamicDowncast<HTMLVideoElement>(node());
}
#endif

LayoutRect AccessibilityNodeObject::checkboxOrRadioRect() const
{
    auto labels = Accessibility::labelsForElement(element());
    if (labels.isEmpty())
        return boundingBoxRect();

    auto* cache = axObjectCache();
    if (!cache)
        return boundingBoxRect();

    // A checkbox or radio button should encompass its label.
    auto selfRect = boundingBoxRect();
    for (auto& label : labels) {
        if (label->renderer()) {
            if (RefPtr axLabel = cache->getOrCreate(label.get()))
                selfRect.unite(axLabel->elementRect());
        }
    }
    return selfRect;
}

LayoutRect AccessibilityNodeObject::elementRect() const
{
    RefPtr node = this->node();
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node.get()); input && (input->isCheckbox() || input->isRadioButton()))
        return checkboxOrRadioRect();

    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node.get())) {
        CheckedPtr renderer = AXImageMapHelpers::rendererFromAreaElement(*areaElement);
        return renderer ? areaElement->computeRect(renderer.get()) : LayoutRect();
    }

    return boundingBoxRect();
}

Path AccessibilityNodeObject::elementPath() const
{
    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node())) {
        CheckedPtr renderer = AXImageMapHelpers::rendererFromAreaElement(*areaElement);
        return renderer ? areaElement->computePath(*renderer) : Path();
    }

    return Path();
}

LayoutRect AccessibilityNodeObject::boundingBoxRect() const
{
    if (hasDisplayContents()) {
        LayoutRect contentsRect;
        for (const auto& child : const_cast<AccessibilityNodeObject*>(this)->unignoredChildren())
            contentsRect.unite(child->elementRect());

        if (!contentsRect.isEmpty())
            return contentsRect;
    }

    // Non-display:contents AccessibilityNodeObjects have no mechanism to return a size or position.
    // Instead, let's return a box at the position of an ancestor that does have a position, make it
    // the width of that ancestor, and about the height of a line of text, so it's clear this object is
    // a descendant of that ancestor.
    return nonEmptyAncestorBoundingBox();
}

LayoutRect AccessibilityNodeObject::nonEmptyAncestorBoundingBox() const
{
    for (RefPtr<AccessibilityObject> ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
        if (!ancestor->renderer())
            continue;
        auto ancestorRect = ancestor->elementRect();
        if (ancestorRect.isEmpty())
            continue;

        return {
            ancestorRect.location(),
            LayoutSize(ancestorRect.width(), LayoutUnit(std::min(10.0f, ancestorRect.height().toFloat())))
        };
    }
    // Fallback to returning a default, non-empty rect at 0, 0.
    return { 0, 0, 1, 1 };
}

Document* AccessibilityNodeObject::document() const
{
    if (!node())
        return nullptr;
    return &node()->document();
}

LocalFrameView* AccessibilityNodeObject::documentFrameView() const
{
    if (auto* node = this->node())
        return node->document().view();
    return AccessibilityObject::documentFrameView();
}

AccessibilityRole AccessibilityNodeObject::determineListRoleWithCleanChildren()
{
    if (!isAccessibilityList())
        return AccessibilityRole::Unknown;

    ASSERT(!needsToUpdateChildren() && childrenInitialized());

    // Directory is mapped to list for now, but does not adhere to the same heuristics.
    if (ariaRoleAttribute() == AccessibilityRole::Directory)
        return AccessibilityRole::List;

    // Heuristic to determine if an ambiguous list is relevant to convey to the accessibility tree.
    //   1. If it's an ordered list or has role="list" defined, then it's a list.
    //      1a. Unless the list has no children, then it's not a list.
    //   2. If it is contained in <nav> or <el role="navigation">, it's a list.
    //   3. If it displays visible list markers, it's a list.
    //   4. If it does not display list markers, it's not a list.
    //   5. If it has one or zero listitem children, it's not a list.
    //   6. Otherwise it's a list.

    auto role = AccessibilityRole::List;

    // Temporarily set role so that we can query children (otherwise canHaveChildren returns false).
    SetForScope temporaryRole(m_role, role);

    unsigned listItemCount = 0;
    bool hasVisibleMarkers = false;

    const auto& children = unignoredChildren();
    // DescriptionLists are always semantically a description list, so do not apply heuristics.
    if (isDescriptionList() && children.size())
        return AccessibilityRole::DescriptionList;

    for (const auto& child : children) {
        RefPtr node = child->node();
        RefPtr axChild = dynamicDowncast<AccessibilityObject>(child.get());
        if (axChild && axChild->ariaRoleAttribute() == AccessibilityRole::ListItem)
            listItemCount++;
        else if (child->role() == AccessibilityRole::ListItem) {
            // Rendered list items always count.
            if (CheckedPtr renderListItem = dynamicDowncast<RenderListItem>(child->renderer())) {
                if (!hasVisibleMarkers && (!renderListItem->style().listStyleType().isNone() || !renderListItem->style().listStyleImage().isNone() || (renderListItem->element() && AXListHelpers::childHasPseudoVisibleListItemMarkers(*renderListItem->element()))))
                    hasVisibleMarkers = true;
                listItemCount++;
            } else if (WebCore::elementName(node.get()) == ElementName::HTML_li) {
                // Inline elements that are in a list with an explicit role should also count.
                if (ariaRoleAttribute() == AccessibilityRole::List)
                    listItemCount++;

                if (node && AXListHelpers::childHasPseudoVisibleListItemMarkers(*node)) {
                    hasVisibleMarkers = true;
                    listItemCount++;
                }
            }
        }
    }

    // Non <ul> lists and ARIA lists only need to have one child.
    // <ul>, <ol> lists need to have visible markers.
    if (ariaRoleAttribute() != AccessibilityRole::Unknown) {
        if (!listItemCount)
            role = AccessibilityRole::Group;
    } else if (!hasVisibleMarkers) {
        // http://webkit.org/b/193382 lists inside of navigation hierarchies should still be considered lists.
        if (Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (auto& object) { return object.role() == AccessibilityRole::LandmarkNavigation; }))
            role = AccessibilityRole::List;
        else
            role = AccessibilityRole::Group;
    }

    return role;
}


AccessibilityRole AccessibilityNodeObject::determineAccessibilityRole()
{
    AXTRACE("AccessibilityNodeObject::determineAccessibilityRole"_s);
    if (m_ariaRole != AccessibilityRole::Unknown)
        return m_ariaRole;

    if (isExposableTable())
        return AccessibilityRole::Table;

    if (isExposedTableRow())
        return AccessibilityRole::Row;

    // FIXME: When the URL changes, we should recompute an object's role.
    if (isImageMapLink())
        return !url().isEmpty() ? AccessibilityRole::Link : AccessibilityRole::Generic;

    auto roleFromNode = determineAccessibilityRoleFromNode();

    if (isTableCell() && (roleFromNode != AccessibilityRole::ColumnHeader && roleFromNode != AccessibilityRole::RowHeader && roleFromNode != AccessibilityRole::Cell && roleFromNode != AccessibilityRole::GridCell)) {
        RefPtr parentTable = this->parentTable();
        if (parentTable && parentTable->isExposableTable())
            return parentTable->hasGridRole() ? AccessibilityRole::GridCell : AccessibilityRole::Cell;
    }

    return roleFromNode;
}

bool AccessibilityNodeObject::matchesTextAreaRole() const
{
    return is<HTMLTextAreaElement>(node()) || hasContentEditableAttributeSet();
}

AccessibilityRole AccessibilityNodeObject::determineAccessibilityRoleFromNode(TreatStyleFormatGroupAsInline treatStyleFormatGroupAsInline) const
{
    AXTRACE("AccessibilityNodeObject::determineAccessibilityRoleFromNode"_s);

    RefPtr node = this->node();
    if (!node)
        return AccessibilityRole::Unknown;

    if (node->isTextNode())
        return AccessibilityRole::StaticText;

    RefPtr element = dynamicDowncast<HTMLElement>(*node);
    if (!element)
        return AccessibilityRole::Unknown;

    if (element->isLink())
        return AccessibilityRole::Link;
    if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(*element))
        return selectElement->multiple() ? AccessibilityRole::ListBox : AccessibilityRole::PopUpButton;
    if (is<HTMLImageElement>(*element) && element->hasAttributeWithoutSynchronization(usemapAttr))
        return AccessibilityRole::ImageMap;

    auto elementName = element->elementName();
    if (elementName == ElementName::HTML_li)
        return AccessibilityRole::ListItem;
    if (elementName == ElementName::HTML_button)
        return buttonRoleType();
    if (elementName == ElementName::HTML_legend)
        return AccessibilityRole::Legend;
    if (elementName == ElementName::HTML_canvas)
        return AccessibilityRole::Canvas;

    if (is<HTMLTableCellElement>(*element))
        return AXTableHelpers::layoutTableCellRole;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*element))
        return roleFromInputElement(*input);

    if (matchesTextAreaRole())
        return AccessibilityRole::TextArea;

    if (headingLevel())
        return AccessibilityRole::Heading;

    if (elementName == ElementName::HTML_code)
        return AccessibilityRole::Code;
    if (elementName == ElementName::HTML_del || elementName == ElementName::HTML_s)
        return AccessibilityRole::Deletion;
    if (elementName == ElementName::HTML_ins)
        return AccessibilityRole::Insertion;
    if (elementName == ElementName::HTML_sub)
        return AccessibilityRole::Subscript;
    if (elementName == ElementName::HTML_sup)
        return AccessibilityRole::Superscript;
    if (elementName == ElementName::HTML_strong)
        return AccessibilityRole::Strong;

    if (elementName == ElementName::HTML_kbd
        || elementName == ElementName::HTML_pre
        || elementName == ElementName::HTML_samp
        || elementName == ElementName::HTML_var
        || elementName == ElementName::HTML_cite)
        return treatStyleFormatGroupAsInline == TreatStyleFormatGroupAsInline::Yes ? AccessibilityRole::Inline : AccessibilityRole::TextGroup;

    if (elementName == ElementName::HTML_dd)
        return AccessibilityRole::DescriptionListDetail;
    if (elementName == ElementName::HTML_dt)
        return AccessibilityRole::DescriptionListTerm;
    if (elementName == ElementName::HTML_dl)
        return AccessibilityRole::DescriptionList;

    if (elementName == ElementName::HTML_menu
        || elementName == ElementName::HTML_ol
        || elementName == ElementName::HTML_ul)
        return AccessibilityRole::List;

    if (elementName == ElementName::HTML_fieldset)
        return AccessibilityRole::Group;
    if (elementName == ElementName::HTML_figure)
        return AccessibilityRole::Figure;
    if (elementName == ElementName::HTML_p)
        return AccessibilityRole::Paragraph;

    if (is<HTMLLabelElement>(*element))
        return AccessibilityRole::Label;
    if (elementName == ElementName::HTML_dfn) {
        // Confusingly, the `dfn` element represents a term being defined, making it equivalent to the "term" ARIA
        // role rather than the "definition" ARIA role. The "definition" ARIA role has no HTML equivalent.
        // https://html.spec.whatwg.org/multipage/text-level-semantics.html#the-dfn-element
        // https://w3c.github.io/aria/#term and https://w3c.github.io/aria/#definition
        return AccessibilityRole::Term;
    }
    if (elementName == ElementName::HTML_div && !isNonNativeTextControl())
        return AccessibilityRole::Generic;
    if (is<HTMLFormElement>(*element))
        return AccessibilityRole::Form;
    if (elementName == ElementName::HTML_article)
        return AccessibilityRole::DocumentArticle;
    if (elementName == ElementName::HTML_main)
        return AccessibilityRole::LandmarkMain;
    if (elementName == ElementName::HTML_nav)
        return AccessibilityRole::LandmarkNavigation;

    if (elementName == ElementName::HTML_aside) {
        if (ariaRoleAttribute() == AccessibilityRole::LandmarkComplementary || !isDescendantOfElementType({ asideTag, articleTag, sectionTag, navTag }))
            return AccessibilityRole::LandmarkComplementary;

        // https://w3c.github.io/html-aam/#el-aside
        // When within a sectioning content elements, complementary landmarks must have accnames to acquire the role.
        return WebCore::hasAccNameAttribute(*element) ? AccessibilityRole::LandmarkComplementary : AccessibilityRole::Generic;
    }

    if (elementName == ElementName::HTML_search)
        return AccessibilityRole::LandmarkSearch;

    if (elementName == ElementName::HTML_section) {
        // https://w3c.github.io/html-aam/#el-section
        // The default role attribute value for the section element, region, became a landmark in ARIA 1.1.
        // The HTML AAM spec says it is "strongly recommended" that ATs only convey and provide navigation
        // for section elements which have names.
        return WebCore::hasAccNameAttribute(*element) ? AccessibilityRole::LandmarkRegion : AccessibilityRole::TextGroup;
    }
    if (elementName == ElementName::HTML_address)
        return AccessibilityRole::Group;
    if (elementName == ElementName::HTML_blockquote)
        return AccessibilityRole::Blockquote;
    if (elementName == ElementName::HTML_caption || elementName == ElementName::HTML_figcaption)
        return AccessibilityRole::Caption;
    if (elementName == ElementName::HTML_dialog)
        return AccessibilityRole::ApplicationDialog;
    if (elementName == ElementName::HTML_mark || equalLettersIgnoringASCIICase(getAttribute(roleAttr), "mark"_s))
        return AccessibilityRole::Mark;
    if (is<HTMLDetailsElement>(*element))
        return AccessibilityRole::Details;
    if (RefPtr summaryElement = dynamicDowncast<HTMLSummaryElement>(*element); summaryElement && summaryElement->isActiveSummary())
        return AccessibilityRole::Summary;

    // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
    // Output elements should be mapped to status role.
    if (isOutput())
        return AccessibilityRole::ApplicationStatus;

#if ENABLE(VIDEO)
    if (is<HTMLVideoElement>(*element))
        return AccessibilityRole::Video;
    if (is<HTMLAudioElement>(*element))
        return AccessibilityRole::Audio;
#endif

#if ENABLE(MODEL_ELEMENT)
    if (elementName == ElementName::HTML_model)
        return AccessibilityRole::Model;
#endif

    // The HTML element should not be exposed as an element. That's what the RenderView element does.
    if (elementName == ElementName::HTML_html)
        return AccessibilityRole::Ignored;

    // There should only be one role="banner" per page.
    // https://w3c.github.io/html-aam/#el-header-ancestorbody
    // Footer elements should be role="banner" if scoped to body, and consequently become a landmark.
    if (elementName == ElementName::HTML_header) {
        if (!isDescendantOfElementType({ articleTag, asideTag, mainTag, navTag, sectionTag }))
            return AccessibilityRole::LandmarkBanner;

        // https://github.com/w3c/aria/pull/1931
        // A <header> that is a descendant of <main> or a sectioning element should be role="sectionheader".
        return AccessibilityRole::SectionHeader;
    }

    // There should only be one role="contentinfo" per page.
    // https://w3c.github.io/html-aam/#el-footer-ancestorbody
    // Footer elements should be role="contentinfo" if scoped to body, and consequently become a landmark.
    if (elementName == ElementName::HTML_footer) {
        if (!isDescendantOfElementType({ articleTag, asideTag, mainTag, navTag, sectionTag }))
            return AccessibilityRole::LandmarkContentInfo;

        // https://github.com/w3c/aria/pull/1931
        // A <footer> that is a descendant of <main> or a sectioning element should be role="sectionfooter".
        return AccessibilityRole::SectionFooter;
    }

    if (elementName == ElementName::HTML_time)
        return AccessibilityRole::Time;
    if (elementName == ElementName::HTML_hr)
        return AccessibilityRole::HorizontalRule;
    if (elementName == ElementName::HTML_em)
        return AccessibilityRole::Emphasis;
    if (elementName == ElementName::HTML_hgroup)
        return AccessibilityRole::Group;

    // If the element does not have role, but it has ARIA attributes, or accepts tab focus, accessibility should fallback to exposing it as a group.
    if (supportsARIAAttributes() || canSetFocusAttribute() || element->isFocusable())
        return AccessibilityRole::Group;

    return AccessibilityRole::Unknown;
}

AccessibilityRole AccessibilityNodeObject::roleFromInputElement(const HTMLInputElement& input) const
{
    AXTRACE("AccessibilityNodeObject::roleFromInputElement"_s);
    ASSERT(dynamicDowncast<HTMLInputElement>(node()) == &input);

    if (input.isTextButton())
        return buttonRoleType();
    if (input.isSwitch())
        return AccessibilityRole::Switch;
    if (input.isCheckbox())
        return AccessibilityRole::Checkbox;
    if (input.isRadioButton())
        return AccessibilityRole::RadioButton;

    if (input.isTextField()) {
        // Text fields may have a combobox ancestor, in which case we want to return role combobox.
        // This was ARIA 1.1 practice, but it has been recommended against since. Keeping this heuristics here in order to support those sites that are still using this structure.
        bool foundCombobox = false;
        for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
            if (ancestor->isComboBox()) {
                foundCombobox = true;
                break;
            }
            if (!ancestor->isGroup() && ancestor->role() != AccessibilityRole::Generic)
                break;
        }
        if (foundCombobox)
            return AccessibilityRole::ComboBox;

        return input.isSearchField() ? AccessibilityRole::SearchField : AccessibilityRole::TextField;
    }

    if (input.isDateField() || input.isDateTimeLocalField() || input.isMonthField() || input.isTimeField() || input.isWeekField())
        return AccessibilityRole::DateTime;
    if (input.isFileUpload())
        return AccessibilityRole::Button;
    if (input.isColorControl())
        return AccessibilityRole::ColorWell;
    if (input.isInputTypeHidden())
        return AccessibilityRole::Ignored;
    if (input.isRangeControl())
        return AccessibilityRole::Slider;

    // All other input type is treated as a text field.
    return AccessibilityRole::TextField;
}

bool AccessibilityNodeObject::isDescendantOfElementType(const HashSet<QualifiedName>& tagNames) const
{
    if (!m_node)
        return false;

    for (Ref ancestorElement : ancestorsOfType<Element>(*m_node)) {
        if (tagNames.contains(ancestorElement->tagQName()))
            return true;
    }
    return false;
}

void AccessibilityNodeObject::updateChildrenIfNecessary()
{
    if (needsToUpdateChildren())
        clearChildren();

    AccessibilityObject::updateChildrenIfNecessary();
}

void AccessibilityNodeObject::clearChildren()
{
    AccessibilityObject::clearChildren();

    m_childrenDirty = false;
    m_containsOnlyStaticText = false;
    m_containsOnlyStaticTextDirty = false;

    if (CheckedPtr rareData = this->rareData())
        rareData->resetChildrenDependentTableFields();
}

void AccessibilityNodeObject::updateOwnedChildrenIfNecessary()
{
    bool didRemoveChild = false;
    auto ownedObjects = this->ownedObjects();
    if (ownedObjects.isEmpty())
        return;

    for (const auto& child : ownedObjects) {
        if (m_children.removeFirst(child)) {
            // If the child already exists as a DOM child, but is also in the owned objects, then
            // we need to re-order this child in the aria-owns order.
            didRemoveChild = true;
        }
        addChild(downcast<AccessibilityObject>(child.get()));
    }

    if (didRemoveChild) {
        // Fix-up the children index-in-parent fields after removing a child in the middle of m_children,
        // as any index after the removed child will now be wrong.
        resetChildrenIndexInParent();
    }
}

void AccessibilityNodeObject::addChildren()
{
    // If the need to add more children in addition to existing children arises,
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_childrenInitialized);
    m_childrenInitialized = true;

    auto clearDirtySubtree = makeScopeExit([&] {
        m_subtreeDirty = false;
    });

    RefPtr node = this->node();
    if (!node)
        return;

    // The only time we add children from the DOM tree to a node with a renderer is when it's a canvas.
    if (renderer() && WebCore::elementName(*node) != ElementName::HTML_canvas)
        return;

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return;

#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (isExposableTable()) {
        // When !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE), the only time children are added for tables
        // are through the rows, columns, and header container added via addTableChildrenAndCellSlots.
        addTableChildrenAndCellSlots();
        return;
    }
#endif

#if USE(ATSPI)
    // FIXME: Consider removing this ATSPI-only branch with https://bugs.webkit.org/show_bug.cgi?id=282117.
    for (auto* child = node->firstChild(); child; child = child->nextSibling())
        addChild(cache->getOrCreate(*child));
#else
    if (RefPtr containerNode = dynamicDowncast<ContainerNode>(*node)) {
        // Specify an InlineContextCapacity template parameter of 0 to avoid allocating ComposedTreeIterator's
        // internal vector on the stack. See comment in AccessibilityRenderObject::addChildren() for a full
        // explanation of this behavior.
        for (Ref child : composedTreeChildren</* InlineContextCapacity */ 0>(*containerNode))
            addChild(cache->getOrCreate(child.get()));
    }
#endif // USE(ATSPI)

    updateOwnedChildrenIfNecessary();

#if ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    if (isExposableTable())
        addTableChildrenAndCellSlots();
#endif

#ifndef NDEBUG
    verifyChildrenIndexInParent();
#endif
}

bool AccessibilityNodeObject::canHaveChildren() const
{
    // When <noscript> is not being used (its renderer() == 0), ignore its children
    if (node() && !renderer() && WebCore::elementName(node()) == ElementName::HTML_noscript)
        return false;
    // If this is an AccessibilityRenderObject, then it's okay if this object
    // doesn't have a node - there are some renderers that don't have associated
    // nodes, like scroll areas and css-generated text.

    // Elements that should not have children.
    switch (role()) {
    case AccessibilityRole::Button:
#if !USE(ATSPI)
    // GTK/ATSPI layout tests expect popup buttons to have children.
    case AccessibilityRole::PopUpButton:
#endif
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Tab:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::StaticText:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::ScrollBar:
    case AccessibilityRole::ProgressIndicator:
    case AccessibilityRole::Switch:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::Splitter:
    case AccessibilityRole::Meter:
        return false;
    default:
        return true;
    }
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::visibleChildren()
{
    // Only listboxes are asked for their visible children.
    CheckedPtr renderListBox = dynamicDowncast<RenderListBox>(renderer());
    if (!renderListBox && ariaRoleAttribute() == AccessibilityRole::ListBox) {
        if (!childrenInitialized())
            addChildren();
        AccessibilityChildrenVector result;
        for (const auto& child : unignoredChildren()) {
            if (!child->isOffScreen())
                result.append(child);
        }
        return result;
    }

    // Handle native listboxes (RenderListBox).
    if (renderListBox && role() == AccessibilityRole::ListBox) {
        if (!childrenInitialized())
            addChildren();

        const auto& children = const_cast<AccessibilityNodeObject*>(this)->unignoredChildren();
        AXCoreObject::AccessibilityChildrenVector result;
        size_t size = children.size();
        for (size_t i = 0; i < size; i++) {
            if (renderListBox->listIndexIsVisible(i))
                result.append(children[i]);
        }
        return result;
    }

    return { };
}

bool AccessibilityNodeObject::isValidTree() const
{
    // A valid tree can only have treeitem or group of treeitems as a child.
    // https://www.w3.org/TR/wai-aria/#tree
    RefPtr node = this->node();
    if (!node)
        return false;

    Deque<Ref<Node>> queue;
    for (RefPtr child = node->firstChild(); child; child = queue.last()->nextSibling())
        queue.append(child.releaseNonNull());

    while (!queue.isEmpty()) {
        Ref child = queue.takeFirst();

        RefPtr childElement = dynamicDowncast<Element>(child);
        if (!childElement)
            continue;
        if (hasRole(*childElement, "treeitem"_s))
            continue;
        if (!hasAnyRole(*childElement, { "group"_s, "presentation"_s }))
            return false;

        for (RefPtr groupChild = child->firstChild(); groupChild; groupChild = queue.last()->nextSibling())
            queue.append(groupChild.releaseNonNull());
    }
    return true;
}

bool AccessibilityNodeObject::computeIsIgnored() const
{
#ifndef NDEBUG
    // Double-check that an AccessibilityObject is never accessed before
    // it's been initialized.
    ASSERT(m_initialized);
#endif
    if (isTree())
        return isIgnoredByDefault();

    RefPtr node = this->node();
    if (!node)
        return true;

    if (node->isTextNode() && !renderer()) {
        RefPtr parent = node->parentNode();
        // Fallback content in iframe nodes should be ignored.
        if (WebCore::elementName(parent.get()) == ElementName::HTML_iframe && parent->renderer())
            return true;

        // Whitespace only text elements should be ignored when they have no renderer.
        if (stringValue().containsOnly<isASCIIWhitespace>())
            return true;
    }

    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    if (isImageMapLink() && !url().isEmpty())
        return false;

    auto role = this->role();
    if (role == AccessibilityRole::Ignored || role == AccessibilityRole::Unknown)
        return true;

    if (isRenderHidden() && !ancestorsOfType<HTMLCanvasElement>(*node).first()) {
        // Only allow display:none / hidden-visibility node-only objects for canvas subtrees.
        return true;
    }

    if (isTableCell())
        return !isExposedTableCell();

    return false;
}

bool AccessibilityNodeObject::hasElementDescendant() const
{
    RefPtr element = dynamicDowncast<Element>(node());
    return element && childrenOfType<Element>(*element).first();
}

static bool isFlowContent(Node& node)
{
    if (auto* element = dynamicDowncast<HTMLElement>(node)) {
        // https://html.spec.whatwg.org/#flow-content
        // Below represents a non-comprehensive list of common flow content elements.
        const AtomString& tag = element->localName();
        if (tag == blockquoteTag
        || tag == canvasTag
        || tag == codeTag
        || tag == divTag
        || tag == olTag
        || tag == pictureTag
        || tag == preTag
        || tag == pTag
        || tag == spanTag
        || tag == ulTag)
            return true;
    }

    auto* text = dynamicDowncast<Text>(node);
    return text && !text->data().containsOnly<isASCIIWhitespace>();
}

bool AccessibilityNodeObject::isNativeTextControl() const
{
    if (is<HTMLTextAreaElement>(node()))
        return true;

    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    return input && (input->isText() || input->isNumberField());
}

bool AccessibilityNodeObject::isSearchField() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    if (role() == AccessibilityRole::SearchField)
        return true;

    RefPtr inputElement = dynamicDowncast<HTMLInputElement>(*node);
    if (!inputElement)
        return false;

    // Some websites don't label their search fields as such. However, they will
    // use the word "search" in either the form or input type. This won't catch every case,
    // but it will catch google.com for example.

    // Check the node name of the input type, sometimes it's "search".
    const AtomString& nameAttribute = getAttribute(nameAttr);
    if (nameAttribute.containsIgnoringASCIICase("search"_s))
        return true;

    // Check the form action and the name, which will sometimes be "search".
    RefPtr form = inputElement->form();
    if (form && (form->name().containsIgnoringASCIICase("search"_s) || form->action().containsIgnoringASCIICase("search"_s)))
        return true;

    return false;
}

bool AccessibilityNodeObject::isNativeImage() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    if (is<HTMLImageElement>(*node))
        return true;

    auto elementName = WebCore::elementName(*node);
    if (elementName == ElementName::HTML_applet || elementName == ElementName::HTML_embed || elementName == ElementName::HTML_object)
        return true;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node))
        return input->isImageButton();

    return false;
}

bool AccessibilityNodeObject::isSecureField() const
{
    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    if (!input || ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;
    return input->isSecureField();
}

bool AccessibilityNodeObject::isEnabled() const
{
    if (isImageMapLink())
        return true;

    // ARIA says that the disabled status applies to the current element and all descendant elements.
    for (AccessibilityObject* object = const_cast<AccessibilityNodeObject*>(this); object; object = object->parentObject()) {
        const AtomString& disabledStatus = object->getAttribute(aria_disabledAttr);
        if (equalLettersIgnoringASCIICase(disabledStatus, "true"_s))
            return false;
        if (equalLettersIgnoringASCIICase(disabledStatus, "false"_s))
            break;
    }

    if (role() == AccessibilityRole::HorizontalRule)
        return false;

    RefPtr element = dynamicDowncast<Element>(node());
    return !element || !element->isDisabledFormControl();
}

bool AccessibilityNodeObject::isIndeterminate() const
{
    if (supportsCheckedState())
        return checkboxOrRadioValue() == AccessibilityButtonState::Mixed;

    // We handle this for native <progress> elements in AccessibilityProgressIndicator::isIndeterminate.
    if (ariaRoleAttribute() == AccessibilityRole::ProgressIndicator)
        return !hasARIAValueNow();

    return false;
}

bool AccessibilityNodeObject::isPressed() const
{
    if (!isButton())
        return false;

    RefPtr node = this->node();
    if (!node)
        return false;

    // If this is an toggle button, check the aria-pressed attribute rather than node()->active()
    if (isToggleButton())
        return equalLettersIgnoringASCIICase(getAttribute(aria_pressedAttr), "true"_s);

    RefPtr element = dynamicDowncast<Element>(*node);
    return element && element->active();
}

bool AccessibilityNodeObject::isChecked() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    // First test for native checkedness semantics
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node))
        return input->matchesCheckedPseudoClass();

    // Else, if this is an ARIA checkbox or radio, respect the aria-checked attribute
    bool validRole = false;
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TreeItem:
        validRole = true;
        break;
    default:
        break;
    }

    if (validRole && equalLettersIgnoringASCIICase(getAttribute(aria_checkedAttr), "true"_s))
        return true;

    return false;
}

bool AccessibilityNodeObject::isMultiSelectable() const
{
    bool hasGridRole = this->hasGridRole();
    if (isTable() && !hasGridRole) {
        // Per https://w3c.github.io/aria/#table, role="table" elements don't support selection,
        // or aria-multiselectable — only role="grid" and role="treegrid".
        return false;
    }

    if (hasGridRole)
        return !equalLettersIgnoringASCIICase(getAttribute(aria_multiselectableAttr), "false"_s);

    const AtomString& ariaMultiSelectable = getAttribute(aria_multiselectableAttr);
    if (equalLettersIgnoringASCIICase(ariaMultiSelectable, "true"_s))
        return true;
    if (equalLettersIgnoringASCIICase(ariaMultiSelectable, "false"_s))
        return false;

    RefPtr select = dynamicDowncast<HTMLSelectElement>(node());
    return select && select->multiple();
}

bool AccessibilityNodeObject::isRequired() const
{
    RefPtr formControlElement = dynamicDowncast<HTMLFormControlElement>(node());
    if (formControlElement && formControlElement->isRequired())
        return true;

    const AtomString& requiredValue = getAttribute(aria_requiredAttr);
    if (equalLettersIgnoringASCIICase(requiredValue, "true"_s))
        return true;
    if (equalLettersIgnoringASCIICase(requiredValue, "false"_s))
        return false;

    return false;
}

String AccessibilityNodeObject::accessKey() const
{
    RefPtr element = this->element();
    return element ? element->attributeWithoutSynchronization(accesskeyAttr) : String();
}

bool AccessibilityNodeObject::supportsDropping() const
{
    return determineDropEffects().size();
}

bool AccessibilityNodeObject::supportsDragging() const
{
    const AtomString& grabbed = getAttribute(aria_grabbedAttr);
    return equalLettersIgnoringASCIICase(grabbed, "true"_s) || equalLettersIgnoringASCIICase(grabbed, "false"_s) || hasAttribute(draggableAttr);
}

bool AccessibilityNodeObject::isGrabbed()
{
#if ENABLE(DRAG_SUPPORT)
    if (RefPtr localMainFrame = this->localMainFrame()) {
        if (localMainFrame->eventHandler().draggingElement() == element())
            return true;
    }
#endif

    return elementAttributeValue(aria_grabbedAttr);
}

Vector<String> AccessibilityNodeObject::determineDropEffects() const
{
    // Order is aria-dropeffect, dropzone, webkitdropzone
    const AtomString& dropEffects = getAttribute(aria_dropeffectAttr);
    if (!dropEffects.isEmpty())
        return makeStringByReplacingAll(dropEffects.string(), '\n', ' ').split(' ');

    auto dropzone = getAttribute(dropzoneAttr);
    if (!dropzone.isEmpty())
        return Vector<String> { dropzone };

    auto webkitdropzone = getAttribute(webkitdropzoneAttr);
    if (!webkitdropzone.isEmpty())
        return Vector<String> { webkitdropzone };

    // FIXME: We should return drop effects for elements with `dragenter` and `dragover` event handlers.
    // dropzone and webkitdropzone used to serve this purpose, but are deprecated in favor of the
    // aforementioned event handlers.
    //
    // https://html.spec.whatwg.org/dev/obsolete.html:
    // "dropzone on all elements: Use script to handle the dragenter and dragover events instead."
    return { };
}

bool AccessibilityNodeObject::supportsARIAOwns() const
{
    return !getAttribute(aria_ownsAttr).isEmpty();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::radioButtonGroup() const
{
    AccessibilityChildrenVector result;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node())) {
        auto radioButtonGroup = input->radioButtonGroup();
        result.reserveInitialCapacity(radioButtonGroup.size());

        WeakPtr cache = axObjectCache();
        for (auto& radioSibling : radioButtonGroup) {
            if (!cache)
                break;
            if (RefPtr object = cache->getOrCreate(radioSibling.ptr()))
                result.append(object.releaseNonNull());
        }
    }

    return result;
}

String AccessibilityNodeObject::valueDescription() const
{
    if (!isRangeControl())
        return String();

    return getAttribute(aria_valuetextAttr).string();
}

float AccessibilityNodeObject::valueForRange() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()); input && input->isRangeControl())
        return input->valueAsNumber();

#if ENABLE(ATTACHMENT_ELEMENT)
    if (RefPtr attachmentElement = dynamicDowncast<HTMLAttachmentElement>(node())) {
        float progress = 0;
        if (AXAttachmentHelpers::hasProgress(*attachmentElement, &progress))
            return progress;
    }
#endif

    if (!isRangeControl())
        return 0.0f;

    // In ARIA 1.1, the implicit value for aria-valuenow on a spin button is 0.
    // For other roles, it is half way between aria-valuemin and aria-valuemax.
    auto& value = getAttribute(aria_valuenowAttr);
    if (!value.isEmpty())
        return value.toFloat();

    return isSpinButton() ? 0 : std::midpoint(minValueForRange(), maxValueForRange());
}

#if ENABLE(ATTACHMENT_ELEMENT)
bool AccessibilityNodeObject::hasProgress() const
{
    if (RefPtr attachmentElement = dynamicDowncast<HTMLAttachmentElement>(node()))
        return AXAttachmentHelpers::hasProgress(*attachmentElement);
    return false;
}
#endif

float AccessibilityNodeObject::maxValueForRange() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()); input && input->isRangeControl())
        return input->maximum();

    if (!isRangeControl())
        return 0.0f;

    auto& value = getAttribute(aria_valuemaxAttr);
    if (!value.isEmpty())
        return value.toFloat();

    // In ARIA 1.1, the implicit value for aria-valuemax on a spin button
    // is that there is no maximum value. For other roles, it is 100.
    return isSpinButton() ? std::numeric_limits<float>::max() : 100.0f;
}

float AccessibilityNodeObject::minValueForRange() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()); input && input->isRangeControl())
        return input->minimum();

    if (!isRangeControl())
        return 0.0f;

    auto& value = getAttribute(aria_valueminAttr);
    if (!value.isEmpty())
        return value.toFloat();

    // In ARIA 1.1, the implicit value for aria-valuemin on a spin button
    // is that there is no minimum value. For other roles, it is 0.
    return isSpinButton() ? -std::numeric_limits<float>::max() : 0.0f;
}

float AccessibilityNodeObject::stepValueForRange() const
{
    return getAttribute(stepAttr).toFloat();
}

std::optional<AccessibilityOrientation> AccessibilityNodeObject::orientationFromARIA() const
{
    const AtomString& ariaOrientation = getAttribute(aria_orientationAttr);
    if (equalLettersIgnoringASCIICase(ariaOrientation, "horizontal"_s))
        return AccessibilityOrientation::Horizontal;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "vertical"_s))
        return AccessibilityOrientation::Vertical;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "undefined"_s))
        return AccessibilityOrientation::Undefined;

    return std::nullopt;
}

bool AccessibilityNodeObject::isBusy() const
{
    return elementAttributeValue(aria_busyAttr);
}

bool AccessibilityNodeObject::isFieldset() const
{
    return elementName() == ElementName::HTML_fieldset;
}

AccessibilityButtonState AccessibilityNodeObject::checkboxOrRadioValue() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()); input && (input->isCheckbox() || input->isRadioButton()))
        return input->indeterminate() && !input->isSwitch() ? AccessibilityButtonState::Mixed : isChecked() ? AccessibilityButtonState::On : AccessibilityButtonState::Off;

    return AccessibilityObject::checkboxOrRadioValue();
}

#if ENABLE(AX_THREAD_TEXT_APIS)
TextEmissionBehavior AccessibilityNodeObject::textEmissionBehavior() const
{
    RefPtr node = this->node();
    if (!node)
        return TextEmissionBehavior::None;

    if (is<HTMLParagraphElement>(*node)) {
        // TextIterator only emits a double-newline for paragraphs conditionally (see shouldEmitExtraNewlineForNode)
        // based on collapsed margin size. But the spec (https://html.spec.whatwg.org/multipage/dom.html#the-innertext-idl-attribute) says:
        //   > If node is a p element, then append 2 (a required line break count) at the beginning and end of items.
        // And Chrome seems to follow the spec: https://chromium.googlesource.com/chromium/src.git/+/8ff781cd5c1aabca068247de9a3f143645e80422
        // WebKit tried to make this change in TextIterator, but it was reverted:
        // https://github.com/WebKit/WebKit/commit/d206c2daf7219264b2c9b0cf0ee4cdce2450445b
        //
        // It's easier to unconditionally emit a double newline, so let's do that for now, since it's more spec-compliant anyways.
        return TextEmissionBehavior::DoubleNewline;
    }

    if (WebCore::shouldEmitNewlinesBeforeAndAfterNode(*node)) {
        if (is<RenderView>(renderer()) || is<HTMLHtmlElement>(*node)) {
            // Don't emit newlines for these objects. This is important because sometimes we start traversing
            // AXTextMarkers from the root, and want to do something for every object that emits a newline,
            // but there are no known cases where this is correct for these root elements.
            return TextEmissionBehavior::None;
        }
        return TextEmissionBehavior::Newline;
    }

    if (CheckedPtr cell = dynamicDowncast<RenderTableCell>(node->renderer()); cell && cell->nextCell()) {
        // https://html.spec.whatwg.org/multipage/dom.html#the-innertext-idl-attribute
        // > If node's computed value of 'display' is 'table-cell', and node's CSS box is not the last 'table-cell'
        // > box of its enclosing 'table-row' box, then append a string containing a single U+0009 TAB code point to items.
        return TextEmissionBehavior::Tab;
    }
    return TextEmissionBehavior::None;
}
#endif // ENABLE(AX_THREAD_TEXT_APIS)

Element* AccessibilityNodeObject::anchorElement() const
{
    RefPtr node = this->node();
    if (!node)
        return nullptr;

    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(*node))
        return areaElement.unsafeGet();

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    for ( ; node; node = node->parentNode()) {
        if (is<HTMLAnchorElement>(*node) || (node->renderer() && cache->getOrCreate(*node)->isLink()))
            return downcast<Element>(node).unsafeGet();
    }

    return nullptr;
}

RefPtr<Element> AccessibilityNodeObject::popoverTargetElement() const
{
    WeakPtr formControlElement = dynamicDowncast<HTMLFormControlElement>(node());
    return formControlElement ? formControlElement->popoverTargetElement() : nullptr;
}

RefPtr<Element> AccessibilityNodeObject::commandForElement() const
{
    RefPtr element = dynamicDowncast<HTMLButtonElement>(node());
    return element ? element->commandForElement() : nullptr;
}

CommandType AccessibilityNodeObject::commandType() const
{
    RefPtr element = dynamicDowncast<HTMLButtonElement>(node());
    return element ? element->commandType() : CommandType::Invalid;
}

AccessibilityObject* AccessibilityNodeObject::internalLinkElement() const
{
    // We don't currently support ARIA links as internal link elements, so exit early if anchorElement() is not a native HTMLAnchorElement.
    WeakPtr anchor = dynamicDowncast<HTMLAnchorElement>(anchorElement());
    if (!anchor)
        return nullptr;

    auto linkURL = anchor->href();
    auto fragmentIdentifier = linkURL.fragmentIdentifier();
    if (fragmentIdentifier.isEmpty())
        return nullptr;

    // Check if URL is the same as current URL
    RefPtr document = this->document();
    if (!document || !equalIgnoringFragmentIdentifier(document->url(), linkURL))
        return nullptr;

    RefPtr linkedNode = document->findAnchor(fragmentIdentifier);
    // The element we find may not be accessible, so find the first accessible object.
    return firstAccessibleObjectFromNode(linkedNode.get());
}

bool AccessibilityNodeObject::toggleDetailsAncestor()
{
    for (RefPtr node = this->node(); node; node = node->parentOrShadowHostNode()) {
        if (RefPtr details = dynamicDowncast<HTMLDetailsElement>(node)) {
            details->toggleOpen();
            return true;
        }
    }
    return false;
}

void AccessibilityNodeObject::revealAncestors()
{
    RefPtr node = this->node();
    if (!node)
        return;
    revealClosedDetailsAndHiddenUntilFoundAncestors(*node);
}

bool AccessibilityNodeObject::isHiddenUntilFoundContainer() const
{
    RefPtr element = dynamicDowncast<HTMLElement>(node());
    return element && element->isHiddenUntilFound();
}

static RefPtr<Element> nodeActionElement(Node& node)
{
    auto elementName = WebCore::elementName(node);
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node)) {
        if (!input->isDisabledFormControl() && (input->isRadioButton() || input->isCheckbox() || input->isTextButton() || input->isFileUpload() || input->isImageButton() || input->isTextField() || input->isDateField() || input->isDateTimeLocalField()))
            return input;
    } else if (elementName == ElementName::HTML_button || elementName == ElementName::HTML_select)
        return &downcast<Element>(node);

    // Content editable nodes should also be considered action elements, so they can accept presses.
    if (RefPtr element = dynamicDowncast<Element>(node)) {
        if (AccessibilityObject::contentEditableAttributeIsEnabled(*element))
            return element;
    }

    return nullptr;
}

static Element* nativeActionElement(Node* start)
{
    if (!start)
        return nullptr;

    // Do a deep-dive to see if any nodes should be used as the action element.
    // We have to look at Nodes, since this method should only be called on objects that do not have children (like buttons).
    // It solves the problem when authors put role="button" on a group and leave the actual button inside the group.

    for (RefPtr child = start->firstChild(); child; child = child->nextSibling()) {
        if (RefPtr element = nodeActionElement(*child))
            return element.unsafeGet();

        if (RefPtr subChild = nativeActionElement(child.get()))
            return subChild.unsafeGet();
    }
    return nullptr;
}

Element* AccessibilityNodeObject::actionElement() const
{
    RefPtr node = this->node();
    if (!node)
        return nullptr;

    if (RefPtr element = nodeActionElement(*node))
        return element.unsafeGet();

    if (AccessibilityObject::isARIAInput(ariaRoleAttribute()))
        return downcast<Element>(node).unsafeGet();

    switch (role()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::Tab:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::ListItem:
        // Check if the author is hiding the real control element inside the ARIA element.
        if (RefPtr nativeElement = nativeActionElement(node.get()))
            return nativeElement.unsafeGet();
        return downcast<Element>(node).unsafeGet();
    default:
        break;
    }

    if (RefPtr element = anchorElement())
        return element.unsafeGet();

    if (RefPtr clickableObject = this->clickableSelfOrAncestor())
        return clickableObject->element();

    return nullptr;
}

bool AccessibilityNodeObject::hasClickHandler() const
{
    RefPtr element = this->element();
    return element && element->hasAnyEventListeners({ eventNames().clickEvent, eventNames().mousedownEvent, eventNames().mouseupEvent });
}

bool AccessibilityNodeObject::showsCursorOnHover() const
{
    // Perform role-based checks first to determine if this heuristic applies, as they
    // are non-virtual and thus fast.
    if (isImplicitlyInteractive()) {
        // If there's an implicitly interactive element in the ancestry, we won't expose
        // AXCoreObject::supportsPressAction, so don't bother doing any work.
        return false;
    }

    if (isStaticText()) {
        // Fast-path (non-virtual) exit for static text, which inherently
        // cannot be a target of any :hover CSS rule.
        return false;
    }

    CheckedPtr renderer = this->renderer();
    if (!renderer || !renderer->hasVisibleBoxDecorations() || renderer->isPseudoElement()) {
        // Only consider rendered, non-pseudo-elements objects with visible box decorations.
        return false;
    }

    RefPtr element = this->element();
    if (!element) {
        // To be a target of a :hover CSS rule, this must be an element.
        return false;
    }

    auto* box = dynamicDowncast<RenderBox>(*renderer);
    if (box && (box->hasScrollableOverflowX() || box->hasScrollableOverflowY())) {
        // This heuristic is not valid for scrollable boxes.
        return false;
    }

    if (hasCursorPointer()) {
        // If something already has a cursor pointer, this heuristic is not needed,
        // since the intent is to check if something would have a cursor indicating
        // interactivity if it were hovered.
        return false;
    }

    if (hasPointerEventsNone()) {
        // pointer-events:none means this cannot possibly have an interactive cursor, so exit.
        return false;
    }

    // If we've made it all the way here, time do the the potentially expensive part: check for
    // a matching :hover style rule that would apply cursor:pointer.
    bool initialHoveredState = element->isUserActionElement() && element->document().userActionElements().isHovered(*element);
    auto scopeExit = makeScopeExit([&element, &initialHoveredState] {
        element->document().userActionElements().setHovered(*element, initialHoveredState);
    });

    for (auto key : Style::makePseudoClassInvalidationKeys(CSSSelector::PseudoClass::Hover, *element)) {
        auto& ruleSets = element->styleResolver().ruleSets();
        auto* invalidationRuleSets = ruleSets.pseudoClassInvalidationRuleSets(key);
        if (!invalidationRuleSets)
            continue;

        for (auto& invalidationRuleSet : *invalidationRuleSets) {
            element->document().userActionElements().setHovered(*element, invalidationRuleSet.isNegation == Style::IsNegation::No);

            Style::ElementRuleCollector ruleCollector(*element, *invalidationRuleSet.ruleSet, nullptr, SelectorChecker::Mode::StyleInvalidation);
            ruleCollector.matchAuthorRules();
            Ref matchResult = ruleCollector.releaseMatchResult();

            for (const auto& matchedProperties : matchResult->authorDeclarations) {
                if (std::optional cursorType = cursorTypeFrom(matchedProperties.properties))
                    return *cursorType == CursorType::Pointer;
            }
        }
    }
    return false;
}

bool AccessibilityNodeObject::hasPointerEventsNone() const
{
    CheckedPtr style = this->style();
    return style && style->pointerEvents() == PointerEvents::None;
}

bool AccessibilityNodeObject::isDescendantOfBarrenParent() const
{
    if (!m_isIgnoredFromParentData.isNull())
        return m_isIgnoredFromParentData.isDescendantOfBarrenParent;

    for (RefPtr object = parentObject(); object; object = object->parentObject()) {
        if (!object->canHaveChildren())
            return true;
    }

    return false;
}

void AccessibilityNodeObject::alterRangeValue(StepAction stepAction)
{
    if (role() != AccessibilityRole::Slider && role() != AccessibilityRole::SpinButton)
        return;

    RefPtr element = this->element();
    if (!element || element->isDisabledFormControl())
        return;

#if PLATFORM(COCOA)
    if (role() == AccessibilityRole::SpinButton) {
        // First try a keyboard event to see if that affects the value of the spinbutton, since authors are supposed to handle up/down keys.
        // We can't check the event listeners directly here since those may be on an ancestor or the document.
        float beforeValue = valueForRange();
        postKeyboardKeysForValueChange(stepAction);
        if (beforeValue != valueForRange()) {
            // Performing a keyboard action (up/down arrow) resulted in a value change, so return early to avoid doubly-modifing the value.
            return;
        }
    }
#endif

    if (!getAttribute(stepAttr).isEmpty())
        changeValueByStep(stepAction);
    else
        changeValueByPercent(stepAction == StepAction::Increment ? 5 : -5);
}

void AccessibilityNodeObject::increment()
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document());
#if PLATFORM(IOS_FAMILY)
    if (RefPtr mediaElement = this->mediaElement()) {
        AccessibilityMediaHelpers::increment(*mediaElement);
        return;
    }
#endif

    alterRangeValue(StepAction::Increment);
}

void AccessibilityNodeObject::decrement()
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document());
#if PLATFORM(IOS_FAMILY)
    if (RefPtr mediaElement = this->mediaElement()) {
        AccessibilityMediaHelpers::decrement(*mediaElement);
        return;
    }
#endif

    alterRangeValue(StepAction::Decrement);
}

static bool dispatchSimulatedKeyboardUpDownEvent(AccessibilityObject* object, const KeyboardEvent::Init& keyInit)
{
    // In case the keyboard event causes this element to be removed.
    Ref<AccessibilityObject> protectedObject(*object);

    bool handled = false;
    if (auto* node = object->node()) {
        auto event = KeyboardEvent::create(eventNames().keydownEvent, keyInit, Event::IsTrusted::Yes);
        node->dispatchEvent(event);
        handled |= event->defaultHandled(); // The browser handled it.
        handled |= event->defaultPrevented(); // A JavaScript event listener handled it.
    }

    // Ensure node is still valid and wasn't removed after the keydown.
    if (auto* node = object->node()) {
        auto event = KeyboardEvent::create(eventNames().keyupEvent, keyInit, Event::IsTrusted::Yes);
        node->dispatchEvent(event);
        handled |= event->defaultHandled(); // The browser handled it.
        handled |= event->defaultPrevented(); // A JavaScript event listener handled it.
    }
    return handled;
}

static void InitializeLegacyKeyInitProperties(KeyboardEvent::Init &keyInit, const AccessibilityObject& object)
{
    keyInit.which = keyInit.keyCode;
    keyInit.code = keyInit.key;

    keyInit.view = object.document()->windowProxy();
    keyInit.cancelable = true;
    keyInit.composed = true;
    keyInit.bubbles = true;
}

bool AccessibilityNodeObject::performDismissAction()
{
    auto keyInit = KeyboardEvent::Init();
    keyInit.key = "Escape"_s;
    keyInit.keyCode = 0x1b;
    keyInit.keyIdentifier = "U+001B"_s;
    InitializeLegacyKeyInitProperties(keyInit, *this);

    return dispatchSimulatedKeyboardUpDownEvent(this, keyInit);
}

// Fire a keyboard event if we were not able to set this value natively.
bool AccessibilityNodeObject::postKeyboardKeysForValueChange(StepAction stepAction)
{
    auto keyInit = KeyboardEvent::Init();
    bool isLTR = page()->userInterfaceLayoutDirection() == UserInterfaceLayoutDirection::LTR;
    // https://w3c.github.io/aria/#spinbutton
    // `spinbutton` elements don't have an implicit orientation, but the spec does say:
    //     > Authors SHOULD also ensure the up and down arrows on a keyboard perform the increment and decrement functions
    // So let's force a vertical orientation for `spinbutton`s so we simulate the correct keypress (either up or down).
    bool vertical = orientation() == AccessibilityOrientation::Vertical || role() == AccessibilityRole::SpinButton;

    // The goal is to mimic existing keyboard dispatch completely, so that this is indistinguishable from a real key press.
    typedef enum { left = 37, up = 38, right = 39, down = 40 } keyCode;
    keyInit.key = stepAction == StepAction::Increment ? (vertical ? "ArrowUp"_s : (isLTR ? "ArrowRight"_s : "ArrowLeft"_s)) : (vertical ? "ArrowDown"_s : (isLTR ? "ArrowLeft"_s : "ArrowRight"_s));
    keyInit.keyCode = stepAction == StepAction::Increment ? (vertical ? keyCode::up : (isLTR ? keyCode::right : keyCode::left)) : (vertical ? keyCode::down : (isLTR ? keyCode::left : keyCode::right));
    keyInit.keyIdentifier = stepAction == StepAction::Increment ? (vertical ? "Up"_s : (isLTR ? "Right"_s : "Left"_s)) : (vertical ? "Down"_s : (isLTR ? "Left"_s : "Right"_s));

    InitializeLegacyKeyInitProperties(keyInit, *this);

    return dispatchSimulatedKeyboardUpDownEvent(this, keyInit);
}

void AccessibilityNodeObject::setNodeValue(StepAction stepAction, float value)
{
    bool didSet = setValue(String::number(value));

    if (didSet) {
        if (auto* cache = axObjectCache())
            cache->postNotification(this, document(), AXNotification::ValueChanged);
    } else
        postKeyboardKeysForValueChange(stepAction);
}

void AccessibilityNodeObject::changeValueByStep(StepAction stepAction)
{
    float step = stepValueForRange();
    float value = valueForRange();

    value += stepAction == StepAction::Increment ? step : -step;
    setNodeValue(stepAction, value);
}

void AccessibilityNodeObject::changeValueByPercent(float percentChange)
{
    if (!percentChange)
        return;

    float range = maxValueForRange() - minValueForRange();
    float step = range * (percentChange / 100);
    float value = valueForRange();

    // Make sure the specified percent will cause a change of one integer step or larger.
    if (std::abs(step) < 1)
        step = std::abs(percentChange) * (1 / percentChange);

    value += step;
    setNodeValue(percentChange > 0 ? StepAction::Increment : StepAction::Decrement, value);
}

bool AccessibilityNodeObject::elementAttributeValue(const QualifiedName& attributeName) const
{
    return equalLettersIgnoringASCIICase(getAttribute(attributeName), "true"_s);
}

bool AccessibilityNodeObject::liveRegionAtomic() const
{
    const auto& atomic = getAttribute(aria_atomicAttr);
    if (equalLettersIgnoringASCIICase(atomic, "true"_s))
        return true;
    if (equalLettersIgnoringASCIICase(atomic, "false"_s))
        return false;

    // WAI-ARIA "alert" and "status" roles have an implicit aria-atomic value of true.
    switch (role()) {
    case AccessibilityRole::ApplicationAlert:
    case AccessibilityRole::ApplicationStatus:
        return true;
    default:
        return false;
    }
}

// This function is like a cross-platform version of - (WebCoreTextMarkerRange*)textMarkerRange. It returns
// a Range that we can convert to a WebCoreTextMarkerRange in the Obj-C file
VisiblePositionRange AccessibilityNodeObject::visiblePositionRange() const
{
    RefPtr node = this->node();
    if (!node)
        return VisiblePositionRange();

    VisiblePosition startPos = firstPositionInOrBeforeNode(node.get());
    VisiblePosition endPos = lastPositionInOrAfterNode(node.get());

    // the VisiblePositions are equal for nodes like buttons, so adjust for that
    // FIXME: Really?  [button, 0] and [button, 1] are distinct (before and after the button)
    // I expect this code is only hit for things like empty divs? In which case I don't think
    // the behavior is correct here -- eseidel
    if (startPos == endPos) {
        endPos = endPos.next();
        if (endPos.isNull())
            endPos = startPos;
    }

    return { WTFMove(startPos), WTFMove(endPos) };
}

VisiblePositionRange AccessibilityNodeObject::selectedVisiblePositionRange() const
{
    RefPtr document = this->document();
    if (RefPtr localFrame = document ? document->frame() : nullptr) {
        if (auto selection = localFrame->selection().selection(); !selection.isNone())
            return selection;
    }
    return { };
}

int AccessibilityNodeObject::indexForVisiblePosition(const VisiblePosition& position) const
{
    RefPtr node = this->node();
    if (!node)
        return 0;
    // We need to consider replaced elements for GTK, as they will be
    // presented with the 'object replacement character' (0xFFFC).
    TextIteratorBehaviors behaviors;
#if USE(ATSPI)
    behaviors.add(TextIteratorBehavior::EmitsObjectReplacementCharacters);
#endif
    return WebCore::indexForVisiblePosition(*node, position, behaviors);
}

VisiblePosition AccessibilityNodeObject::visiblePositionForIndex(int index) const
{
    RefPtr node = this->node();
    if (!node)
        return { };
#if USE(ATSPI)
    // We need to consider replaced elements for GTK, as they will be presented with the 'object replacement character' (0xFFFC).
    return WebCore::visiblePositionForIndex(index, node.get(), TextIteratorBehavior::EmitsObjectReplacementCharacters);
#else
    return visiblePositionForIndexUsingCharacterIterator(*node, index);
#endif
}

VisiblePositionRange AccessibilityNodeObject::visiblePositionRangeForLine(unsigned lineCount) const
{
    if (!lineCount)
        return { };

    RefPtr document = this->document();
    auto* renderView = document ? document->renderView() : nullptr;
    if (!renderView)
        return { };

    // iterate over the lines
    // FIXME: This is wrong when lineNumber is lineCount+1, because nextLinePosition takes you to the last offset of the last line.
    auto position = renderView->visiblePositionForPoint(IntPoint(), HitTestSource::User);
    while (--lineCount) {
        auto previousLinePosition = position;
        position = nextLinePosition(position, 0);
        if (position.isNull() || position == previousLinePosition)
            return VisiblePositionRange();
    }

    // make a caret selection for the marker position, then extend it to the line
    // NOTE: Ignores results of sel.modify because it returns false when starting at an empty line.
    // The resulting selection in that case will be a caret at position.
    auto selection = makeUniqueRef<FrameSelection>();
    selection->setSelection(position);
    selection->modify(FrameSelection::Alteration::Extend, SelectionDirection::Right, TextGranularity::LineBoundary);
    return selection->selection();
}

bool AccessibilityNodeObject::isGenericFocusableElement() const
{
    if (!canSetFocusAttribute())
        return false;

    // If it's a control, it's not generic.
    if (isControl())
        return false;

    auto role = this->role();
    if (role == AccessibilityRole::Video || role == AccessibilityRole::Audio)
        return false;

    // If it has an aria role, it's not generic.
    if (m_ariaRole != AccessibilityRole::Unknown)
        return false;

    // If the content editable attribute is set on this element, that's the reason
    // it's focusable, and existing logic should handle this case already - so it's not a
    // generic focusable element.

    if (hasContentEditableAttributeSet())
        return false;

    // The web area and body element are both focusable, but existing logic handles these
    // cases already, so we don't need to include them here.
    if (role == AccessibilityRole::WebArea)
        return false;
    if (elementName() == ElementName::HTML_body)
        return false;

    // An SVG root is focusable by default, but it's probably not interactive, so don't
    // include it. It can still be made accessible by giving it an ARIA role.
    if (role == AccessibilityRole::SVGRoot)
        return false;

    return true;
}

AccessibilityObject* AccessibilityNodeObject::cellForColumnAndRow(unsigned column, unsigned row)
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    if (!rareData)
        return nullptr;
    auto& cellSlots = rareData->cellSlots();

    if (row >= cellSlots.size() || column >= cellSlots[row].size())
        return nullptr;

    if (Markable cellID = cellSlots[row][column]) {
        CheckedPtr cache = axObjectCache();
        return cache ? cache->objectForID(*cellID) : nullptr;
    }
    return nullptr;
}

AXObjectRareData* AccessibilityNodeObject::rareDataWithCleanTableChildren()
{
    if (!isTable())
        return nullptr;
    updateChildrenIfNecessary();
    return rareData();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::cells()
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    if (!rareData)
        return { };

    AXCoreObject::AccessibilityChildrenVector cells;
    // row * columns may not be exactly correct when considering things
    // like rowspan / colspan, but it should be close enough.
    cells.reserveInitialCapacity(rareData->rowCount() * rareData->columnCount());
    for (const auto& row : rareData->tableRows())
        cells.appendVector(row->unignoredChildren());
    return cells;
}

unsigned AccessibilityNodeObject::columnCount()
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    return rareData ? rareData->columnCount() : 0;
}

unsigned AccessibilityNodeObject::rowCount()
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    return rareData ? rareData->rowCount() : 0;
}

Vector<Vector<Markable<AXID>>> AccessibilityNodeObject::cellSlots()
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    return rareData ? rareData->cellSlots() : Vector<Vector<Markable<AXID>>>();
}

int AccessibilityNodeObject::axRowCount() const
{
    if (!isTable())
        return 0;

    int rowCountInt = integralAttribute(aria_rowcountAttr);
    // The ARIA spec states, "Authors must set the value of aria-rowcount to an integer equal to the
    // number of rows in the full table. If the total number of rows is unknown, authors must set
    // the value of aria-rowcount to -1 to indicate that the value should not be calculated by the
    // user agent." If we have a valid value, make it available to platforms.
    if (rowCountInt == -1 || rowCountInt >= (int)const_cast<AccessibilityNodeObject*>(this)->rowCount())
        return rowCountInt;
    return 0;
}

int AccessibilityNodeObject::axColumnCount() const
{
    if (!isTable())
        return 0;

    int colCountInt = integralAttribute(aria_colcountAttr);
    // The ARIA spec states, "Authors must set the value of aria-colcount to an integer equal to the
    // number of columns in the full table. If the total number of columns is unknown, authors must
    // set the value of aria-colcount to -1 to indicate that the value should not be calculated by
    // the user agent." If we have a valid value, make it available to platforms.
    if (colCountInt == -1 || colCountInt >= (int)const_cast<AccessibilityNodeObject*>(this)->columnCount())
        return colCountInt;
    return 0;
}

void AccessibilityNodeObject::updateRowDescendantRoles()
{
    CheckedPtr rareData = isTable() ? this->rareData() : nullptr;
    if (!rareData)
        return;

    for (const auto& row : rareData->tableRows()) {
        downcast<AccessibilityObject>(row.get()).updateRole();
        for (const auto& cell : row->unignoredChildren())
            downcast<AccessibilityObject>(cell.get()).updateRole();
    }
}

void AccessibilityNodeObject::setCellSlotsDirty()
{
    if (!isTable())
        return;

    // Because the cell-slots grid is (necessarily) computed in conjunction with children, mark
    // the children as dirty by clearing them.
    //
    // It's necessary to compute the cell-slots grid together with children because they are both
    // influenced by the same factors. For example, if `setCellSlotsDirty` is called because
    // a child increased in column span, that may also result in more column children being
    // added if that column span change increased the "width" of the table.
    clearChildren();
}

AccessibilityObject* AccessibilityNodeObject::tableHeaderContainer()
{
    CheckedPtr rareData = rareDataWithCleanTableChildren();
    if (!rareData)
        return nullptr;

    if (auto* headerContainer = rareData->tableHeaderContainer())
        return headerContainer;

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return nullptr;

    Ref tableHeader = downcast<AccessibilityMockObject>(*cache->create(AccessibilityRole::TableHeaderContainer));
    tableHeader->setParent(this);
    rareData->setTableHeaderContainer(tableHeader.get());

    return tableHeader.unsafePtr();
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::columns()
{
    if (CheckedPtr rareData = rareDataWithCleanTableChildren())
        return rareData->tableColumns();
    return { };
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::rows()
{
    if (CheckedPtr rareData = rareDataWithCleanTableChildren())
        return rareData->tableRows();
    return { };
}

// The following is a heuristic used to determine if a <table> should be exposed as an AXTable.
// The goal is to only show "data" tables.
bool AccessibilityNodeObject::isDataTable() const
{
    WeakPtr cache = axObjectCache();
    if (!cache)
        return false;

    auto ariaRole = ariaRoleAttribute();
    if (!AXTableHelpers::isTableRole(ariaRole) && ariaRole != AccessibilityRole::Unknown) {
        // Do not consider it a data table if it has a non-table ARIA role.
        return false;
    }

    // When a section of the document is contentEditable, all tables should be
    // treated as data tables, otherwise users may not be able to work with rich
    // text editors that allow creating and editing tables.
    if (node() && node()->hasEditableStyle())
        return true;

    if (RefPtr tableElement = AXTableHelpers::tableElementIncludingAncestors(node(), renderer())) {
        if (AXTableHelpers::tableElementIndicatesAccessibleTable(*tableElement))
            return true;
    }

    RefPtr table = dynamicDowncast<HTMLTableElement>(node());
    // The following checks should only apply if this is a real <table> element.
    if (!table)
        return false;

    // If the author has used ARIA to specify a valid column or row count, assume they
    // want us to treat the table as a data table.
    auto ariaRowOrColCountIsSet = [this] (const QualifiedName& attribute) {
        int result = integralAttribute(attribute);
        return result == -1 || result > 0;
    };
    if (ariaRowOrColCountIsSet(aria_colcountAttr) || ariaRowOrColCountIsSet(aria_rowcountAttr))
        return true;

    return AXTableHelpers::isDataTableWithTraversal(*table, *cache);
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::rowHeaders()
{
    AccessibilityChildrenVector headers;

    if (isTableRow() || isTable()) {
        auto rowsCopy = rows();
        for (const auto& row : rowsCopy) {
            if (RefPtr header = row->rowHeader())
                headers.append(header.releaseNonNull());
        }
    } else if (isTableCell()) {
        RefPtr parent = parentTable();
        if (!parent)
            return headers;

        auto rowRange = rowIndexRange();
        auto columnRange = columnIndexRange();

        for (unsigned column = 0; column < columnRange.first; column++) {
            RefPtr tableCell = parent->cellForColumnAndRow(column, rowRange.first);
            if (!tableCell || tableCell == this || headers.containsIf([&tableCell] (const auto& header) {
                return header.ptr() == tableCell.get();
            }))
                continue;

            if (tableCell->cellScope() == "rowgroup"_s && isTableCellInSameRowGroup(*tableCell))
                headers.append(tableCell.releaseNonNull());
            else if (tableCell->isRowHeader())
                headers.append(tableCell.releaseNonNull());
        }
    }

    return headers;
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::visibleRows()
{
    auto rows = this->rows();
    rows.removeAllMatching([] (const auto& row) {
        return row->isOffScreen();
    });
    return rows;
}

void AccessibilityNodeObject::addTableChildrenAndCellSlots()
{
    // isExposableTable() should've been checked before this method was even called.
    ASSERT(isExposableTable());

    if (!isExposableTable()) [[unlikely]]
        return;

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return;
    unsigned desiredColumnCount = computeCellSlots();

    CheckedRef rareData = ensureRareData();
    for (unsigned i = 0; i < desiredColumnCount; ++i) {
        Ref column = downcast<AccessibilityTableColumn>(*cache->create(AccessibilityRole::Column));
        column->setColumnIndex(i);
        column->setParent(this);
        rareData->appendColumn(column.get());
        addChild(column.get(), DescendIfIgnored::No);
    }
    addChild(tableHeaderContainer(), DescendIfIgnored::No);

    m_subtreeDirty = false;
    // Sometimes the cell gets the wrong role initially because it is created before the parent
    // determines whether it is an accessibility table. Iterate all the cells and allow them to
    // update their roles now that the table knows its status.
    // see bug: https://bugs.webkit.org/show_bug.cgi?id=147001
    updateRowDescendantRoles();
}

// Returns the number of columns the table should have.
unsigned AccessibilityNodeObject::computeCellSlots()
{
    if (!isExposableTable())
        return 0;
    WeakPtr cache = axObjectCache();
    if (!cache)
        return 0;

    RefPtr protectedThis = this;
    CheckedRef rareData = ensureRareData();
    auto& cellSlots = rareData->mutableCellSlots();
    auto ensureRowAndColumn = [&] (unsigned rowIndex, unsigned columnIndex) {
        if (cellSlots.size() < rowIndex + 1)
            cellSlots.grow(rowIndex + 1);

        if (cellSlots[rowIndex].size() < columnIndex + 1)
            cellSlots[rowIndex].grow(columnIndex + 1);
    };

    // This function implements the "forming a table" algorithm for determining
    // the correct cell positions and spans (and storing those in m_cellSlots for later use).
    // https://html.spec.whatwg.org/multipage/tables.html#forming-a-table

    // Step 1.
    unsigned xWidth = 0;
    // Step 2.
    unsigned yHeight = 0;
    // Step 3: Let pending tfoot elements be a list of tfoot elements, initially empty.
    Vector<Ref<Element>> pendingTfootElements;
    // Step 10.
    unsigned yCurrent = 0;
#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
    bool didAddCaption = false;
#endif

    struct DownwardGrowingCell {
        WeakRef<AccessibilityRenderObject> axObject;
        // The column the cell starts in.
        unsigned x;
        // The number of columns the cell spans (called "width" in the spec).
        unsigned colSpan;
        unsigned remainingRowsToSpan;
    };
    Vector<DownwardGrowingCell> downwardGrowingCells;

    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-growing-downward-growing-cells
    auto growDownwardsCells = [&] () {
        // ...for growing downward-growing cells, the user agent must, for each {cell, cellX, width} tuple in the list
        // of downward-growing cells, extend the cell so that it also covers the slots with coordinates (x, yCurrent), where cellX ≤ x < cellX+width.
        for (auto& cell : downwardGrowingCells) {
            if (!cell.remainingRowsToSpan)
                continue;
            --cell.remainingRowsToSpan;
            cell.axObject->incrementEffectiveRowSpan();

            for (unsigned column = cell.x; column < cell.x + cell.colSpan; column++) {
                ensureRowAndColumn(yCurrent, column);
                cellSlots[yCurrent][column] = cell.axObject->objectID();
            }
        }
    };

    HashSet<AccessibilityObject*> processedRows;
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-processing-rows
    auto processRow = [&] (AccessibilityRenderObject* row) {
        if (!row || processedRows.contains(row))
            return;
        processedRows.add(row);

        if (row->role() != AccessibilityRole::Unknown && row->isIgnored()) {
            // Skip ignored rows (except for those ignored because they have an unknown role, which will happen after a table has become un-exposed but is potentially becoming re-exposed).
            // This is an addition on top of the HTML algorithm because the computed AX table has extra restrictions (e.g. cannot contain aria-hidden or role="presentation" rows).
            return;
        }

        // Step 1: If yheight is equal to ycurrent, then increase yheight by 1. (ycurrent must never be greater than yheight.)
        if (yHeight <= yCurrent)
            yHeight = yCurrent + 1;

        // Step 2.
        unsigned xCurrent = 0;
        // Step 3: Run the algorithm for growing downward-growing cells.
        growDownwardsCells();

        // Step 4: If the tr element being processed has no td or th element children, then increase ycurrent by 1, abort this set of steps, and return to the algorithm above.
        for (const auto& child : row->unignoredChildren()) {
            RefPtr currentCell = dynamicDowncast<AccessibilityRenderObject>(child.get());
            if (!currentCell || !currentCell->isTableCell())
                continue;
            // (Not specified): As part of beginning to process this cell, reset its effective rowspan in case it had a non-default value set from a previous call to AccessibilityTable::addChildren().
            currentCell->resetEffectiveRowSpan();

            // Step 6: While the slot with coordinate (xcurrent, ycurrent) already has a cell assigned to it, increase xcurrent by 1.
            ensureRowAndColumn(yCurrent, xCurrent);
            while (cellSlots[yCurrent][xCurrent]) {
                xCurrent += 1;
                ensureRowAndColumn(yCurrent, xCurrent);
            }
            // Step 7: If xcurrent is equal to xwidth, increase xwidth by 1. (xcurrent is never greater than xwidth.)
            if (xCurrent >= xWidth)
                xWidth = xCurrent + 1;
            // Step 8: If the current cell has a colspan attribute, then parse that attribute's value, and let colspan be the result.
            unsigned colSpan = currentCell->colSpan();
            // Step 9: If the current cell has a rowspan attribute, then parse that attribute's value, and let rowspan be the result.
            unsigned rowSpan = currentCell->rowSpan();

            // Step 10: If rowspan is zero and the table element's node document is not set to quirks mode, then let
            // cell grows downward be true, and set rowspan to 1. Otherwise, let cell grows downward be false.
            // NOTE: We intentionally don't implement this step because the rendering code doesn't, so implementing it
            // would cause AX to not match the visual state of the page.

            // Step 11: If xwidth < xcurrent+colspan, then let xwidth be xcurrent+colspan.
            if (xWidth < xCurrent + colSpan)
                xWidth = xCurrent + colSpan;

            // Step 12: If yheight < ycurrent+rowspan, then let yheight be ycurrent+rowspan.
            // NOTE: An explicit choice is made not to follow this part of the spec, because rowspan
            // can be some arbitrarily large number (up to 65535) that will not actually reflect how
            // many rows the cell spans in the final table. Taking it as-provided will cause incorrect
            // results in many scenarios. Instead, only check for yHeight < yCurrent.
            if (yHeight < yCurrent)
                yHeight = yCurrent;

            // Step 13: Let the slots with coordinates (x, y) such that xcurrent ≤ x < xcurrent+colspan and
            // ycurrent ≤ y < ycurrent+rowspan be covered by a new cell c, anchored at (xcurrent, ycurrent),
            // which has width colspan and height rowspan, corresponding to the current cell element.
            // NOTE: We don't implement this exactly, instead using the downward-growing cell algorithm to accurately
            // handle rowspan cells. This makes it easy to avoid extending cells outside their rowgroup.
            currentCell->setRowIndex(yCurrent);
            currentCell->setColumnIndex(xCurrent);
            for (unsigned x = xCurrent; x < xCurrent + colSpan; x++) {
                ensureRowAndColumn(yCurrent, x);
                cellSlots[yCurrent][x] = currentCell->objectID();
            }

            // Step 14: If cell grows downward is true, then add the tuple {c, xcurrent, colspan} to the
            // list of downward-growing cells.
            // NOTE: We use the downward-growing cell algorithm to expand rowspanned cells.
            if (rowSpan > 1) {
                downwardGrowingCells.append({
                    *currentCell,
                    xCurrent,
                    colSpan,
                    rowSpan - 1
                });
            } else if (!rowSpan) {
                // Zero is a special value for rowspan that means it spans all remaining rows.
                // Pass the max rowspan value for DownwardGrowingCell::remainingRowsToSpan, allowing
                // this cell to span for as long as the table extends.
                downwardGrowingCells.append({
                    *currentCell,
                    xCurrent,
                    colSpan,
                    HTMLTableCellElement::maxRowspan - yCurrent
                });
            }

            // Step 15.
            xCurrent += colSpan;

            // Step 16 handled below.
            // Step 17 and 18: Let current cell be the next td or th element child in the tr element being processed. (This is implemented by allowing the loop to continue above).
        }

        // Not specified: update some internal data structures.
        rareData->appendRow(*row);
        row->setRowIndex(yCurrent);
#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        addChild(*row);
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

        // Step 16: If current cell is the last td or th element child in the tr element being processed, then increase ycurrent by 1, abort this set of steps, and return to the algorithm above.
        yCurrent += 1;
    };
    auto needsToDescend = [&processedRows] (AXCoreObject& axObject) {
        return !axObject.isTableRow() && !processedRows.contains(&downcast<AccessibilityObject>(axObject));
    };
    std::function<void(AXCoreObject&)> processRowDescendingIfNeeded = [&] (AXCoreObject& axObject) {
        // Descend past anonymous renderers and non-rows.
        if (needsToDescend(axObject)) {
            for (const auto& child : axObject.unignoredChildren())
                processRowDescendingIfNeeded(child.get());
        } else if (axObject.isTableRow())
            processRow(dynamicDowncast<AccessibilityRenderObject>(axObject));
    };
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-ending-a-row-group
    auto endRowGroup = [&] () {
        // 1. While yCurrent is less than yHeight, follow these steps:
        while (yCurrent < yHeight) {
            // 1a. Run the algorithm for growing downward-growing cells.
            growDownwardsCells();
            // 1b. Increase yCurrent by 1.
            ++yCurrent;
        }
        // 2. Empty the list of downward-growing cells.
        downwardGrowingCells.clear();
    };
    // https://html.spec.whatwg.org/multipage/tables.html#algorithm-for-processing-row-groups
    auto processRowGroup = [&] (Element& sectionElement) {
        // Step 1: Let ystart have the value of yheight. Not implemented because it's only useful for step 3, which we skip.

        // Step 2: For each tr element that is a child of the element being processed,
        // in tree order, run the algorithm for processing rows.
        if (RefPtr tableSection = dynamicDowncast<HTMLTableSectionElement>(sectionElement)) {
            for (Ref row : childrenOfType<HTMLTableRowElement>(*tableSection)) {
                if (RefPtr tableRow = cache->getOrCreate(row.get()); tableRow && tableRow->isTableRow())
                    processRow(dynamicDowncast<AccessibilityRenderObject>(tableRow).get());
            }
        } else if (RefPtr sectionAxObject = cache->getOrCreate(sectionElement)) {
            ASSERT_WITH_MESSAGE(hasRole(sectionElement, "rowgroup"_s), "processRowGroup should only be called with native table section elements, or role=rowgroup elements");
            for (const auto& child : sectionAxObject->unignoredChildren())
                processRowDescendingIfNeeded(child.get());
        }
        // Step 3: If yheight > ystart, then let all the last rows in the table from y=ystart to y=yheight-1
        // form a new row group, anchored at the slot with coordinate (0, ystart), with height yheight-ystart,
        // corresponding to the element being processed. Not implemented.

        // Step 4: Run the algorithm for ending a row group.
        endRowGroup();
    };

    // Step 4: Let the table be the table represented by the table element.
    RefPtr tableElement = this->node();
    // `isAriaTable()` will return true for table-like ARIA structures (grid, treegrid, table).
    if (!is<HTMLTableElement>(tableElement.get()) && !isAriaTable())
        return 0;

    bool withinImplicitRowGroup = false;
    std::function<void(Node*)> processTableDescendant = [&] (Node* node) {
        auto* element = dynamicDowncast<Element>(node);
        // Step 8: While the current element is not one of the following elements, advance the
        // current element to the next child of the table.
        bool descendantIsRow = element && (element->elementName() == ElementName::HTML_tr || hasRole(*element, "row"_s));
        bool descendantIsRowGroup = !descendantIsRow && element && isRowGroup(*element);

#if !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)
        // Not needed for ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE) because we add captions via AccessibilityRenderObject::addChildren().
        if (auto* caption = dynamicDowncast<HTMLTableCaptionElement>(element)) {
            // Step 6: Associate the first caption element child of the table element with the table.
            if (!didAddCaption) {
                if (RefPtr axCaption = cache->getOrCreate(*caption)) {
                    addChild(*axCaption, DescendIfIgnored::No);
                    didAddCaption = true;
                }
            }
            return;
        }
#endif // !ENABLE(INCLUDE_IGNORED_IN_CORE_AX_TREE)

        if (descendantIsRowGroup)
            withinImplicitRowGroup = false;
        else {
            // (Not specified): For ARIA tables, we need to track implicit rowgroups (allowed by the ARIA spec)
            // in order to properly perform the downward-growing cell algorithm.
            withinImplicitRowGroup = protectedThis->isAriaTable();
        }

        // Step 9: Handle the colgroup element. Not implemented.
        // Step 10: Handled above.
        // Step 11: Let the list of downward-growing cells be an empty list.
        if (!withinImplicitRowGroup)
            downwardGrowingCells.clear();
        // Step 12: While the current element is not one of the following elements, advance the current element to the next child of the table
        if (!descendantIsRow && !descendantIsRowGroup) {
            if (isAriaTable()) {
                // We are forgiving with ARIA grid markup, descending past disallowed elements to build the grid structure (this is not specified, but consistent with other browsers).
                if (RefPtr axObject = cache->getOrCreate(node); axObject && needsToDescend(*axObject)) {
                    for (const auto& child : axObject->childrenIncludingIgnored())
                        processTableDescendant(child->node());
                }
            }
            return;
        }

        // Step 13: If the current element is a tr, then run the algorithm for processing rows,
        // advance the current element to the next child of the table, and return to the step labeled rows.
        if (descendantIsRow)
            processRow(dynamicDowncast<AccessibilityRenderObject>(cache->getOrCreate(element)));

        // Step 14: Run the algorithm for ending a row group.
        if (!withinImplicitRowGroup)
            endRowGroup();

        // Step 15: If the current element is a tfoot...
        if (element->elementName() == ElementName::HTML_tfoot) {
            // ...then add that element to the list of pending tfoot elements
            pendingTfootElements.append(*element);
            // ...advance the current element to the next child of the table.
            return;
        }

        // Step 16: If the current element is either a thead or a tbody, run the algorithm for processing row groups. (Not specified: include role="rowgroups").
        if (descendantIsRowGroup)
            processRowGroup(*element);
    };
    // Step 7: Let the current element be the first element child of the table element.
    for (RefPtr currentElement = tableElement->firstChild(); currentElement; currentElement = currentElement->nextSibling()) {
        processTableDescendant(currentElement.get());
        // Step 17 + 18: Advance the current element to the next child of the table.
    }

    // Step 19: For each tfoot element in the list of pending tfoot elements, in tree order,
    // run the algorithm for processing row groups.
    for (const auto& tfootElement : pendingTfootElements)
        processRowGroup(tfootElement.get());

    return xWidth;
}

void AccessibilityNodeObject::recomputeIsExposableIfNecessary()
{
    if (!isTable())
        return;
    // Make sure children are up-to-date, because if we do end up changing is-exposed state, we want to make
    // sure updateRowDescendantRoles iterates over those children before they change.
    updateChildrenIfNecessary();
    CheckedRef rareData = ensureRareData();

    bool previouslyExposable = rareData->isExposableTable();
    bool newIsExposable = computeIsTableExposableThroughAccessibility();
    rareData->setIsExposableTable(newIsExposable);
    if (previouslyExposable != newIsExposable) {
        // A table's role value is dependent on whether it's exposed, so recompute it now.
        updateRole();

        // Before resetting our existing children, possibly losing references to them, ensure we update their role (since a table cell's role is dependent on whether its parent table is exposable).
        updateRowDescendantRoles();

        m_childrenDirty = true;
    }
}

AccessibilityObject* AccessibilityNodeObject::parentTable() const
{
    CheckedPtr cache = axObjectCache();
    // If the document no longer exists, we might not have an axObjectCache.
    if (!cache)
        return nullptr;

    // ARIA gridcells may have multiple levels of unignored ancestors that are not the parent table,
    // including rows and interactive rowgroups. In addition, poorly-formed grids may contain elements
    // which pass the tests for inclusion.
    if (isARIAGridCell()) {
        return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
            return ancestor.isExposableTable() && !ancestor.isIgnored();
        });
    }

    if (isTableCell()) {
        // Do not use getOrCreate. parentTable() can be called while the render tree is being modified
        // by javascript, and creating a table element may try to access the render tree while in a bad state.
        // By using only get() implies that the AXTable must be created before AXTableCells. This should
        // always be the case when AT clients access a table.
        // https://bugs.webkit.org/show_bug.cgi?id=42652
        RefPtr<AccessibilityObject> tableFromRenderTree;
        if (CheckedPtr renderTableCell = dynamicDowncast<RenderTableCell>(renderer()))
            tableFromRenderTree = cache->get(renderTableCell->checkedTable().get());

        if (!tableFromRenderTree || !tableFromRenderTree->isTable()) {
            if (node()) {
                return Accessibility::findAncestor<AccessibilityObject>(*this, false, [] (const auto& ancestor) {
                    return ancestor.isTable();
                });
            }
            return nullptr;
        }

        // The RenderTableCell's table() object might be anonymous sometimes. We should handle it gracefully
        // by finding the right table.
        if (!tableFromRenderTree->node()) {
            for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
                // If this is a non-anonymous table object, but not an accessibility table, we should stop because
                // we don't want to choose another ancestor table as this cell's table.
                if (ancestor->isTable()) {
                    if (ancestor->isExposableTable())
                        return ancestor.unsafeGet();
                    if (ancestor->node())
                        break;
                }
            }
            return nullptr;
        }

        return tableFromRenderTree.unsafeGet();
    }

    if (isTableRow()) {
        // The parent table might not be the direct ancestor of the row unfortunately. ARIA states that role="grid" should
        // only have "row" elements, but if not, we still should handle it gracefully by finding the right table.
        for (RefPtr ancestor = parentObject(); ancestor; ancestor = ancestor->parentObject()) {
            if (ancestor->isTable()) {
                bool isNonGridRowOrValidAriaTable = !isARIAGridRow() || ancestor->isAriaTable() || elementName() == ElementName::HTML_tr;
                if (ancestor->isExposableTable() && isNonGridRowOrValidAriaTable)
                    return ancestor.unsafeGet();

                // If this is a non-anonymous table object, but not an accessibility table, we should stop because we don't want to
                // choose another ancestor table as this row's table.
                // Don't exit for ARIA grids, since they could have <table>'s between rows and the owning grid (see aria-grid-with-strange-hierarchy.html).
                if (!isARIAGridRow() && ancestor->node())
                    break;
            }
        }
    }

    return nullptr;
}

void AccessibilityNodeObject::setRowIndex(unsigned rowIndex)
{
    if (!hasCellOrRowRole())
        return;

    CheckedRef rareData = ensureRareData();
    if (rareData->rowIndex() == rowIndex)
        return;
    rareData->setRowIndex(rowIndex);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (CheckedPtr cache = axObjectCache())
        cache->rowIndexChanged(*this);
#endif
}

std::optional<unsigned> AccessibilityNodeObject::axColumnIndex() const
{
    if (!hasCellOrRowRole())
        return std::nullopt;

    if (int value = integralAttribute(aria_colindexAttr); value >= 1)
        return value;

    // "ARIA 1.1: If the set of columns which is present in the DOM is contiguous, and if there are no cells which span more than one row
    // or column in that set, then authors may place aria-colindex on each row, setting the value to the index of the first column of the set."
    // Here, we let its parent row to set its index beforehand, so we don't have to go through the siblings to calculate the index.
    if (hasRareData() && rareData()->axColIndexFromRow() != -1 && parentRow())
        return rareData()->axColIndexFromRow();

    return std::nullopt;
}

std::optional<unsigned> AccessibilityNodeObject::axRowIndex() const
{
    if (!hasCellOrRowRole())
        return std::nullopt;

    // ARIA 1.1: Authors should place aria-rowindex on each row. Authors may also place
    // aria-rowindex on all of the children or owned elements of each row.
    if (int value = integralAttribute(aria_rowindexAttr); value >= 1)
        return value;

    if (RefPtr parentRow = this->parentRow())
        return parentRow->axRowIndex();

    return { };
}

String AccessibilityNodeObject::axRowIndexText() const
{
    if (String text = getAttribute(aria_rowindextextAttr); !text.isNull())
        return text;

    if (RefPtr parentRow = isTableCell() ? this->parentRow() : nullptr)
        return parentRow->axRowIndexText();

    return { };
}

AccessibilityObject::AccessibilityChildrenVector AccessibilityNodeObject::disclosedRows()
{
    if (!isARIATreeGridRow())
        return AccessibilityObject::disclosedRows();

    // The contiguous disclosed rows will be the rows in the table that
    // have an aria-level of plus 1 from this row.
    RefPtr parent = parentObjectUnignored();
    if (!parent || !parent->isExposableTable())
        return { };

    AccessibilityChildrenVector disclosedRows;

    // Search for rows that match the correct level.
    // Only take the subsequent rows from this one that are +1 from this row's level.
    int rowIndex = this->rowIndex();
    if (rowIndex < 0)
        return disclosedRows;

    unsigned level = hierarchicalLevel();
    auto allRows = parent->rows();
    for (int k = rowIndex + 1; k < (int)allRows.size(); ++k) {
        Ref row = allRows[k];
        // Stop at the first row that doesn't match the correct level.
        if (row->hierarchicalLevel() != level + 1)
            break;

        disclosedRows.append(row);
    }
    return disclosedRows;
}

AccessibilityObject* AccessibilityNodeObject::disclosedByRow() const
{
    if (!isARIATreeGridRow())
        return AccessibilityObject::disclosedByRow();

    // The row that discloses this one is the row in the table
    // that is aria-level subtract 1 from this row.
    RefPtr parent = dynamicDowncast<AccessibilityNodeObject>(parentObjectUnignored());
    if (!parent || !parent->isExposableTable())
        return nullptr;

    // If the level is 1 or less, than nothing discloses this row.
    unsigned level = hierarchicalLevel();
    if (level <= 1)
        return nullptr;

    // Search for the previous row that matches the correct level.
    int index = rowIndex();
    auto allRows = parent->rows();
    if (index >= (int)allRows.size())
        return nullptr;

    for (int k = index - 1; k >= 0; --k) {
        if (allRows[k]->hierarchicalLevel() == level - 1)
            return downcast<AccessibilityObject>(allRows[k]).unsafePtr();
    }
    return nullptr;
}

bool AccessibilityNodeObject::isARIAGridRow() const
{
    RefPtr element = this->element();
    return element ? AXTableHelpers::hasRowRole(*element) : false;
}

bool AccessibilityNodeObject::isARIATreeGridRow() const
{
    if (!isARIAGridRow())
        return false;

    RefPtr parent = parentTable();
    return parent && parent->isTreeGrid();
}

bool AccessibilityNodeObject::isTableRow() const
{
    RefPtr element = this->element();
    return element && AXTableHelpers::isTableRowElement(*element);
}

AXCoreObject* AccessibilityNodeObject::parentTableIfExposedTableRow() const
{
    RefPtr element = this->element();
    if (!element || !AXTableHelpers::isTableRowElement(*element))
        return nullptr;

    RefPtr table = parentTable();
    return table && table->isExposableTable() ? table.unsafeGet() : nullptr;
}

bool AccessibilityNodeObject::isTableCell() const
{
    RefPtr element = this->element();
    return element ? AXTableHelpers::isTableCellElement(*element) : false;
}

bool AccessibilityNodeObject::isARIAGridCell() const
{
    RefPtr element = this->element();
    return element && hasCellARIARole(*element);
}

bool AccessibilityNodeObject::isExposedTableCell() const
{
    // If the parent table is an accessibility table, then we are a table cell.
    // This used to check if the unignoredParent was a row, but that exploded performance if
    // this was in nested tables. This check should be just as good.
    if (!isTableCell())
        return false;

    RefPtr parentTable = this->parentTable();
    return parentTable && parentTable->isExposableTable();
}

AccessibilityObject* AccessibilityNodeObject::parentTableIfTableCell() const
{
    return isTableCell() ? parentTable() : nullptr;
}

bool AccessibilityNodeObject::isTableHeaderCell() const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    auto elementName = WebCore::elementName(*element);
    if (elementName == ElementName::HTML_th)
        return true;

    if (elementName == ElementName::HTML_td) {
        RefPtr current = element->parentNode();
        // i < 2 is used here because in a properly structured table, the thead should be 2 levels away from the td.
        for (int i = 0; i < 2 && current; i++) {
            if (WebCore::elementName(*current) == ElementName::HTML_thead)
                return true;
            current = current->parentNode();
        }
    }
    return false;
}

bool AccessibilityNodeObject::isColumnHeader() const
{
    if (role() == AccessibilityRole::ColumnHeader)
        return true;
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "col"_s || scope == "colgroup"_s)
        return true;
    if (scope == "row"_s || scope == "rowgroup"_s)
        return false;
    if (!isTableHeaderCell())
        return false;

    // We are in a situation after checking the scope attribute.
    // It is an attempt to resolve the type of th element without support in the specification.
    // Checking tableTag and tbodyTag allows to check the case of direct row placement in the table and lets stop the loop at the table level.
    RefPtr element = this->element();
    for (RefPtr ancestor = element->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        auto elementName = WebCore::elementName(*ancestor);
        if (elementName == ElementName::HTML_thead)
            return true;
        if (elementName == ElementName::HTML_tfoot)
            return false;
        if (elementName == ElementName::HTML_table || elementName == ElementName::HTML_tbody) {
            // If we're in the first row, we're a column header.
            if (!rowIndexRange().first)
                return true;
            return false;
        }
    }
    return false;
}

bool AccessibilityNodeObject::isRowHeader() const
{
    if (role() == AccessibilityRole::RowHeader)
        return true;
    const AtomString& scope = getAttribute(scopeAttr);
    if (scope == "row"_s || scope == "rowgroup"_s)
        return true;
    if (scope == "col"_s || scope == "colgroup"_s)
        return false;
    if (!isTableHeaderCell())
        return false;

    // We are in a situation after checking the scope attribute.
    // It is an attempt to resolve the type of th element without support in the specification.
    // Checking tableTag allows to check the case of direct row placement in the table and lets stop the loop at the table level.
    RefPtr element = this->element();
    for (RefPtr ancestor = element->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
        auto elementName = WebCore::elementName(*ancestor);
        if (elementName == ElementName::HTML_tfoot || elementName == ElementName::HTML_tbody || elementName == ElementName::HTML_table) {
            // If we're in the first column, we're a row header.
            if (!columnIndexRange().first)
                return true;
            return false;
        }

        if (elementName == ElementName::HTML_thead)
            return false;
    }
    return false;
}

std::pair<unsigned, unsigned> AccessibilityNodeObject::rowIndexRange() const
{
    ensureIndexesUpToDate();
    return hasRareData() ? std::make_pair(rareData()->rowIndex(), rareData()->effectiveRowSpan()) : std::make_pair(0u, 1u);
}

std::pair<unsigned, unsigned> AccessibilityNodeObject::columnIndexRange() const
{
    ensureIndexesUpToDate();
    return hasRareData() ? std::make_pair(rareData()->columnIndex(), colSpan()) : std::make_pair(0u, 1u);
}

String AccessibilityNodeObject::axColumnIndexText() const
{
    return getAttribute(aria_colindextextAttr);
}

unsigned AccessibilityNodeObject::colSpan() const
{
    if (!isTableCell())
        return 1;

    if (auto colSpan = parseHTMLInteger(getAttribute(colspanAttr)); colSpan && *colSpan >= 1) {
        // https://html.spec.whatwg.org/multipage/tables.html
        // If colspan is greater than 1000, let it be 1000 instead.
        return std::min(std::max(*colSpan, 1), 1000);
    }
    if (auto ariaColSpan = parseHTMLInteger(getAttribute(aria_colspanAttr)); ariaColSpan && *ariaColSpan >= 1)
        return std::min(std::max(*ariaColSpan, 1), 1000);
    return 1;
}

unsigned AccessibilityNodeObject::rowSpan() const
{
    if (!isTableCell())
        return 1;
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (auto rowSpan = parseHTMLInteger(getAttribute(rowspanAttr))) {
        if (*rowSpan < 0)
            return 1;
        return std::min(static_cast<unsigned>(*rowSpan), HTMLTableCellElement::maxRowspan);
    }

    if (auto ariaRowSpan = parseHTMLInteger(getAttribute(aria_rowspanAttr))) {
        if (*ariaRowSpan < 0)
            return 1;
        return std::min(static_cast<unsigned>(*ariaRowSpan), HTMLTableCellElement::maxRowspan);
    }

    return 1;
}

void AccessibilityNodeObject::incrementEffectiveRowSpan()
{
    if (hasRareData())
        rareData()->incrementEffectiveRowSpan();
}

void AccessibilityNodeObject::resetEffectiveRowSpan()
{
    if (hasRareData())
        rareData()->resetEffectiveRowSpan();
}

void AccessibilityNodeObject::setAXColIndexFromRow(int index)
{
    if (!hasRareData() && index == -1)
        return;
    ensureRareData().setAXColIndexFromRow(index);
}

void AccessibilityNodeObject::setColumnIndex(unsigned index)
{
    if (!isTableCell())
        return;

    CheckedRef rareData = ensureRareData();
    if (rareData->columnIndex() == index)
        return;
    rareData->setColumnIndex(index);

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (CheckedPtr cache = axObjectCache())
        cache->columnIndexChanged(*this);
#endif
}

AccessibilityNodeObject* AccessibilityNodeObject::parentRow() const
{
    RefPtr parent = isTableCell() ? parentObjectUnignored() : nullptr;
    return parent && parent->isExposedTableRow() ? dynamicDowncast<AccessibilityRenderObject>(parent.get()) : nullptr;
}

#if USE(ATSPI)
int AccessibilityNodeObject::axColumnSpan() const
{
    // According to the ARIA spec, "If aria-colpan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-colspan."
    if (hasAttribute(colspanAttr))
        return -1;

    // ARIA 1.1: Authors must set the value of aria-colspan to an integer greater than or equal to 1.
    if (int value = integralAttribute(aria_colspanAttr); value >= 1)
        return value;

    return -1;
}

int AccessibilityNodeObject::axRowSpan() const
{
    // According to the ARIA spec, "If aria-rowspan is used on an element for which the host language
    // provides an equivalent attribute, user agents must ignore the value of aria-rowspan."
    if (hasAttribute(rowspanAttr))
        return -1;

    // ARIA 1.1: Authors must set the value of aria-rowspan to an integer greater than or equal to 0.
    // Setting the value to 0 indicates that the cell or gridcell is to span all the remaining rows in the row group.
    if (getAttribute(aria_rowspanAttr) == "0"_s)
        return 0;
    if (int value = integralAttribute(aria_rowspanAttr); value >= 1)
        return value;

    return -1;
}
#endif // USE(ATSPI)

void AccessibilityNodeObject::ensureIndexesUpToDate() const
{
    if (RefPtr parentTable = this->parentTable())
        parentTable->ensureCellIndexesUpToDate();
}

bool AccessibilityNodeObject::isTable() const
{
    auto ariaRole = ariaRoleAttribute();
    if (AXTableHelpers::isTableRole(ariaRole))
        return true;
    if (ariaRole != AccessibilityRole::Unknown) {
        // If the ARIA role is set to a non-table role, this isn't a table.
        return false;
    }

    CheckedPtr renderer = this->renderer();
    bool isAnonymous = false;
#if USE(ATSPI)
    // This branch is only necessary because ATSPI walks the render tree rather than the DOM to build the accessibility tree.
    // FIXME: Consider removing this with https://bugs.webkit.org/show_bug.cgi?id=282117.
    isAnonymous = renderer && renderer->isAnonymous();
#endif
    RefPtr node = this->node();
    if ((is<RenderTable>(renderer) && !isAnonymous && !is<HTMLTableSectionElement>(node.get())) || is<HTMLTableElement>(node.get())) {
        // Regarding the !is<HTMLTableSectionElement> check: some websites put display:table on
        // tbody / thead / tfoot, resulting in a RenderTable being generated. We don't want to
        // consider these tables (since they are typically wrapped by an actual <table> element).
        return true;
    }
    return false;
}

AccessibilityObject* AccessibilityNodeObject::controlForLabelElement() const
{
    RefPtr labelElement = labelElementContainer();
    return labelElement ? axObjectCache()->getOrCreate(Accessibility::controlForLabelElement(*labelElement).get()) : nullptr;
}

String AccessibilityNodeObject::ariaAccessibilityDescription() const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        return ariaLabeledBy;

    auto ariaLabel = getAttributeTrimmed(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    return String();
}

AccessibilityObject* AccessibilityNodeObject::captionForFigure() const
{
    if (!isFigureElement())
        return nullptr;

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    RefPtr node = this->node();
    for (RefPtr child = node->firstChild(); child; child = child->nextSibling()) {
        if (WebCore::elementName(*child) == ElementName::HTML_figcaption)
            return cache->getOrCreate(*child);
    }
    return nullptr;
}

bool AccessibilityNodeObject::usesAltForTextComputation() const
{
    bool usesAltTag = isImage() || isInputImage() || isNativeImage() || isCanvas() || elementName() == ElementName::HTML_img || isImageMapLink();
#if ENABLE(MODEL_ELEMENT)
    usesAltTag |= isModel();
#endif
    return usesAltTag;
}

bool AccessibilityNodeObject::isLabelable() const
{
    RefPtr node = this->node();
    if (!node)
        return false;
    return is<HTMLInputElement>(*node) || isControl() || isProgressIndicator() || isMeter();
}

String AccessibilityNodeObject::textAsLabelFor(const AccessibilityObject& labeledObject) const
{
    auto labelAttribute = getAttributeTrimmed(aria_labelAttr);
    if (!labelAttribute.isEmpty())
        return labelAttribute;

    labelAttribute = altTextFromAttributeOrStyle();
    if (!labelAttribute.isEmpty())
        return labelAttribute;

    labelAttribute = getAttribute(titleAttr);
    if (!labelAttribute.isEmpty())
        return labelAttribute;

    if (isNativeLabel()) {
        StringBuilder builder;
        for (const auto& child : const_cast<AccessibilityNodeObject*>(this)->unignoredChildren()) {
            if (child.ptr() == &labeledObject)
                continue;

            if (child->isListBox()) {
                auto selectedChildren = child->selectedChildren();
                for (const auto& selectedGrandChild : selectedChildren)
                    appendNameToStringBuilder(builder, accessibleNameForNode(*selectedGrandChild->node()));
                continue;
            }

            if (child->isComboBox()) {
                appendNameToStringBuilder(builder, child->stringValue());
                continue;
            }

            if (child->isTextControl()) {
                appendNameToStringBuilder(builder, child->text());
                continue;
            }

            if (child->isSlider() || child->isSpinButton()) {
                appendNameToStringBuilder(builder, String::number(child->valueForRange()));
                continue;
            }

            appendNameToStringBuilder(builder, child->textUnderElement());
        }
        if (builder.length())
            return builder.toString().trim(isASCIIWhitespace).simplifyWhiteSpace(isHTMLSpaceButNotLineBreak);
    }

    String text = this->text();
    if (!text.isEmpty())
        return text;
    return textUnderElement();
}

String AccessibilityNodeObject::textForLabelElements(Vector<Ref<HTMLElement>>&& labelElements) const
{
    // https://www.w3.org/TR/html-aam-1.0/#input-type-text-input-type-password-input-type-number-input-type-search-input-type-tel-input-type-email-input-type-url-and-textarea-element-accessible-name-computation
    // "...if more than one label is associated; concatenate by DOM order, delimited by spaces."
    StringBuilder result;

    WeakPtr cache = axObjectCache();
    for (auto& labelElement : labelElements) {
        RefPtr label = cache ? cache->getOrCreate(labelElement.get()) : nullptr;
        if (!label)
            continue;

        if (label.get() == this) {
            // This object labels itself, so use its textAsLabel.
            appendNameToStringBuilder(result, textAsLabelFor(*this));
            continue;
        }

        auto ariaLabeledBy = label->ariaLabeledByAttribute();
        if (!ariaLabeledBy.isEmpty())
            appendNameToStringBuilder(result, WTFMove(ariaLabeledBy));
#if PLATFORM(COCOA)
        else if (RefPtr axLabel = dynamicDowncast<AccessibilityNodeObject>(*label); axLabel && axLabel->isNativeLabel())
            appendNameToStringBuilder(result, axLabel->textAsLabelFor(*this));
#endif
        else
            appendNameToStringBuilder(result, accessibleNameForNode(labelElement.get()));
    }

    return result.toString();
}

HTMLLabelElement* AccessibilityNodeObject::labelElementContainer() const
{
    // The control element should not be considered part of the label.
    if (isControl())
        return nullptr;

    // Find an ancestor label element.
    for (auto* parentNode = node(); parentNode; parentNode = parentNode->parentNode()) {
        if (auto* label = dynamicDowncast<HTMLLabelElement>(*parentNode))
            return label;
    }
    return nullptr;
}

void AccessibilityNodeObject::labelText(Vector<AccessibilityText>& textOrder) const
{
    RefPtr element = this->element();
    if (!element)
        return;

    if (AXTableHelpers::appendCaptionTextIfNecessary(*element, textOrder))
        return;

    Vector<Ref<HTMLElement>> elementLabels;
    auto axLabels = labeledByObjects();
    if (axLabels.size()) {
        elementLabels.appendVector(WTF::compactMap(axLabels, [] (auto& axLabel) {
            return RefPtr { dynamicDowncast<HTMLElement>(axLabel->element()) };
        }));
    }
    if (!elementLabels.size())
        elementLabels = Accessibility::labelsForElement(element.get());

    String label = textForLabelElements(WTFMove(elementLabels));
    if (!label.isEmpty()) {
        textOrder.append({ WTFMove(label), isMeter() ? AccessibilityTextSource::Alternative : AccessibilityTextSource::LabelByElement });
        return;
    }

    auto ariaLabel = getAttributeTrimmed(aria_labelAttr);
    if (!ariaLabel.isEmpty()) {
        textOrder.append({ WTFMove(ariaLabel), AccessibilityTextSource::LabelByElement });
        return;
    }
}

bool AccessibilityNodeObject::hasTextAlternative() const
{
    // ARIA: section 2A, bullet #3 says if aria-labeledby or aria-label appears, it should
    // override the "label" element association.
    return ariaAccessibilityDescription().length();
}

void AccessibilityNodeObject::alternativeText(Vector<AccessibilityText>& textOrder) const
{
    if (isWebArea()) {
        String webAreaText = alternativeTextForWebArea();
        if (!webAreaText.isEmpty())
            textOrder.append(AccessibilityText(WTFMove(webAreaText), AccessibilityTextSource::Alternative));
        return;
    }

    ariaLabeledByText(textOrder);

    bool hasValidAriaLabel = false;
    {
        // Scoped since we potentially move |ariaLabel| here. The scope prevents accidental use-after-move later.
        auto ariaLabel = getAttributeTrimmed(aria_labelAttr);
        if (!ariaLabel.isEmpty()) {
            hasValidAriaLabel = true;
            textOrder.append(AccessibilityText(WTFMove(ariaLabel), AccessibilityTextSource::Alternative));
        }
    }

    if (usesAltForTextComputation()) {
        if (auto* renderImage = dynamicDowncast<RenderImage>(renderer())) {
            String renderAltText = renderImage->altText();

            // RenderImage will return title as a fallback from altText, but we don't want title here because we consider that in helpText.
            if (!renderAltText.isEmpty() && renderAltText != getAttribute(titleAttr)) {
                textOrder.append(AccessibilityText(WTFMove(renderAltText), AccessibilityTextSource::Alternative));
                return;
            }
        }
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        if (String alt = altTextFromAttributeOrStyle(); !alt.isNull())
            textOrder.append(AccessibilityText(WTFMove(alt), AccessibilityTextSource::Alternative));
    }

    RefPtr node = this->node();
    if (!node)
        return;

    auto objectCache = axObjectCache();
    // The fieldset element derives its alternative text from the first associated legend element if one is available.
    if (RefPtr fieldset = dynamicDowncast<HTMLFieldSetElement>(*node); fieldset && objectCache) {
        RefPtr object = objectCache->getOrCreate(fieldset->legend());
        if (object && !object->isHidden())
            textOrder.append(AccessibilityText(accessibleNameForNode(*object->node()), AccessibilityTextSource::Alternative));
    }

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(*node)) {
        // https://github.com/w3c/aria/pull/2224
        // Per html-aam, <img> elements that are unlabeled (e.g., alt attribute, ARIA, title) derive accname
        // from an ancestor figure's <figcaption> if and only if the <figure> does not contain other flow content (besides the <figcaption>).
        const AtomString& alt = image->attributeWithoutSynchronization(altAttr);

        if (alt.isEmpty() && image->attributeWithoutSynchronization(titleAttr).isEmpty()) {
            for (RefPtr ancestor = node->parentNode(); ancestor; ancestor = ancestor->parentNode()) {
                if (auto* figure = dynamicDowncast<HTMLElement>(ancestor.get()); figure && figure->hasTagName(figureTag)) {
                    bool figureHasFlowContent = false;
                    // Iterate over the direct children of the <img>'s ancestor <figure> for any common
                    // flow content, including non-whitespace text nodes.
                    for (RefPtr figureNodeChild = figure->firstChild(); figureNodeChild; figureNodeChild = figureNodeChild->nextSibling()) {
                        if (isFlowContent(*figureNodeChild)) {
                            figureHasFlowContent = true;
                            break;
                        }
                    }
                    // If no flow content is present in the <figure>, the <img> derives accname from its <figcaption>.
                    if (!figureHasFlowContent) {
                        RefPtr figureObject = objectCache ? objectCache->getOrCreate(*figure) : nullptr;
                        RefPtr caption = figureObject && figureObject->isFigureElement() ? downcast<AccessibilityNodeObject>(figureObject)->captionForFigure() : nullptr;
                        if (caption && !caption->isHidden()) {
                            RefPtr captionNode = caption->node();
                            if (String captionAccname = captionNode ? accessibleNameForNode(*captionNode) : emptyString(); !captionAccname.isEmpty())
                                textOrder.append(AccessibilityText(WTFMove(captionAccname), AccessibilityTextSource::Alternative));
                        }
                    }
                    break;
                }
            }
        }
    }

    // Tree items missing a label are labeled by all child elements.
    if (isTreeItem() && !hasValidAriaLabel && ariaLabeledByAttribute().isEmpty())
        textOrder.append(AccessibilityText(accessibleNameForNode(*node), AccessibilityTextSource::Alternative));

    if (accessibleNameDerivesFromHeading()) {
        CheckedPtr cache = axObjectCache();
        // Where an element supports nameFrom: heading and no nameFrom: content/author is supplied, its accname may be
        // derived from the first descendant node that is a heading (depth-first search, preorder traversal).
        if (RefPtr containerNode = dynamicDowncast<ContainerNode>(node); containerNode && cache) {
            for (Ref element : descendantsOfType<Element>(*containerNode)) {
                if (RefPtr descendantObject = cache->getOrCreate(element.get()); descendantObject && descendantObject->isHeading()) {
                    TextUnderElementMode mode;
                    mode.includeFocusableContent = true;
                    String nameFromHeading = descendantObject->textUnderElement(mode);
                    if (!nameFromHeading.isEmpty())
                        textOrder.append(AccessibilityText(nameFromHeading, AccessibilityTextSource::Heading));
                }
            }
        }
    }

#if ENABLE(MATHML)
    if (node->isMathMLElement())
        textOrder.append(AccessibilityText(getAttribute(MathMLNames::alttextAttr), AccessibilityTextSource::Alternative));
#endif

    if (CheckedPtr style = this->style()) {
        String altText = style->altFromContent();
        if (!altText.isEmpty())
            textOrder.append(AccessibilityText(WTFMove(altText), AccessibilityTextSource::Alternative));
    }
}

void AccessibilityNodeObject::visibleText(Vector<AccessibilityText>& textOrder) const
{
    WeakPtr node = this->node();
    if (!node)
        return;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node); input && input->isTextButton()) {
        textOrder.append(AccessibilityText(input->valueWithDefault(), AccessibilityTextSource::Visible));
        return;
    }

    // If this node isn't rendered, there's no inner text we can extract from a select element.
    if (!isAccessibilityRenderObject() && WebCore::elementName(*node) == ElementName::HTML_select)
        return;

    if (dependsOnTextUnderElement()) {
        TextUnderElementMode mode;

        // Headings often include links as direct children. Those links need to be included in text under element.
        if (isHeading())
            mode.includeFocusableContent = true;

        String text = textUnderElement(mode);
        if (!text.isEmpty())
            textOrder.append(AccessibilityText(WTFMove(text), AccessibilityTextSource::Children));
    }
}

void AccessibilityNodeObject::helpText(Vector<AccessibilityText>& textOrder) const
{
    const AtomString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty()) [[unlikely]]
        textOrder.append(AccessibilityText(ariaHelp, AccessibilityTextSource::Help));

#if !PLATFORM(COCOA)
    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        textOrder.append(AccessibilityText(describedBy, AccessibilityTextSource::Summary));
#endif

    if (isControl()) {
        // For controls, use their fieldset parent's described-by text if available.
        auto matchFunc = [] (const AccessibilityObject& object) {
            return object.isFieldset() && !object.ariaDescribedByAttribute().isEmpty();
        };
        if (RefPtr parent = Accessibility::findAncestor<AccessibilityObject>(*this, false, WTFMove(matchFunc)))
            textOrder.append(AccessibilityText(parent->ariaDescribedByAttribute(), AccessibilityTextSource::Summary));
    }

    // Summary attribute used as help text on tables.
    const AtomString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        textOrder.append(AccessibilityText(summary, AccessibilityTextSource::Summary));

    // The title attribute should be used as help text unless it is already being used as descriptive text.
    // However, when the title attribute is the only text alternative provided, it may be exposed as the
    // descriptive text. This is problematic in the case of meters because the HTML spec suggests authors
    // can expose units through this attribute. Therefore, if the element is a meter, change its source
    // type to AccessibilityTextSource::Help.
    const AtomString& title = getAttribute(titleAttr);
    if (!title.isEmpty()) {
        if (!isMeter() && !roleIgnoresTitle())
            textOrder.append(AccessibilityText(title, AccessibilityTextSource::TitleTag));
        else
            textOrder.append(AccessibilityText(title, AccessibilityTextSource::Help));
    }
}

void AccessibilityNodeObject::accessibilityText(Vector<AccessibilityText>& textOrder) const
{
#if ENABLE(ATTACHMENT_ELEMENT)
    if (RefPtr attachmentElement = dynamicDowncast<HTMLAttachmentElement>(node())) {
        AXAttachmentHelpers::accessibilityText(*attachmentElement, textOrder);
        return;
    }
#endif

    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node())) {
        AXImageMapHelpers::accessibilityText(*areaElement, description(), textOrder);
        return;
    }

    labelText(textOrder);
    alternativeText(textOrder);
    visibleText(textOrder);
    helpText(textOrder);

    String placeholder = placeholderValue();
    if (!placeholder.isEmpty())
        textOrder.append(AccessibilityText(WTFMove(placeholder), AccessibilityTextSource::Placeholder));
}

void AccessibilityNodeObject::ariaLabeledByText(Vector<AccessibilityText>& textOrder) const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        textOrder.append(AccessibilityText(WTFMove(ariaLabeledBy), AccessibilityTextSource::Alternative));
}

String AccessibilityNodeObject::alternativeTextForWebArea() const
{
    // The WebArea description should follow this order:
    //     aria-label on the <html>
    //     title on the <html>
    //     <title> inside the <head> (of it was set through JS)
    //     name on the <html>
    // For iframes:
    //     aria-label on the <iframe>
    //     title on the <iframe>
    //     name on the <iframe>

    RefPtr document = this->document();
    if (!document)
        return String();

    // Check if the HTML element has an aria-label for the webpage.
    if (RefPtr documentElement = document->documentElement()) {
        const AtomString& ariaLabel = documentElement->attributeWithoutSynchronization(aria_labelAttr);
        if (!ariaLabel.isEmpty())
            return ariaLabel;
    }

    if (RefPtr owner = document->ownerElement()) {
        auto elementName = owner->elementName();
        if (elementName == ElementName::HTML_frame || elementName == ElementName::HTML_iframe) {
            const AtomString& title = owner->attributeWithoutSynchronization(titleAttr);
            if (!title.isEmpty())
                return title;
        }
        return owner->getNameAttribute();
    }

    String documentTitle = document->title();
    if (!documentTitle.isEmpty())
        return documentTitle;

    if (RefPtr body = document->bodyOrFrameset())
        return body->getNameAttribute();

    return String();
}

String AccessibilityNodeObject::description() const
{
    // Static text should not have a description, it should only have a stringValue.
    if (role() == AccessibilityRole::StaticText)
        return { };

    String ariaDescription = ariaAccessibilityDescription();
    if (!ariaDescription.isEmpty())
        return ariaDescription;

    if (usesAltForTextComputation()) {
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        if (String alt = altTextFromAttributeOrStyle(); !alt.isNull())
            return alt;
        // Image map elements shouldn't fallback.
        if (isImageMapLink())
            return { };
    }

#if ENABLE(MATHML)
    if (is<MathMLElement>(node()))
        return getAttribute(MathMLNames::alttextAttr);
#endif

    // An element's descriptive text is comprised of title() (what's visible on the screen) and description() (other descriptive text).
    // Both are used to generate what a screen reader speaks.
    // If this point is reached (i.e. there's no accessibilityDescription) and there's no title(), we should fallback to using the title attribute.
    // The title attribute is normally used as help text (because it is a tooltip), but if there is nothing else available, this should be used (according to ARIA).
    // https://bugs.webkit.org/show_bug.cgi?id=170475: An exception is when the element is semantically unimportant. In those cases, title text should remain as help text.
    if (!roleIgnoresTitle()) {
        // title() can be an expensive operation because it can invoke textUnderElement for all descendants. Thus call it last.
        auto titleAttribute = getAttribute(titleAttr);
        if (!titleAttribute.isEmpty() && title().isEmpty())
            return titleAttribute;
    }

    return { };
}

// Returns whether the role was not intended to play a semantically meaningful part of the
// accessibility hierarchy. This applies to generic groups like <div>'s with no role value set.
bool AccessibilityNodeObject::roleIgnoresTitle() const
{
    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    switch (role()) {
    case AccessibilityRole::Generic:
    case AccessibilityRole::Unknown:
        return true;
    default:
        return false;
    }
}

String AccessibilityNodeObject::helpText() const
{
    WeakPtr node = this->node();
    if (!node)
        return { };

    const auto& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty()) [[unlikely]]
        return ariaHelp;

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    String description = this->description();
    for (RefPtr ancestor = node.get(); ancestor; ancestor = ancestor->parentNode()) {
        if (RefPtr element = dynamicDowncast<HTMLElement>(ancestor)) {
            const auto& summary = element->getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;

            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const auto& title = element->getAttribute(titleAttr);
            if (!title.isEmpty() && description != title)
                return title;
        }

        auto* cache = axObjectCache();
        if (!cache)
            return { };

        // Only take help text from an ancestor element if its a group or an unknown role. If help was
        // added to those kinds of elements, it is likely it was meant for a child element.
        if (RefPtr axAncestor = cache->getOrCreate(*ancestor)) {
            if (!axAncestor->isGroup() && axAncestor->role() != AccessibilityRole::Unknown)
                break;
        }
    }

    return { };
}

URL AccessibilityNodeObject::url() const
{
    RefPtr node = this->node();
    if (RefPtr anchor = dynamicDowncast<HTMLAnchorElement>(node); anchor && isLink())
        return anchor->href();

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(node); image && isImage())
        return image->getURLAttribute(srcAttr);

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node); input && isInputImage())
        return input->getURLAttribute(srcAttr);

    if (RefPtr areaElement = dynamicDowncast<HTMLAreaElement>(node))
        return areaElement->href();

#if ENABLE(VIDEO)
    if (RefPtr video = dynamicDowncast<HTMLVideoElement>(node); video && isVideo())
        return video->currentSrc();
#endif

    return URL();
}

void AccessibilityNodeObject::setIsExpanded(bool expand)
{
    if (RefPtr details = dynamicDowncast<HTMLDetailsElement>(node())) {
        if (expand != details->hasAttribute(openAttr))
            details->toggleOpen();
    }
}

// When building the textUnderElement for an object, determine whether or not
// we should include the inner text of this given descendant object or skip it.
static bool shouldUseAccessibilityObjectInnerText(AccessibilityObject& object, TextUnderElementMode mode)
{
#if USE(ATSPI)
    // Only ATSPI ever sets IncludeAllChildren.
    // Do not use any heuristic if we are explicitly asking to include all the children.
    if (mode.childrenInclusion == TextUnderElementMode::Children::IncludeAllChildren)
        return true;
#endif // USE(ATSPI)

    // Consider this hypothetical example:
    // <div tabindex=0>
    //   <h2>
    //     Table of contents
    //   </h2>
    //   <a href="#start">Jump to start of book</a>
    //   <ul>
    //     <li><a href="#1">Chapter 1</a></li>
    //     <li><a href="#1">Chapter 2</a></li>
    //   </ul>
    // </div>
    //
    // The goal is to return a reasonable title for the outer container div, because
    // it's focusable - but without making its title be the full inner text, which is
    // quite long. As a heuristic, skip links, controls, and elements that are usually
    // containers with lots of children.

    // ARIA states that certain elements are not allowed to expose their children content for name calculation.
    if (mode.childrenInclusion == TextUnderElementMode::Children::IncludeNameFromContentsChildren
        && !object.accessibleNameDerivesFromContent())
        return false;

    if (equalLettersIgnoringASCIICase(object.getAttribute(aria_hiddenAttr), "true"_s))
        return false;

    // If something doesn't expose any children, then we can always take the inner text content.
    // This is what we want when someone puts an <a> inside a <button> for example.
    if (object.isDescendantOfBarrenParent())
        return true;

    // Skip focusable children, so we don't include the text of links and controls.
    if (object.canSetFocusAttribute() && !mode.includeFocusableContent)
        return false;

    // Skip big container elements like lists, tables, etc.
    if (object.isAccessibilityList())
        return false;

    if (object.isExposableTable())
        return false;

    if (object.isTree() || object.isCanvas())
        return false;

#if ENABLE(MODEL_ELEMENT)
    if (object.isModel())
        return false;
#endif

    return true;
}

static void appendNameToStringBuilder(StringBuilder& builder, String&& text, bool prependSpace)
{
    if (text.isEmpty())
        return;

    if (prependSpace && !isHTMLLineBreak(text[0]) && builder.length() && !isHTMLLineBreak(builder[builder.length() - 1]))
        builder.append(' ');
    builder.append(WTFMove(text));
}


static bool displayTypeNeedsSpace(DisplayType type)
{
    return type == DisplayType::Block
        || type == DisplayType::InlineBlock
        || type == DisplayType::InlineFlex
        || type == DisplayType::InlineGrid
        || type == DisplayType::InlineGridLanes
        || type == DisplayType::InlineTable
        || type == DisplayType::TableCell;
}

static bool needsSpaceFromDisplay(AccessibilityObject& axObject)
{
    CheckedPtr renderer = axObject.renderer();
    if (is<RenderText>(renderer)) {
        // Never add a space for RenderTexts. They are inherently inline, but take their parent's style, which may
        // be block, erroneously adding a space.
        return false;
    }

    if (auto* style = renderer ? &downcast<RenderElement>(*renderer).style() : axObject.style())
        return displayTypeNeedsSpace(style->display());
    return false;
}

static bool shouldPrependSpace(AccessibilityObject& object, AccessibilityObject* previousObject)
{
    return needsSpaceFromDisplay(object)
        || (previousObject && needsSpaceFromDisplay(*previousObject))
        || object.isControl()
        || (previousObject && previousObject->isControl());
}

String AccessibilityNodeObject::textUnderElement(TextUnderElementMode mode) const
{
    RefPtr node = this->node();
    if (auto* text = dynamicDowncast<Text>(node.get()))
        return !mode.isHidden() ? text->data() : emptyString();

    const auto* style = this->style();
    mode.inHiddenSubtree = WebCore::isRenderHidden(style);
    // The Accname specification states that if the current node is hidden, and not directly
    // referenced by aria-labelledby or aria-describedby, and is not a host language text
    // alternative, the empty string should be returned.
    if (mode.isHidden() && node && !ancestorsOfType<HTMLCanvasElement>(*node).first()) {
        if (!labelForObjects().isEmpty() || !descriptionForObjects().isEmpty()) {
            // This object is a hidden label or description for another object, so ignore hidden states for our
            // subtree text under element traversals too.
            //
            // https://w3c.github.io/accname/#comp_labelledby
            // "The result of LabelledBy Recursion in combination with Hidden Not Referenced means that user
            // agents MUST include all nodes in the subtree as part of the accessible name or accessible
            // description, when the node referenced by aria-labelledby or aria-describedby is hidden."
            mode.considerHiddenState = false;
        } else if (style && style->display() == DisplayType::None) {
            // Unlike visibility:visible + visiblity:visible where the latter can override the former in a subtree,
            // display:none guarantees nothing within will be rendered, so we can exit early.
            return { };
        }
    }

    StringBuilder builder;
    RefPtr<AccessibilityObject> previous;
    bool previousRequiresSpace = false;
    auto appendTextUnderElement = [&] (auto& object) {
        // We don't want to trim whitespace in these intermediate calls to textUnderElement, as doing so will wipe out
        // spaces we need to build the string properly. If anything (depending on the original `mode`), we will trim
        // whitespace at the very end.
        SetForScope resetModeTrim(mode.trimWhitespace, TrimWhitespace::No);

        auto childText = object.textUnderElement(mode);
        if (childText.length()) {
            appendNameToStringBuilder(builder, WTFMove(childText), previousRequiresSpace || shouldPrependSpace(object, previous.get()));
            previousRequiresSpace = false;
        }
    };

    auto childIterator = AXChildIterator(*this);
    for (auto child = childIterator.begin(); child != childIterator.end(); previous = child.ptr(), ++child) {
        if (mode.ignoredChildNode && child->node() == mode.ignoredChildNode)
            continue;

        if (mode.isHidden()) {
            // If we are within a hidden context, don't add any text for this node. Instead, fan out downwards
            // to search for un-hidden nodes (e.g. visibility:visible nodes within a visibility:hidden ancestor).
            appendTextUnderElement(*child);
            continue;
        }

        bool shouldDeriveNameFromAuthor = (mode.childrenInclusion == TextUnderElementMode::Children::IncludeNameFromContentsChildren && !child->accessibleNameDerivesFromContent());
        if (shouldDeriveNameFromAuthor) {
            auto nameForNode = accessibleNameForNode(*child->node());
            bool nameIsEmpty = nameForNode.isEmpty();
            appendNameToStringBuilder(builder, WTFMove(nameForNode));
            // Separate author-provided text with a space.
            previousRequiresSpace = previousRequiresSpace || !nameIsEmpty;
            continue;
        }

        if (!shouldUseAccessibilityObjectInnerText(*child, mode))
            continue;

        if (RefPtr accessibilityNodeObject = dynamicDowncast<AccessibilityNodeObject>(*child)) {
            // We should ignore the child if it's labeled by this node.
            // This could happen when this node labels multiple child nodes and we didn't
            // skip in the above ignoredChildNode check.
            auto labeledByElements = accessibilityNodeObject->ariaLabeledByElements();
            if (labeledByElements.containsIf([&](auto& element) { return element.ptr() == node; }))
                continue;

            Vector<AccessibilityText> textOrder;
            accessibilityNodeObject->alternativeText(textOrder);
            if (textOrder.size() > 0 && textOrder[0].text.length()) {
                appendNameToStringBuilder(builder, WTFMove(textOrder[0].text));
                // Alternative text (e.g. from aria-label, aria-labelledby, alt, etc) requires space separation.
                previousRequiresSpace = true;
                continue;
            }
        }

        appendTextUnderElement(*child);
    }

    auto result = builder.toString();
    return mode.trimWhitespace == TrimWhitespace::Yes
        ? result.trim(isASCIIWhitespace).simplifyWhiteSpace(isHTMLSpaceButNotLineBreak)
        : result;
}

String AccessibilityNodeObject::revealableText() const
{
    if (!isStaticText())
        return nullString();

    CheckedPtr<const RenderStyle> style = this->style();
    if (!style || !style->autoRevealsWhenFound())
        return nullString();

    if (RefPtr characterData = dynamicDowncast<CharacterData>(node()))
        return characterData->data().trim(isASCIIWhitespace).simplifyWhiteSpace(isASCIIWhitespace);
    return nullString();
}

String AccessibilityNodeObject::text() const
{
    if (isSecureField())
        return secureFieldValue();

    // Static text can be either an element with role="text", aka ARIA static text, or inline rendered text.
    // In the former case, prefer any alt text that may have been specified.
    // If no alt text is present, fallback to the inline static text case where textUnderElement is used.
    if (isARIAStaticText()) {
        Vector<AccessibilityText> textOrder;
        alternativeText(textOrder);
        if (textOrder.size() > 0 && textOrder[0].text.length())
            return textOrder[0].text;
    }

    if (role() == AccessibilityRole::StaticText)
        return textUnderElement();

    if (!isTextControl())
        return { };

    RefPtr element = dynamicDowncast<Element>(node());
    if (RefPtr formControl = dynamicDowncast<HTMLTextFormControlElement>(element); formControl && isNativeTextControl())
        return formControl->value();
    return element ? element->innerText() : String();
}

bool AccessibilityNodeObject::isBlockFlow() const
{
    return is<RenderBlockFlow>(renderer());
}

Vector<Vector<AXID>> AccessibilityNodeObject::stitchGroups() const
{
    CheckedPtr renderBlockFlow = dynamicDowncast<RenderBlockFlow>(renderer());
    if (!renderBlockFlow)
        return { };

    CheckedPtr inlineLayout = renderBlockFlow->inlineLayout();
    if (!inlineLayout)
        return { };

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return { };

    bool shouldStop = false;
    StitchingContext context { *this };
    Vector<Vector<AXID>> stitchGroups;
    Vector<AXID> currentGroup;
    for (auto lineBox = inlineLayout->firstLineBox(); lineBox && !shouldStop; lineBox.traverseNext()) {
        for (auto box = lineBox->logicalLeftmostLeafBox(); box; box.traverseLogicalRightwardOnLine()) {
            if (box->isAtomicInlineBox()) {
                // Atomic inline boxes (like buttons) should break up stitch groups.
                shouldStop = true;
                break;
            }

            // FIXME: We should also be able to stitch ellipsis-type boxes.
            if (box->isText() || box->isLineBreak()) {
                const auto& renderer = box->renderer();
                RefPtr object = cache->getOrCreate(const_cast<RenderObject&>(renderer));
                if (!object)
                    continue;
                AXID axID = object->objectID();

                if (shouldStopStitchingAt(renderer, *object, context)) {
                    if (currentGroup.size() > 1)
                        stitchGroups.append(std::exchange(currentGroup, { }));
                    else
                        currentGroup.clear();
                } else {
                    CheckedPtr renderText = dynamicDowncast<RenderText>(renderer);

                    // Avoid doing the wrong thing when !renderText->hasRenderedText() is only true
                    // because it has dirty layout. We should not run this function when layout is dirty.
                    ASSERT(!renderText || !renderText->needsLayout() || !renderText->text().length());

                    if (!renderText || !renderText->hasRenderedText())
                        continue;

                    if (currentGroup.isEmpty()) {
                        if (renderText->text().containsOnly<isASCIIWhitespace>()) {
                            // Do not start a stitch-group with whitspace.
                            continue;
                        }
                    }
                    currentGroup.append(axID);
                }
            }
        }
    }

    if (currentGroup.size() > 1) {
        // Only do this for stitch-groups of multiple elements, since stitching a single element
        // doesn't make any sense.
        stitchGroups.append(WTFMove(currentGroup));
    }
    return stitchGroups;
}

AXCoreObject::StitchState AccessibilityNodeObject::stitchState(IncludeStitchGroup includeStitchGroup) const
{
    if (!AXObjectCache::isAXTextStitchingEnabled())
        return { };

    RefPtr blockFlowAncestor = downcast<AccessibilityObject>(blockFlowAncestorForStitchable());
    if (!blockFlowAncestor)
        return { };

    CheckedPtr cache = axObjectCache();
    if (!cache)
        return { };
    return stitchStateFromGroups(cache->stitchGroupsOwnedBy(*blockFlowAncestor), includeStitchGroup);
}

String AccessibilityNodeObject::stringValue() const
{
    RefPtr node = this->node();
    if (!node)
        return { };

    if (isARIAStaticText()) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }

    if (node->isTextNode()) {
        auto stitchState = this->stitchState();
        if (!stitchState.stitchedIntoID || *stitchState.stitchedIntoID != objectID())
            return textUnderElement();

        // |this| is the sum of several stitched text-like objects. Our string value should
        // include all of them.
        //
        // We can compute the stringValue of rendered text using AXProperty::TextRuns.
        // See AccessibilityObject::shouldCacheStringValue.
        CheckedPtr cache = axObjectCache();

        RefPtr endNode = !stitchState.group.isEmpty() && cache ? lastNode(stitchState.group, *cache) : nullptr;
        if (!endNode || endNode == node)
            return textUnderElement();

        std::optional range = makeSimpleRange(positionBeforeNode(node.get()), positionAfterNode(endNode.get()));
        return range ? plainText(*range, textIteratorBehaviorForTextRange()) : emptyString();
    }

    if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(*node)) {
        int selectedIndex = selectElement->selectedIndex();
        auto& listItems = selectElement->listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            if (RefPtr selectedItem = listItems[selectedIndex].get()) {
                auto overriddenDescription = selectedItem->attributeTrimmedWithDefaultARIA(aria_labelAttr);
                if (!overriddenDescription.isEmpty())
                    return overriddenDescription;
            }
        }
        if (!selectElement->multiple())
            return selectElement->value();
        return { };
    }

    if (isComboBox()) {
        for (const auto& child : const_cast<AccessibilityNodeObject*>(this)->unignoredChildren()) {
            if (!child->isListBox())
                continue;

            if (auto selectedChildren = child->selectedChildren(); selectedChildren.size())
                return selectedChildren.first()->stringValue();
            break;
        }
    }

    if (isTextControl())
        return text();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return { };
}

WallTime AccessibilityNodeObject::dateTimeValue() const
{
    if (!isDateTime())
        return { };

    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    return input ? input->accessibilityValueAsDate() : WallTime();
}

DateComponentsType AccessibilityObject::dateTimeComponentsType() const
{
    if (!isDateTime())
        return DateComponentsType::Invalid;

    auto* input = dynamicDowncast<HTMLInputElement>(node());
    return input ? input->dateType() : DateComponentsType::Invalid;
}

SRGBA<uint8_t> AccessibilityNodeObject::colorValue() const
{
    if (!isColorWell())
        return Color::black;

    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    if (!input)
        return Color::black;

    return input->valueAsColor().toColorTypeLossy<SRGBA<uint8_t>>();
}

// This function implements the ARIA accessible name as described by the Mozilla
// ARIA Implementer's Guide.
static String accessibleNameForNode(Node& node, Node* labelledbyNode)
{
    auto* element = dynamicDowncast<Element>(node);

    auto ariaLabel = element ? element->attributeTrimmedWithDefaultARIA(aria_labelAttr) : nullAtom();
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    const AtomString& alt = element ? element->attributeWithoutSynchronization(altAttr) : nullAtom();
    if (!alt.isEmpty())
        return alt;

    // If the node can be turned into an AX object, we can use standard name computation rules.
    // If however, the node cannot (because there's no renderer e.g.) fallback to using the basic text underneath.
    auto* cache = node.document().axObjectCache();
    RefPtr axObject = cache ? cache->getOrCreate(node) : nullptr;
    if (axObject) {
        String valueDescription = axObject->valueDescription();
        if (!valueDescription.isEmpty())
            return valueDescription;

        // The Accname specification states that if the name is being calculated for a combobox
        // or listbox inside a labeling element, return the text alternative of the chosen option.
        AXCoreObject::AccessibilityChildrenVector selectedChildren;
        if (axObject->isListBox())
            selectedChildren = axObject->selectedChildren();
        else if (axObject->isComboBox()) {
            for (const auto& child : axObject->unignoredChildren()) {
                if (child->isListBox()) {
                    selectedChildren = child->selectedChildren();
                    break;
                }
            }
        }

        StringBuilder builder;
        for (const auto& child : selectedChildren)
            appendNameToStringBuilder(builder, accessibleNameForNode(*child->node()));

        String childText = builder.toString();
        if (!childText.isEmpty())
            return childText;
    }

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(element)) {
        String inputValue = input->value();
        if (input->isPasswordField()) {
            StringBuilder passwordValue;
            passwordValue.reserveCapacity(inputValue.length());
            for (size_t i = 0; i < inputValue.length(); i++)
                passwordValue.append(String::fromUTF8("•"));
            return passwordValue.toString();
        }
        return inputValue;
    }
    if (RefPtr option = dynamicDowncast<HTMLOptionElement>(element))
        return option->value();

    String text;
    if (axObject) {
        if (axObject->accessibleNameDerivesFromContent())
            text = axObject->textUnderElement({ TextUnderElementMode::Children::IncludeNameFromContentsChildren, true, true, false, TrimWhitespace::Yes, labelledbyNode });
    } else
        text = (element ? element->innerText() : node.textContent()).simplifyWhiteSpace(isASCIIWhitespace);

    if (!text.isEmpty())
        return text;

    const AtomString& title = element ? element->attributeWithoutSynchronization(titleAttr) : nullAtom();
    if (!title.isEmpty())
        return title;

    auto* slotElement = dynamicDowncast<HTMLSlotElement>(node);
    // Compute the accessible name for a slot's contents only if it's being used to label another node.
    if (auto* assignedNodes = (slotElement && labelledbyNode) ? slotElement->assignedNodes() : nullptr) {
        StringBuilder builder;
        for (const auto& assignedNode : *assignedNodes)
            appendNameToStringBuilder(builder, accessibleNameForNode(*assignedNode));

        auto assignedNodesText = builder.toString();
        if (!assignedNodesText.isEmpty())
            return assignedNodesText;
    }

    return { };
}

String AccessibilityNodeObject::accessibilityDescriptionForChildren() const
{
    RefPtr node = this->node();
    if (!node)
        return String();

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return String();

    StringBuilder builder;
    for (RefPtr child = node->firstChild(); child; child = child->nextSibling()) {
        if (!is<Element>(child))
            continue;

        if (RefPtr axObject = cache->getOrCreate(*child)) {
            String description = axObject->ariaLabeledByAttribute();
            if (description.isEmpty())
                description = accessibleNameForNode(*child);
            appendNameToStringBuilder(builder, WTFMove(description));
        }
    }

    return builder.toString();
}

String AccessibilityNodeObject::descriptionForElements(const Vector<Ref<Element>>& elements) const
{
    StringBuilder builder;
    for (auto& element : elements)
        appendNameToStringBuilder(builder, accessibleNameForNode(element.get(), node()));
    return builder.toString();
}

String AccessibilityNodeObject::ariaDescribedByAttribute() const
{
    return descriptionForElements(elementsFromAttribute(aria_describedbyAttr));
}

Vector<Ref<Element>> AccessibilityNodeObject::ariaLabeledByElements() const
{
    // FIXME: should walk the DOM elements only once.
    auto elements = elementsFromAttribute(aria_labelledbyAttr);
    if (elements.size())
        return elements;
    return elementsFromAttribute(aria_labeledbyAttr);
}


String AccessibilityNodeObject::ariaLabeledByAttribute() const
{
    return descriptionForElements(ariaLabeledByElements());
}

bool AccessibilityNodeObject::hasAccNameAttribute() const
{
    RefPtr element = this->element();
    return element && WebCore::hasAccNameAttribute(*element);
}

bool AccessibilityNodeObject::hasAttributesRequiredForInclusion() const
{
    RefPtr element = this->element();
    if (!element)
        return false;

    if (WebCore::hasAccNameAttribute(*element))
        return true;

#if ENABLE(MATHML)
    if (element->attributeWithoutSynchronization(MathMLNames::alttextAttr).length())
        return true;
#endif

    if (element->attributeWithoutSynchronization(altAttr).length())
        return true;

    if (element->attributeWithoutSynchronization(aria_helpAttr).length()) [[unlikely]]
        return true;

    return false;
}

bool AccessibilityNodeObject::isFocused() const
{
    if (!m_node)
        return false;

    Ref document = node()->document();
    RefPtr focusedElement = document->focusedElement();
    if (!focusedElement)
        return false;

    if (focusedElement.get() == node())
        return true;

    // A web area is represented by the Document node in the DOM tree which isn't focusable.
    // Instead, check if the frame's selection is focused.
    if (role() != AccessibilityRole::WebArea)
        return false;

    RefPtr frame = document->frame();
    return frame ? frame->selection().isFocusedAndActive() : false;
}

void AccessibilityNodeObject::setFocused(bool on)
{
    // Call the base class setFocused to ensure the view is focused and active.
    AccessibilityObject::setFocused(on);

    if (!canSetFocusAttribute())
        return;

    RefPtr document = this->document();

    // This is needed or else focus won't always go into iframes with different origins.
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document.get());

    // Handle clearing focus.
    if (!on || !is<Element>(node())) {
        document->setFocusedElement(nullptr);
        return;
    }

    // When a node is told to set focus, that can cause it to be deallocated, which means that doing
    // anything else inside this object will crash. To fix this, we added a RefPtr to protect this object
    // long enough for duration.
    RefPtr<AccessibilityObject> protectedThis(this);

    // If this node is already the currently focused node, then calling focus() won't do anything.
    // That is a problem when focus is removed from the webpage to chrome, and then returns.
    // In these cases, we need to do what keyboard and mouse focus do, which is reset focus first.
    if (document->focusedElement() == node())
        document->setFocusedElement(nullptr);

    // If we return from setFocusedElement and our element has been removed from a tree, axObjectCache() may be null.
    if (auto* cache = axObjectCache()) {
        cache->setIsSynchronizingSelection(true);
        downcast<Element>(*m_node).focus();
        cache->setIsSynchronizingSelection(false);
    }
}

bool AccessibilityNodeObject::canSetFocusAttribute() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    if (isWebArea())
        return true;

    // NOTE: It would be more accurate to ask the document whether setFocusedElement() would
    // do anything. For example, setFocusedElement() will do nothing if the current focused
    // node will not relinquish the focus.
    RefPtr element = dynamicDowncast<Element>(*node);
    return element && !element->isDisabledFormControl() && element->supportsFocus();
}

bool AccessibilityNodeObject::canSetValueAttribute() const
{
    RefPtr node = this->node();
    if (!node)
        return false;

    // The host-language readonly attribute trumps aria-readonly.
    if (RefPtr textarea = dynamicDowncast<HTMLTextAreaElement>(*node))
        return !textarea->isReadOnly();
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node); input && input->isTextField())
        return !input->isReadOnly();

    String readOnly = readOnlyValue();
    if (!readOnly.isEmpty())
        return readOnly == "true"_s ? false : true;

    if (isNonNativeTextControl())
        return true;

    if (isMeter())
        return false;

    if (isProgressIndicator() || isSlider() || isScrollbar())
        return true;

#if USE(ATSPI)
    // In ATSPI, input types which support aria-readonly are treated as having a
    // settable value if the user can modify the widget's value or its state.
    if (supportsReadOnly())
        return true;

    if (isRadioButton()) {
        auto radioGroup = radioGroupAncestor();
        return radioGroup ? radioGroup->readOnlyValue() != "true"_s : true;
    }
#endif

    if (isWebArea()) {
        RefPtr document = this->document();
        if (!document)
            return false;

        if (RefPtr body = document->bodyOrFrameset()) {
            if (body->hasEditableStyle())
                return true;
        }

        return document->hasEditableStyle();
    }

    return node->hasEditableStyle();
}

AccessibilityRole AccessibilityNodeObject::determineAriaRoleAttribute() const
{
    const AtomString& ariaRole = getAttribute(roleAttr);
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return AccessibilityRole::Unknown;

    AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);

    // ARIA states if an item can get focus, it should not be presentational.
    if (role == AccessibilityRole::Presentational && canSetFocusAttribute())
        return AccessibilityRole::Unknown;

    if (role == AccessibilityRole::Button)
        role = buttonRoleType();

    // If ariaRoleToWebCoreRole computed AccessibilityRole::TextField, we need to figure out if we should use the single-line WebCore textbox role (AccessibilityRole::TextField)
    // or the multi-line WebCore textbox role (AccessibilityRole::TextArea) because the "textbox" ARIA role is overloaded and can mean either.
    if (role == AccessibilityRole::TextField) {
        auto ariaMultiline = getAttribute(aria_multilineAttr);
        if (equalLettersIgnoringASCIICase(ariaMultiline, "true"_s) || (!equalLettersIgnoringASCIICase(ariaMultiline, "false"_s) && matchesTextAreaRole()))
            role = AccessibilityRole::TextArea;
    }

    role = remapAriaRoleDueToParent(role);

    // Presentational roles are invalidated by the presence of ARIA attributes.
    if (role == AccessibilityRole::Presentational && supportsARIAAttributes())
        role = AccessibilityRole::Unknown;

    // https://w3c.github.io/aria/#document-handling_author-errors_roles
    // In situations where an author has not specified names for the form and
    // region landmarks, it is considered an authoring error. The user agent
    // MUST treat such element as if no role had been provided.
    if ((role == AccessibilityRole::LandmarkRegion || role == AccessibilityRole::Form) && !hasAccNameAttribute()) {
        // If a region has no label, but it does have a fallback role, use that instead.
        auto nextRole = ariaRoleToWebCoreRole(ariaRole, [] (const AccessibilityRole& skipRole) {
            return skipRole == AccessibilityRole::LandmarkRegion;
        });
        if (nextRole != role)
            role = nextRole;
        else
            role = AccessibilityRole::Unknown;
    }
    if (enumToUnderlyingType(role))
        return role;

    return AccessibilityRole::Unknown;
}

AccessibilityRole AccessibilityNodeObject::remapAriaRoleDueToParent(AccessibilityRole role) const
{
    // Some objects change their role based on their parent.
    // However, asking for the unignoredParent calls isIgnored(), which can trigger a loop.
    // While inside the call stack of creating an element, we need to avoid isIgnored().
    // https://bugs.webkit.org/show_bug.cgi?id=65174

    if (role != AccessibilityRole::ListBoxOption && role != AccessibilityRole::MenuItem)
        return role;

    for (RefPtr parent = parentObject(); parent && !parent->isIgnored(); parent = parent->parentObject()) {
        AccessibilityRole parentAriaRole = parent->ariaRoleAttribute();

        // Selects and listboxes both have options as child roles, but they map to different roles within WebCore.
        if (role == AccessibilityRole::ListBoxOption && parentAriaRole == AccessibilityRole::Menu)
            return AccessibilityRole::MenuItem;

        // If the parent had a different role, then we don't need to continue searching up the chain.
        if (parentAriaRole != AccessibilityRole::Unknown)
            break;
    }

    return role;
}

void AccessibilityNodeObject::setSelectedChildren(const AccessibilityChildrenVector& children)
{
    if (role() != AccessibilityRole::ListBox || !canSetSelectedChildren())
        return;

    // Unselect any selected option.
    for (const auto& child : unignoredChildren()) {
        if (child->isSelected())
            child->setSelected(false);
    }

    for (const auto& object : children) {
        if (object->isListBoxOption())
            object->setSelected(true);
    }
}

bool AccessibilityNodeObject::canSetSelectedAttribute() const
{
    if (isColumnHeader())
        return false;

    if (isRowHeader() && isEnabled())
        return true;

    // Elements that can be selected
    switch (role()) {
    case AccessibilityRole::Cell:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::Row:
    case AccessibilityRole::TabList:
    case AccessibilityRole::Tab:
    case AccessibilityRole::TreeGrid:
    case AccessibilityRole::TreeItem:
    case AccessibilityRole::Tree:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::MenuItem:
        return isEnabled();
    default:
        return false;
    }
}

bool AccessibilityNodeObject::isAccessibilityList() const
{
    RefPtr element = this->element();
    return element ? AXListHelpers::isAccessibilityList(*element) : false;
}

bool AccessibilityNodeObject::isUnorderedList() const
{
    if (ariaRoleAttribute() == AccessibilityRole::List)
        return true;

    auto elementName = this->elementName();
    return elementName == ElementName::HTML_menu || elementName == ElementName::HTML_ul;
}

bool AccessibilityNodeObject::isOrderedList() const
{
    return ariaRoleAttribute() == AccessibilityRole::Directory || elementName() == ElementName::HTML_ol;
}

bool AccessibilityNodeObject::isDescriptionList() const
{
    return elementName() == ElementName::HTML_dl;
}

static bool childrenContainOnlyStaticText(const AccessibilityObject::AccessibilityChildrenVector& children)
{
    if (children.isEmpty())
        return false;
    for (const auto& child : children) {
        if (child->role() == AccessibilityRole::StaticText)
            continue;
        if (child->isGroup()) {
            if (!childrenContainOnlyStaticText(child->unignoredChildren()))
                return false;
        } else
            return false;
    }
    return true;
}

bool AccessibilityNodeObject::isLabelContainingOnlyStaticText() const
{
    ASSERT(isNativeLabel());

    // m_containsOnlyStaticTextDirty is set (if necessary) by addChildren(), so update our children before checking the flag.
    const_cast<AccessibilityNodeObject*>(this)->updateChildrenIfNecessary();
    if (m_containsOnlyStaticTextDirty) {
        m_containsOnlyStaticTextDirty = false;
        m_containsOnlyStaticText = childrenContainOnlyStaticText(const_cast<AccessibilityNodeObject*>(this)->unignoredChildren());
    }
    return m_containsOnlyStaticText;
}

bool AccessibilityNodeObject::isNativeLabel() const
{
    RefPtr labelElement = dynamicDowncast<HTMLLabelElement>(node());
    return labelElement && hasRole(*labelElement, nullAtom());
}

namespace Accessibility {

RefPtr<HTMLElement> controlForLabelElement(const HTMLLabelElement& label)
{
    auto control = label.control();
    // Make sure the corresponding control isn't a descendant of this label that's in the middle of being destroyed.
    if (!control || (control->renderer() && !control->renderer()->parent()))
        return nullptr;
    return control;
}

Vector<Ref<HTMLElement>> labelsForElement(Element* element)
{
    RefPtr htmlElement = dynamicDowncast<HTMLElement>(element);
    if (!htmlElement || !htmlElement->isLabelable())
        return { };

    Vector<Ref<HTMLElement>> result;
    const auto& idAttribute = htmlElement->getIdAttribute();
    if (!idAttribute.isEmpty()) {
        if (htmlElement->hasAttributeWithoutSynchronization(aria_labelAttr))
            return { };

        if (auto* treeScopeLabels = htmlElement->treeScope().labelElementsForId(idAttribute); treeScopeLabels && !treeScopeLabels->isEmpty()) {
            result.appendVector(WTF::compactMap(*treeScopeLabels, [] (auto& label) {
                return RefPtr { dynamicDowncast<HTMLLabelElement>(label.get()) };
            }));
            if (result.size())
                return result;
        }
    }

    if (htmlElement->hasAttributeWithoutSynchronization(aria_labelAttr))
        return { };

    if (RefPtr nearestLabel = ancestorsOfType<HTMLLabelElement>(*htmlElement).first()) {
        // Only use the nearest label if it isn't pointing at something else.
        const auto& forAttribute = nearestLabel->attributeWithoutSynchronization(forAttr);
        if (forAttribute.isEmpty() || forAttribute == idAttribute)
            return { nearestLabel.releaseNonNull() };
    }
    return { };
}

} // namespace Accessibility

} // namespace WebCore
