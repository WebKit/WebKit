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

#include "AXObjectCache.h"
#include "AccessibilityImageMapLink.h"
#include "AccessibilityList.h"
#include "AccessibilityListBox.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "Editing.h"
#include "ElementIterator.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLCanvasElement.h"
#include "HTMLDetailsElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "LabelableElement.h"
#include "LocalizedStrings.h"
#include "MathMLElement.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "NodeTraversal.h"
#include "ProgressTracker.h"
#include "RenderImage.h"
#include "RenderView.h"
#include "SVGElement.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "UserGestureIndicator.h"
#include "VisibleUnits.h"
#include "Widget.h"
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
    m_role = determineAccessibilityRole();
}

Ref<AccessibilityNodeObject> AccessibilityNodeObject::create(Node* node)
{
    return adoptRef(*new AccessibilityNodeObject(node));
}

void AccessibilityNodeObject::detach(AccessibilityDetachmentType detachmentType, AXObjectCache* cache)
{
    // AccessibilityObject calls clearChildren.
    AccessibilityObject::detach(detachmentType, cache);
    m_node = nullptr;
}

void AccessibilityNodeObject::childrenChanged()
{
    // This method is meant as a quick way of marking a portion of the accessibility tree dirty.
    if (!node() && !renderer())
        return;

    AXObjectCache* cache = axObjectCache();
    if (!cache)
        return;
    cache->postNotification(this, document(), AXObjectCache::AXChildrenChanged);
    
    // Should make the sub tree dirty so that everything below will be updated correctly.
    this->setNeedsToUpdateSubtree();
    bool shouldStopUpdatingParent = false;

    // Go up the accessibility parent chain, but only if the element already exists. This method is
    // called during render layouts, minimal work should be done. 
    // If AX elements are created now, they could interrogate the render tree while it's in a funky state.
    // At the same time, process ARIA live region changes.
    for (AccessibilityObject* parent = this; parent; parent = parent->parentObjectIfExists()) {
        if (!shouldStopUpdatingParent)
            parent->setNeedsToUpdateChildren();
        

        // These notifications always need to be sent because screenreaders are reliant on them to perform. 
        // In other words, they need to be sent even when the screen reader has not accessed this live region since the last update.

        // If this element supports ARIA live regions, then notify the AT of changes.
        // Sometimes this function can be called many times within a short period of time, leading to posting too many AXLiveRegionChanged
        // notifications. To fix this, we used a timer to make sure we only post one notification for the children changes within a pre-defined
        // time interval.
        if (parent->supportsLiveRegion())
            cache->postLiveRegionChangeNotification(parent);
        
        // If this element is an ARIA text control, notify the AT of changes.
        if (parent->isNonNativeTextControl()) {
            cache->postNotification(parent, parent->document(), AXObjectCache::AXValueChanged);
            
            // Do not let the parent that's above the editable ancestor update its children
            // since we already notify the AT of changes.
            shouldStopUpdatingParent = true;
        }
    }
}

void AccessibilityNodeObject::updateAccessibilityRole()
{
    bool ignoredStatus = accessibilityIsIgnored();
    m_role = determineAccessibilityRole();
    
    // The AX hierarchy only needs to be updated if the ignored status of an element has changed.
    if (ignoredStatus != accessibilityIsIgnored())
        childrenChanged();
}
    
AccessibilityObject* AccessibilityNodeObject::firstChild() const
{
    if (!node())
        return nullptr;
    
    Node* firstChild = node()->firstChild();

    if (!firstChild)
        return nullptr;
    
    return axObjectCache()->getOrCreate(firstChild);
}

AccessibilityObject* AccessibilityNodeObject::lastChild() const
{
    if (!node())
        return nullptr;
    
    Node* lastChild = node()->lastChild();
    if (!lastChild)
        return nullptr;
    
    return axObjectCache()->getOrCreate(lastChild);
}

AccessibilityObject* AccessibilityNodeObject::previousSibling() const
{
    if (!node())
        return nullptr;

    Node* previousSibling = node()->previousSibling();
    if (!previousSibling)
        return nullptr;

    return axObjectCache()->getOrCreate(previousSibling);
}

AccessibilityObject* AccessibilityNodeObject::nextSibling() const
{
    if (!node())
        return nullptr;

    Node* nextSibling = node()->nextSibling();
    if (!nextSibling)
        return nullptr;

    return axObjectCache()->getOrCreate(nextSibling);
}
    
AccessibilityObject* AccessibilityNodeObject::parentObjectIfExists() const
{
    return parentObject();
}
    
AccessibilityObject* AccessibilityNodeObject::parentObject() const
{
    if (!node())
        return nullptr;

    Node* parentObj = node()->parentNode();
    if (!parentObj)
        return nullptr;
    
    if (AXObjectCache* cache = axObjectCache())
        return cache->getOrCreate(parentObj);
    
    return nullptr;
}

LayoutRect AccessibilityNodeObject::elementRect() const
{
    return boundingBoxRect();
}

