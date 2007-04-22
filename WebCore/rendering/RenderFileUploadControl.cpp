/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "RenderFileUploadControl.h"

#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "Icon.h"
#include "LocalizedStrings.h"
#include "RenderButton.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "TextStyle.h"
#include <math.h>

using namespace std;

namespace WebCore {

using namespace HTMLNames;

const int afterButtonSpacing = 4;
const int iconHeight = 16;
const int iconWidth = 16;
const int iconFilenameSpacing = 2;
const int defaultWidthNumChars = 34;
const int buttonShadowHeight = 2;

class HTMLFileUploadInnerButtonElement : public HTMLInputElement {
public:
    HTMLFileUploadInnerButtonElement(Document*, Node* shadowParent);

    virtual bool isShadowNode() const { return true; }
    virtual Node* shadowParentNode() { return m_shadowParent; }

private:
    Node* m_shadowParent;    
};

RenderFileUploadControl::RenderFileUploadControl(HTMLInputElement* input)
    : RenderBlock(input)
    , m_button(0)
    , m_fileChooser(FileChooser::create(this, input->value()))
{
}

RenderFileUploadControl::~RenderFileUploadControl()
{
    if (m_button)
        m_button->detach();
    m_fileChooser->disconnectClient();
}

void RenderFileUploadControl::setStyle(RenderStyle* newStyle)
{
    // Force text-align to match the direction
    if (newStyle->direction() == LTR)
        newStyle->setTextAlign(LEFT);
    else
        newStyle->setTextAlign(RIGHT);

    RenderBlock::setStyle(newStyle);
    if (m_button)
        m_button->renderer()->setStyle(createButtonStyle(newStyle));

    setReplaced(isInline());
}

void RenderFileUploadControl::valueChanged()
{
    // onChange may destroy this renderer
    RefPtr<FileChooser> fileChooser = m_fileChooser;

    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(node());
    inputElement->setValueFromRenderer(fileChooser->filename());
    inputElement->onChange();
 
    // only repaint if it doesn't seem we have been destroyed
    if (!fileChooser->disconnected())
        repaint();
}

void RenderFileUploadControl::click()
{
     m_fileChooser->openFileChooser(node()->document());
}

void RenderFileUploadControl::updateFromElement()
{
    HTMLInputElement* inputElement = static_cast<HTMLInputElement*>(node());

    if (!m_button) {
        m_button = new HTMLFileUploadInnerButtonElement(document(), inputElement);
        m_button->setInputType("button");
        m_button->setValue(fileButtonChooseFileLabel());
        RenderStyle* buttonStyle = createButtonStyle(style());
        RenderObject* renderer = m_button->createRenderer(renderArena(), buttonStyle);
        m_button->setRenderer(renderer);
        renderer->setStyle(buttonStyle);
        renderer->updateFromElement();
        m_button->setAttached();
        m_button->setInDocument(true);

        addChild(renderer);
    }

    m_button->setDisabled(!theme()->isEnabled(this));

    // This only supports clearing out the filename, but that's OK because for
    // security reasons that's the only change the DOM is allowed to make.
    if (inputElement->value().isEmpty() || !m_fileChooser->filename().isEmpty()) {
        m_fileChooser->clear();
        repaint();
    }
}

int RenderFileUploadControl::maxFilenameWidth() const
{
    return max(0, contentWidth() - m_button->renderer()->width() - afterButtonSpacing
        - (m_fileChooser->icon() ? iconWidth + iconFilenameSpacing : 0));
}

RenderStyle* RenderFileUploadControl::createButtonStyle(RenderStyle* parentStyle) const
{
    RenderStyle* style = getPseudoStyle(RenderStyle::FILE_UPLOAD_BUTTON);
    if (!style) {
        style = new (renderArena()) RenderStyle;
        if (parentStyle)
            style->inheritFrom(parentStyle);
    }

    // Button text will wrap on file upload controls with widths smaller than the intrinsic button width
    // without this setWhiteSpace.
    style->setWhiteSpace(NOWRAP);

    return style;
}

void RenderFileUploadControl::paintObject(PaintInfo& paintInfo, int tx, int ty)
{
    // Push a clip.
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect(tx + borderLeft(), ty + borderTop(),
                         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop() + buttonShadowHeight);
        if (clipRect.isEmpty())
            return;
        paintInfo.context->save();
        paintInfo.context->clip(clipRect);
    }

