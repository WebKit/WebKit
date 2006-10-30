/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
#include "DeleteButtonController.h"

#include "CachedImage.h"
#include "CSSMutableStyleDeclaration.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyNames.h"
#include "CSSValueKeywords.h"
#include "DeleteButton.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "htmlediting.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "Node.h"
#include "Range.h"
#include "RemoveNodeCommand.h"
#include "RenderObject.h"
#include "SelectionController.h"

namespace WebCore {

using namespace HTMLNames;

const char* const DeleteButtonController::containerElementIdentifier = "WebKit-Editing-Delete-Container";
const char* const DeleteButtonController::buttonElementIdentifier = "WebKit-Editing-Delete-Button";
const char* const DeleteButtonController::outlineElementIdentifier = "WebKit-Editing-Delete-Outline";

DeleteButtonController::DeleteButtonController(Frame* frame)
    : m_frame(frame)
    , m_wasStaticPositioned(false)
    , m_wasAutoZIndex(false)
{
}

static bool isDeletableElement(Node* node)
{
    if (!node || !node->isHTMLElement() || !node->isContentEditable())
        return false;

    const unsigned minimumWidth = 25;
    const unsigned minimumHeight = 25;
    const unsigned minimumVisibleBorders = 3;

    RenderObject* renderer = node->renderer();
    if (!renderer || renderer->width() < minimumWidth || renderer->height() < minimumHeight)
        return false;

    if (renderer->isTable())
        return true;

    if (node->hasTagName(ulTag) || node->hasTagName(olTag))
        return true;

    if (renderer->isPositioned())
        return true;

    // allow block elements (excluding table cells) that have some non-transparent borders
    if (renderer->isRenderBlock() && !renderer->isTableCell()) {
        RenderStyle* style = renderer->style();
        if (style && style->hasBorder()) {
            unsigned visibleBorders = !style->borderTop().isTransparent() + !style->borderBottom().isTransparent() + !style->borderLeft().isTransparent() + !style->borderRight().isTransparent();
            if (visibleBorders >= minimumVisibleBorders)
                return true;
        }
    }

    return false;
}

static HTMLElement* enclosingDeletableElement(const Selection& selection)
{
    if (!selection.isContentEditable())
        return 0;

    RefPtr<Range> range = selection.toRange();
    if (!range)
        return 0;

    ExceptionCode ec = 0;
    Node* container = range->commonAncestorContainer(ec);
    ASSERT(container);
    ASSERT(ec == 0);

    // The enclosingNodeOfType function only works on nodes that are editable
    // (which is strange, given its name).
    if (!container->isContentEditable())
        return 0;

    Node* element = enclosingNodeOfType(container, &isDeletableElement);
    if (!element)
        return 0;

    ASSERT(element->isHTMLElement());
    return static_cast<HTMLElement*>(element);
}

void DeleteButtonController::respondToChangedSelection(const Selection& oldSelection)
{
    HTMLElement* oldElement = enclosingDeletableElement(oldSelection);
    HTMLElement* newElement = enclosingDeletableElement(m_frame->selectionController()->selection());
    if (oldElement == newElement)
        return;

    // If the base is inside a deletable element, give the element a delete widget.
    if (newElement)
        show(newElement);
    else
        hide();
}

void DeleteButtonController::respondToChangedContents()
{
    updateOutlineStyle();
}

void DeleteButtonController::updateOutlineStyle()
{
    if (!m_element || !m_element->renderer() || !m_outlineElement)
        return;

    CSSMutableStyleDeclaration* style = m_outlineElement->getInlineStyleDecl();
    style->setProperty(CSS_PROP_WIDTH, String::number(m_element->renderer()->overflowWidth()) + "px");
    style->setProperty(CSS_PROP_HEIGHT, String::number(m_element->renderer()->overflowHeight()) + "px");
}

void DeleteButtonController::show(HTMLElement* element)
{
    hide();

    if (!element->renderer() || !element->renderer()->isRenderBlock())
        return;

    if (!m_frame->editor()->shouldShowDeleteInterface(static_cast<HTMLElement*>(element)))
        return;

    m_element = element;

    m_containerElement = new HTMLDivElement(m_element->document());
    m_containerElement->setId(containerElementIdentifier);

    CSSMutableStyleDeclaration* style = m_containerElement->getInlineStyleDecl();
    style->setProperty(CSS_PROP_POSITION, CSS_VAL_ABSOLUTE);
    style->setProperty(CSS_PROP_TOP, String::number(-m_element->renderer()->borderTop()) + "px");
    style->setProperty(CSS_PROP_LEFT, String::number(-m_element->renderer()->borderLeft()) + "px");
    style->setProperty(CSS_PROP__WEBKIT_USER_DRAG, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_SELECT, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_NONE);

    m_outlineElement = new HTMLDivElement(m_element->document());
    m_outlineElement->setId(outlineElementIdentifier);

    const int borderWidth = 4;
    const int borderRadius = 6;

    style = m_outlineElement->getInlineStyleDecl();
    style->setProperty(CSS_PROP_POSITION, CSS_VAL_ABSOLUTE);
    style->setProperty(CSS_PROP_CURSOR, CSS_VAL_DEFAULT);
    style->setProperty(CSS_PROP__WEBKIT_USER_DRAG, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_SELECT, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_NONE);
    style->setProperty(CSS_PROP_Z_INDEX, String::number(-1));
    style->setProperty(CSS_PROP_TOP, String::number(-borderWidth) + "px");
    style->setProperty(CSS_PROP_LEFT, String::number(-borderWidth) + "px");
    style->setProperty(CSS_PROP_BORDER, String::number(borderWidth) + "px solid rgba(0, 0, 0, 0.6)");
    style->setProperty(CSS_PROP__WEBKIT_BORDER_RADIUS, String::number(borderRadius) + "px");

    updateOutlineStyle();

    ExceptionCode ec = 0;
    m_containerElement->appendChild(m_outlineElement.get(), ec);
    ASSERT(ec == 0);
    if (ec) {
        hide();
        return;
    }

    m_buttonElement = new DeleteButton(m_element->document());
    m_buttonElement->setId(buttonElementIdentifier);

    const int buttonWidth = 30;
    const int buttonHeight = 30;

    style = m_buttonElement->getInlineStyleDecl();
    style->setProperty(CSS_PROP_POSITION, CSS_VAL_ABSOLUTE);
    style->setProperty(CSS_PROP_CURSOR, CSS_VAL_DEFAULT);
    style->setProperty(CSS_PROP__WEBKIT_USER_DRAG, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_SELECT, CSS_VAL_NONE);
    style->setProperty(CSS_PROP__WEBKIT_USER_MODIFY, CSS_VAL_NONE);
    style->setProperty(CSS_PROP_LEFT, String::number(-buttonWidth / 2) + "px");
    style->setProperty(CSS_PROP_TOP, String::number(-buttonHeight / 2) + "px");
    style->setProperty(CSS_PROP_WIDTH, String::number(buttonWidth) + "px");
    style->setProperty(CSS_PROP_HEIGHT, String::number(buttonHeight) + "px");

    m_buttonElement->setCachedImage(new CachedImage(Image::loadPlatformResource("deleteButton")));

    m_containerElement->appendChild(m_buttonElement.get(), ec);
    ASSERT(ec == 0);
    if (ec) {
        hide();
        return;
    }

    m_element->appendChild(m_containerElement.get(), ec);
    ASSERT(ec == 0);
    if (ec) {
        hide();
        return;
    }

    if (m_element->renderer()->style()->position() == StaticPosition) {
        m_element->getInlineStyleDecl()->setProperty(CSS_PROP_POSITION, CSS_VAL_RELATIVE);
        m_wasStaticPositioned = true;
    }

    if (m_element->renderer()->style()->hasAutoZIndex()) {
        m_element->getInlineStyleDecl()->setProperty(CSS_PROP_Z_INDEX, "0");
        m_wasAutoZIndex = true;
    }

    m_element->renderer()->repaint(true);
}

void DeleteButtonController::hide()
{
    ExceptionCode ec = 0;
    if (m_containerElement)
        m_containerElement->parentNode()->removeChild(m_containerElement.get(), ec);

    if (m_element && m_wasStaticPositioned)
        m_element->getInlineStyleDecl()->setProperty(CSS_PROP_POSITION, CSS_VAL_STATIC);

    if (m_element && m_wasAutoZIndex)
        m_element->getInlineStyleDecl()->setProperty(CSS_PROP_Z_INDEX, CSS_VAL_AUTO);

    m_wasStaticPositioned = false;
    m_wasAutoZIndex = false;
    m_element = 0;
    m_outlineElement = 0;
    m_buttonElement = 0;
    m_containerElement = 0;
}

void DeleteButtonController::deleteTarget()
{
    if (!m_element)
        return;

    RefPtr<Node> element = m_element;
    hide();

    RefPtr<RemoveNodeCommand> command = new RemoveNodeCommand(element.get());
    command->apply();
}

} // namespace WebCore