LayoutRect AccessibilityNodeObject::boundingBoxRect() const
{
    // AccessibilityNodeObjects have no mechanism yet to return a size or position.
    // For now, let's return the position of the ancestor that does have a position,
    // and make it the width of that parent, and about the height of a line of text, so that it's clear the object is a child of the parent.
    
    LayoutRect boundingBox;
    
    for (AccessibilityObject* positionProvider = parentObject(); positionProvider; positionProvider = positionProvider->parentObject()) {
        if (positionProvider->isAccessibilityRenderObject()) {
            LayoutRect parentRect = positionProvider->elementRect();
            boundingBox.setSize(LayoutSize(parentRect.width(), LayoutUnit(std::min(10.0f, parentRect.height().toFloat()))));
            boundingBox.setLocation(parentRect.location());
            break;
        }
    }
    
    return boundingBox;
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

AccessibilityRole AccessibilityNodeObject::determineAccessibilityRole()
{
    if (!node())
        return AccessibilityRole::Unknown;

    if ((m_ariaRole = determineAriaRoleAttribute()) != AccessibilityRole::Unknown)
        return m_ariaRole;
    
    if (node()->isLink())
        return AccessibilityRole::WebCoreLink;
    if (node()->isTextNode())
        return AccessibilityRole::StaticText;
    if (node()->hasTagName(buttonTag))
        return buttonRoleType();
    if (is<HTMLInputElement>(*node())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node());
        if (input.isCheckbox())
            return AccessibilityRole::CheckBox;
        if (input.isRadioButton())
            return AccessibilityRole::RadioButton;
        if (input.isTextButton())
            return buttonRoleType();
        if (input.isRangeControl())
            return AccessibilityRole::Slider;
        if (input.isInputTypeHidden())
            return AccessibilityRole::Ignored;
        if (input.isSearchField())
            return AccessibilityRole::SearchField;
#if ENABLE(INPUT_TYPE_COLOR)
        if (input.isColorControl())
            return AccessibilityRole::ColorWell;
#endif
        return AccessibilityRole::TextField;
    }
    if (node()->hasTagName(selectTag)) {
        HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*node());
        return selectElement.multiple() ? AccessibilityRole::ListBox : AccessibilityRole::PopUpButton;
    }
    if (is<HTMLTextAreaElement>(*node()))
        return AccessibilityRole::TextArea;
    if (headingLevel())
        return AccessibilityRole::Heading;
    if (node()->hasTagName(blockquoteTag))
        return AccessibilityRole::Blockquote;
    if (node()->hasTagName(divTag))
        return AccessibilityRole::Div;
    if (node()->hasTagName(pTag))
        return AccessibilityRole::Paragraph;
    if (is<HTMLLabelElement>(*node()))
        return AccessibilityRole::Label;
    if (is<Element>(*node()) && downcast<Element>(*node()).isFocusable())
        return AccessibilityRole::Group;
    
    return AccessibilityRole::Unknown;
}

void AccessibilityNodeObject::addChildren()
{
    // If the need to add more children in addition to existing children arises, 
    // childrenChanged should have been called, leaving the object with no children.
    ASSERT(!m_haveChildren); 
    
    if (!m_node)
        return;

    m_haveChildren = true;

    // The only time we add children from the DOM tree to a node with a renderer is when it's a canvas.
    if (renderer() && !m_node->hasTagName(canvasTag))
        return;
    
    for (Node* child = m_node->firstChild(); child; child = child->nextSibling())
        addChild(axObjectCache()->getOrCreate(child));
    
    m_subtreeDirty = false;
}

bool AccessibilityNodeObject::canHaveChildren() const
{
    // If this is an AccessibilityRenderObject, then it's okay if this object
    // doesn't have a node - there are some renderers that don't have associated
    // nodes, like scroll areas and css-generated text.
    if (!node() && !isAccessibilityRenderObject())
        return false;

    // When <noscript> is not being used (its renderer() == 0), ignore its children.
    if (node() && !renderer() && node()->hasTagName(noscriptTag))
        return false;
    
    // Elements that should not have children
    switch (roleValue()) {
    case AccessibilityRole::Image:
    case AccessibilityRole::Button:
    case AccessibilityRole::PopUpButton:
    case AccessibilityRole::CheckBox:
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
        return false;
    case AccessibilityRole::DocumentMath:
#if ENABLE(MATHML)
        return node()->isMathMLElement();
#endif
        return false;
    default:
        return true;
    }
}

bool AccessibilityNodeObject::computeAccessibilityIsIgnored() const
{
#ifndef NDEBUG
    // Double-check that an AccessibilityObject is never accessed before
    // it's been initialized.
    ASSERT(m_initialized);
#endif

    // Handle non-rendered text that is exposed through aria-hidden=false.
    if (m_node && m_node->isTextNode() && !renderer()) {
        // Fallback content in iframe nodes should be ignored.
        if (m_node->parentNode() && m_node->parentNode()->hasTagName(iframeTag) && m_node->parentNode()->renderer())
            return true;

        // Whitespace only text elements should be ignored when they have no renderer.
        String string = stringValue().stripWhiteSpace().simplifyWhiteSpace();
        if (!string.length())
            return true;
    }

    AccessibilityObjectInclusion decision = defaultObjectInclusion();
    if (decision == AccessibilityObjectInclusion::IncludeObject)
        return false;
    if (decision == AccessibilityObjectInclusion::IgnoreObject)
        return true;
    // If this element is within a parent that cannot have children, it should not be exposed.
    if (isDescendantOfBarrenParent())
        return true;

    if (roleValue() == AccessibilityRole::Ignored)
        return true;
    
    return m_role == AccessibilityRole::Unknown;
}

bool AccessibilityNodeObject::canvasHasFallbackContent() const
{
    Node* node = this->node();
    if (!is<HTMLCanvasElement>(node))
        return false;
    HTMLCanvasElement& canvasElement = downcast<HTMLCanvasElement>(*node);
    // If it has any children that are elements, we'll assume it might be fallback
    // content. If it has no children or its only children are not elements
    // (e.g. just text nodes), it doesn't have fallback content.
    return childrenOfType<Element>(canvasElement).first();
}

bool AccessibilityNodeObject::isImageButton() const
{
    return isNativeImage() && isButton();
}

