/*
 * Copyright (C) 2006, 2008, 2009 Apple Inc. All rights reserved.
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
#include "CSSPrimitiveValue.h"
#include "CompositeEditCommand.h"
#include "Document.h"
#include "EditorClient.h"
#include "htmlediting.h"
#include "HTMLDivElement.h"
#include "HTMLNames.h"
#include "Image.h"
#include "Node.h"
#include "Page.h"
#include "RemoveNodeCommand.h"
#include "RenderBox.h"
#include "StyleProperties.h"

namespace WebCore {

using namespace HTMLNames;

#if ENABLE(DELETION_UI)

const char* const DeleteButtonController::containerElementIdentifier = "WebKit-Editing-Delete-Container";
const char* const DeleteButtonController::buttonElementIdentifier = "WebKit-Editing-Delete-Button";
const char* const DeleteButtonController::outlineElementIdentifier = "WebKit-Editing-Delete-Outline";

DeleteButtonController::DeleteButtonController(Frame& frame)
    : m_frame(frame)
    , m_wasStaticPositioned(false)
    , m_wasAutoZIndex(false)
    , m_disableStack(0)
{
}

static bool isDeletableElement(const Node* node)
{
    if (!node || !node->isHTMLElement() || !node->inDocument() || !node->hasEditableStyle())
        return false;

    // In general we want to only draw the UI around object of a certain area, but we still keep the min width/height to
    // make sure we don't end up with very thin or very short elements getting the UI.
    const int minimumArea = 2500;
    const int minimumWidth = 48;
    const int minimumHeight = 16;
    const unsigned minimumVisibleBorders = 1;

    RenderObject* renderer = node->renderer();
    if (!renderer || !renderer->isBox())
        return false;

    // Disallow the body element since it isn't practical to delete, and the deletion UI would be clipped.
    if (node->hasTagName(bodyTag))
        return false;

    // Disallow elements with any overflow clip, since the deletion UI would be clipped as well. <rdar://problem/6840161>
    if (renderer->hasOverflowClip())
        return false;

    // Disallow Mail blockquotes since the deletion UI would get in the way of editing for these.
    if (isMailBlockquote(node))
        return false;

    RenderBox* box = toRenderBox(renderer);
    IntRect borderBoundingBox = box->borderBoundingBox();
    if (borderBoundingBox.width() < minimumWidth || borderBoundingBox.height() < minimumHeight)
        return false;

    if ((borderBoundingBox.width() * borderBoundingBox.height()) < minimumArea)
        return false;

    if (box->isTable())
        return true;

    if (node->hasTagName(ulTag) || node->hasTagName(olTag) || node->hasTagName(iframeTag))
        return true;

    if (box->isOutOfFlowPositioned())
        return true;

    if (box->isRenderBlock() && !box->isTableCell()) {
        const RenderStyle& style = box->style();

        // Allow blocks that have background images
        if (style.hasBackgroundImage()) {
            for (const FillLayer* background = style.backgroundLayers(); background; background = background->next()) {
                if (background->image() && background->image()->canRender(box, 1))
                    return true;
            }
        }

        // Allow blocks with a minimum number of non-transparent borders
        unsigned visibleBorders = style.borderTop().isVisible() + style.borderBottom().isVisible() + style.borderLeft().isVisible() + style.borderRight().isVisible();
        if (visibleBorders >= minimumVisibleBorders)
            return true;

        // Allow blocks that have a different background from it's parent
        ContainerNode* parentNode = node->parentNode();
        if (!parentNode)
            return false;

        auto parentRenderer = parentNode->renderer();
        if (!parentRenderer)
            return false;

        const RenderStyle& parentStyle = parentRenderer->style();

        if (box->hasBackground() && (!parentRenderer->hasBackground() || style.visitedDependentColor(CSSPropertyBackgroundColor) != parentStyle.visitedDependentColor(CSSPropertyBackgroundColor)))
            return true;
    }

    return false;
}

static HTMLElement* enclosingDeletableElement(const VisibleSelection& selection)
{
    if (!selection.isContentEditable())
        return 0;

    RefPtr<Range> range = selection.toNormalizedRange();
    if (!range)
        return 0;

    Node* container = range->commonAncestorContainer(ASSERT_NO_EXCEPTION);
    ASSERT(container);

    // The enclosingNodeOfType function only works on nodes that are editable
    // (which is strange, given its name).
    if (!container->hasEditableStyle())
        return 0;

    Node* element = enclosingNodeOfType(firstPositionInNode(container), &isDeletableElement);
    return element && element->isHTMLElement() ? toHTMLElement(element) : 0;
}

void DeleteButtonController::respondToChangedSelection(const VisibleSelection& oldSelection)
{
    if (!enabled())
        return;

    HTMLElement* oldElement = enclosingDeletableElement(oldSelection);
    HTMLElement* newElement = enclosingDeletableElement(m_frame.selection().selection());
    if (oldElement == newElement)
        return;

    // If the base is inside a deletable element, give the element a delete widget.
    if (newElement)
        show(newElement);
    else
        hide();
}

void DeleteButtonController::deviceScaleFactorChanged()
{
    if (!enabled())
        return;
    
    HTMLElement* currentTarget = m_target.get();
    hide();

    // Setting m_containerElement to 0 will force the deletionUI to be re-created with
    // artwork of the appropriate resolution in show().
    m_containerElement = 0;
    show(currentTarget);
}

void DeleteButtonController::createDeletionUI()
{
    RefPtr<HTMLDivElement> container = HTMLDivElement::create(m_target->document());
    container->setIdAttribute(containerElementIdentifier);

    container->setInlineStyleProperty(CSSPropertyWebkitUserDrag, CSSValueNone);
    container->setInlineStyleProperty(CSSPropertyWebkitUserSelect, CSSValueNone);
    container->setInlineStyleProperty(CSSPropertyWebkitUserModify, CSSValueReadOnly);
    container->setInlineStyleProperty(CSSPropertyVisibility, CSSValueHidden);
    container->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    container->setInlineStyleProperty(CSSPropertyCursor, CSSValueDefault);
    container->setInlineStyleProperty(CSSPropertyTop, 0, CSSPrimitiveValue::CSS_PX);
    container->setInlineStyleProperty(CSSPropertyRight, 0, CSSPrimitiveValue::CSS_PX);
    container->setInlineStyleProperty(CSSPropertyBottom, 0, CSSPrimitiveValue::CSS_PX);
    container->setInlineStyleProperty(CSSPropertyLeft, 0, CSSPrimitiveValue::CSS_PX);

    RefPtr<HTMLDivElement> outline = HTMLDivElement::create(m_target->document());
    outline->setIdAttribute(outlineElementIdentifier);

    const int borderWidth = 4;
    const int borderRadius = 6;

    outline->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    outline->setInlineStyleProperty(CSSPropertyZIndex, ASCIILiteral("-1000000"));
    outline->setInlineStyleProperty(CSSPropertyTop, -borderWidth - m_target->renderBox()->borderTop(), CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyRight, -borderWidth - m_target->renderBox()->borderRight(), CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyBottom, -borderWidth - m_target->renderBox()->borderBottom(), CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyLeft, -borderWidth - m_target->renderBox()->borderLeft(), CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyBorderWidth, borderWidth, CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyBorderStyle, CSSValueSolid);
    outline->setInlineStyleProperty(CSSPropertyBorderColor, ASCIILiteral("rgba(0, 0, 0, 0.6)"));
    outline->setInlineStyleProperty(CSSPropertyBorderRadius, borderRadius, CSSPrimitiveValue::CSS_PX);
    outline->setInlineStyleProperty(CSSPropertyVisibility, CSSValueVisible);

    ExceptionCode ec = 0;
    container->appendChild(outline.get(), ec);
    ASSERT(!ec);
    if (ec)
        return;

    RefPtr<DeleteButton> button = DeleteButton::create(m_target->document());
    button->setIdAttribute(buttonElementIdentifier);

    const int buttonWidth = 30;
    const int buttonHeight = 30;
    const int buttonBottomShadowOffset = 2;

    button->setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    button->setInlineStyleProperty(CSSPropertyZIndex, ASCIILiteral("1000000"));
    button->setInlineStyleProperty(CSSPropertyTop, (-buttonHeight / 2) - m_target->renderBox()->borderTop() - (borderWidth / 2) + buttonBottomShadowOffset, CSSPrimitiveValue::CSS_PX);
    button->setInlineStyleProperty(CSSPropertyLeft, (-buttonWidth / 2) - m_target->renderBox()->borderLeft() - (borderWidth / 2), CSSPrimitiveValue::CSS_PX);
    button->setInlineStyleProperty(CSSPropertyWidth, buttonWidth, CSSPrimitiveValue::CSS_PX);
    button->setInlineStyleProperty(CSSPropertyHeight, buttonHeight, CSSPrimitiveValue::CSS_PX);
    button->setInlineStyleProperty(CSSPropertyVisibility, CSSValueVisible);

    float deviceScaleFactor = WebCore::deviceScaleFactor(&m_frame);
    RefPtr<Image> buttonImage;
    if (deviceScaleFactor >= 2)
        buttonImage = Image::loadPlatformResource("deleteButton@2x");
    else
        buttonImage = Image::loadPlatformResource("deleteButton");

    if (buttonImage->isNull())
        return;

    button->setCachedImage(new CachedImage(buttonImage.get()));

    container->appendChild(button.get(), ec);
    ASSERT(!ec);
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

    EditorClient* client = m_frame.editor().client();
    if (!client || !client->shouldShowDeleteInterface(element))
        return;

    // we rely on the renderer having current information, so we should update the layout if needed
    m_frame.document()->updateLayoutIgnorePendingStylesheets();

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
    ASSERT(!ec);
    if (ec) {
        hide();
        return;
    }

    if (m_target->renderer()->style().position() == StaticPosition) {
        m_target->setInlineStyleProperty(CSSPropertyPosition, CSSValueRelative);
        m_wasStaticPositioned = true;
    }

    if (m_target->renderer()->style().hasAutoZIndex()) {
        m_target->setInlineStyleProperty(CSSPropertyZIndex, ASCIILiteral("0"));
        m_wasAutoZIndex = true;
    }
}

void DeleteButtonController::hide()
{
    m_outlineElement = 0;
    m_buttonElement = 0;

    if (m_containerElement && m_containerElement->parentNode())
        m_containerElement->parentNode()->removeChild(m_containerElement.get(), IGNORE_EXCEPTION);

    if (m_target) {
        if (m_wasStaticPositioned)
            m_target->setInlineStyleProperty(CSSPropertyPosition, CSSValueStatic);
        if (m_wasAutoZIndex)
            m_target->setInlineStyleProperty(CSSPropertyZIndex, CSSValueAuto);
    }

    m_wasStaticPositioned = false;
    m_wasAutoZIndex = false;
}

void DeleteButtonController::enable()
{
#if !PLATFORM(IOS)
    ASSERT(m_disableStack > 0);
    if (m_disableStack > 0)
        m_disableStack--;
    if (enabled()) {
        // Determining if the element is deletable currently depends on style
        // because whether something is editable depends on style, so we need
        // to recalculate style before calling enclosingDeletableElement.
        m_frame.document()->updateStyleIfNeeded();
        show(enclosingDeletableElement(m_frame.selection().selection()));
    }
#endif
}

void DeleteButtonController::disable()
{
#if !PLATFORM(IOS)
    if (enabled())
        hide();
    m_disableStack++;
#endif
}

class RemoveTargetCommand : public CompositeEditCommand {
public:
    static PassRefPtr<RemoveTargetCommand> create(Document& document, PassRefPtr<Node> target)
    {
        return adoptRef(new RemoveTargetCommand(document, target));
    }

private:
    RemoveTargetCommand(Document& document, PassRefPtr<Node> target)
        : CompositeEditCommand(document)
        , m_target(target)
    { }

    void doApply()
    {
        removeNode(m_target);
    }

private:
    RefPtr<Node> m_target;
};

void DeleteButtonController::deleteTarget()
{
    if (!enabled() || !m_target)
        return;

    hide();

    // Because the deletion UI only appears when the selection is entirely
    // within the target, we unconditionally update the selection to be
    // a caret where the target had been.
    Position pos = positionInParentBeforeNode(m_target.get());
    ASSERT(m_frame.document());
    applyCommand(RemoveTargetCommand::create(*m_frame.document(), m_target));
    m_frame.selection().setSelection(VisiblePosition(pos));
}
#endif

} // namespace WebCore