    if (paintInfo.phase == PaintPhaseForeground) {
        const String& displayedFilename = m_fileChooser->basenameForWidth(style()->font(), maxFilenameWidth());        
        unsigned length = displayedFilename.length();
        const UChar* string = displayedFilename.characters();
        TextStyle textStyle(0, 0, 0, false, true);
        RenderBlock::CharacterBuffer characterBuffer;

        if (style()->direction() == RTL && style()->unicodeBidi() == Override)
            textStyle.setRTL(true);
        else if ((style()->direction() == RTL || style()->unicodeBidi() != Override) && !style()->visuallyOrdered()) {
            // If necessary, reorder characters by running the string through the bidi algorithm
            characterBuffer.append(string, length);
            RenderBlock::bidiReorderCharacters(document(), style(), characterBuffer);
            string = characterBuffer.data();
        }
        TextRun textRun(string, length);
        
        // Determine where the filename should be placed
        int contentLeft = tx + borderLeft() + paddingLeft();
        int buttonAndIconWidth = m_button->renderer()->width() + afterButtonSpacing
            + (m_fileChooser->icon() ? iconWidth + iconFilenameSpacing : 0);
        int textX;
        if (style()->direction() == LTR)
            textX = contentLeft + buttonAndIconWidth;
        else
            textX = contentLeft + contentWidth() - buttonAndIconWidth - style()->font().width(textRun);
        // We want to match the button's baseline
        RenderButton* buttonRenderer = static_cast<RenderButton*>(m_button->renderer());
        int textY = buttonRenderer->absoluteBoundingBoxRect().y()
            + buttonRenderer->marginTop() + buttonRenderer->borderTop() + buttonRenderer->paddingTop()
            + buttonRenderer->baselinePosition(true, false);

        paintInfo.context->setFont(style()->font());
        paintInfo.context->setFillColor(style()->color());
        
        // Draw the filename
        paintInfo.context->drawText(textRun, IntPoint(textX, textY), textStyle);
        
        if (m_fileChooser->icon()) {
            // Determine where the icon should be placed
            int iconY = ty + borderTop() + paddingTop() + (contentHeight() - iconHeight) / 2;
            int iconX;
            if (style()->direction() == LTR)
                iconX = contentLeft + m_button->renderer()->width() + afterButtonSpacing;
            else
                iconX = contentLeft + contentWidth() - m_button->renderer()->width() - afterButtonSpacing - iconWidth;

            // Draw the file icon
            m_fileChooser->icon()->paint(paintInfo.context, IntRect(iconX, iconY, iconWidth, iconHeight));
        }
    }

    // Paint the children.
    RenderBlock::paintObject(paintInfo, tx, ty);

    // Pop the clip.
    if (paintInfo.phase == PaintPhaseForeground || paintInfo.phase == PaintPhaseChildBlockBackgrounds)
        paintInfo.context->restore();
}

void RenderFileUploadControl::calcMinMaxWidth()
{
    m_minWidth = 0;
    m_maxWidth = 0;

    if (style()->width().isFixed() && style()->width().value() > 0)
        m_minWidth = m_maxWidth = calcContentBoxWidth(style()->width().value());
    else {
        // Figure out how big the filename space needs to be for a given number of characters
        // (using "0" as the nominal character).
        const UChar ch = '0';
        float charWidth = style()->font().floatWidth(TextRun(&ch, 1), TextStyle(0, 0, 0, false, false, false));
        m_maxWidth = (int)ceilf(charWidth * defaultWidthNumChars);
    }

    if (style()->minWidth().isFixed() && style()->minWidth().value() > 0) {
        m_maxWidth = max(m_maxWidth, calcContentBoxWidth(style()->minWidth().value()));
        m_minWidth = max(m_minWidth, calcContentBoxWidth(style()->minWidth().value()));
    } else if (style()->width().isPercent() || (style()->width().isAuto() && style()->height().isPercent()))
        m_minWidth = 0;
    else
        m_minWidth = m_maxWidth;

    if (style()->maxWidth().isFixed() && style()->maxWidth().value() != undefinedLength) {
        m_maxWidth = min(m_maxWidth, calcContentBoxWidth(style()->maxWidth().value()));
        m_minWidth = min(m_minWidth, calcContentBoxWidth(style()->maxWidth().value()));
    }

    int toAdd = paddingLeft() + paddingRight() + borderLeft() + borderRight();
    m_minWidth += toAdd;
    m_maxWidth += toAdd;

    setMinMaxKnown();
}

HTMLFileUploadInnerButtonElement::HTMLFileUploadInnerButtonElement(Document* doc, Node* shadowParent)
    : HTMLInputElement(doc)
    , m_shadowParent(shadowParent)
{
}

} // namespace WebCore
