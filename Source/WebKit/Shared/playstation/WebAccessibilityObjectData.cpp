/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebAccessibilityObjectData.h"

#include "WebFrame.h"
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/HTMLAnchorElement.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/RenderStyle.h>

namespace WebKit {
using namespace WebCore;

WebAccessibilityObjectData::WebAccessibilityObjectData(AXCoreObject* axObject)
{
    set(axObject);
}

String WebAccessibilityObjectData::getAccessibilityTitle(AXCoreObject* axObject, Vector<AccessibilityText>& textOrder)
{
    if (axObject->isListBoxOption())
        return axObject->stringValue();

    if (axObject->isPopUpButton())
        return axObject->stringValue();

    // For input fields, use only the text of the label element.
    if (axObject->isTextControl()) {
        if (auto* label = axObject->titleUIElement())
            return label->textUnderElement();
    }

    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative)
            break;

        if (text.textSource == AccessibilityTextSource::Visible || text.textSource == AccessibilityTextSource::Children)
            return text.text;

        // If there's an element that labels this object, then we should use that text as our title.
        if (text.textSource == AccessibilityTextSource::LabelByElement)
            return text.text;

        // FIXME: The title tag is used in certain cases for the title.
        // This usage should probably be in the description field since it's not "visible".
        if (text.textSource == AccessibilityTextSource::TitleTag)
            return text.text;
    }

    return { };
}

String WebAccessibilityObjectData::getAccessibilityDescription(AXCoreObject* axObject, Vector<AccessibilityText>& textOrder)
{
    if (axObject->roleValue() == AccessibilityRole::StaticText)
        return axObject->textUnderElement();

    for (const auto& text : textOrder) {
        if (text.textSource == AccessibilityTextSource::Alternative || text.textSource == AccessibilityTextSource::TitleTag)
            return text.text;
    }

    return { };
}

String WebAccessibilityObjectData::getAccessibilityHelpText(AXCoreObject*, Vector<AccessibilityText>& textOrder)
{
    bool descriptiveTextAvailable = false;
    for (const auto& text : textOrder) {
        switch (text.textSource) {
        case AccessibilityTextSource::Help:
        case AccessibilityTextSource::Summary:
            return text.text;
        case AccessibilityTextSource::Alternative:
        case AccessibilityTextSource::Visible:
        case AccessibilityTextSource::Children:
        case AccessibilityTextSource::LabelByElement:
            descriptiveTextAvailable = true;
            break;
        case AccessibilityTextSource::TitleTag:
            if (descriptiveTextAvailable)
                return text.text;
            break;
        default:
            break;
        }
    }

    return { };
}

bool WebAccessibilityObjectData::isImageLinked(AXCoreObject* axObject)
{
    Element* anchor = static_cast<AccessibilityObject*>(axObject)->anchorElement();
    return is<HTMLAnchorElement>(anchor) && !downcast<HTMLAnchorElement>(*anchor).href().isEmpty();
}

bool WebAccessibilityObjectData::isImageVisited(AXCoreObject* axObject)
{
    auto* style = static_cast<AccessibilityObject*>(axObject)->style();
    return style && style->insideLink() == InsideLink::InsideVisited;
}

void WebAccessibilityObjectData::setValue(AXCoreObject* axObject)
{
    if (axObject->supportsRangeValue())
        m_value = String::number(axObject->valueForRange());
    else if (axObject->roleValue() == AccessibilityRole::SliderThumb)
        m_value = String::number(axObject->parentObject()->valueForRange());
    else if (axObject->isHeading())
        m_value = String::number(axObject->headingLevel());
    else if (axObject->isTextControl()) {
        if (!axObject->stringValue().isEmpty())
            m_value = axObject->stringValue(); // inputted text
        else
            m_value = axObject->placeholderValue();
    } else
        m_value = String();
}

void WebAccessibilityObjectData::setState(AXCoreObject* axObject)
{
    OptionSet<State> state { };

    if (axObject && axObject->element() && axObject->document()
        && axObject->element() == axObject->document()->focusNavigationStartingNode(FocusDirection::None))
        m_state.add(State::Focused);

    if (!axObject->isEnabled() || (axObject->isTextControl() && !axObject->canSetValueAttribute()))
        m_state.add(State::Unavailable);

    if (axObject->isSelected())
        m_state.add(State::Selected);

    if (axObject->isImage() && isImageLinked(axObject)) {
        m_state.add(State::Linked);
        if (isImageVisited(axObject))
            m_state.add(State::Visited);
    }

    if (axObject->isLink()) {
        m_state.add(State::Linked);
        if (axObject->isVisited())
            m_state.add(State::Visited);
    }

    m_state = state;
}

void WebAccessibilityObjectData::set(AXCoreObject* axObject)
{
    Document* document = axObject->document();
    if (!document || !document->frame())
        return;

    WebLocalFrameLoaderClient* webFrameLoaderClient = toWebLocalFrameLoaderClient(document->frame()->loader().client());
    if (!webFrameLoaderClient)
        return;
    m_webFrameID = webFrameLoaderClient->webFrame().frameID();

    if (axObject->isStaticText()) {
        if (Node* node = axObject->node()) {
            // If link and heading, prefer link.
            if (AXCoreObject* link = AccessibilityObject::anchorElementForNode(node))
                axObject = link;
            else if (AXCoreObject* heading = AccessibilityObject::headingElementForNode(node))
                axObject = heading;
        }
    }

    m_role = axObject->roleValue();

    Vector<AccessibilityText> textOrder;
    axObject->accessibilityText(textOrder);
    m_title = getAccessibilityTitle(axObject, textOrder);
    m_description = getAccessibilityDescription(axObject, textOrder);
    m_helpText = getAccessibilityHelpText(axObject, textOrder);

    // link or frame url
    m_url = axObject->url();

    // checkbox or radio button state
    m_buttonState = axObject->checkboxOrRadioValue();

    setValue(axObject);
    setState(axObject);

    // object rectangle
    // We would like to use relativeFrame() / elementRect(),
    // but there are cases where the rectangle size becomes incorrect when zooming.
    // Therefore, if there is a Node, use that; if not, use relativeFrame().
    FloatRect rect;
    if (Node* node = axObject->node()) {
        bool isReplaced;
        rect = axObject->convertFrameToSpace(node->absoluteBoundingRect(&isReplaced), AccessibilityConversionSpace::Page);
    } else
        rect = axObject->relativeFrame();
    m_rect = enclosingIntRect(rect);

    // ListBox, ListBoxOption
    if (axObject->isListBox()) {
        AccessibilityObject::AccessibilityChildrenVector children = axObject->selectedChildren();
        m_value = String::number(children.size());
    }
}

} // namespace WebKit
