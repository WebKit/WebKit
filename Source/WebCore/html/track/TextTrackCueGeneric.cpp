/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(VIDEO_TRACK)

#include "TextTrackCueGeneric.h"

#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSValueKeywords.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "RenderObject.h"
#include "ScriptExecutionContext.h"
#include "StyleProperties.h"
#include "TextTrackCue.h"
#include <wtf/MathExtras.h>

namespace WebCore {

// This default value must be the same as the one specified in mediaControlsApple.css for -webkit-media-controls-closed-captions-container
const static int DEFAULTCAPTIONFONTSIZE = 10;

class TextTrackCueGenericBoxElement final : public VTTCueBox {
public:
    static PassRefPtr<TextTrackCueGenericBoxElement> create(Document& document, TextTrackCueGeneric& cue)
    {
        return adoptRef(new TextTrackCueGenericBoxElement(document, cue));
    }
    
    virtual void applyCSSProperties(const IntSize&) override;
    
private:
    TextTrackCueGenericBoxElement(Document&, VTTCue&);
};

TextTrackCueGenericBoxElement::TextTrackCueGenericBoxElement(Document& document, VTTCue& cue)
    : VTTCueBox(document, cue)
{
}

void TextTrackCueGenericBoxElement::applyCSSProperties(const IntSize& videoSize)
{
    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValueWebkitPlaintext);
    
    TextTrackCueGeneric* cue = static_cast<TextTrackCueGeneric*>(getCue());
    RefPtr<HTMLSpanElement> cueElement = cue->element();

    CSSValueID alignment = cue->getCSSAlignment();
    float size = static_cast<float>(cue->getCSSSize());
    if (cue->useDefaultPosition()) {
        setInlineStyleProperty(CSSPropertyBottom, 0, CSSPrimitiveValue::CSS_PX);
        setInlineStyleProperty(CSSPropertyMarginBottom, 1.0, CSSPrimitiveValue::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyLeft, static_cast<float>(cue->position()), CSSPrimitiveValue::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyTop, static_cast<float>(cue->line()), CSSPrimitiveValue::CSS_PERCENTAGE);

        double authorFontSize = videoSize.height() * cue->baseFontSizeRelativeToVideoHeight() / 100.0;
        if (!authorFontSize)
            authorFontSize = DEFAULTCAPTIONFONTSIZE;

        if (cue->fontSizeMultiplier())
            authorFontSize *= cue->fontSizeMultiplier() / 100;

        double multiplier = m_fontSizeFromCaptionUserPrefs / authorFontSize;
        double newCueSize = std::min(size * multiplier, 100.0);
        if (cue->getWritingDirection() == VTTCue::Horizontal) {
            setInlineStyleProperty(CSSPropertyWidth, newCueSize, CSSPrimitiveValue::CSS_PERCENTAGE);
            if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
                setInlineStyleProperty(CSSPropertyLeft, static_cast<double>(cue->position() - (newCueSize - m_cue.getCSSSize()) / 2), CSSPrimitiveValue::CSS_PERCENTAGE);
        } else {
            setInlineStyleProperty(CSSPropertyHeight, newCueSize,  CSSPrimitiveValue::CSS_PERCENTAGE);
            if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
                setInlineStyleProperty(CSSPropertyTop, static_cast<double>(cue->line() - (newCueSize - m_cue.getCSSSize()) / 2), CSSPrimitiveValue::CSS_PERCENTAGE);
        }
    }

    double textPosition = m_cue.position();
    double maxSize = 100.0;
    
    if (alignment == CSSValueEnd || alignment == CSSValueRight)
        maxSize = textPosition;
    else if (alignment == CSSValueStart || alignment == CSSValueLeft)
        maxSize = 100.0 - textPosition;

    if (cue->getWritingDirection() == VTTCue::Horizontal) {
        setInlineStyleProperty(CSSPropertyMinWidth, "-webkit-min-content");
        setInlineStyleProperty(CSSPropertyMaxWidth, maxSize, CSSPrimitiveValue::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyMinHeight, "-webkit-min-content");
        setInlineStyleProperty(CSSPropertyMaxHeight, maxSize, CSSPrimitiveValue::CSS_PERCENTAGE);
    }

    if (cue->foregroundColor().isValid())
        cueElement->setInlineStyleProperty(CSSPropertyColor, cue->foregroundColor().serialized());
    if (cue->highlightColor().isValid())
        cueElement->setInlineStyleProperty(CSSPropertyBackgroundColor, cue->highlightColor().serialized());

