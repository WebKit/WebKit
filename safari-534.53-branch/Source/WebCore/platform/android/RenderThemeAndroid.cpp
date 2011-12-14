/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderThemeAndroid.h"

#include "Color.h"
#include "Element.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOptionElement.h"
#include "HTMLSelectElement.h"
#include "Node.h"
#include "PlatformGraphicsContext.h"
#include "RenderSkinAndroid.h"
#include "RenderSkinButton.h"
#include "RenderSkinCombo.h"
#include "RenderSkinRadio.h"
#include "SkCanvas.h"

namespace WebCore {

// Add a constant amount of padding to the textsize to get the final height
// of buttons, so that our button images are large enough to properly fit
// the text.
const int buttonPadding = 18;

// Add padding to the fontSize of ListBoxes to get their maximum sizes.
// Listboxes often have a specified size.  Since we change them into
// dropdowns, we want a much smaller height, which encompasses the text.
const int listboxPadding = 5;

// This is the color of selection in a textfield.  It was obtained by checking
// the color of selection in TextViews in the system.
const RGBA32 selectionColor = makeRGB(255, 146, 0);

static SkCanvas* getCanvasFromInfo(const PaintInfo& info)
{
    return info.context->platformContext()->mCanvas;
}

RenderTheme* theme()
{
    DEFINE_STATIC_LOCAL(RenderThemeAndroid, androidTheme, ());
    return &androidTheme;
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* rt = RenderThemeAndroid::create().releaseRef();
    return rt;
}

PassRefPtr<RenderTheme> RenderThemeAndroid::create()
{
    return adoptRef(new RenderThemeAndroid());
}

RenderThemeAndroid::RenderThemeAndroid()
{
}

RenderThemeAndroid::~RenderThemeAndroid()
{
}

void RenderThemeAndroid::close()
{
}

bool RenderThemeAndroid::stateChanged(RenderObject* obj, ControlState state) const
{
    if (CheckedState == state) {
        obj->repaint();
        return true;
    }
    return false;
}

Color RenderThemeAndroid::platformActiveSelectionBackgroundColor() const
{
    return Color(selectionColor);
}

Color RenderThemeAndroid::platformInactiveSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformInactiveSelectionForegroundColor() const
{
    return Color::black;
}

Color RenderThemeAndroid::platformTextSearchHighlightColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionBackgroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformActiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

Color RenderThemeAndroid::platformInactiveListBoxSelectionForegroundColor() const
{
    return Color(Color::transparent);
}

int RenderThemeAndroid::baselinePosition(const RenderObject* obj) const
{
    // From the description of this function in RenderTheme.h:
    // A method to obtain the baseline position for a "leaf" control.  This will only be used if a baseline
    // position cannot be determined by examining child content. Checkboxes and radio buttons are examples of
    // controls that need to do this.
    //
    // Our checkboxes and radio buttons need to be offset to line up properly.
    return RenderTheme::baselinePosition(obj) - 2;
}

void RenderThemeAndroid::addIntrinsicMargins(RenderStyle* style) const
{
    // Cut out the intrinsic margins completely if we end up using a small font size
    if (style->fontSize() < 11)
        return;
    
    // Intrinsic margin value.
    const int m = 2;
    
    // FIXME: Using width/height alone and not also dealing with min-width/max-width is flawed.
    if (style->width().isIntrinsicOrAuto()) {
        if (style->marginLeft().quirk())
            style->setMarginLeft(Length(m, Fixed));
        if (style->marginRight().quirk())
            style->setMarginRight(Length(m, Fixed));
    }

    if (style->height().isAuto()) {
        if (style->marginTop().quirk())
            style->setMarginTop(Length(m, Fixed));
        if (style->marginBottom().quirk())
            style->setMarginBottom(Length(m, Fixed));
    }
}

bool RenderThemeAndroid::supportsFocus(ControlPart appearance)
{
    switch (appearance) {
    case PushButtonPart:
    case ButtonPart:
    case TextFieldPart:
        return true;
    default:
        return false;
    }

    return false;
}

void RenderThemeAndroid::adjustButtonStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    // Padding code is taken from RenderThemeSafari.cpp
    // It makes sure we have enough space for the button text.
    const int padding = 8;
    style->setPaddingLeft(Length(padding, Fixed));
    style->setPaddingRight(Length(padding, Fixed));
    style->setMinHeight(Length(style->fontSize() + buttonPadding, Fixed));
}

bool RenderThemeAndroid::paintCheckbox(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, true);
    return false;
}

bool RenderThemeAndroid::paintButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    // If it is a disabled button, simply paint it to the master picture.
    Node* node = obj->node();
    Element* formControlElement = static_cast<Element*>(node);
    if (formControlElement && !formControlElement->isEnabledFormControl())
        RenderSkinButton::Draw(getCanvasFromInfo(info), rect, RenderSkinAndroid::kDisabled);
    else
        // Store all the important information in the platform context.
        info.context->platformContext()->storeButtonInfo(node, rect);

    // We always return false so we do not request to be redrawn.
    return false;
}

bool RenderThemeAndroid::paintRadio(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    RenderSkinRadio::Draw(getCanvasFromInfo(info), obj->node(), rect, false);
    return false;
}

void RenderThemeAndroid::setCheckboxSize(RenderStyle* style) const
{
    style->setWidth(Length(19, Fixed));
    style->setHeight(Length(19, Fixed));
}

