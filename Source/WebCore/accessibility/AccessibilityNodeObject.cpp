/*
* Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "AXLogger.h"
#include "AXObjectCache.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityLabel.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "Editing.h"
#include "ElementAncestorIteratorInlines.h"
#include "ElementChildIteratorInlines.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "HTMLAudioElement.h"
#include "HTMLCanvasElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLSlotElement.h"
#include "HTMLSummaryElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "HTMLVideoElement.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "LocalizedStrings.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "ProgressTracker.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "ShadowRoot.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "UserGestureIndicator.h"
#include "VisibleUnits.h"
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

using namespace HTMLNames;

static String accessibleNameForNode(Node* node, Node* labelledbyNode = nullptr);

AccessibilityNodeObject::AccessibilityNodeObject(Node* node)
    : AccessibilityObject()
    , m_node(node)
{
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
    AccessibilityObject::init();
}

Ref<AccessibilityNodeObject> AccessibilityNodeObject::create(Node& node)
{
    return adoptRef(*new AccessibilityNodeObject(&node));
}

void AccessibilityNodeObject::detachRemoteParts(AccessibilityDetachmentType detachmentType)
{
    // AccessibilityObject calls clearChildren.
    AccessibilityObject::detachRemoteParts(detachmentType);
    m_node = nullptr;
}

AccessibilityObject* AccessibilityNodeObject::firstChild() const
{
    auto* currentChild = node() ? node()->firstChild() : nullptr;
    if (!currentChild)
        return nullptr;

    auto* cache = axObjectCache();
    if (!cache)
        return nullptr;

    auto* axCurrentChild = cache->getOrCreate(currentChild);
    while (!axCurrentChild && currentChild) {
        currentChild = currentChild->nextSibling();
        axCurrentChild = cache->getOrCreate(currentChild);
    }
    return axCurrentChild;
}

AccessibilityObject* AccessibilityNodeObject::lastChild() const
{
    if (!node())
        return nullptr;
    
    Node* lastChild = node()->lastChild();
    if (!lastChild)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(lastChild) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::previousSibling() const
{
    if (!node())
        return nullptr;

    Node* previousSibling = node()->previousSibling();
    if (!previousSibling)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(previousSibling) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::nextSibling() const
{
    if (!node())
        return nullptr;

    Node* nextSibling = node()->nextSibling();
    if (!nextSibling)
        return nullptr;

    auto objectCache = axObjectCache();
    return objectCache ? objectCache->getOrCreate(nextSibling) : nullptr;
}
    
AccessibilityObject* AccessibilityNodeObject::parentObjectIfExists() const
{
    return parentObject();
}
    
AccessibilityObject* AccessibilityNodeObject::ownerParentObject() const
{
    auto owners = this->owners();
    ASSERT(owners.size() <= 1);
    return owners.size() ? dynamicDowncast<AccessibilityObject>(owners.first().get()) : nullptr;
}

AccessibilityObject* AccessibilityNodeObject::parentObject() const
{
    if (!node())
        return nullptr;

    if (auto* ownerParent = ownerParentObject())
        return ownerParent;
    
    Node* parentObj = node()->parentNode();
    if (!parentObj)
        return nullptr;
    
    if (AXObjectCache* cache = axObjectCache())
        return cache->getOrCreate(parentObj);
    
    return nullptr;
}

static Vector<Ref<HTMLLabelElement>> labelsForNode(Node* node)
{
    auto* element = dynamicDowncast<HTMLElement>(node);
    if (!element || !element->isLabelable())
        return { };

    const auto& id = element->getIdAttribute();
    if (!id.isEmpty()) {
        if (auto* treeScopeLabels = element->treeScope().labelElementsForId(id); treeScopeLabels && !treeScopeLabels->isEmpty()) {
            return WTF::compactMap(*treeScopeLabels, [](auto& label) {
                return RefPtr<HTMLLabelElement> { dynamicDowncast<HTMLLabelElement>(label.get()) };
            });
        }
    }

    if (RefPtr nearestLabel = ancestorsOfType<HTMLLabelElement>(*element).first()) {
        // Only use the nearest label if it isn't pointing at something else.
        const auto& forAttribute = nearestLabel->attributeWithoutSynchronization(forAttr);
        if (forAttribute.isEmpty() || forAttribute == id)
            return { nearestLabel.releaseNonNull() };
    }
    return { };
}

static RefPtr<HTMLLabelElement> labelForNode(Node* node)
{
    auto labels = labelsForNode(node);
#if PLATFORM(COCOA)
    // We impose the restriction that if there is more than one label element for the given Node, then we should return none.
    // FIXME: the behavior should be the same in all platforms.
    return labels.size() == 1 ? RefPtr { WTFMove(labels.first()) } : nullptr;
#else
    return labels.size() >= 1 ? RefPtr { WTFMove(labels.first()) } : nullptr;
#endif
}

LayoutRect AccessibilityNodeObject::checkboxOrRadioRect() const
{
    auto labels = labelsForNode(node());
    if (labels.isEmpty())
        return boundingBoxRect();

    auto* cache = axObjectCache();
    if (!cache)
        return boundingBoxRect();

    // A checkbox or radio button should encompass its label.
    auto selfRect = boundingBoxRect();
    for (auto& label : labels) {
        if (label->renderer()) {
            if (auto* axLabel = cache->getOrCreate(label.ptr()))
                selfRect.unite(axLabel->elementRect());
        }
    }
    return selfRect;
}

LayoutRect AccessibilityNodeObject::elementRect() const
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node()); input && (input->isCheckbox() || input->isRadioButton()))
        return checkboxOrRadioRect();

    return boundingBoxRect();
}

LayoutRect AccessibilityNodeObject::boundingBoxRect() const
{
    if (hasDisplayContents()) {
        LayoutRect contentsRect;
        for (const auto& child : const_cast<AccessibilityNodeObject*>(this)->children(false))
            contentsRect.unite(child->elementRect());

        if (!contentsRect.isEmpty())
            return contentsRect;
    }

    // Non-display:contents AccessibilityNodeObjects have no mechanism to return a size or position.
    // Instead, let's return a box at the position of an ancestor that does have a position, make it
    // the width of that ancestor, and about the height of a line of text, so it's clear this object is
    // a descendant of that ancestor.
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
    return { };
}

void AccessibilityNodeObject::setNode(Node* node)
{
    m_node = node;
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

AccessibilityRole AccessibilityNodeObject::determineAccessibilityRole()
{
    AXTRACE("AccessibilityNodeObject::determineAccessibilityRole"_s);
    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;

    return determineAccessibilityRoleFromNode();
}

bool AccessibilityNodeObject::matchesTextAreaRole() const
{
#if !PLATFORM(COCOA)
    if (hasContentEditableAttributeSet())
        return true;
#endif
    return is<HTMLTextAreaElement>(node());
}

AccessibilityRole AccessibilityNodeObject::determineAccessibilityRoleFromNode(TreatStyleFormatGroupAsInline treatStyleFormatGroupAsInline) const
{
    RefPtr node = this->node();
    if (!node)
        return AccessibilityRole::Unknown;

    if (RefPtr element = dynamicDowncast<Element>(*node); element && element->isLink())
        return AccessibilityRole::WebCoreLink;
    if (node->isTextNode())
        return AccessibilityRole::StaticText;
    if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(*node))
        return selectElement->multiple() ? AccessibilityRole::ListBox : AccessibilityRole::PopUpButton;
    if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(*node); imgElement && imgElement->hasAttributeWithoutSynchronization(usemapAttr))
        return AccessibilityRole::ImageMap;
    if (node->hasTagName(liTag))
        return AccessibilityRole::ListItem;
    if (node->hasTagName(buttonTag))
        return buttonRoleType();
    if (node->hasTagName(legendTag))
        return AccessibilityRole::Legend;
    if (node->hasTagName(canvasTag))
        return AccessibilityRole::Canvas;
    if (isFileUploadButton())
        return AccessibilityRole::Button;
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node)) {
        if (input->isSwitch())
            return AccessibilityRole::Switch;
        if (input->isCheckbox())
            return AccessibilityRole::Checkbox;
        if (input->isRadioButton())
            return AccessibilityRole::RadioButton;
        if (input->isTextButton())
            return buttonRoleType();
        // On iOS, the date field and time field are popup buttons. On other platforms they are text fields.
#if PLATFORM(IOS_FAMILY)
        if (input->isDateField() || input->isTimeField())
            return AccessibilityRole::PopUpButton;
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        if (input->isColorControl())
            return AccessibilityRole::ColorWell;
#endif
        if (input->isInputTypeHidden())
            return AccessibilityRole::Ignored;
        if (input->isRangeControl())
            return AccessibilityRole::Slider;
        if (input->isSearchField())
            return AccessibilityRole::SearchField;

        return AccessibilityRole::TextField;
    }

    if (matchesTextAreaRole())
        return AccessibilityRole::TextArea;

    if (headingLevel())
        return AccessibilityRole::Heading;

    if (node->hasTagName(codeTag))
        return AccessibilityRole::Code;
    if (node->hasTagName(delTag))
        return AccessibilityRole::Deletion;
    if (node->hasTagName(insTag))
        return AccessibilityRole::Insertion;
    if (node->hasTagName(subTag))
        return AccessibilityRole::Subscript;
    if (node->hasTagName(supTag))
        return AccessibilityRole::Superscript;
    if (isStyleFormatGroup())
        return treatStyleFormatGroupAsInline == TreatStyleFormatGroupAsInline::Yes ? AccessibilityRole::Inline : AccessibilityRole::TextGroup;

    if (node->hasTagName(ddTag))
        return AccessibilityRole::DescriptionListDetail;
    if (node->hasTagName(dtTag))
        return AccessibilityRole::DescriptionListTerm;
    if (node->hasTagName(dlTag))
        return AccessibilityRole::DescriptionList;
    if (node->hasTagName(menuTag) || node->hasTagName(olTag) || node->hasTagName(ulTag))
        return AccessibilityRole::List;
    if (node->hasTagName(fieldsetTag))
        return AccessibilityRole::ApplicationGroup;
    if (node->hasTagName(figureTag))
        return AccessibilityRole::Figure;
    if (node->hasTagName(pTag))
        return AccessibilityRole::Paragraph;
    if (is<HTMLLabelElement>(node.get()))
        return AccessibilityRole::Label;
    if (node->hasTagName(dfnTag))
        return AccessibilityRole::Definition;
    if (node->hasTagName(divTag) && !isNonNativeTextControl())
        return AccessibilityRole::Generic;
    if (is<HTMLFormElement>(node.get()))
        return AccessibilityRole::Form;
    if (node->hasTagName(articleTag))
        return AccessibilityRole::DocumentArticle;
    if (node->hasTagName(mainTag))
        return AccessibilityRole::LandmarkMain;
    if (node->hasTagName(navTag))
        return AccessibilityRole::LandmarkNavigation;
    if (node->hasTagName(asideTag)) {
        if (ariaRoleAttribute() == AccessibilityRole::LandmarkComplementary)
            return AccessibilityRole::LandmarkComplementary;
        // The aside element should not assume the complementary role when nested
        // within the sectioning content elements.
        // https://w3c.github.io/html-aam/#el-aside
        if (isDescendantOfElementType({ asideTag, articleTag, sectionTag, navTag })) {
            // Return LandmarkComplementary if the aside element has an accessible name.
            if (hasAttribute(aria_labelAttr) || hasAttribute(aria_labelledbyAttr) || hasAttribute(aria_descriptionAttr) || hasAttribute(aria_describedbyAttr))
                return AccessibilityRole::LandmarkComplementary;
            return AccessibilityRole::Generic;
        }
        return AccessibilityRole::LandmarkComplementary;
    }
    if (node->hasTagName(searchTag))
        return AccessibilityRole::LandmarkSearch;

    // The default role attribute value for the section element, region, became a landmark in ARIA 1.1.
    // The HTML AAM spec says it is "strongly recommended" that ATs only convey and provide navigation
    // for section elements which have names.
    if (node->hasTagName(sectionTag))
        return hasAttribute(aria_labelAttr) || hasAttribute(aria_labelledbyAttr) ? AccessibilityRole::LandmarkRegion : AccessibilityRole::TextGroup;
    if (node->hasTagName(addressTag))
        return AccessibilityRole::ApplicationGroup;
    if (node->hasTagName(blockquoteTag))
        return AccessibilityRole::Blockquote;
    if (node->hasTagName(captionTag) || node->hasTagName(figcaptionTag))
        return AccessibilityRole::Caption;
    if (node->hasTagName(dialogTag))
        return AccessibilityRole::ApplicationDialog;
    if (node->hasTagName(markTag) || equalLettersIgnoringASCIICase(getAttribute(roleAttr), "mark"_s))
        return AccessibilityRole::Mark;
    if (node->hasTagName(preTag))
        return AccessibilityRole::Pre;
    if (is<HTMLDetailsElement>(node.get()))
        return AccessibilityRole::Details;
    if (auto* summaryElement = dynamicDowncast<HTMLSummaryElement>(node.get()); summaryElement && summaryElement->isActiveSummary())
        return AccessibilityRole::Summary;

#if PLATFORM(COCOA)
    if (isNonNativeTextControl())
        return AccessibilityRole::Group;
#endif

    // http://rawgit.com/w3c/aria/master/html-aam/html-aam.html
    // Output elements should be mapped to status role.
    if (isOutput())
        return AccessibilityRole::ApplicationStatus;

#if ENABLE(VIDEO)
    if (is<HTMLVideoElement>(node.get()))
        return AccessibilityRole::Video;
    if (is<HTMLAudioElement>(node.get()))
        return AccessibilityRole::Audio;
#endif

#if ENABLE(MODEL_ELEMENT)
    if (node->hasTagName(modelTag))
        return AccessibilityRole::Model;
#endif

    // The HTML element should not be exposed as an element. That's what the RenderView element does.
    if (node->hasTagName(htmlTag))
        return AccessibilityRole::Ignored;

    // There should only be one banner/contentInfo per page. If header/footer are being used within an article or section then it should not be exposed as whole page's banner/contentInfo.
    // https://w3c.github.io/html-aam/#el-header
    if (node->hasTagName(headerTag) && !isDescendantOfElementType({ articleTag, asideTag, navTag, sectionTag }))
        return AccessibilityRole::LandmarkBanner;

    // http://webkit.org/b/190138 Footers should become contentInfo's if scoped to body (and consequently become a landmark).
    // It should remain a footer if scoped to main, sectioning elements (article, aside, nav, section) or root sectioning element (blockquote, details, dialog, fieldset, figure, td).
    // https://w3c.github.io/html-aam/#el-footer
    if (node->hasTagName(footerTag)) {
        if (!isDescendantOfElementType({ articleTag, asideTag, navTag, sectionTag, mainTag, blockquoteTag, detailsTag, dialogTag, fieldsetTag, figureTag, tdTag }))
            return AccessibilityRole::LandmarkContentInfo;
        return AccessibilityRole::Footer;
    }

    if (node->hasTagName(timeTag))
        return AccessibilityRole::Time;
    if (node->hasTagName(hrTag))
        return AccessibilityRole::HorizontalRule;

    // If the element does not have role, but it has ARIA attributes, or accepts tab focus, accessibility should fallback to exposing it as a group.
    if (supportsARIAAttributes() || canSetFocusAttribute())
        return AccessibilityRole::Group;
    if (RefPtr element = dynamicDowncast<Element>(*node); element && element->isFocusable())
        return AccessibilityRole::Group;

    return AccessibilityRole::Unknown;
}

bool AccessibilityNodeObject::isDescendantOfElementType(const HashSet<QualifiedName>& tagNames) const
{
    if (!m_node)
        return false;

    for (auto& ancestorElement : ancestorsOfType<Element>(*m_node)) {
        if (tagNames.contains(ancestorElement.tagQName()))
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
}

void AccessibilityNodeObject::updateOwnedChildren()
{
    for (RefPtr child : ownedObjects()) {
        // If the child already exists as a DOM child, but is also in the owned objects, then
        // we need to re-order this child in the aria-owns order.
        m_children.removeFirst(child);
        addChild(child.get());
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

    WeakPtr node = this->node();
    if (!node || !canHaveChildren())
        return;

    // The only time we add children from the DOM tree to a node with a renderer is when it's a canvas.
    if (renderer() && !node->hasTagName(canvasTag))
        return;

    auto objectCache = axObjectCache();
    if (!objectCache)
        return;

    for (auto* child = node->firstChild(); child; child = child->nextSibling())
        addChild(objectCache->getOrCreate(child));

    updateOwnedChildren();
}

bool AccessibilityNodeObject::canHaveChildren() const
{
    // When <noscript> is not being used (its renderer() == 0), ignore its children.
    if (node() && !renderer() && node()->hasTagName(noscriptTag))
        return false;
    // If this is an AccessibilityRenderObject, then it's okay if this object
    // doesn't have a node - there are some renderers that don't have associated
    // nodes, like scroll areas and css-generated text.

    // Elements that should not have children.
    switch (roleValue()) {
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
    // Native list boxes would be AccessibilityListBoxes, so only check for aria list boxes.
    if (ariaRoleAttribute() != AccessibilityRole::ListBox)
        return { };

    if (!childrenInitialized())
        addChildren();

    AccessibilityChildrenVector result;
    for (const auto& child : children()) {
        if (!child->isOffScreen())
            result.append(child);
    }
    return result;
}

bool AccessibilityNodeObject::computeAccessibilityIsIgnored() const
{
#ifndef NDEBUG
    // Double-check that an AccessibilityObject is never accessed before
    // it's been initialized.
    ASSERT(m_initialized);
#endif
    if (!node())
        return true;

    // Handle non-rendered text that is exposed through aria-hidden=false.
    if (node() && node()->isTextNode() && !renderer()) {
        // Fallback content in iframe nodes should be ignored.
        if (node()->parentNode() && node()->parentNode()->hasTagName(iframeTag) && node()->parentNode()->renderer())
            return true;

        // Whitespace only text elements should be ignored when they have no renderer.
        if (stringValue().containsOnly<deprecatedIsSpaceOrNewline>())
            return true;
    }

    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;

    auto role = roleValue();
    return role == AccessibilityRole::Ignored || role == AccessibilityRole::Unknown;
}

bool AccessibilityNodeObject::canvasHasFallbackContent() const
{
    RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(node());
    // If it has any children that are elements, we'll assume it might be fallback
    // content. If it has no children or its only children are not elements
    // (e.g. just text nodes), it doesn't have fallback content.
    return canvasElement && childrenOfType<Element>(*canvasElement).first();
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
    Node* node = this->node();
    if (!node)
        return false;

    if (roleValue() == AccessibilityRole::SearchField)
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
    Node* node = this->node();
    if (!node)
        return false;

    if (is<HTMLImageElement>(*node))
        return true;

    if (node->hasTagName(appletTag) || node->hasTagName(embedTag) || node->hasTagName(objectTag))
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

bool AccessibilityNodeObject::isInputImage() const
{
    if (roleValue() != AccessibilityRole::Button)
        return false;
    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    return input && input->isImageButton();
}

bool AccessibilityNodeObject::isEnabled() const
{
    // ARIA says that the disabled status applies to the current element and all descendant elements.
    for (AccessibilityObject* object = const_cast<AccessibilityNodeObject*>(this); object; object = object->parentObject()) {
        const AtomString& disabledStatus = object->getAttribute(aria_disabledAttr);
        if (equalLettersIgnoringASCIICase(disabledStatus, "true"_s))
            return false;
        if (equalLettersIgnoringASCIICase(disabledStatus, "false"_s))
            break;
    }
    
    if (roleValue() == AccessibilityRole::HorizontalRule)
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

    Node* node = this->node();
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
    Node* node = this->node();
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

bool AccessibilityNodeObject::isHovered() const
{
    RefPtr element = dynamicDowncast<Element>(node());
    return element && element->hovered();
}

bool AccessibilityNodeObject::isMultiSelectable() const
{
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
    auto* element = this->element();
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
    if (mainFrame() && mainFrame()->eventHandler().draggingElement() == element())
        return true;
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

    return { };
}

bool AccessibilityNodeObject::supportsARIAOwns() const
{
    return !getAttribute(aria_ownsAttr).isEmpty();
}

bool AccessibilityNodeObject::supportsRequiredAttribute() const
{
    switch (roleValue()) {
    case AccessibilityRole::Button:
        return isFileUploadButton();
    case AccessibilityRole::Cell:
    case AccessibilityRole::ColumnHeader:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ComboBox:
    case AccessibilityRole::Grid:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::Incrementor:
    case AccessibilityRole::ListBox:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::RadioGroup:
    case AccessibilityRole::RowHeader:
    case AccessibilityRole::Slider:
    case AccessibilityRole::SpinButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::TableHeaderContainer:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

unsigned AccessibilityNodeObject::headingLevel() const
{
    // headings can be in block flow and non-block flow
    Node* node = this->node();
    if (!node)
        return 0;

    if (isHeading()) {
        if (auto level = getIntegralAttribute(aria_levelAttr); level > 0)
            return level;
    }

    if (node->hasTagName(h1Tag))
        return 1;

    if (node->hasTagName(h2Tag))
        return 2;

    if (node->hasTagName(h3Tag))
        return 3;

    if (node->hasTagName(h4Tag))
        return 4;

    if (node->hasTagName(h5Tag))
        return 5;

    if (node->hasTagName(h6Tag))
        return 6;

    return 0;
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

    if (!isRangeControl())
        return 0.0f;

    // In ARIA 1.1, the implicit value for aria-valuenow on a spin button is 0.
    // For other roles, it is half way between aria-valuemin and aria-valuemax.
    auto& value = getAttribute(aria_valuenowAttr);
    if (!value.isEmpty())
        return value.toFloat();

    return isSpinButton() ? 0 : (minValueForRange() + maxValueForRange()) / 2;
}

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

AccessibilityOrientation AccessibilityNodeObject::orientation() const
{
    const AtomString& ariaOrientation = getAttribute(aria_orientationAttr);
    if (equalLettersIgnoringASCIICase(ariaOrientation, "horizontal"_s))
        return AccessibilityOrientation::Horizontal;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "vertical"_s))
        return AccessibilityOrientation::Vertical;
    if (equalLettersIgnoringASCIICase(ariaOrientation, "undefined"_s))
        return AccessibilityOrientation::Undefined;

    // In ARIA 1.1, the implicit value of aria-orientation changed from horizontal
    // to undefined on all roles that don't have their own role-specific values. In
    // addition, the implicit value of combobox became undefined.
    if (isComboBox() || isRadioGroup() || isTreeGrid())
        return AccessibilityOrientation::Undefined;

    if (isScrollbar() || isListBox() || isMenu() || isTree())
        return AccessibilityOrientation::Vertical;

    if (isMenuBar() || isSplitter() || isTabList() || isToolbar() || isSlider())
        return AccessibilityOrientation::Horizontal;

    return AccessibilityObject::orientation();
}

bool AccessibilityNodeObject::isLink() const
{
    return roleValue() == AccessibilityRole::WebCoreLink;
}

bool AccessibilityNodeObject::isBusy() const
{
    return elementAttributeValue(aria_busyAttr);
}

bool AccessibilityNodeObject::isControl() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return is<HTMLFormControlElement>(*node) || AccessibilityObject::isARIAControl(ariaRoleAttribute()) || roleValue() == AccessibilityRole::Button;
}

bool AccessibilityNodeObject::isFieldset() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return node->hasTagName(fieldsetTag);
}

AccessibilityButtonState AccessibilityNodeObject::checkboxOrRadioValue() const
{
    if (auto* input = dynamicDowncast<HTMLInputElement>(node()); input && (input->isCheckbox() || input->isRadioButton()))
        return input->indeterminate() && !input->isSwitch() ? AccessibilityButtonState::Mixed : isChecked() ? AccessibilityButtonState::On : AccessibilityButtonState::Off;

    return AccessibilityObject::checkboxOrRadioValue();
}

Element* AccessibilityNodeObject::anchorElement() const
{
    Node* node = this->node();
    if (!node)
        return nullptr;

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    for ( ; node; node = node->parentNode()) {
        if (is<HTMLAnchorElement>(*node) || (node->renderer() && cache->getOrCreate(node->renderer())->isLink()))
            return downcast<Element>(node);
    }

    return nullptr;
}

RefPtr<Element> AccessibilityNodeObject::popoverTargetElement() const
{
    WeakPtr formControlElement = dynamicDowncast<HTMLFormControlElement>(node());
    return formControlElement ? formControlElement->popoverTargetElement() : nullptr;
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
    auto* document = this->document();
    if (!document || !equalIgnoringFragmentIdentifier(document->url(), linkURL))
        return nullptr;

    RefPtr linkedNode = document->findAnchor(fragmentIdentifier);
    if (!linkedNode)
        return nullptr;

    // The element we find may not be accessible, so find the first accessible object.
    return firstAccessibleObjectFromNode(linkedNode.get());
}

void AccessibilityNodeObject::addRadioButtonGroupChildren(AXCoreObject& parent, AccessibilityChildrenVector& linkedUIElements) const
{
    for (const auto& child : parent.children()) {
        if (child->roleValue() == AccessibilityRole::RadioButton)
            linkedUIElements.append(child);
        else
            addRadioButtonGroupChildren(*child, linkedUIElements);
    }
}

void AccessibilityNodeObject::addRadioButtonGroupMembers(AccessibilityChildrenVector& linkedUIElements) const
{
    if (roleValue() != AccessibilityRole::RadioButton)
        return;

    WeakPtr node = this->node();
    if (auto* input = dynamicDowncast<HTMLInputElement>(node.get())) {
        for (auto& radioSibling : input->radioButtonGroup()) {
            if (auto* object = axObjectCache()->getOrCreate(radioSibling.ptr()))
                linkedUIElements.append(object);
        }
    } else {
        // If we didn't find any radio button siblings with the traditional naming, lets search for a radio group role and find its children.
        for (auto* parent = parentObject(); parent; parent = parent->parentObject()) {
            if (parent->roleValue() == AccessibilityRole::RadioGroup)
                addRadioButtonGroupChildren(*parent, linkedUIElements);
        }
    }
}

AXCoreObject::AccessibilityChildrenVector AccessibilityNodeObject::linkedObjects() const
{
    auto linkedObjects = flowToObjects();

    if (isLink()) {
        if (auto* linkedAXElement = internalLinkElement())
            linkedObjects.append(linkedAXElement);
    }

    if (roleValue() == AccessibilityRole::RadioButton)
        addRadioButtonGroupMembers(linkedObjects);

    linkedObjects.appendVector(controlledObjects());
    return linkedObjects;
}

bool AccessibilityNodeObject::toggleDetailsAncestor()
{
    for (auto* node = this->node(); node; node = node->parentOrShadowHostNode()) {
        if (auto* details = dynamicDowncast<HTMLDetailsElement>(node)) {
            details->toggleOpen();
            return true;
        }
    }
    return false;
}

static bool isNodeActionElement(Node* node)
{
    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node)) {
        if (!input->isDisabledFormControl() && (input->isRadioButton() || input->isCheckbox() || input->isTextButton() || input->isFileUpload() || input->isImageButton()))
            return true;
    } else if (node->hasTagName(buttonTag) || node->hasTagName(selectTag))
        return true;

    return false;
}
    
static Element* nativeActionElement(Node* start)
{
    if (!start)
        return nullptr;
    
    // Do a deep-dive to see if any nodes should be used as the action element.
    // We have to look at Nodes, since this method should only be called on objects that do not have children (like buttons).
    // It solves the problem when authors put role="button" on a group and leave the actual button inside the group.
    
    for (Node* child = start->firstChild(); child; child = child->nextSibling()) {
        if (isNodeActionElement(child))
            return downcast<Element>(child);

        if (Element* subChild = nativeActionElement(child))
            return subChild;
    }
    return nullptr;
}
    
Element* AccessibilityNodeObject::actionElement() const
{
    Node* node = this->node();
    if (!node)
        return nullptr;

    if (isNodeActionElement(node))
        return downcast<Element>(node);
    
    if (AccessibilityObject::isARIAInput(ariaRoleAttribute()))
        return downcast<Element>(node);

    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::Tab:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::ListItem:
        // Check if the author is hiding the real control element inside the ARIA element.
        if (Element* nativeElement = nativeActionElement(node))
            return nativeElement;
        return downcast<Element>(node);
    default:
        break;
    }
    
    Element* elt = anchorElement();
    if (!elt)
        elt = mouseButtonListener();
    return elt;
}

Element* AccessibilityNodeObject::mouseButtonListener(MouseButtonListenerResultFilter filter) const
{
    WeakPtr node = this->node();
    if (!node)
        return nullptr;

    // check if our parent is a mouse button listener
    // FIXME: Do the continuation search like anchorElement does
    for (auto& element : lineageOfType<Element>(*node)) {
        // If we've reached the body and this is not a control element, do not expose press action for this element unless filter is IncludeBodyElement.
        // It can cause false positives, where every piece of text is labeled as accepting press actions.
        if (element.hasTagName(bodyTag) && isStaticText() && filter == ExcludeBodyElement)
            break;
        
        if (element.hasEventListeners(eventNames().clickEvent) || element.hasEventListeners(eventNames().mousedownEvent) || element.hasEventListeners(eventNames().mouseupEvent))
            return &element;
    }

    return nullptr;
}

bool AccessibilityNodeObject::isDescendantOfBarrenParent() const
{
    if (!m_isIgnoredFromParentData.isNull())
        return m_isIgnoredFromParentData.isDescendantOfBarrenParent;
    
    for (AccessibilityObject* object = parentObject(); object; object = object->parentObject()) {
        if (!object->canHaveChildren())
            return true;
    }

    return false;
}

void AccessibilityNodeObject::alterRangeValue(StepAction stepAction)
{
    if (roleValue() != AccessibilityRole::Slider && roleValue() != AccessibilityRole::SpinButton)
        return;
    
    auto element = this->element();
    if (!element || element->isDisabledFormControl())
        return;

    if (!getAttribute(stepAttr).isEmpty())
        changeValueByStep(stepAction);
    else
        changeValueByPercent(stepAction == StepAction::Increment ? 5 : -5);
}
    
void AccessibilityNodeObject::increment()
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document());
    alterRangeValue(StepAction::Increment);
}

void AccessibilityNodeObject::decrement()
{
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document());
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
        handled |= event->defaultHandled();
    }
    
    // Ensure node is still valid and wasn't removed after the keydown.
    if (auto* node = object->node()) {
        auto event = KeyboardEvent::create(eventNames().keyupEvent, keyInit, Event::IsTrusted::Yes);
        node->dispatchEvent(event);
        handled |= event->defaultHandled();
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
    bool vertical = orientation() == AccessibilityOrientation::Vertical || roleValue() == AccessibilityRole::SpinButton;

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
            cache->postNotification(this, document(), AXObjectCache::AXValueChanged);
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

const String AccessibilityNodeObject::liveRegionStatus() const
{
    const auto& liveRegionStatus = getAttribute(aria_liveAttr);
    if (liveRegionStatus.isEmpty())
        return defaultLiveRegionStatusForRole(roleValue());

    return liveRegionStatus;
}

const String AccessibilityNodeObject::liveRegionRelevant() const
{
    const auto& relevant = getAttribute(aria_relevantAttr);
    // Default aria-relevant = "additions text".
    if (relevant.isEmpty())
        return "additions text"_s;

    return relevant;
}

bool AccessibilityNodeObject::liveRegionAtomic() const
{
    const auto& atomic = getAttribute(aria_atomicAttr);
    if (equalLettersIgnoringASCIICase(atomic, "true"_s))
        return true;
    if (equalLettersIgnoringASCIICase(atomic, "false"_s))
        return false;

    // WAI-ARIA "alert" and "status" roles have an implicit aria-atomic value of true.
    switch (roleValue()) {
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
    auto* document = this->document();
    if (auto* localFrame = document ? document->frame() : nullptr) {
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

    auto* document = this->document();
    auto* renderView = document ? document->renderView() : nullptr;
    if (!renderView)
        return { };

    // iterate over the lines
    // FIXME: This is wrong when lineNumber is lineCount+1, because nextLinePosition takes you to the last offset of the last line.
    VisiblePosition position = renderView->positionForPoint(IntPoint(), nullptr);
    while (--lineCount) {
        auto previousLinePosition = position;
        position = nextLinePosition(position, 0);
        if (position.isNull() || position == previousLinePosition)
            return VisiblePositionRange();
    }

    // make a caret selection for the marker position, then extend it to the line
    // NOTE: Ignores results of sel.modify because it returns false when starting at an empty line.
    // The resulting selection in that case will be a caret at position.
    FrameSelection selection;
    selection.setSelection(position);
    selection.modify(FrameSelection::Alteration::Extend, SelectionDirection::Right, TextGranularity::LineBoundary);
    return selection.selection();
}

bool AccessibilityNodeObject::isGenericFocusableElement() const
{
    if (!canSetFocusAttribute())
        return false;

    // If it's a control, it's not generic.
    if (isControl())
        return false;
    
    AccessibilityRole role = roleValue();
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
    if (node() && node()->hasTagName(bodyTag))
        return false;

    // An SVG root is focusable by default, but it's probably not interactive, so don't
    // include it. It can still be made accessible by giving it an ARIA role.
    if (role == AccessibilityRole::SVGRoot)
        return false;

    return true;
}

AccessibilityObject* AccessibilityNodeObject::correspondingControlForLabelElement() const
{
    auto* labelElement = labelElementContainer();
    if (!labelElement)
        return nullptr;

    auto correspondingControl = labelElement->control();
    if (!correspondingControl)
        return nullptr;

    // Make sure the corresponding control isn't a descendant of this label that's in the middle of being destroyed.
    if (correspondingControl->renderer() && !correspondingControl->renderer()->parent())
        return nullptr;

    return axObjectCache()->getOrCreate(correspondingControl.get());
}

AccessibilityObject* AccessibilityNodeObject::correspondingLabelForControlElement() const
{
    // ARIA: section 2A, bullet #3 says if aria-labeledby or aria-label appears, it should
    // override the "label" element association.
    if (hasTextAlternative())
        return nullptr;

    return axObjectCache()->getOrCreate(labelForNode(node()).get());
}

String AccessibilityNodeObject::ariaAccessibilityDescription() const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        return ariaLabeledBy;

    const AtomString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    return String();
}

static Element* siblingWithAriaRole(Node* node, ASCIILiteral role)
{
    // FIXME: Either we should add a null check here or change the function to take a reference instead of a pointer.
    ContainerNode* parent = node->parentNode();
    if (!parent)
        return nullptr;

    for (auto& sibling : childrenOfType<Element>(*parent)) {
        // FIXME: Should skip sibling that is the same as the node.
        if (equalIgnoringASCIICase(sibling.attributeWithoutSynchronization(roleAttr), role))
            return &sibling;
    }

    return nullptr;
}

Element* AccessibilityNodeObject::menuElementForMenuButton() const
{
    if (ariaRoleAttribute() != AccessibilityRole::MenuButton)
        return nullptr;

    return siblingWithAriaRole(node(), "menu"_s);
}

AccessibilityObject* AccessibilityNodeObject::menuForMenuButton() const
{
    if (AXObjectCache* cache = axObjectCache())
        return cache->getOrCreate(menuElementForMenuButton());
    return nullptr;
}

Element* AccessibilityNodeObject::menuItemElementForMenu() const
{
    if (ariaRoleAttribute() != AccessibilityRole::Menu)
        return nullptr;
    
    return siblingWithAriaRole(node(), "menuitem"_s);
}

AccessibilityObject* AccessibilityNodeObject::menuButtonForMenu() const
{
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;

    Element* menuItem = menuItemElementForMenu();

    if (menuItem) {
        // ARIA just has generic menu items. AppKit needs to know if this is a top level items like MenuBarButton or MenuBarItem
        AccessibilityObject* menuItemAX = cache->getOrCreate(menuItem);
        if (menuItemAX && menuItemAX->isMenuButton())
            return menuItemAX;
    }
    return nullptr;
}

AccessibilityObject* AccessibilityNodeObject::captionForFigure() const
{
    if (!isFigureElement())
        return nullptr;
    
    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return nullptr;
    
    Node* node = this->node();
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(figcaptionTag))
            return cache->getOrCreate(child);
    }
    return nullptr;
}

bool AccessibilityNodeObject::usesAltTagForTextComputation() const
{
    bool usesAltTag = isImage() || isInputImage() || isNativeImage() || isCanvas() || (node() && node()->hasTagName(imgTag));
#if ENABLE(MODEL_ELEMENT)
    usesAltTag |= isModel();
#endif
    return usesAltTag;
}

bool AccessibilityNodeObject::isLabelable() const
{
    Node* node = this->node();
    if (!node)
        return false;
    
    return is<HTMLInputElement>(*node) || isControl() || isProgressIndicator() || isMeter();
}

String AccessibilityNodeObject::textForLabelElements(Vector<Ref<HTMLLabelElement>>&& labelElements) const
{
    auto* cache = axObjectCache();
    // https://www.w3.org/TR/html-aam-1.0/#input-type-text-input-type-password-input-type-number-input-type-search-input-type-tel-input-type-email-input-type-url-and-textarea-element-accessible-name-computation
    // "...if more than one label is associated; concatenate by DOM order, delimited by spaces."
    StringBuilder result;
    for (auto& labelElement : labelElements) {
        auto appendLabel = [&] (String&& string) {
            if (string.isEmpty())
                return;

            if (!result.isEmpty())
                result.append(" ");
            result.append(WTFMove(string));
        };

        // Check to see if there's aria-labelledby attribute on the label element.
        auto* axLabel = cache ? cache->getOrCreate(labelElement.ptr()) : nullptr;
        auto ariaLabeledByText = axLabel ? axLabel->ariaLabeledByAttribute() : String();
        if (!ariaLabeledByText.isEmpty())
            appendLabel(WTFMove(ariaLabeledByText));
        else
            appendLabel(accessibleNameForNode(labelElement.ptr()));
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

void AccessibilityNodeObject::titleElementText(Vector<AccessibilityText>& textOrder) const
{
    RefPtr node = this->node();
    if (!node)
        return;
    
    // Only use <label> text if there's no ARIA override.
    if (isLabelable() && !ariaAccessibilityDescription()) {
        String labelText = textForLabelElements(labelsForNode(node.get()));
        if (!labelText.isEmpty()) {
            textOrder.append(AccessibilityText(WTFMove(labelText), isMeter() ? AccessibilityTextSource::Alternative : AccessibilityTextSource::LabelByElement));
            return;
        }
    }
    
    if (titleUIElement())
        textOrder.append(AccessibilityText(String(), AccessibilityTextSource::LabelByElement));
}

AccessibilityObject* AccessibilityNodeObject::titleUIElement() const
{
    // If aria-label is a non-empty string, return null to force clients to use the aria-label.
    if (!getAttribute(aria_labelAttr).isEmpty())
        return nullptr;

    if (isFigureElement())
        return captionForFigure();
    return axObjectCache()->getOrCreate(labelForNode(node()).get());
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
            textOrder.append(AccessibilityText(webAreaText, AccessibilityTextSource::Alternative));
        return;
    }
    
    ariaLabeledByText(textOrder);
    
    const AtomString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        textOrder.append(AccessibilityText(ariaLabel, AccessibilityTextSource::Alternative));
    
    if (usesAltTagForTextComputation()) {
        if (auto* renderImage = dynamicDowncast<RenderImage>(renderer())) {
            String renderAltText = renderImage->altText();

            // RenderImage will return title as a fallback from altText, but we don't want title here because we consider that in helpText.
            if (!renderAltText.isEmpty() && renderAltText != getAttribute(titleAttr)) {
                textOrder.append(AccessibilityText(renderAltText, AccessibilityTextSource::Alternative));
                return;
            }
        }
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        const AtomString& alt = getAttribute(altAttr);
        if (!alt.isEmpty())
            textOrder.append(AccessibilityText(alt, AccessibilityTextSource::Alternative));
    }
    
    Node* node = this->node();
    if (!node)
        return;
    
    auto objectCache = axObjectCache();
    // The fieldset element derives its alternative text from the first associated legend element if one is available.
    if (RefPtr fieldset = dynamicDowncast<HTMLFieldSetElement>(*node); fieldset && objectCache) {
        AccessibilityObject* object = objectCache->getOrCreate(fieldset->legend());
        if (object && !object->isHidden())
            textOrder.append(AccessibilityText(accessibleNameForNode(object->node()), AccessibilityTextSource::Alternative));
    }
    
    // The figure element derives its alternative text from the first associated figcaption element if one is available.
    if (isFigureElement()) {
        AccessibilityObject* captionForFigure = this->captionForFigure();
        if (captionForFigure && !captionForFigure->isHidden())
            textOrder.append(AccessibilityText(accessibleNameForNode(captionForFigure->node()), AccessibilityTextSource::Alternative));
    }
    
    // Tree items missing a label are labeled by all child elements.
    if (isTreeItem() && ariaLabel.isEmpty() && ariaLabeledByAttribute().isEmpty())
        textOrder.append(AccessibilityText(accessibleNameForNode(node), AccessibilityTextSource::Alternative));
    
#if ENABLE(MATHML)
    if (node->isMathMLElement())
        textOrder.append(AccessibilityText(getAttribute(MathMLNames::alttextAttr), AccessibilityTextSource::Alternative));
#endif
}

void AccessibilityNodeObject::visibleText(Vector<AccessibilityText>& textOrder) const
{
    Node* node = this->node();
    if (!node)
        return;

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node); input && input->isTextButton()) {
        textOrder.append(AccessibilityText(input->valueWithDefault(), AccessibilityTextSource::Visible));
        return;
    }

    // If this node isn't rendered, there's no inner text we can extract from a select element.
    if (!isAccessibilityRenderObject() && node->hasTagName(selectTag))
        return;
    
    bool useTextUnderElement = false;
    
    switch (roleValue()) {
    case AccessibilityRole::PopUpButton:
        // Native popup buttons should not use their button children's text as a title. That value is retrieved through stringValue().
        if (node->hasTagName(selectTag))
            break;
        FALLTHROUGH;
    case AccessibilityRole::Summary:
        // The text node for a <summary> element should be included in its visible text, unless a title attribute is present.
        if (!hasAttribute(titleAttr))
            useTextUnderElement = true;
        break;
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ListBoxOption:
    // MacOS does not expect native <li> elements to expose label information, it only expects leaf node elements to do that.
#if !PLATFORM(COCOA)
    case AccessibilityRole::ListItem:
#endif
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
        useTextUnderElement = true;
        break;
    default:
        break;
    }
    
    // If it's focusable but it's not content editable or a known control type, then it will appear to
    // the user as a single atomic object, so we should use its text as the default title.
    if (isHeading() || isLink())
        useTextUnderElement = true;
    
    if (isOutput())
        useTextUnderElement = true;
    
    if (useTextUnderElement) {
        AccessibilityTextUnderElementMode mode;
        
        // Headings often include links as direct children. Those links need to be included in text under element.
        if (isHeading())
            mode.includeFocusableContent = true;

        String text = textUnderElement(mode);
        if (!text.isEmpty())
            textOrder.append(AccessibilityText(text, AccessibilityTextSource::Children));
    }
}

void AccessibilityNodeObject::helpText(Vector<AccessibilityText>& textOrder) const
{
    const AtomString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
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
        if (const auto* parent = Accessibility::findAncestor<AccessibilityObject>(*this, false, WTFMove(matchFunc)))
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
    titleElementText(textOrder);
    alternativeText(textOrder);
    visibleText(textOrder);
    helpText(textOrder);
    
    String placeholder = placeholderValue();
    if (!placeholder.isEmpty())
        textOrder.append(AccessibilityText(placeholder, AccessibilityTextSource::Placeholder));
}
    
void AccessibilityNodeObject::ariaLabeledByText(Vector<AccessibilityText>& textOrder) const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        textOrder.append(AccessibilityText(ariaLabeledBy, AccessibilityTextSource::Alternative));
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
    
    Document* document = this->document();
    if (!document)
        return String();
    
    // Check if the HTML element has an aria-label for the webpage.
    if (Element* documentElement = document->documentElement()) {
        const AtomString& ariaLabel = documentElement->attributeWithoutSynchronization(aria_labelAttr);
        if (!ariaLabel.isEmpty())
            return ariaLabel;
    }
    
    if (auto* owner = document->ownerElement()) {
        if (owner->hasTagName(frameTag) || owner->hasTagName(iframeTag)) {
            const AtomString& title = owner->attributeWithoutSynchronization(titleAttr);
            if (!title.isEmpty())
                return title;
        }
        return owner->getNameAttribute();
    }
    
    String documentTitle = document->title();
    if (!documentTitle.isEmpty())
        return documentTitle;
    
    if (auto* body = document->bodyOrFrameset())
        return body->getNameAttribute();
    
    return String();
}

String AccessibilityNodeObject::description() const
{
    // Static text should not have a description, it should only have a stringValue.
    if (roleValue() == AccessibilityRole::StaticText)
        return { };

    String ariaDescription = ariaAccessibilityDescription();
    if (!ariaDescription.isEmpty())
        return ariaDescription;

    if (usesAltTagForTextComputation()) {
        // Images should use alt as long as the attribute is present, even if empty.                    
        // Otherwise, it should fallback to other methods, like the title attribute.                    
        const AtomString& alt = getAttribute(altAttr);
        if (!alt.isNull())
            return alt;
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

    switch (roleValue()) {
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
    if (!ariaHelp.isEmpty())
        return ariaHelp;

    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;

    String description = this->description();
    for (Node* ancestor = node.get(); ancestor; ancestor = ancestor->parentNode()) {
        if (auto* element = dynamicDowncast<HTMLElement>(ancestor)) {
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
        if (auto* axAncestor = cache->getOrCreate(ancestor)) {
            if (!axAncestor->isGroup() && axAncestor->roleValue() != AccessibilityRole::Unknown)
                break;
        }
    }

    return { };
}

URL AccessibilityNodeObject::url() const
{
    auto* node = this->node();
    if (RefPtr anchor = dynamicDowncast<HTMLAnchorElement>(node); anchor && isLink())
        return anchor->href();

    if (RefPtr image = dynamicDowncast<HTMLImageElement>(node); image && isImage())
        return image->src();

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(node); input && isInputImage())
        return input->src();

#if ENABLE(VIDEO)
    if (RefPtr video = dynamicDowncast<HTMLVideoElement>(node); video && isVideo())
        return video->currentSrc();
#endif

    return URL();
}

unsigned AccessibilityNodeObject::hierarchicalLevel() const
{
    Node* node = this->node();
    if (!is<Element>(node))
        return 0;

    auto& element = downcast<Element>(*node);
    if (!element.attributeWithoutSynchronization(aria_levelAttr).isEmpty())
        return element.getIntegralAttribute(aria_levelAttr);

    // Only tree item will calculate its level through the DOM currently.
    if (roleValue() != AccessibilityRole::TreeItem)
        return 0;
    
    // Hierarchy leveling starts at 1, to match the aria-level spec.
    // We measure tree hierarchy by the number of groups that the item is within.
    unsigned level = 1;
    for (AccessibilityObject* parent = parentObject(); parent; parent = parent->parentObject()) {
        AccessibilityRole parentRole = parent->ariaRoleAttribute();
        if (parentRole == AccessibilityRole::ApplicationGroup)
            level++;
        else if (parentRole == AccessibilityRole::Tree)
            break;
    }
    
    return level;
}

void AccessibilityNodeObject::setIsExpanded(bool expand)
{
    if (is<HTMLDetailsElement>(node())) {
        auto& details = downcast<HTMLDetailsElement>(*node());
        if (expand != details.hasAttribute(openAttr))
            details.toggleOpen();
    }
}
    
// When building the textUnderElement for an object, determine whether or not
// we should include the inner text of this given descendant object or skip it.
static bool shouldUseAccessibilityObjectInnerText(AccessibilityObject* obj, AccessibilityTextUnderElementMode mode)
{
    // Do not use any heuristic if we are explicitly asking to include all the children.
    if (mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren)
        return true;

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
    if (mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeNameFromContentsChildren
        && !obj->accessibleNameDerivesFromContent())
        return false;
    
    if (equalLettersIgnoringASCIICase(obj->getAttribute(aria_hiddenAttr), "true"_s))
        return false;
    
    // If something doesn't expose any children, then we can always take the inner text content.
    // This is what we want when someone puts an <a> inside a <button> for example.
    if (obj->isDescendantOfBarrenParent())
        return true;
    
    // Skip focusable children, so we don't include the text of links and controls.
    if (obj->canSetFocusAttribute() && !mode.includeFocusableContent)
        return false;

    // Skip big container elements like lists, tables, etc.
    if (is<AccessibilityList>(*obj))
        return false;

    if (is<AccessibilityTable>(*obj) && downcast<AccessibilityTable>(*obj).isExposable())
        return false;

    if (obj->isTree() || obj->isCanvas())
        return false;

#if ENABLE(MODEL_ELEMENT)
    if (obj->isModel())
        return false;
#endif

    return true;
}

static bool shouldAddSpaceBeforeAppendingNextElement(StringBuilder& builder, const String& childText)
{
    if (!builder.length() || !childText.length())
        return false;

    // We don't need to add an additional space before or after a line break.
    return !(isHTMLLineBreak(childText[0]) || isHTMLLineBreak(builder[builder.length() - 1]));
}
    
static void appendNameToStringBuilder(StringBuilder& builder, const String& text)
{
    if (shouldAddSpaceBeforeAppendingNextElement(builder, text))
        builder.append(' ');
    builder.append(text);
}

String AccessibilityNodeObject::textUnderElement(AccessibilityTextUnderElementMode mode) const
{
    Node* node = this->node();
    if (is<Text>(node))
        return downcast<Text>(*node).wholeText();

    bool isAriaVisible = Accessibility::findAncestor<AccessibilityObject>(*this, true, [] (const AccessibilityObject& object) {
        return equalLettersIgnoringASCIICase(object.getAttribute(aria_hiddenAttr), "false"_s);
    }) != nullptr;

    // The Accname specification states that if the current node is hidden, and not directly
    // referenced by aria-labelledby or aria-describedby, and is not a host language text
    // alternative, the empty string should be returned.
    if (isDOMHidden() && !isAriaVisible && !is<HTMLLabelElement>(node) && (node && !ancestorsOfType<HTMLCanvasElement>(*node).first())) {
        if (labelForObjects().isEmpty() && descriptionForObjects().isEmpty())
            return { };
    }

    StringBuilder builder;
    for (RefPtr child = firstChild(); child; child = child->nextSibling()) {
        if (mode.ignoredChildNode && child->node() == mode.ignoredChildNode)
            continue;
        
        bool shouldDeriveNameFromAuthor = (mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeNameFromContentsChildren && !child->accessibleNameDerivesFromContent());
        if (shouldDeriveNameFromAuthor) {
            appendNameToStringBuilder(builder, accessibleNameForNode(child->node()));
            continue;
        }
        
        if (!shouldUseAccessibilityObjectInnerText(child.get(), mode))
            continue;

        if (is<AccessibilityNodeObject>(*child)) {
            // We should ignore the child if it's labeled by this node.
            // This could happen when this node labels multiple child nodes and we didn't
            // skip in the above ignoredChildNode check.
            auto labeledByElements = downcast<AccessibilityNodeObject>(*child).ariaLabeledByElements();
            if (labeledByElements.containsIf([&](auto& element) { return element.ptr() == node; }))
                continue;
            
            Vector<AccessibilityText> textOrder;
            downcast<AccessibilityNodeObject>(*child).alternativeText(textOrder);
            if (textOrder.size() > 0 && textOrder[0].text.length()) {
                appendNameToStringBuilder(builder, textOrder[0].text);
                continue;
            }
        }

        if (node) {
            auto* childParentElement = child->node() ? child->node()->parentElement() : nullptr;
            auto isInShadowTree = [] (Node& node) {
                return node.isInShadowTree() || node.isInUserAgentShadowTree();
            };
            // Do not take the textUnderElement for a different element (determined by child's element parent not being us). Otherwise we may doubly-expose the same text.
            // But for now, don't do this if either node is in the shadow DOM, as determining descendancy is suprisingly tricky. We may want to change this in the future.
            if (childParentElement && childParentElement != node && !isInShadowTree(*node) && !isInShadowTree(*child->node()))
                continue;
        }

        String childText = child->textUnderElement(mode);
        if (childText.length())
            appendNameToStringBuilder(builder, WTFMove(childText));
    }

    return builder.toString().trim(deprecatedIsSpaceOrNewline).simplifyWhiteSpace(isHTMLSpaceButNotLineBreak);
}

String AccessibilityNodeObject::title() const
{
    WeakPtr node = this->node();
    if (!node)
        return { };

    if (RefPtr input = dynamicDowncast<HTMLInputElement>(*node); input && input->isTextButton())
        return input->valueWithDefault();

    if (isLabelable()) {
        auto labels = labelsForNode(node.get());
        // Use the label text as the title if there's no ARIA override.
        if (!labels.isEmpty() && !ariaAccessibilityDescription().length())
            return textForLabelElements(WTFMove(labels));
    }

    // For <select> elements, title should be empty if they are not rendered or have role PopUpButton.
    if (node && node->hasTagName(selectTag)
        && (!isAccessibilityRenderObject() || roleValue() == AccessibilityRole::PopUpButton))
        return { };

    switch (roleValue()) {
    case AccessibilityRole::Button:
    case AccessibilityRole::Checkbox:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
    case AccessibilityRole::ToggleButton:
        return textUnderElement();
    // SVGRoots should not use the text under itself as a title. That could include the text of objects like <text>.
    case AccessibilityRole::SVGRoot:
        return String();
    default:
        break;
    }

    if (isLink())
        return textUnderElement();
    if (isHeading())
        return textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeSkipIgnoredChildren, true));

    return { };
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

    if (roleValue() == AccessibilityRole::StaticText)
        return textUnderElement();

    if (!isTextControl())
        return { };

    RefPtr element = dynamicDowncast<Element>(node());
    if (RefPtr formControl = dynamicDowncast<HTMLTextFormControlElement>(element); formControl && isNativeTextControl())
        return formControl->value();
    return element ? element->innerText() : String();
}

String AccessibilityNodeObject::stringValue() const
{
    Node* node = this->node();
    if (!node)
        return { };

    if (isARIAStaticText()) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }

    if (node->isTextNode())
        return textUnderElement();

    if (RefPtr selectElement = dynamicDowncast<HTMLSelectElement>(*node)) {
        int selectedIndex = selectElement->selectedIndex();
        auto& listItems = selectElement->listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            if (RefPtr selectedItem = listItems[selectedIndex].get()) {
                const AtomString& overriddenDescription = selectedItem->attributeWithoutSynchronization(aria_labelAttr);
                if (!overriddenDescription.isNull())
                    return overriddenDescription;
            }
        }
        if (!selectElement->multiple())
            return selectElement->value();
        return String();
    }

    if (isTextControl())
        return text();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return { };
}

SRGBA<uint8_t> AccessibilityNodeObject::colorValue() const
{
#if !ENABLE(INPUT_TYPE_COLOR)
    return Color::transparentBlack;
#else
    if (!isColorWell())
        return Color::transparentBlack;

    RefPtr input = dynamicDowncast<HTMLInputElement>(node());
    if (!input)
        return Color::transparentBlack;

    return input->valueAsColor().toColorTypeLossy<SRGBA<uint8_t>>();
#endif
}

// This function implements the ARIA accessible name as described by the Mozilla                                        
// ARIA Implementer's Guide.                                                                                            
static String accessibleNameForNode(Node* node, Node* labelledbyNode)
{
    ASSERT(node);

    auto* element = dynamicDowncast<Element>(node);
    const AtomString& ariaLabel = element ? element->attributeWithoutSynchronization(aria_labelAttr) : nullAtom();
    if (!ariaLabel.isEmpty())
        return ariaLabel;
    
    const AtomString& alt = element ? element->attributeWithoutSynchronization(altAttr) : nullAtom();
    if (!alt.isEmpty())
        return alt;

    // If the node can be turned into an AX object, we can use standard name computation rules.
    // If however, the node cannot (because there's no renderer e.g.) fallback to using the basic text underneath.
    auto* axObject = node->document().axObjectCache()->getOrCreate(node);
    if (axObject) {
        String valueDescription = axObject->valueDescription();
        if (!valueDescription.isEmpty())
            return valueDescription;

        // The Accname specification states that if the name is being calculated for a combobox
        // or listbox inside a labeling element, return the text alternative of the chosen option.
        AccessibilityObject::AccessibilityChildrenVector selectedChildren;
        if (axObject->isListBox())
            selectedChildren = axObject->selectedChildren();
        else if (axObject->isComboBox()) {
            for (const auto& child : axObject->children()) {
                if (child->isListBox()) {
                    selectedChildren = child->selectedChildren();
                    break;
                }
            }
        }

        StringBuilder builder;
        String childText;
        for (const auto& child : selectedChildren)
            appendNameToStringBuilder(builder, accessibleNameForNode(child->node()));

        childText = builder.toString();
        if (!childText.isEmpty())
            return childText;
    }

    if (auto* input = dynamicDowncast<HTMLInputElement>(element)) {
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
    if (auto* option = dynamicDowncast<HTMLOptionElement>(element))
        return option->value();

    String text;
    if (axObject) {
        if (axObject->accessibleNameDerivesFromContent())
            text = axObject->textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeNameFromContentsChildren, true, labelledbyNode));
    } else
        text = (element ? element->innerText() : node->textContent()).simplifyWhiteSpace(deprecatedIsSpaceOrNewline);

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
            appendNameToStringBuilder(builder, accessibleNameForNode(assignedNode.get()));

        auto assignedNodesText = builder.toString();
        if (!assignedNodesText.isEmpty())
            return assignedNodesText;
    }
    
    return String();
}

String AccessibilityNodeObject::accessibilityDescriptionForChildren() const
{
    Node* node = this->node();
    if (!node)
        return String();

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return String();

    StringBuilder builder;
    for (Node* child = node->firstChild(); child; child = child->nextSibling()) {
        if (!is<Element>(child))
            continue;

        if (AccessibilityObject* axObject = cache->getOrCreate(child)) {
            String description = axObject->ariaLabeledByAttribute();
            if (description.isEmpty())
                description = accessibleNameForNode(child);
            appendNameToStringBuilder(builder, description);
        }
    }

    return builder.toString();
}

String AccessibilityNodeObject::descriptionForElements(const Vector<Ref<Element>>& elements) const
{
    StringBuilder builder;
    for (auto& element : elements)
        appendNameToStringBuilder(builder, accessibleNameForNode(element.ptr(), node()));
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

bool AccessibilityNodeObject::hasAttributesRequiredForInclusion() const
{
    if (AccessibilityObject::hasAttributesRequiredForInclusion())
        return true;

    // Avoid calculating the actual description here, which is expensive.
    // This means there might be more accessible elements in the tree if the labelledBy points to invalid elements, but that shouldn't cause any real problems.
    if (getAttribute(aria_labelledbyAttr).length() || getAttribute(aria_labeledbyAttr).length() || getAttribute(aria_labelAttr).length())
        return true;

    return false;
}

bool AccessibilityNodeObject::isFocused() const
{
    if (!m_node)
        return false;

    auto& document = node()->document();
    auto* focusedElement = document.focusedElement();
    if (!focusedElement)
        return false;

    if (focusedElement == node())
        return true;

    // A web area is represented by the Document node in the DOM tree which isn't focusable.
    // Instead, check if the frame's selection is focused.
    if (roleValue() != AccessibilityRole::WebArea)
        return false;

    auto* frame = document.frame();
    return frame ? frame->selection().isFocusedAndActive() : false;
}

void AccessibilityNodeObject::setFocused(bool on)
{
    // Call the base class setFocused to ensure the view is focused and active.
    AccessibilityObject::setFocused(on);

    if (!canSetFocusAttribute())
        return;

    auto* document = this->document();

    // This is needed or else focus won't always go into iframes with different origins.
    UserGestureIndicator gestureIndicator(IsProcessingUserGesture::Yes, document);

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
    Node* node = this->node();
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
    Node* node = this->node();
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
        Document* document = this->document();
        if (!document)
            return false;

        if (HTMLElement* body = document->bodyOrFrameset()) {
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

    if (ariaRole == "text"_s) {
        auto* document = this->document();
        // FIXME: Eventually, let's remove support for this role entirely. This is tracked by https://bugs.webkit.org/show_bug.cgi?id=260685.
        if (!document || !document->settings().ariaTextRoleEnabled())
            return AccessibilityRole::Unknown;
    }
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
    if ((role == AccessibilityRole::LandmarkRegion || role == AccessibilityRole::Form) && getAttribute(aria_labelAttr).isEmpty() && getAttribute(aria_labelledbyAttr).isEmpty() && getAttribute(aria_labeledbyAttr).isEmpty())
        role = AccessibilityRole::Unknown;

    if (enumToUnderlyingType(role))
        return role;

    return AccessibilityRole::Unknown;
}

AccessibilityRole AccessibilityNodeObject::remapAriaRoleDueToParent(AccessibilityRole role) const
{
    // Some objects change their role based on their parent.
    // However, asking for the unignoredParent calls accessibilityIsIgnored(), which can trigger a loop. 
    // While inside the call stack of creating an element, we need to avoid accessibilityIsIgnored().
    // https://bugs.webkit.org/show_bug.cgi?id=65174

    if (role != AccessibilityRole::ListBoxOption && role != AccessibilityRole::MenuItem)
        return role;
    
    for (AccessibilityObject* parent = parentObject(); parent && !parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
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

bool AccessibilityNodeObject::canSetSelectedAttribute() const
{
    // Elements that can be selected
    switch (roleValue()) {
    case AccessibilityRole::Cell:
    case AccessibilityRole::GridCell:
    case AccessibilityRole::RowHeader:
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

} // namespace WebCore
