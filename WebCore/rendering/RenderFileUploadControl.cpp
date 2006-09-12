/**
 *
 * Copyright (C) 2006 Apple Computer, Inc.
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

const int afterButtonSpacing = 4;
const int iconHeight = 16;
const int iconWidth = 16;
const int iconFilenameSpacing = 2;
const int defaultFilenameNumChars = 23;

using namespace HTMLNames;

class RenderFileUploadInnerFileBox : public RenderFlexibleBox {
public:
    RenderFileUploadInnerFileBox(Node*, RenderFileUploadControl*);
    
    virtual void setStyle(RenderStyle*);
    virtual void calcMinMaxWidth();
    virtual void layout();
    
    void setHasIcon(bool hasIcon = true);
    
    void setFilename(const String&);
    
    virtual const char* renderName() const { return "RenderFileUploadInnerFileBox"; }
    
private:
    RenderFileUploadControl* m_uploadControl;
    RenderText* m_filenameText;
    
    String m_filename;
};

class HTMLFileUploadInnerButtonElement : public HTMLInputElement {
public:
    HTMLFileUploadInnerButtonElement(Document*, Node* shadowParent = 0);
    
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    
    virtual bool isShadowNode() const { return true; }
    
    virtual Node* shadowParentNode() { return m_shadowParent; }
    
private:
        Node* m_shadowParent;    
};
    
RenderFileUploadControl::RenderFileUploadControl(Node* node)
    : RenderFlexibleBox(node)
    , m_button(0)
    , m_fileBox(0)
    , m_fileChooser(new FileChooser(document(), this))
{
}

RenderFileUploadControl::~RenderFileUploadControl()
{
    if (m_button)
        m_button->detach();
    if (m_fileChooser) {
        m_fileChooser->uploadControlDetaching();
        delete m_fileChooser;
    }
}

void RenderFileUploadControl::setStyle(RenderStyle* s)
{
    RenderFlexibleBox::setStyle(s);
    if (m_button)
        m_button->renderer()->setStyle(createButtonStyle(s));
    if (m_fileBox) {
        RenderStyle* fileBoxStyle = new (renderArena()) RenderStyle();
        fileBoxStyle->inheritFrom(s);
        m_fileBox->setStyle(fileBoxStyle);
    }
    setReplaced(isInline());
}

void RenderFileUploadControl::valueChanged()
{
    static_cast<HTMLInputElement*>(node())->setValueFromRenderer(m_fileChooser->filename());
    static_cast<HTMLInputElement*>(node())->onChange();
    updateIconAndFilename();
}

void RenderFileUploadControl::updateIconAndFilename()
{
    if (m_fileChooser->icon())
        m_fileBox->setHasIcon();
    else
        m_fileBox->setHasIcon(false);
    
    // FIXME: We don't actually seem to truncate the string when our width is less than the button width and we're in RTL
    m_fileBox->setFilename(m_fileChooser->basenameForWidth(maxFilenameWidth()));
}

void RenderFileUploadControl::click(bool sendMouseEvents)
{
    m_fileChooser->openFileChooser();
}

void RenderFileUploadControl::updateFromElement()
{
    if (!m_button) {
        m_button = new HTMLFileUploadInnerButtonElement(document(), node());
        RenderStyle* buttonStyle = createButtonStyle(style());
        m_button->setRenderer(m_button->createRenderer(renderArena(), buttonStyle));
        m_button->renderer()->setStyle(buttonStyle);
        static_cast<RenderButton*>(m_button->renderer())->setText(fileButtonChooseFileLabel());
        m_button->setAttached();
        m_button->setInDocument(true);
        
        addChild(m_button->renderer());
    }

    if (!m_fileBox) {
        m_fileBox = new (renderArena()) RenderFileUploadInnerFileBox(document(), this);
        RenderStyle* fileBoxStyle = new (renderArena()) RenderStyle();
        fileBoxStyle->inheritFrom(style());
        m_fileBox->setStyle(fileBoxStyle);
        addChild(m_fileBox);
    }

    updateIconAndFilename();
}

int RenderFileUploadControl::maxFilenameWidth()
{
    return max(0, m_fileBox->contentWidth());
}

RenderStyle* RenderFileUploadControl::createButtonStyle(RenderStyle* parentStyle)
{
    RenderStyle* style = getPseudoStyle(RenderStyle::FILE_UPLOAD_BUTTON);
    if (!style)
        style = new (renderArena()) RenderStyle();
    
    if (parentStyle)
        style->inheritFrom(parentStyle);
    
    // Button text will wrap on file upload controls with widths smaller than the intrinsic button width
    // without this setWhiteSpace.
    style->setWhiteSpace(NOWRAP);
    
    // This makes the button block-level so that the flexbox layout works correctly.
    style->setDisplay(BOX);
    
    return style;
}

void RenderFileUploadControl::paintObject(PaintInfo& i, int tx, int ty)
{
    // Since we draw the file icon without going through the graphics context,
    // we need to make sure painting is not disabled before drawing
    if (i.p->paintingDisabled())
        return;
    
    // Push a clip.
    if (i.phase == PaintPhaseForeground || i.phase == PaintPhaseChildBlockBackgrounds) {
        IntRect clipRect(tx + borderLeft(), ty + borderTop(),
                         width() - borderLeft() - borderRight(), height() - borderBottom() - borderTop());
        if (clipRect.width() == 0 || clipRect.height() == 0)
            return;
        i.p->save();
        i.p->addClip(clipRect);
    }
    
    if (i.phase == PaintPhaseForeground && m_fileChooser->icon()) {
        // Draw the file icon
        int top = m_fileBox->absoluteBoundingBoxRect().y() + (m_fileBox->absoluteBoundingBoxRect().height() - iconHeight) / 2;
        int left;
        if (style()->direction() == LTR)
            left = m_fileBox->absoluteBoundingBoxRect().x() + afterButtonSpacing;
        else
            left = m_fileBox->absoluteBoundingBoxRect().right() - afterButtonSpacing - iconWidth;
        m_fileChooser->icon()->paint(i.p, IntRect(left, top, iconWidth, iconHeight));
    }
    
    // Paint the children.
    RenderFlexibleBox::paintObject(i, tx, ty);
    
    // Pop the clip.
    if (i.phase == PaintPhaseForeground || i.phase == PaintPhaseChildBlockBackgrounds)
        i.p->restore();
}

RenderFileUploadInnerFileBox::RenderFileUploadInnerFileBox(Node* node, RenderFileUploadControl* uploadControl)
    : RenderFlexibleBox(node)
    , m_uploadControl(uploadControl)
    , m_filenameText(0)
{
}

void RenderFileUploadInnerFileBox::setStyle(RenderStyle* s)
{
    if (m_filenameText)
        m_filenameText->setStyle(s);
    
    s->setBoxFlex(1.0f);
    s->setDisplay(BOX);
    s->setWhiteSpace(NOWRAP);

    if (s->direction() == LTR)
        s->setPaddingLeft(Length(afterButtonSpacing, Fixed));
    else
        s->setPaddingRight(Length(afterButtonSpacing, Fixed));
    
    setReplaced(isInline());
    
    RenderFlexibleBox::setStyle(s);
}

void RenderFileUploadInnerFileBox::layout()
{
    RenderFlexibleBox::layout();
    
    ASSERT(minMaxKnown());
    // We should know our width now, so display the filename
    m_uploadControl->updateIconAndFilename();
}

void RenderFileUploadInnerFileBox::setHasIcon(bool hasIcon)
{
    int padding = afterButtonSpacing;
    
    if (hasIcon)
        padding += iconWidth + iconFilenameSpacing;
    
    if (style()->direction() == LTR)
        style()->setPaddingLeft(Length(padding, Fixed));
    else
        style()->setPaddingRight(Length(padding, Fixed));
}

void RenderFileUploadInnerFileBox::setFilename(const String& filename)
{
    if (!m_filenameText) {
        m_filenameText = new (renderArena()) RenderText(document(), String("").impl());
        m_filenameText->setStyle(style());
        addChild(m_filenameText);
    }
    
    if (filename != m_filename) {
        m_filenameText->setText(filename.impl());
        m_filename = filename;
        m_uploadControl->node()->setChanged();
    }
}

void RenderFileUploadInnerFileBox::calcMinMaxWidth()
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
        m_maxWidth = (int)ceilf(charWidth * defaultFilenameNumChars);
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
    setInputType("button");
}

RenderObject* HTMLFileUploadInnerButtonElement::createRenderer(RenderArena* arena, RenderStyle* style)
{
    return HTMLInputElement::createRenderer(arena, style);
}

}
