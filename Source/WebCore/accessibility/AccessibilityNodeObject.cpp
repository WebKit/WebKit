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
* 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#include "AccessibilityListBox.h"
#include "AccessibilitySpinButton.h"
#include "AccessibilityTable.h"
#include "EventNames.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLAreaElement.h"
#include "HTMLFieldSetElement.h"
#include "HTMLFormElement.h"
#include "HTMLFrameElementBase.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLLabelElement.h"
#include "HTMLLegendElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HTMLOptGroupElement.h"
#include "HTMLOptionElement.h"
#include "HTMLOptionsCollection.h"
#include "HTMLPlugInImageElement.h"
#include "HTMLSelectElement.h"
#include "HTMLTextAreaElement.h"
#include "HTMLTextFormControlElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "LocalizedStrings.h"
#include "MathMLNames.h"
#include "NodeList.h"
#include "Page.h"
#include "ProgressTracker.h"
#include "Text.h"
#include "TextControlInnerElements.h"
#include "TextIterator.h"
#include "Widget.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/unicode/CharacterNames.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

AccessibilityNodeObject::AccessibilityNodeObject(Node* node)
    : AccessibilityObject()
    , m_ariaRole(UnknownRole)
    , m_childrenDirty(false)
    , m_roleForMSAA(UnknownRole)
    , m_node(node)
{
}

AccessibilityNodeObject::~AccessibilityNodeObject()
{
    ASSERT(isDetached());
}

void AccessibilityNodeObject::init()
{
    m_role = determineAccessibilityRole();
}

PassRefPtr<AccessibilityNodeObject> AccessibilityNodeObject::create(Node* node)
{
    AccessibilityNodeObject* obj = new AccessibilityNodeObject(node);
    obj->init();
    return adoptRef(obj);
}

void AccessibilityNodeObject::detach()
{
    clearChildren();
    AccessibilityObject::detach();
    m_node = 0;
}

