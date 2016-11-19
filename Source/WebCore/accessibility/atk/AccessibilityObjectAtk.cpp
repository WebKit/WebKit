/*
 * Copyright (C) 2008 Apple Ltd.
 * Copyright (C) 2008 Alp Toker <alp@atoker.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "AccessibilityObject.h"

#include "HTMLSpanElement.h"
#include "RenderBlock.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderTableCell.h"
#include "RenderText.h"
#include "TextControlInnerElements.h"
#include <glib-object.h>

#if HAVE(ACCESSIBILITY)

namespace WebCore {

bool AccessibilityObject::accessibilityIgnoreAttachment() const
{
    return false;
}

AccessibilityObjectInclusion AccessibilityObject::accessibilityPlatformIncludesObject() const
{
    AccessibilityObject* parent = parentObject();
    if (!parent)
        return DefaultBehavior;

    // If the author has provided a role, platform-specific inclusion likely doesn't apply.
    if (ariaRoleAttribute() != UnknownRole)
        return DefaultBehavior;

    AccessibilityRole role = roleValue();
    // We expose the slider as a whole but not its value indicator.
    if (role == SliderThumbRole)
        return IgnoreObject;

    // When a list item is made up entirely of children (e.g. paragraphs)
    // the list item gets ignored. We need it.
    if (isGroup() && parent->isList())
        return IncludeObject;

    // Entries and password fields have extraneous children which we want to ignore.
    if (parent->isPasswordField() || parent->isTextControl())
        return IgnoreObject;

    // Include all tables, even layout tables. The AT can decide what to do with each.
    if (role == CellRole || role == TableRole || role == ColumnHeaderRole || role == RowHeaderRole)
        return IncludeObject;

    // The object containing the text should implement AtkText itself.
    // However, WebCore also maps ARIA's "text" role to the StaticTextRole.
    if (role == StaticTextRole)
        return ariaRoleAttribute() != UnknownRole ? DefaultBehavior : IgnoreObject;

    // Include all list items, regardless they have or not inline children
    if (role == ListItemRole)
        return IncludeObject;

    // Bullets/numbers for list items shouldn't be exposed as AtkObjects.
    if (role == ListMarkerRole)
        return IgnoreObject;

    // Never expose an unknown object, since AT's won't know what to
    // do with them. This is what is done on the Mac as well.
    if (role == UnknownRole)
        return IgnoreObject;

    if (role == InlineRole)
        return IncludeObject;

    // Lines past this point only make sense for AccessibilityRenderObjects.
    RenderObject* renderObject = renderer();
    if (!renderObject)
        return DefaultBehavior;

    // We always want to include paragraphs that have rendered content.
    // WebCore Accessibility does so unless there is a RenderBlock child.
    if (role == ParagraphRole) {
        auto child = childrenOfType<RenderBlock>(downcast<RenderElement>(*renderObject)).first();
        return child ? IncludeObject : DefaultBehavior;
    }

    // We always want to include table cells (layout and CSS) that have rendered text content.
    if (is<RenderTableCell>(renderObject)) {
        for (const auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(*renderObject))) {
            if (is<RenderInline>(child) || is<RenderText>(child) || is<HTMLSpanElement>(child.node()))
                return IncludeObject;
        }
        return DefaultBehavior;
    }

    if (renderObject->isAnonymousBlock()) {
        // The text displayed by an ARIA menu item is exposed through the accessible name.
        if (parent->isMenuItem())
            return IgnoreObject;

        // The text displayed in headings is typically exposed in the heading itself.
        if (parent->isHeading())
            return IgnoreObject;

        // The text displayed in list items is typically exposed in the list item itself.
        if (parent->isListItem())
            return IgnoreObject;

        // The text displayed in links is typically exposed in the link itself.
        if (parent->isLink())
            return IgnoreObject;

        // FIXME: This next one needs some further consideration. But paragraphs are not
        // typically huge (like divs). And ignoring anonymous block children of paragraphs
        // will preserve existing behavior.
        if (parent->roleValue() == ParagraphRole)
            return IgnoreObject;

        return DefaultBehavior;
    }

    Node* node = renderObject->node();
    if (!node)
        return DefaultBehavior;

    // We don't want <span> elements to show up in the accessibility hierarchy unless
    // we have good reasons for that (e.g. focusable or visible because of containing
    // a meaningful accessible name, maybe set through ARIA), so we can use
    // atk_component_grab_focus() to set the focus to it.
    if (is<HTMLSpanElement>(node) && !canSetFocusAttribute() && !hasAttributesRequiredForInclusion() && !supportsARIAAttributes())
        return IgnoreObject;

    // If we include TextControlInnerTextElement children, changes to those children
    // will result in focus and text notifications that suggest the user is no longer
    // in the control. This can be especially problematic for screen reader users with
    // key echo enabled when typing in a password input.
    if (is<TextControlInnerTextElement>(node))
        return IgnoreObject;

    return DefaultBehavior;
}

AccessibilityObjectWrapper* AccessibilityObject::wrapper() const
{
    return m_wrapper;
}

void AccessibilityObject::setWrapper(AccessibilityObjectWrapper* wrapper)
{
    if (wrapper == m_wrapper)
        return;

    if (m_wrapper)
        g_object_unref(m_wrapper);

    m_wrapper = wrapper;

    if (m_wrapper)
        g_object_ref(m_wrapper);
}

bool AccessibilityObject::allowsTextRanges() const
{
    // Check type for the AccessibilityObject.
    if (isTextControl() || isWebArea() || isGroup() || isLink() || isHeading() || isListItem() || isTableCell())
        return true;

    // Check roles as the last fallback mechanism.
    AccessibilityRole role = roleValue();
    return role == ParagraphRole || role == LabelRole || role == DivRole || role == FormRole || role == PreRole;
}

unsigned AccessibilityObject::getLengthForTextRange() const
{
    unsigned textLength = text().length();

    if (textLength)
        return textLength;

    // Gtk ATs need this for all text objects; not just text controls.
    Node* node = this->node();
    RenderObject* renderer = node ? node->renderer() : nullptr;
    if (is<RenderText>(renderer))
        textLength = downcast<RenderText>(*renderer).textLength();

    // Get the text length from the elements under the
    // accessibility object if the value is still zero.
    if (!textLength && allowsTextRanges())
        textLength = textUnderElement(AccessibilityTextUnderElementMode(AccessibilityTextUnderElementMode::TextUnderElementModeIncludeAllChildren)).length();

    return textLength;
}

} // namespace WebCore

#endif // HAVE(ACCESSIBILITY)
