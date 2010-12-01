/*
 * Copyright (C) 2005 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
 *
 */

#include "config.h"
#include "RadioInputType.h"

#include "Frame.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "KeyboardEvent.h"
#include "LocalizedStrings.h"
#include "MouseEvent.h"
#include "Settings.h"
#include "SpatialNavigation.h"
#include <wtf/PassOwnPtr.h>

namespace WebCore {

using namespace HTMLNames;

PassOwnPtr<InputType> RadioInputType::create(HTMLInputElement* element)
{
    return adoptPtr(new RadioInputType(element));
}

const AtomicString& RadioInputType::formControlType() const
{
    return InputTypeNames::radio();
}

bool RadioInputType::valueMissing(const String&) const
{
    return !element()->checkedRadioButtons().checkedButtonForGroup(element()->name());
}

String RadioInputType::valueMissingText() const
{
    return validationMessageValueMissingForRadioText();
}

bool RadioInputType::handleClickEvent(MouseEvent* event)
{
    event->setDefaultHandled();
    return true;
}

bool RadioInputType::handleKeydownEvent(KeyboardEvent* event)
{
    if (BaseCheckableInputType::handleKeydownEvent(event))
        return true;
    const String& key = event->keyIdentifier();
    if (key != "Up" && key != "Down" && key != "Left" && key != "Right")
        return false;

    // Left and up mean "previous radio button".
    // Right and down mean "next radio button".
    // Tested in WinIE, and even for RTL, left still means previous radio button (and so moves
    // to the right).  Seems strange, but we'll match it.
    // However, when using Spatial Navigation, we need to be able to navigate without changing the selection.
    Document* document = element()->document();
    if (isSpatialNavigationEnabled(document->frame()))
        return false;
    bool forward = (key == "Down" || key == "Right");

    // We can only stay within the form's children if the form hasn't been demoted to a leaf because
    // of malformed HTML.
    Node* node = element();
    while ((node = (forward ? node->traverseNextNode() : node->traversePreviousNode()))) {
        // Once we encounter a form element, we know we're through.
        if (node->hasTagName(formTag))
            break;
        // Look for more radio buttons.
        if (!node->hasTagName(inputTag))
            continue;
        HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(node);
        if (inputElement->form() != element()->form())
            break;
        if (inputElement->isRadioButton() && inputElement->name() == element()->name() && inputElement->isFocusable()) {
            inputElement->setChecked(true);
            document->setFocusedNode(inputElement);
            inputElement->dispatchSimulatedClick(event, false, false);
            event->setDefaultHandled();
            return true;
        }
    }
    return false;
}

bool RadioInputType::handleKeyupEvent(KeyboardEvent* event)
{
    const String& key = event->keyIdentifier();
    if (key != "U+0020")
        return false;
    // If an unselected radio is tabbed into (because the entire group has nothing
    // checked, or because of some explicit .focus() call), then allow space to check it.
    if (element()->checked())
        return false;
    dispatchSimulatedClickIfActive(event);
    return true;
}

} // namespace WebCore