void RenderThemeAndroid::setRadioSize(RenderStyle* style) const
{
    // This is the same as checkboxes.
    setCheckboxSize(style);
}

void RenderThemeAndroid::adjustTextFieldStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;    
}

void RenderThemeAndroid::adjustTextAreaStyle(CSSStyleSelector*, RenderStyle* style, WebCore::Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintTextArea(RenderObject* obj, const PaintInfo& info, const IntRect& rect)
{
    if (!obj->isListBox())
        return true;

    paintCombo(obj, info, rect);
    RenderStyle* style = obj->style();
    if (style)
        style->setColor(Color::transparent);
    Node* node = obj->node();
    if (!node || !node->hasTagName(HTMLNames::selectTag))
        return true;

    HTMLSelectElement* select = static_cast<HTMLSelectElement*>(node);
    // The first item may be visible.  Make sure it does not draw.
    // If it has a style, it overrides the RenderListBox's style, so we
    // need to make sure both are set to transparent.
    node = select->item(0);
    if (node) {
        RenderObject* renderer = node->renderer();
        if (renderer) {
            RenderStyle* renderStyle = renderer->style();
            if (renderStyle)
                renderStyle->setColor(Color::transparent);
        }
    }
    // Find the first selected option, and draw its text.
    // FIXME: In a later change, if there is more than one item selected,
    // draw a string that says "X items" like iPhone Safari does
    int index = select->selectedIndex();
    node = select->item(index);
    if (!node || !node->hasTagName(HTMLNames::optionTag))
        return true;

    HTMLOptionElement* option = static_cast<HTMLOptionElement*>(node);
    String label = option->textIndentedToRespectGroupLabel();
    SkRect r(rect);

    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setTextEncoding(SkPaint::kUTF16_TextEncoding);
    // Values for text size and positioning determined by trial and error
    paint.setTextSize(r.height() - SkIntToScalar(6));

    SkCanvas* canvas = getCanvasFromInfo(info);
    int saveCount = canvas->save();
    r.fRight -= SkIntToScalar(RenderSkinCombo::extraWidth());
    canvas->clipRect(r);
    canvas->drawText(label.characters(), label.length() << 1,
             r.fLeft + SkIntToScalar(5), r.fBottom - SkIntToScalar(5), paint);
    canvas->restoreToCount(saveCount);

    return true;    
}

void RenderThemeAndroid::adjustSearchFieldStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintSearchField(RenderObject*, const PaintInfo&, const IntRect&)
{
    return true;    
}

void RenderThemeAndroid::adjustListboxStyle(CSSStyleSelector*, RenderStyle* style, Element*) const
{
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
    style->setMaxHeight(Length(style->fontSize() + listboxPadding, Fixed));
    // Make webkit draw invisible, since it will simply draw the first element
    style->setColor(Color::transparent);
    addIntrinsicMargins(style);
}

static void adjustMenuListStyleCommon(RenderStyle* style, Element* e)
{
    // Added to make room for our arrow.
    style->setPaddingRight(Length(RenderSkinCombo::extraWidth(), Fixed));
}

void RenderThemeAndroid::adjustMenuListStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    adjustMenuListStyleCommon(style, e);
    addIntrinsicMargins(style);
}

bool RenderThemeAndroid::paintCombo(RenderObject* obj, const PaintInfo& info,  const IntRect& rect)
{
    if (obj->style() && !obj->style()->backgroundColor().alpha())
        return true;
    return RenderSkinCombo::Draw(getCanvasFromInfo(info), obj->node(), rect.x(), rect.y(), rect.width(), rect.height());
}

bool RenderThemeAndroid::paintMenuList(RenderObject* obj, const PaintInfo& info, const IntRect& rect) 
{ 
    return paintCombo(obj, info, rect);
}

void RenderThemeAndroid::adjustMenuListButtonStyle(CSSStyleSelector*, RenderStyle* style, Element* e) const
{
    // Copied from RenderThemeSafari.
    const float baseFontSize = 11.0f;
    const int baseBorderRadius = 5;
    float fontScale = style->fontSize() / baseFontSize;
    
    style->resetPadding();
    style->setBorderRadius(IntSize(int(baseBorderRadius + fontScale - 1), int(baseBorderRadius + fontScale - 1))); // FIXME: Round up?

    const int minHeight = 15;
    style->setMinHeight(Length(minHeight, Fixed));
    
    style->setLineHeight(RenderStyle::initialLineHeight());
    // Found these padding numbers by trial and error.
    const int padding = 4;
    style->setPaddingTop(Length(padding, Fixed));
    style->setPaddingLeft(Length(padding, Fixed));
    adjustMenuListStyleCommon(style, e);
}

bool RenderThemeAndroid::paintMenuListButton(RenderObject* obj, const PaintInfo& info, const IntRect& rect) 
{
    return paintCombo(obj, info, rect);
}

bool RenderThemeAndroid::supportsFocusRing(const RenderStyle* style) const
{
    return style->opacity() > 0
        && style->hasAppearance() 
        && style->appearance() != TextFieldPart 
        && style->appearance() != SearchFieldPart 
        && style->appearance() != TextAreaPart 
        && style->appearance() != CheckboxPart
        && style->appearance() != RadioPart
        && style->appearance() != PushButtonPart
        && style->appearance() != SquareButtonPart
        && style->appearance() != ButtonPart
        && style->appearance() != ButtonBevelPart
        && style->appearance() != MenulistPart
        && style->appearance() != MenulistButtonPart;
}

} // namespace WebCore