    if (cue->getWritingDirection() == VTTCue::Horizontal)
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    else
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);

    if (cue->baseFontSizeRelativeToVideoHeight())
        cue->setFontSize(cue->baseFontSizeRelativeToVideoHeight(), videoSize, false);

    if (cue->getAlignment() == VTTCue::Middle)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueCenter);
    else if (cue->getAlignment() == VTTCue::End)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueEnd);
    else
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueStart);

    if (cue->backgroundColor().isValid())
        setInlineStyleProperty(CSSPropertyBackgroundColor, cue->backgroundColor().serialized());
    setInlineStyleProperty(CSSPropertyWebkitWritingMode, cue->getCSSWritingMode(), false);
    setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePreWrap);
}

TextTrackCueGeneric::TextTrackCueGeneric(ScriptExecutionContext& context, double start, double end, const String& content)
    : VTTCue(context, start, end, content)
    , m_baseFontSizeRelativeToVideoHeight(0)
    , m_fontSizeMultiplier(0)
    , m_defaultPosition(true)
{
}

PassRefPtr<VTTCueBox> TextTrackCueGeneric::createDisplayTree()
{
    return TextTrackCueGenericBoxElement::create(ownerDocument(), *this);
}

void TextTrackCueGeneric::setLine(double line, ExceptionCode& ec)
{
    m_defaultPosition = false;
    VTTCue::setLine(line, ec);
}

void TextTrackCueGeneric::setPosition(double position, ExceptionCode& ec)
{
    m_defaultPosition = false;
    VTTCue::setPosition(position, ec);
}

void TextTrackCueGeneric::setFontSize(int fontSize, const IntSize& videoSize, bool important)
{
    if (!hasDisplayTree() || !fontSize)
        return;
    
    if (important || !baseFontSizeRelativeToVideoHeight()) {
        VTTCue::setFontSize(fontSize, videoSize, important);
        return;
    }

    double size = videoSize.height() * baseFontSizeRelativeToVideoHeight() / 100;
    if (fontSizeMultiplier())
        size *= fontSizeMultiplier() / 100;
    displayTreeInternal()->setInlineStyleProperty(CSSPropertyFontSize, lround(size), CSSPrimitiveValue::CSS_PX);

    LOG(Media, "TextTrackCueGeneric::setFontSize - setting cue font size to %li", lround(size));
}

bool TextTrackCueGeneric::cueContentsMatch(const TextTrackCue& cue) const
{
    // Do call the parent class cueContentsMatch here, because we want to confirm
    // the content of the two cues are identical (even though the types are not the same).
    if (!VTTCue::cueContentsMatch(cue))
        return false;
    
    const TextTrackCueGeneric* other = static_cast<const TextTrackCueGeneric*>(&cue);

    if (m_baseFontSizeRelativeToVideoHeight != other->baseFontSizeRelativeToVideoHeight())
        return false;
    if (m_fontSizeMultiplier != other->fontSizeMultiplier())
        return false;
    if (m_fontName != other->fontName())
        return false;
    if (m_foregroundColor != other->foregroundColor())
        return false;
    if (m_backgroundColor != other->backgroundColor())
        return false;

    return true;
}

bool TextTrackCueGeneric::isEqual(const TextTrackCue& cue, TextTrackCue::CueMatchRules match) const
{
    // Do not call the parent class isEqual here, because we are not cueType() == VTTCue,
    // and will fail that equality test.
    if (!TextTrackCue::isEqual(cue, match))
        return false;

    if (cue.cueType() != TextTrackCue::Generic)
        return false;

    return cueContentsMatch(cue);
}

    
bool TextTrackCueGeneric::doesExtendCue(const TextTrackCue& cue) const
{
    if (!cueContentsMatch(cue))
        return false;
    
    return VTTCue::doesExtendCue(cue);
}

bool TextTrackCueGeneric::isOrderedBefore(const TextTrackCue* that) const
{
    if (VTTCue::isOrderedBefore(that))
        return true;

    if (that->cueType() == Generic && startTime() == that->startTime() && endTime() == that->endTime()) {
        // Further order generic cues by their calculated line value.
        std::pair<double, double> thisPosition = getPositionCoordinates();
        std::pair<double, double> thatPosition = toVTTCue(that)->getPositionCoordinates();
        return thisPosition.second > thatPosition.second || (thisPosition.second == thatPosition.second && thisPosition.first < thatPosition.first);
    }

    return false;
}

bool TextTrackCueGeneric::isPositionedAbove(const TextTrackCue* that) const
{
    if (that->cueType() == Generic && startTime() == that->startTime() && endTime() == that->endTime()) {
        // Further order generic cues by their calculated line value.
        std::pair<double, double> thisPosition = getPositionCoordinates();
        std::pair<double, double> thatPosition = toVTTCue(that)->getPositionCoordinates();
        return thisPosition.second > thatPosition.second || (thisPosition.second == thatPosition.second && thisPosition.first < thatPosition.first);
    }
    
    if (that->cueType() == Generic)
        return startTime() > that->startTime();
    
    return VTTCue::isOrderedBefore(that);
}
    
} // namespace WebCore

#endif