void AccessibilityNodeObject::childrenChanged()
{
    // This method is meant as a quick way of marking a portion of the accessibility tree dirty.
    if (!node() && !renderer())
        return;

    axObjectCache()->postNotification(this, document(), AXObjectCache::AXChildrenChanged, true);

    // Go up the accessibility parent chain, but only if the element already exists. This method is
    // called during render layouts, minimal work should be done. 
    // If AX elements are created now, they could interrogate the render tree while it's in a funky state.
    // At the same time, process ARIA live region changes.
    for (AccessibilityObject* parent = this; parent; parent = parent->parentObjectIfExists()) {
        parent->setNeedsToUpdateChildren();

        // These notifications always need to be sent because screenreaders are reliant on them to perform. 
        // In other words, they need to be sent even when the screen reader has not accessed this live region since the last update.

        // If this element supports ARIA live regions, then notify the AT of changes.
        if (parent->supportsARIALiveRegion())
            axObjectCache()->postNotification(parent, parent->document(), AXObjectCache::AXLiveRegionChanged, true);
        
        // If this element is an ARIA text control, notify the AT of changes.
        if (parent->isARIATextControl() && !parent->isNativeTextControl() && !parent->node()->rendererIsEditable())
            axObjectCache()->postNotification(parent, parent->document(), AXObjectCache::AXValueChanged, true);
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
        return 0;
    
    Node* firstChild = node()->firstChild();

    if (!firstChild)
        return 0;
    
    return axObjectCache()->getOrCreate(firstChild);
}

AccessibilityObject* AccessibilityNodeObject::lastChild() const
{
    if (!node())
        return 0;
    
    Node* lastChild = node()->lastChild();
    if (!lastChild)
        return 0;
    
    return axObjectCache()->getOrCreate(lastChild);
}

AccessibilityObject* AccessibilityNodeObject::previousSibling() const
{
    if (!node())
        return 0;

    Node* previousSibling = node()->previousSibling();
    if (!previousSibling)
        return 0;

    return axObjectCache()->getOrCreate(previousSibling);
}

AccessibilityObject* AccessibilityNodeObject::nextSibling() const
{
    if (!node())
        return 0;

    Node* nextSibling = node()->nextSibling();
    if (!nextSibling)
        return 0;

    return axObjectCache()->getOrCreate(nextSibling);
}
    
AccessibilityObject* AccessibilityNodeObject::parentObjectIfExists() const
{
    return parentObject();
}
    
AccessibilityObject* AccessibilityNodeObject::parentObject() const
{
    if (!node())
        return 0;

    Node* parentObj = node()->parentNode();
    if (parentObj)
        return axObjectCache()->getOrCreate(parentObj);
    
    return 0;
}

LayoutRect AccessibilityNodeObject::elementRect() const
{
    return boundingBoxRect();
}

void AccessibilityNodeObject::setNode(Node* node)
{
    m_node = node;
}

Document* AccessibilityNodeObject::document() const
{
    if (!node())
        return 0;
    return node()->document();
}

AccessibilityRole AccessibilityNodeObject::determineAccessibilityRole()
{
    if (!node())
        return UnknownRole;

    m_ariaRole = determineAriaRoleAttribute();
    
    AccessibilityRole ariaRole = ariaRoleAttribute();
    if (ariaRole != UnknownRole)
        return ariaRole;

    if (node()->isLink())
        return WebCoreLinkRole;
    if (node()->isTextNode())
        return StaticTextRole;
    if (node()->hasTagName(buttonTag))
        return buttonRoleType();
    if (node()->hasTagName(inputTag)) {
        HTMLInputElement* input = static_cast<HTMLInputElement*>(node());
        if (input->isCheckbox())
            return CheckBoxRole;
        if (input->isRadioButton())
            return RadioButtonRole;
        if (input->isTextButton())
            return buttonRoleType();
        return TextFieldRole;
    }
    if (node()->hasTagName(selectTag)) {
        HTMLSelectElement* selectElement = toHTMLSelectElement(node());
        return selectElement->multiple() ? ListRole : PopUpButtonRole;
    }
    if (node()->isFocusable())
        return GroupRole;
    
    return UnknownRole;
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
    
    for (Node* child = m_node->firstChild(); child; child = child->nextSibling()) {
        RefPtr<AccessibilityObject> obj = axObjectCache()->getOrCreate(child);
        obj->clearChildren();
        if (obj->accessibilityIsIgnored()) {
            AccessibilityChildrenVector children = obj->children();
            size_t length = children.size();
            for (size_t i = 0; i < length; ++i)
                m_children.append(children[i]);
        } else {
            ASSERT(obj->parentObject() == this);
            m_children.append(obj);
        }
    }
}

bool AccessibilityNodeObject::accessibilityIsIgnored() const
{
    return m_role == UnknownRole;
}

bool AccessibilityNodeObject::canSetFocusAttribute() const
{
    Node* node = this->node();

    if (isWebArea())
        return true;
    
    // NOTE: It would be more accurate to ask the document whether setFocusedNode() would
    // do anything. For example, setFocusedNode() will do nothing if the current focused
    // node will not relinquish the focus.
    if (!node)
        return false;

    if (node->isElementNode() && !static_cast<Element*>(node)->isEnabledFormControl())
        return false;

    return node->supportsFocus();
}

AccessibilityRole AccessibilityNodeObject::determineAriaRoleAttribute() const
{
    const AtomicString& ariaRole = getAttribute(roleAttr);
    if (ariaRole.isNull() || ariaRole.isEmpty())
        return UnknownRole;
    
    AccessibilityRole role = ariaRoleToWebCoreRole(ariaRole);

    // ARIA states if an item can get focus, it should not be presentational.
    if (role == PresentationalRole && canSetFocusAttribute())
        return UnknownRole;

    if (role == ButtonRole)
        role = buttonRoleType();

    if (role == TextAreaRole && !ariaIsMultiline())
        role = TextFieldRole;

    role = remapAriaRoleDueToParent(role);
    
    if (role)
        return role;

    return UnknownRole;
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

    if (role != ListBoxOptionRole && role != MenuItemRole)
        return role;
    
    for (AccessibilityObject* parent = parentObject(); parent && !parent->accessibilityIsIgnored(); parent = parent->parentObject()) {
        AccessibilityRole parentAriaRole = parent->ariaRoleAttribute();

        // Selects and listboxes both have options as child roles, but they map to different roles within WebCore.
        if (role == ListBoxOptionRole && parentAriaRole == MenuRole)
            return MenuItemRole;
        // An aria "menuitem" may map to MenuButton or MenuItem depending on its parent.
        if (role == MenuItemRole && parentAriaRole == GroupRole)
            return MenuButtonRole;
        
        // If the parent had a different role, then we don't need to continue searching up the chain.
        if (parentAriaRole)
            break;
    }
    
    return role;
}   

} // namespace WebCore