bool AccessibilityNodeObject::isNativeTextControl() const
{
    Node* node = this->node();
    if (!node)
        return false;

    if (is<HTMLTextAreaElement>(*node))
        return true;

    if (is<HTMLInputElement>(*node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        return input.isText() || input.isNumberField();
    }

    return false;
}

bool AccessibilityNodeObject::isSearchField() const
{
    Node* node = this->node();
    if (!node)
        return false;

    if (roleValue() == AccessibilityRole::SearchField)
        return true;

    if (!is<HTMLInputElement>(*node))
        return false;

    auto& inputElement = downcast<HTMLInputElement>(*node);

    // Some websites don't label their search fields as such. However, they will
    // use the word "search" in either the form or input type. This won't catch every case,
    // but it will catch google.com for example.

    // Check the node name of the input type, sometimes it's "search".
    const AtomicString& nameAttribute = getAttribute(nameAttr);
    if (nameAttribute.containsIgnoringASCIICase("search"))
        return true;

    // Check the form action and the name, which will sometimes be "search".
    auto* form = inputElement.form();
    if (form && (form->name().containsIgnoringASCIICase("search") || form->action().containsIgnoringASCIICase("search")))
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

    if (is<HTMLInputElement>(*node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        return input.isImageButton();
    }

    return false;
}

bool AccessibilityNodeObject::isImage() const
{
    return roleValue() == AccessibilityRole::Image;
}

bool AccessibilityNodeObject::isPasswordField() const
{
    auto* node = this->node();
    if (!is<HTMLInputElement>(node))
        return false;

    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    return downcast<HTMLInputElement>(*node).isPasswordField();
}

AccessibilityObject* AccessibilityNodeObject::passwordFieldOrContainingPasswordField()
{
    Node* node = this->node();
    if (!node)
        return nullptr;

    if (is<HTMLInputElement>(*node) && downcast<HTMLInputElement>(*node).isPasswordField())
        return this;

    auto* element = node->shadowHost();
    if (!is<HTMLInputElement>(element))
        return nullptr;

    if (auto* cache = axObjectCache())
        return cache->getOrCreate(element);

    return nullptr;
}

bool AccessibilityNodeObject::isInputImage() const
{
    Node* node = this->node();
    if (is<HTMLInputElement>(node) && roleValue() == AccessibilityRole::Button) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        return input.isImageButton();
    }

    return false;
}

bool AccessibilityNodeObject::isProgressIndicator() const
{
    return roleValue() == AccessibilityRole::ProgressIndicator;
}

bool AccessibilityNodeObject::isSlider() const
{
    return roleValue() == AccessibilityRole::Slider;
}

bool AccessibilityNodeObject::isMenuRelated() const
{
    switch (roleValue()) {
    case AccessibilityRole::Menu:
    case AccessibilityRole::MenuBar:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
        return true;
    default:
        return false;
    }
}

bool AccessibilityNodeObject::isMenu() const
{
    return roleValue() == AccessibilityRole::Menu;
}

bool AccessibilityNodeObject::isMenuBar() const
{
    return roleValue() == AccessibilityRole::MenuBar;
}

bool AccessibilityNodeObject::isMenuButton() const
{
    return roleValue() == AccessibilityRole::MenuButton;
}

bool AccessibilityNodeObject::isMenuItem() const
{
    switch (roleValue()) {
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::MenuItemCheckbox:
        return true;
    default:
        return false;
    }
}

bool AccessibilityNodeObject::isNativeCheckboxOrRadio() const
{
    Node* node = this->node();
    if (!is<HTMLInputElement>(node))
        return false;

    auto& input = downcast<HTMLInputElement>(*node);
    return input.isCheckbox() || input.isRadioButton();
}

bool AccessibilityNodeObject::isEnabled() const
{
    // ARIA says that the disabled status applies to the current element and all descendant elements.
    for (AccessibilityObject* object = const_cast<AccessibilityNodeObject*>(this); object; object = object->parentObject()) {
        const AtomicString& disabledStatus = object->getAttribute(aria_disabledAttr);
        if (equalLettersIgnoringASCIICase(disabledStatus, "true"))
            return false;
        if (equalLettersIgnoringASCIICase(disabledStatus, "false"))
            break;
    }
    
    if (roleValue() == AccessibilityRole::HorizontalRule)
        return false;
    
    Node* node = this->node();
    if (!is<Element>(node))
        return true;

    return !downcast<Element>(*node).isDisabledFormControl();
}

bool AccessibilityNodeObject::isIndeterminate() const
{
    return equalLettersIgnoringASCIICase(getAttribute(indeterminateAttr), "true");
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
        return equalLettersIgnoringASCIICase(getAttribute(aria_pressedAttr), "true");

    if (!is<Element>(*node))
        return false;
    return downcast<Element>(*node).active();
}

bool AccessibilityNodeObject::isChecked() const
{
    Node* node = this->node();
    if (!node)
        return false;

    // First test for native checkedness semantics
    if (is<HTMLInputElement>(*node))
        return downcast<HTMLInputElement>(*node).shouldAppearChecked();

    // Else, if this is an ARIA checkbox or radio, respect the aria-checked attribute
    bool validRole = false;
    switch (ariaRoleAttribute()) {
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::Switch:
        validRole = true;
        break;
    default:
        break;
    }
    
    if (validRole && equalLettersIgnoringASCIICase(getAttribute(aria_checkedAttr), "true"))
        return true;

    return false;
}

bool AccessibilityNodeObject::isHovered() const
{
    Node* node = this->node();
    return is<Element>(node) && downcast<Element>(*node).hovered();
}

bool AccessibilityNodeObject::isMultiSelectable() const
{
    const AtomicString& ariaMultiSelectable = getAttribute(aria_multiselectableAttr);
    if (equalLettersIgnoringASCIICase(ariaMultiSelectable, "true"))
        return true;
    if (equalLettersIgnoringASCIICase(ariaMultiSelectable, "false"))
        return false;
    
    return node() && node()->hasTagName(selectTag) && downcast<HTMLSelectElement>(*node()).multiple();
}

bool AccessibilityNodeObject::isRequired() const
{
    // Explicit aria-required values should trump native required attributes.
    const AtomicString& requiredValue = getAttribute(aria_requiredAttr);
    if (equalLettersIgnoringASCIICase(requiredValue, "true"))
        return true;
    if (equalLettersIgnoringASCIICase(requiredValue, "false"))
        return false;

    Node* n = this->node();
    if (is<HTMLFormControlElement>(n))
        return downcast<HTMLFormControlElement>(*n).isRequired();

    return false;
}

bool AccessibilityNodeObject::supportsRequiredAttribute() const
{
    switch (roleValue()) {
    case AccessibilityRole::Button:
        return isFileUploadButton();
    case AccessibilityRole::Cell:
    case AccessibilityRole::ColumnHeader:
    case AccessibilityRole::CheckBox:
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
    case AccessibilityRole::TableHeaderContainer:
    case AccessibilityRole::TextArea:
    case AccessibilityRole::TextField:
    case AccessibilityRole::ToggleButton:
        return true;
    default:
        return false;
    }
}

int AccessibilityNodeObject::headingLevel() const
{
    // headings can be in block flow and non-block flow
    Node* node = this->node();
    if (!node)
        return false;

    if (isHeading()) {
        int ariaLevel = getAttribute(aria_levelAttr).toInt();
        if (ariaLevel > 0)
            return ariaLevel;
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

    // The implicit value of aria-level is 2 for the heading role.
    // https://www.w3.org/TR/wai-aria-1.1/#heading
    if (ariaRoleAttribute() == AccessibilityRole::Heading)
        return 2;

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
    if (is<HTMLInputElement>(node())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node());
        if (input.isRangeControl())
            return input.valueAsNumber();
    }

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
    if (is<HTMLInputElement>(node())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node());
        if (input.isRangeControl())
            return input.maximum();
    }

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
    if (is<HTMLInputElement>(node())) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node());
        if (input.isRangeControl())
            return input.minimum();
    }

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

