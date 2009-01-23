/*
 * Copyright (C) 2006, 2008 Apple Inc. All rights reserved.
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
#include "RenderBox.h"
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
    , m_disableStack(0)
{
}

static bool isDeletableElement(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->inDocument() || !node->isContentEditable())
        return false;

    const int minimumWidth = 25;
    const int minimumHeight = 25;
    const unsigned minimumVisibleBorders = 3;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isBox())
        return false;

    RenderBox* box = toRenderBox(renderer);
    if (box->width() < minimumWidth || box->height() < minimumHeight)
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
            unsigned visibleBorders = style->borderTop().isVisible() + style->borderBottom().isVisible() + style->borderLeft().isVisible() + style->borderRight().isVisible();
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

    Node* element = enclosingNodeOfType(Position(container, 0), &isDeletableElement);
    if (!element)
        return 0;

    ASSERT(element->isHTMLElement());
    return static_cast<HTMLElement*>(element);
}

void DeleteButtonController::respondToChangedSelection(const Selection& oldSelection)
{
    if (!enabled())
        return;

    HTMLElement* oldElement = enclosingDeletableElement(oldSelection);
    HTMLElement* newElement = enclosingDeletableElement(m_frame->selection()->selection());
    if (oldElement == newElement)
        return;

    // If the base is inside a deletable element, give the element a delete widget.
    if (newElement)
        show(newElement);
    else
        hide();
}

void DeleteButtonController::createDeletionUI()
{
    RefPtr<HTMLDivElement> container = new HTMLDivElement(divTag, m_target->document());
    container->setId(containerElementIdentifier);

    CSSMutableStyleDeclaration* style = container->getInlineStyleDecl();
    style->setProperty(CSSPropertyWebkitUserDrag, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserSelect, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserModify, CSSValueNone);

    RefPtr<HTMLDivElement> outline = new HTMLDivElement(divTag, m_target->document());
    outline->setId(outlineElementIdentifier);

    const int borderWidth = 4;
    const int borderRadius = 6;

    style = outline->getInlineStyleDecl();
    style->setProperty(CSSPropertyPosition, CSSValueAbsolute);
    style->setProperty(CSSPropertyCursor, CSSValueDefault);
    style->setProperty(CSSPropertyWebkitUserDrag, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserSelect, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserModify, CSSValueNone);
    style->setProperty(CSSPropertyZIndex, String::number(-1000000));
    style->setProperty(CSSPropertyTop, String::number(-borderWidth - m_target->renderBox()->borderTop()) + "px");
    style->setProperty(CSSPropertyRight, String::number(-borderWidth - m_target->renderBox()->borderRight()) + "px");
    style->setProperty(CSSPropertyBottom, String::number(-borderWidth - m_target->renderBox()->borderBottom()) + "px");
    style->setProperty(CSSPropertyLeft, String::number(-borderWidth - m_target->renderBox()->borderLeft()) + "px");
    style->setProperty(CSSPropertyBorder, String::number(borderWidth) + "px solid rgba(0, 0, 0, 0.6)");
    style->setProperty(CSSPropertyWebkitBorderRadius, String::number(borderRadius) + "px");

    ExceptionCode ec = 0;
    container->appendChild(outline.get(), ec);
    ASSERT(ec == 0);
    if (ec)
        return;

    RefPtr<DeleteButton> button = new DeleteButton(m_target->document());
    button->setId(buttonElementIdentifier);

    const int buttonWidth = 30;
    const int buttonHeight = 30;
    const int buttonBottomShadowOffset = 2;

    style = button->getInlineStyleDecl();
    style->setProperty(CSSPropertyPosition, CSSValueAbsolute);
    style->setProperty(CSSPropertyCursor, CSSValueDefault);
    style->setProperty(CSSPropertyWebkitUserDrag, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserSelect, CSSValueNone);
    style->setProperty(CSSPropertyWebkitUserModify, CSSValueNone);
    style->setProperty(CSSPropertyZIndex, String::number(1000000));
    style->setProperty(CSSPropertyTop, String::number((-buttonHeight / 2) - m_target->renderBox()->borderTop() - (borderWidth / 2) + buttonBottomShadowOffset) + "px");
    style->setProperty(CSSPropertyLeft, String::number((-buttonWidth / 2) - m_target->renderBox()->borderLeft() - (borderWidth / 2)) + "px");
    style->setProperty(CSSPropertyWidth, String::number(buttonWidth) + "px");
    style->setProperty(CSSPropertyHeight, String::number(buttonHeight) + "px");

    RefPtr<Image> buttonImage = Image::loadPlatformResource("deleteButton");
    if (buttonImage->isNull())
        return;

    button->setCachedImage(new CachedImage(buttonImage.get()));

    container->appendChild(button.get(), ec);
    ASSERT(ec == 0);
    if (ec)
        return;

    m_containerElement = container.release();
    m_outlineElement = outline.release();
    m_buttonElement = button.release();
}

void DeleteButtonController::show(HTMLElement* element)
{
    hide();

    if (!enabled() || !element || !element->inDocument() || !isDeletableElement(element))
        return;

    if (!m_frame->editor()->shouldShowDeleteInterface(static_cast<HTMLElement*>(element)))
        return;

    // we rely on the renderer having current information, so we should update the layout if needed
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    m_target = element;

    if (!m_containerElement) {
        createDeletionUI();
        if (!m_containerElement) {
            hide();
            return;
        }
    }

    ExceptionCode ec = 0;
    m_target->appendChild(m_containerElement.get(), ec);
    ASSERT(ec == 0);
    if (ec) {
        hide();
        return;
    }

    if (m_target->renderer()->style()->position() == StaticPosition) {
        m_target->getInlineStyleDecl()->setProperty(CSSPropertyPosition, CSSValueRelative);
        m_wasStaticPositioned = true;
    }

    if (m_target->renderer()->style()->hasAutoZIndex()) {
        m_target->getInlineStyleDecl()->setProperty(CSSPropertyZIndex, "0");
        m_wasAutoZIndex = true;
    }
}

void DeleteButtonController::hide()
{
    m_outlineElement = 0;
    m_buttonElement = 0;

    ExceptionCode ec = 0;
    if (m_containerElement && m_containerElement->parentNode())
        m_containerElement->parentNode()->removeChild(m_containerElement.get(), ec);

    if (m_target) {
        if (m_wasStaticPositioned)
            m_target->getInlineStyleDecl()->setProperty(CSSPropertyPosition, CSSValueStatic);
        if (m_wasAutoZIndex)
            m_target->getInlineStyleDecl()->setProperty(CSSPropertyZIndex, CSSValueAuto);
    }

    m_wasStaticPositioned = false;
    m_wasAutoZIndex = false;
}

void DeleteButtonController::enable()
{
    ASSERT(m_disableStack > 0);
    if (m_disableStack > 0)
        m_disableStack--;
    if (enabled())
        show(enclosingDeletableElement(m_frame->selection()->selection()));
}

void DeleteButtonController::disable()
{
    if (enabled())
        hide();
    m_disableStack++;
}

void DeleteButtonController::deleteTarget()
{
    if (!enabled() || !m_target)
        return;

    RefPtr<Node> element = m_target;
    hide();

    // Because the deletion UI only appears when the selection is entirely
    // within the target, we unconditionally update the selection to be
    // a caret where the target had been.
    Position pos = positionBeforeNode(element.get());
    applyCommand(RemoveNodeCommand::create(element.release()));
    m_frame->selection()->setSelection(VisiblePosition(pos));
}

} // namespace WebCore