bool AccessibilityNodeObject::isHeading() const
{
    return roleValue() == AccessibilityRole::Heading;
}

bool AccessibilityNodeObject::isLink() const
{
    return roleValue() == AccessibilityRole::WebCoreLink;
}

bool AccessibilityNodeObject::isControl() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return is<HTMLFormControlElement>(*node) || AccessibilityObject::isARIAControl(ariaRoleAttribute());
}

bool AccessibilityNodeObject::isFieldset() const
{
    Node* node = this->node();
    if (!node)
        return false;

    return node->hasTagName(fieldsetTag);
}

bool AccessibilityNodeObject::isGroup() const
{
    AccessibilityRole role = roleValue();
    return role == AccessibilityRole::Group || role == AccessibilityRole::TextGroup || role == AccessibilityRole::ApplicationGroup || role == AccessibilityRole::ApplicationTextGroup;
}

AccessibilityObject* AccessibilityNodeObject::selectedRadioButton()
{
    if (!isRadioGroup())
        return nullptr;

    // Find the child radio button that is selected (ie. the intValue == 1).
    for (const auto& child : children()) {
        if (child->roleValue() == AccessibilityRole::RadioButton && child->checkboxOrRadioValue() == AccessibilityButtonState::On)
            return child.get();
    }
    return nullptr;
}

AccessibilityObject* AccessibilityNodeObject::selectedTabItem()
{
    if (!isTabList())
        return nullptr;

    // FIXME: Is this valid? ARIA tab items support aria-selected; not aria-checked.
    // Find the child tab item that is selected (ie. the intValue == 1).
    AccessibilityObject::AccessibilityChildrenVector tabs;
    tabChildren(tabs);

    for (const auto& child : children()) {
        if (child->isTabItem() && (child->isChecked() || child->isSelected()))
            return child.get();
    }
    return nullptr;
}

AccessibilityButtonState AccessibilityNodeObject::checkboxOrRadioValue() const
{
    if (isNativeCheckboxOrRadio())
        return isIndeterminate() ? AccessibilityButtonState::Mixed : isChecked() ? AccessibilityButtonState::On : AccessibilityButtonState::Off;

    return AccessibilityObject::checkboxOrRadioValue();
}

Element* AccessibilityNodeObject::anchorElement() const
{
    Node* node = this->node();
    if (!node)
        return nullptr;

    AXObjectCache* cache = axObjectCache();

    // search up the DOM tree for an anchor element
    // NOTE: this assumes that any non-image with an anchor is an HTMLAnchorElement
    for ( ; node; node = node->parentNode()) {
        if (is<HTMLAnchorElement>(*node) || (node->renderer() && cache->getOrCreate(node->renderer())->isLink()))
            return downcast<Element>(node);
    }

    return nullptr;
}

static bool isNodeActionElement(Node* node)
{
    if (is<HTMLInputElement>(*node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        if (!input.isDisabledFormControl() && (input.isRadioButton() || input.isCheckbox() || input.isTextButton() || input.isFileUpload() || input.isImageButton()))
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
    Node* node = this->node();
    if (!node)
        return nullptr;

    // check if our parent is a mouse button listener
    // FIXME: Do the continuation search like anchorElement does
    for (auto& element : elementLineage(is<Element>(*node) ? downcast<Element>(node) : node->parentElement())) {
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

void AccessibilityNodeObject::alterSliderValue(bool increase)
{
    if (roleValue() != AccessibilityRole::Slider)
        return;
    
    auto element = this->element();
    if (!element || element->isDisabledFormControl())
        return;

    if (!getAttribute(stepAttr).isEmpty())
        changeValueByStep(increase);
    else
        changeValueByPercent(increase ? 5 : -5);
}
    
void AccessibilityNodeObject::increment()
{
    if (dispatchAccessibilityEventWithType(AccessibilityEventType::Increment))
        return;
    UserGestureIndicator gestureIndicator(ProcessingUserGesture, document());
    alterSliderValue(true);
}

void AccessibilityNodeObject::decrement()
{
    if (dispatchAccessibilityEventWithType(AccessibilityEventType::Decrement))
        return;
    UserGestureIndicator gestureIndicator(ProcessingUserGesture, document());
    alterSliderValue(false);
}

void AccessibilityNodeObject::changeValueByStep(bool increase)
{
    float step = stepValueForRange();
    float value = valueForRange();

    value += increase ? step : -step;

    setValue(String::number(value));

    axObjectCache()->postNotification(node(), AXObjectCache::AXValueChanged);
}

void AccessibilityNodeObject::changeValueByPercent(float percentChange)
{
    float range = maxValueForRange() - minValueForRange();
    float step = range * (percentChange / 100);
    float value = valueForRange();

    // Make sure the specified percent will cause a change of one integer step or larger.
    if (fabs(step) < 1)
        step = fabs(percentChange) * (1 / percentChange);

    value += step;
    setValue(String::number(value));

    axObjectCache()->postNotification(node(), AXObjectCache::AXValueChanged);
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

HTMLLabelElement* AccessibilityNodeObject::labelForElement(Element* element) const
{
    if (!is<HTMLElement>(*element) || !downcast<HTMLElement>(*element).isLabelable())
        return nullptr;

    const AtomicString& id = element->getIdAttribute();
    if (!id.isEmpty()) {
        if (HTMLLabelElement* label = element->treeScope().labelElementForId(id))
            return label;
    }

    return ancestorsOfType<HTMLLabelElement>(*element).first();
}

String AccessibilityNodeObject::ariaAccessibilityDescription() const
{
    String ariaLabeledBy = ariaLabeledByAttribute();
    if (!ariaLabeledBy.isEmpty())
        return ariaLabeledBy;

    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;

    return String();
}

static Element* siblingWithAriaRole(Node* node, const char* role)
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

    return siblingWithAriaRole(node(), "menu");
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
    
    return siblingWithAriaRole(node(), "menuitem");
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
    return isImage() || isInputImage() || isNativeImage() || isCanvas() || (node() && node()->hasTagName(imgTag));
}

bool AccessibilityNodeObject::isLabelable() const
{
    Node* node = this->node();
    if (!node)
        return false;
    
    return is<HTMLInputElement>(*node) || AccessibilityObject::isARIAInput(ariaRoleAttribute()) || isControl() || isProgressIndicator() || isMeter();
}

String AccessibilityNodeObject::textForLabelElement(Element* element) const
{
    String result = String();
    if (!is<HTMLLabelElement>(*element))
        return result;
    
    HTMLLabelElement* label = downcast<HTMLLabelElement>(element);
    // Check to see if there's aria-labelledby attribute on the label element.
    if (AccessibilityObject* labelObject = axObjectCache()->getOrCreate(label))
        result = labelObject->ariaLabeledByAttribute();
    
    return !result.isEmpty() ? result : accessibleNameForNode(label);
}
    
void AccessibilityNodeObject::titleElementText(Vector<AccessibilityText>& textOrder) const
{
    Node* node = this->node();
    if (!node)
        return;
    
    if (isLabelable()) {
        if (HTMLLabelElement* label = labelForElement(downcast<Element>(node))) {
            String innerText = textForLabelElement(label);
            
            // Only use the <label> text if there's no ARIA override.
            if (!innerText.isEmpty() && !ariaAccessibilityDescription())
                textOrder.append(AccessibilityText(innerText, isMeter() ? AccessibilityTextSource::Alternative : AccessibilityTextSource::LabelByElement, axObjectCache()->getOrCreate(label)));
            return;
        }
    }
    
    AccessibilityObject* titleUIElement = this->titleUIElement();
    if (titleUIElement)
        textOrder.append(AccessibilityText(String(), AccessibilityTextSource::LabelByElement, titleUIElement));
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
    
    const AtomicString& ariaLabel = getAttribute(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        textOrder.append(AccessibilityText(ariaLabel, AccessibilityTextSource::Alternative));
    
    if (usesAltTagForTextComputation()) {
        if (is<RenderImage>(renderer())) {
            String renderAltText = downcast<RenderImage>(*renderer()).altText();

            // RenderImage will return title as a fallback from altText, but we don't want title here because we consider that in helpText.
            if (!renderAltText.isEmpty() && renderAltText != getAttribute(titleAttr)) {
                textOrder.append(AccessibilityText(renderAltText, AccessibilityTextSource::Alternative));
                return;
            }
        }
        // Images should use alt as long as the attribute is present, even if empty.
        // Otherwise, it should fallback to other methods, like the title attribute.
        const AtomicString& alt = getAttribute(altAttr);
        if (!alt.isEmpty())
            textOrder.append(AccessibilityText(alt, AccessibilityTextSource::Alternative));
    }
    
    Node* node = this->node();
    if (!node)
        return;
    
    // The fieldset element derives its alternative text from the first associated legend element if one is available.
    if (is<HTMLFieldSetElement>(*node)) {
        AccessibilityObject* object = axObjectCache()->getOrCreate(downcast<HTMLFieldSetElement>(*node).legend());
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
    
    bool isInputTag = is<HTMLInputElement>(*node);
    if (isInputTag) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        if (input.isTextButton()) {
            textOrder.append(AccessibilityText(input.valueWithDefault(), AccessibilityTextSource::Visible));
            return;
        }
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
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::CheckBox:
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
    const AtomicString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
        textOrder.append(AccessibilityText(ariaHelp, AccessibilityTextSource::Help));
    
    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        textOrder.append(AccessibilityText(describedBy, AccessibilityTextSource::Summary));
    else if (isControl()) {
        // For controls, use their fieldset parent's described-by text if available.
        auto matchFunc = [] (const AccessibilityObject& object) {
            return object.isFieldset() && !object.ariaDescribedByAttribute().isEmpty();
        };
        if (const auto* parent = AccessibilityObject::matchedParent(*this, false, WTFMove(matchFunc)))
            textOrder.append(AccessibilityText(parent->ariaDescribedByAttribute(), AccessibilityTextSource::Summary));
    }

    // Summary attribute used as help text on tables.
    const AtomicString& summary = getAttribute(summaryAttr);
    if (!summary.isEmpty())
        textOrder.append(AccessibilityText(summary, AccessibilityTextSource::Summary));

    // The title attribute should be used as help text unless it is already being used as descriptive text.
    // However, when the title attribute is the only text alternative provided, it may be exposed as the
    // descriptive text. This is problematic in the case of meters because the HTML spec suggests authors
    // can expose units through this attribute. Therefore, if the element is a meter, change its source
    // type to AccessibilityTextSource::Help.
    const AtomicString& title = getAttribute(titleAttr);
    if (!title.isEmpty()) {
        if (!isMeter() && !roleIgnoresTitle())
            textOrder.append(AccessibilityText(title, AccessibilityTextSource::TitleTag));
        else
            textOrder.append(AccessibilityText(title, AccessibilityTextSource::Help));
    }
}

void AccessibilityNodeObject::accessibilityText(Vector<AccessibilityText>& textOrder)
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
    if (!ariaLabeledBy.isEmpty()) {
        Vector<Element*> elements;
        ariaLabeledByElements(elements);
        
        Vector<RefPtr<AccessibilityObject>> axElements;
        for (const auto& element : elements)
            axElements.append(axObjectCache()->getOrCreate(element));
        
        textOrder.append(AccessibilityText(ariaLabeledBy, AccessibilityTextSource::Alternative, WTFMove(axElements)));
    }
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
        const AtomicString& ariaLabel = documentElement->attributeWithoutSynchronization(aria_labelAttr);
        if (!ariaLabel.isEmpty())
            return ariaLabel;
    }
    
    if (auto* owner = document->ownerElement()) {
        if (owner->hasTagName(frameTag) || owner->hasTagName(iframeTag)) {
            const AtomicString& title = owner->attributeWithoutSynchronization(titleAttr);
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
    
String AccessibilityNodeObject::accessibilityDescription() const
{
    // Static text should not have a description, it should only have a stringValue.
    if (roleValue() == AccessibilityRole::StaticText)
        return String();

    String ariaDescription = ariaAccessibilityDescription();
    if (!ariaDescription.isEmpty())
        return ariaDescription;

    if (usesAltTagForTextComputation()) {
        // Images should use alt as long as the attribute is present, even if empty.                    
        // Otherwise, it should fallback to other methods, like the title attribute.                    
        const AtomicString& alt = getAttribute(altAttr);
        if (!alt.isNull())
            return alt;
    }
    
#if ENABLE(MATHML)
    if (is<MathMLElement>(m_node))
        return getAttribute(MathMLNames::alttextAttr);
#endif

    // An element's descriptive text is comprised of title() (what's visible on the screen) and accessibilityDescription() (other descriptive text).
    // Both are used to generate what a screen reader speaks.                                                           
    // If this point is reached (i.e. there's no accessibilityDescription) and there's no title(), we should fallback to using the title attribute.
    // The title attribute is normally used as help text (because it is a tooltip), but if there is nothing else available, this should be used (according to ARIA).
    // https://bugs.webkit.org/show_bug.cgi?id=170475: An exception is when the element is semantically unimportant. In those cases, title text should remain as help text.
    if (title().isEmpty() && !roleIgnoresTitle())
        return getAttribute(titleAttr);

    return String();
}

// Returns whether the role was not intended to play a semantically meaningful part of the
// accessibility hierarchy. This applies to generic groups like <div>'s with no role value set.
bool AccessibilityNodeObject::roleIgnoresTitle() const
{
    if (ariaRoleAttribute() != AccessibilityRole::Unknown)
        return false;

    switch (roleValue()) {
    case AccessibilityRole::Div:
    case AccessibilityRole::Unknown:
        return true;
    default:
        return false;
    }
}

String AccessibilityNodeObject::helpText() const
{
    Node* node = this->node();
    if (!node)
        return String();
    
    const AtomicString& ariaHelp = getAttribute(aria_helpAttr);
    if (!ariaHelp.isEmpty())
        return ariaHelp;
    
    String describedBy = ariaDescribedByAttribute();
    if (!describedBy.isEmpty())
        return describedBy;
    
    String description = accessibilityDescription();
    for (Node* ancestor = node; ancestor; ancestor = ancestor->parentNode()) {
        if (is<HTMLElement>(*ancestor)) {
            HTMLElement& element = downcast<HTMLElement>(*ancestor);
            const AtomicString& summary = element.getAttribute(summaryAttr);
            if (!summary.isEmpty())
                return summary;
            
            // The title attribute should be used as help text unless it is already being used as descriptive text.
            const AtomicString& title = element.getAttribute(titleAttr);
            if (!title.isEmpty() && description != title)
                return title;
        }
        
        // Only take help text from an ancestor element if its a group or an unknown role. If help was 
        // added to those kinds of elements, it is likely it was meant for a child element.
        if (AccessibilityObject* axObj = axObjectCache()->getOrCreate(ancestor)) {
            if (!axObj->isGroup() && axObj->roleValue() != AccessibilityRole::Unknown)
                break;
        }
    }
    
    return String();
}
    
unsigned AccessibilityNodeObject::hierarchicalLevel() const
{
    Node* node = this->node();
    if (!is<Element>(node))
        return 0;
    Element& element = downcast<Element>(*node);
    const AtomicString& ariaLevel = element.attributeWithoutSynchronization(aria_levelAttr);
    if (!ariaLevel.isEmpty())
        return ariaLevel.toInt();
    
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
        if (expand != details.isOpen())
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
    
    if (equalLettersIgnoringASCIICase(obj->getAttribute(aria_hiddenAttr), "true"))
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

    if (is<AccessibilityTable>(*obj) && downcast<AccessibilityTable>(*obj).isExposableThroughAccessibility())
        return false;

    if (obj->isTree() || obj->isCanvas())
        return false;

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

    // The Accname specification states that if the current node is hidden, and not directly
    // referenced by aria-labelledby or aria-describedby, and is not a host language text
    // alternative, the empty string should be returned.
    if (isHidden() && !is<HTMLLabelElement>(node) && (node && !ancestorsOfType<HTMLCanvasElement>(*node).first())) {
        AccessibilityObject::AccessibilityChildrenVector labelFor;
        AccessibilityObject::AccessibilityChildrenVector descriptionFor;
        ariaLabelledByReferencingElements(labelFor);
        ariaDescribedByReferencingElements(descriptionFor);
        if (!labelFor.size() && !descriptionFor.size())
            return String();
    }

    StringBuilder builder;
    for (AccessibilityObject* child = firstChild(); child; child = child->nextSibling()) {
        if (mode.ignoredChildNode && child->node() == mode.ignoredChildNode)
            continue;
        
        bool shouldDeriveNameFromAuthor = (mode.childrenInclusion == AccessibilityTextUnderElementMode::TextUnderElementModeIncludeNameFromContentsChildren && !child->accessibleNameDerivesFromContent());
        if (shouldDeriveNameFromAuthor) {
            appendNameToStringBuilder(builder, accessibleNameForNode(child->node()));
            continue;
        }
        
        if (!shouldUseAccessibilityObjectInnerText(child, mode))
            continue;

        if (is<AccessibilityNodeObject>(*child)) {
            // We should ignore the child if it's labeled by this node.
            // This could happen when this node labels multiple child nodes and we didn't
            // skip in the above ignoredChildNode check.
            Vector<Element*> labeledByElements;
            downcast<AccessibilityNodeObject>(*child).ariaLabeledByElements(labeledByElements);
            if (labeledByElements.contains(node))
                continue;
            
            Vector<AccessibilityText> textOrder;
            downcast<AccessibilityNodeObject>(*child).alternativeText(textOrder);
            if (textOrder.size() > 0 && textOrder[0].text.length()) {
                appendNameToStringBuilder(builder, textOrder[0].text);
                continue;
            }
        }
        
        String childText = child->textUnderElement(mode);
        if (childText.length())
            appendNameToStringBuilder(builder, childText);
    }

    return builder.toString().stripWhiteSpace().simplifyWhiteSpace(isHTMLSpaceButNotLineBreak);
}

String AccessibilityNodeObject::title() const
{
    Node* node = this->node();
    if (!node)
        return String();

    bool isInputTag = is<HTMLInputElement>(*node);
    if (isInputTag) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        if (input.isTextButton())
            return input.valueWithDefault();
    }

    if (isLabelable()) {
        HTMLLabelElement* label = labelForElement(downcast<Element>(node));
        // Use the label text as the title if 1) the title element is NOT an exposed element and 2) there's no ARIA override.
        if (label && !exposesTitleUIElement() && !ariaAccessibilityDescription().length())
            return textForLabelElement(label);
    }

    // If this node isn't rendered, there's no inner text we can extract from a select element.
    if (!isAccessibilityRenderObject() && node->hasTagName(selectTag))
        return String();

    switch (roleValue()) {
    case AccessibilityRole::PopUpButton:
        // Native popup buttons should not use their button children's text as a title. That value is retrieved through stringValue().
        if (node->hasTagName(selectTag))
            return String();
        FALLTHROUGH;
    case AccessibilityRole::Button:
    case AccessibilityRole::ToggleButton:
    case AccessibilityRole::CheckBox:
    case AccessibilityRole::ListBoxOption:
    case AccessibilityRole::ListItem:
    case AccessibilityRole::MenuButton:
    case AccessibilityRole::MenuItem:
    case AccessibilityRole::MenuItemCheckbox:
    case AccessibilityRole::MenuItemRadio:
    case AccessibilityRole::RadioButton:
    case AccessibilityRole::Switch:
    case AccessibilityRole::Tab:
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

    return String();
}

String AccessibilityNodeObject::text() const
{
    // If this is a user defined static text, use the accessible name computation.                                      
    if (isARIAStaticText()) {
        Vector<AccessibilityText> textOrder;
        alternativeText(textOrder);
        if (textOrder.size() > 0 && textOrder[0].text.length())
            return textOrder[0].text;
    }

    if (!isTextControl())
        return String();

    Node* node = this->node();
    if (!node)
        return String();

    if (isNativeTextControl() && is<HTMLTextFormControlElement>(*node))
        return downcast<HTMLTextFormControlElement>(*node).value();

    if (!node->isElementNode())
        return String();

    return downcast<Element>(node)->innerText();
}

String AccessibilityNodeObject::stringValue() const
{
    Node* node = this->node();
    if (!node)
        return String();

    if (isARIAStaticText()) {
        String staticText = text();
        if (!staticText.length())
            staticText = textUnderElement();
        return staticText;
    }

    if (node->isTextNode())
        return textUnderElement();

    if (node->hasTagName(selectTag)) {
        HTMLSelectElement& selectElement = downcast<HTMLSelectElement>(*node);
        int selectedIndex = selectElement.selectedIndex();
        const Vector<HTMLElement*>& listItems = selectElement.listItems();
        if (selectedIndex >= 0 && static_cast<size_t>(selectedIndex) < listItems.size()) {
            const AtomicString& overriddenDescription = listItems[selectedIndex]->attributeWithoutSynchronization(aria_labelAttr);
            if (!overriddenDescription.isNull())
                return overriddenDescription;
        }
        if (!selectElement.multiple())
            return selectElement.value();
        return String();
    }

    if (isTextControl())
        return text();

    // FIXME: We might need to implement a value here for more types
    // FIXME: It would be better not to advertise a value at all for the types for which we don't implement one;
    // this would require subclassing or making accessibilityAttributeNames do something other than return a
    // single static array.
    return String();
}

void AccessibilityNodeObject::colorValue(int& r, int& g, int& b) const
{
    r = 0;
    g = 0;
    b = 0;

#if ENABLE(INPUT_TYPE_COLOR)
    if (!isColorWell())
        return;

    if (!is<HTMLInputElement>(node()))
        return;

    auto color = downcast<HTMLInputElement>(*node()).valueAsColor();
    r = color.red();
    g = color.green();
    b = color.blue();
#endif
}

// This function implements the ARIA accessible name as described by the Mozilla                                        
// ARIA Implementer's Guide.                                                                                            
static String accessibleNameForNode(Node* node, Node* labelledbyNode)
{
    ASSERT(node);
    if (!is<Element>(node))
        return String();
    
    Element& element = downcast<Element>(*node);
    const AtomicString& ariaLabel = element.attributeWithoutSynchronization(aria_labelAttr);
    if (!ariaLabel.isEmpty())
        return ariaLabel;
    
    const AtomicString& alt = element.attributeWithoutSynchronization(altAttr);
    if (!alt.isEmpty())
        return alt;

    // If the node can be turned into an AX object, we can use standard name computation rules.
    // If however, the node cannot (because there's no renderer e.g.) fallback to using the basic text underneath.
    AccessibilityObject* axObject = node->document().axObjectCache()->getOrCreate(node);
    if (axObject) {
        String valueDescription = axObject->valueDescription();
        if (!valueDescription.isEmpty())
            return valueDescription;

        // The Accname specification states that if the name is being calculated for a combobox
        // or listbox inside a labeling element, return the text alternative of the chosen option.
        AccessibilityObject::AccessibilityChildrenVector children;
        if (axObject->isListBox())
            axObject->selectedChildren(children);
        else if (axObject->isComboBox()) {
            for (const auto& child : axObject->children()) {
                if (child->isListBox()) {
                    child->selectedChildren(children);
                    break;
                }
            }
        }

        StringBuilder builder;
        String childText;
        for (const auto& child : children)
            appendNameToStringBuilder(builder, accessibleNameForNode(child->node()));

        childText = builder.toString();
        if (!childText.isEmpty())
            return childText;
    }
    
    if (is<HTMLInputElement>(*node))
        return downcast<HTMLInputElement>(*node).value();
    
    String text;
    if (axObject) {
        if (axObject->accessibleNameDerivesFromContent())
            text = axObject->textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeNameFromContentsChildren, true, labelledbyNode));
    } else
        text = element.innerText().simplifyWhiteSpace();

    if (!text.isEmpty())
        return text;
    
    const AtomicString& title = element.attributeWithoutSynchronization(titleAttr);
    if (!title.isEmpty())
        return title;
    
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

String AccessibilityNodeObject::accessibilityDescriptionForElements(Vector<Element*> &elements) const
{
    StringBuilder builder;
    unsigned size = elements.size();
    for (unsigned i = 0; i < size; ++i)
        appendNameToStringBuilder(builder, accessibleNameForNode(elements[i], node()));
    return builder.toString();
}

String AccessibilityNodeObject::ariaDescribedByAttribute() const
{
    Vector<Element*> elements;
    elementsFromAttribute(elements, aria_describedbyAttr);
    
    return accessibilityDescriptionForElements(elements);
}

void AccessibilityNodeObject::ariaLabeledByElements(Vector<Element*>& elements) const
{
    elementsFromAttribute(elements, aria_labelledbyAttr);
    if (!elements.size())
        elementsFromAttribute(elements, aria_labeledbyAttr);
}


String AccessibilityNodeObject::ariaLabeledByAttribute() const
{
    Vector<Element*> elements;
    ariaLabeledByElements(elements);

    return accessibilityDescriptionForElements(elements);
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
    if (!is<Element>(node))
        return false;

    Element& element = downcast<Element>(*node);

    if (element.isDisabledFormControl())
        return false;

    return element.supportsFocus();
}

bool AccessibilityNodeObject::canSetValueAttribute() const
{
    Node* node = this->node();
    if (!node)
        return false;

    // The host-language readonly attribute trumps aria-readonly.
    if (is<HTMLTextAreaElement>(*node))
        return !downcast<HTMLTextAreaElement>(*node).isReadOnly();
    if (is<HTMLInputElement>(*node)) {
        HTMLInputElement& input = downcast<HTMLInputElement>(*node);
        if (input.isTextField())
            return !input.isReadOnly();
    }

    String readOnly = readOnlyValue();
    if (!readOnly.isEmpty())
        return readOnly == "true" ? false : true;

    if (isNonNativeTextControl())
        return true;

    if (isMeter())
        return false;

    if (isProgressIndicator() || isSlider() || isScrollbar())
        return true;

#if PLATFORM(GTK)
    // In ATK, input types which support aria-readonly are treated as having a
    // settable value if the user can modify the widget's value or its state.
    if (supportsReadOnly())
        return true;

    if (isRadioButton()) {
        auto radioGroup = radioGroupAncestor();
        return radioGroup ? radioGroup->readOnlyValue() != "true" : true;
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
    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return AccessibilityRole::Unknown;
    
    AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);

    // ARIA states if an item can get focus, it should not be presentational.
    if (role == AccessibilityRole::Presentational && canSetFocusAttribute())
        return AccessibilityRole::Unknown;

    if (role == AccessibilityRole::Button)
        role = buttonRoleType();

    if (role == AccessibilityRole::TextArea && !ariaIsMultiline())
        role = AccessibilityRole::TextField;

    role = remapAriaRoleDueToParent(role);
    
    // Presentational roles are invalidated by the presence of ARIA attributes.
    if (role == AccessibilityRole::Presentational && supportsARIAAttributes())
        role = AccessibilityRole::Unknown;
    
    // The ARIA spec states, "Authors must give each element with role region a brief label that
    // describes the purpose of the content in the region." The Core AAM states, "Special case:
    // if the region does not have an accessible name, do not expose the element as a landmark.
    // Use the native host language role of the element instead."
    if (role == AccessibilityRole::LandmarkRegion && !hasAttribute(aria_labelAttr) && !hasAttribute(aria_labelledbyAttr))
        role = AccessibilityRole::Unknown;

    if (static_cast<int>(role))
        return role;

    return AccessibilityRole::Unknown;
}

AccessibilityRole AccessibilityNodeObject::ariaRoleAttribute() const
{
    return m_ariaRole;
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
        // An aria "menuitem" may map to MenuButton or MenuItem depending on its parent.
        if (role == AccessibilityRole::MenuItem && parentAriaRole == AccessibilityRole::ApplicationGroup)
            return AccessibilityRole::MenuButton;
        
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
