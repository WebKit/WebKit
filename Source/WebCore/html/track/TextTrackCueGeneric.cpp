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

#if ENABLE(VIDEO)

#include "TextTrackCueGeneric.h"

#include "CSSPropertyNames.h"
#include "CSSStyleDeclaration.h"
#include "CSSValueKeywords.h"
#include "ColorSerialization.h"
#include "HTMLSpanElement.h"
#include "InbandTextTrackPrivateClient.h"
#include "Logging.h"
#include "RenderObject.h"
#include "ScriptExecutionContext.h"
#include "StyleProperties.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(TextTrackCueGeneric);

static constexpr int DEFAULTCAPTIONFONTSIZE = 22; // Keep in sync with `font-size` in `video::-webkit-media-text-track-display`.

class TextTrackCueGenericBoxElement final : public VTTCueBox {
    WTF_MAKE_ISO_ALLOCATED_INLINE(TextTrackCueGenericBoxElement);
public:
    static Ref<TextTrackCueGenericBoxElement> create(Document&, TextTrackCueGeneric&);
    
    void applyCSSProperties(const IntSize&) override;
    
private:
    TextTrackCueGenericBoxElement(Document&, VTTCue&);
};

Ref<TextTrackCueGenericBoxElement> TextTrackCueGenericBoxElement::create(Document& document, TextTrackCueGeneric& cue)
{
    auto box = adoptRef(*new TextTrackCueGenericBoxElement(document, cue));
    box->initialize();
    return box;
}

TextTrackCueGenericBoxElement::TextTrackCueGenericBoxElement(Document& document, VTTCue& cue)
    : VTTCueBox(document, cue)
{
}

void TextTrackCueGenericBoxElement::applyCSSProperties(const IntSize& videoSize)
{
    RefPtr<TextTrackCueGeneric> cue = static_cast<TextTrackCueGeneric*>(getCue());
    if (!cue)
        return;

    setInlineStyleProperty(CSSPropertyPosition, CSSValueAbsolute);
    setInlineStyleProperty(CSSPropertyUnicodeBidi, CSSValuePlaintext);

    Ref<HTMLSpanElement> cueElement = cue->element();

    double textPosition = cue->calculateComputedTextPosition();
    double linePosition = cue->calculateComputedLinePosition();

    CSSValueID alignment = cue->getCSSAlignment();
    float size = static_cast<float>(cue->getCSSSize());
    if (cue->useDefaultPosition()) {
        setInlineStyleProperty(CSSPropertyBottom, 0, CSSUnitType::CSS_PX);
        setInlineStyleProperty(CSSPropertyMarginBottom, 1.0, CSSUnitType::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyLeft, static_cast<float>(textPosition), CSSUnitType::CSS_PERCENTAGE);
        setInlineStyleProperty(CSSPropertyTop, static_cast<float>(linePosition), CSSUnitType::CSS_PERCENTAGE);

        double authorFontSize = videoSize.height() * cue->baseFontSizeRelativeToVideoHeight() / 100.0;
        if (!authorFontSize)
            authorFontSize = DEFAULTCAPTIONFONTSIZE;

        if (cue->fontSizeMultiplier())
            authorFontSize *= cue->fontSizeMultiplier() / 100;

        double multiplier = fontSizeFromCaptionUserPrefs() / authorFontSize;
        double newCueSize = std::min(size * multiplier, 100.0);
        if (cue->getWritingDirection() == VTTCue::Horizontal) {
            setInlineStyleProperty(CSSPropertyWidth, newCueSize, CSSUnitType::CSS_PERCENTAGE);
            if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
                setInlineStyleProperty(CSSPropertyLeft, static_cast<double>(textPosition - (newCueSize - cue->getCSSSize()) / 2), CSSUnitType::CSS_PERCENTAGE);
        } else {
            setInlineStyleProperty(CSSPropertyHeight, newCueSize,  CSSUnitType::CSS_PERCENTAGE);
            if ((alignment == CSSValueMiddle || alignment == CSSValueCenter) && multiplier != 1.0)
                setInlineStyleProperty(CSSPropertyTop, static_cast<double>(linePosition - (newCueSize - cue->getCSSSize()) / 2), CSSUnitType::CSS_PERCENTAGE);
        }
    }

    double maxSize = 100.0;
    
    if (alignment == CSSValueEnd || alignment == CSSValueRight)
        maxSize = textPosition;
    else if (alignment == CSSValueStart || alignment == CSSValueLeft)
        maxSize = 100.0 - textPosition;

    if (cue->getWritingDirection() == VTTCue::Horizontal) {
        setInlineStyleProperty(CSSPropertyMinWidth, "min-content"_s);
        setInlineStyleProperty(CSSPropertyMaxWidth, maxSize, CSSUnitType::CSS_PERCENTAGE);
    } else {
        setInlineStyleProperty(CSSPropertyMinHeight, "min-content"_s);
        setInlineStyleProperty(CSSPropertyMaxHeight, maxSize, CSSUnitType::CSS_PERCENTAGE);
    }

    if (cue->foregroundColor().isValid())
        cueElement->setInlineStyleProperty(CSSPropertyColor, serializationForHTML(cue->foregroundColor()));
    if (cue->highlightColor().isValid())
        cueElement->setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForHTML(cue->highlightColor()));

    if (cue->getWritingDirection() == VTTCue::Horizontal)
        setInlineStyleProperty(CSSPropertyHeight, CSSValueAuto);
    else
        setInlineStyleProperty(CSSPropertyWidth, CSSValueAuto);

    if (cue->baseFontSizeRelativeToVideoHeight())
        cue->setFontSize(cue->baseFontSizeRelativeToVideoHeight(), videoSize, false);

    if (cue->getAlignment() == VTTCue::Center)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueCenter);
    else if (cue->getAlignment() == VTTCue::End)
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueEnd);
    else
        setInlineStyleProperty(CSSPropertyTextAlign, CSSValueStart);

    if (cue->backgroundColor().isValid())
        setInlineStyleProperty(CSSPropertyBackgroundColor, serializationForHTML(cue->backgroundColor()));
    setInlineStyleProperty(CSSPropertyWritingMode, cue->getCSSWritingMode(), false);
    setInlineStyleProperty(CSSPropertyWhiteSpace, CSSValuePreWrap);

    // Make sure shadow or stroke is not clipped.
    setInlineStyleProperty(CSSPropertyOverflow, CSSValueVisible);
    cueElement->setInlineStyleProperty(CSSPropertyOverflow, CSSValueVisible);
}

Ref<TextTrackCueGeneric> TextTrackCueGeneric::create(ScriptExecutionContext& context, const MediaTime& start, const MediaTime& end, const String& content)
{
    auto cue = adoptRef(*new TextTrackCueGeneric(downcast<Document>(context), start, end, content));
    cue->suspendIfNeeded();
    return cue;
}

TextTrackCueGeneric::TextTrackCueGeneric(Document& document, const MediaTime& start, const MediaTime& end, const String& content)
    : VTTCue(document, start, end, String { content })
{
}

RefPtr<VTTCueBox> TextTrackCueGeneric::createDisplayTree()
{
    if (auto* document = this->document())
        return TextTrackCueGenericBoxElement::create(*document, *this);
    return nullptr;
}

ExceptionOr<void> TextTrackCueGeneric::setLine(const LineAndPositionSetting& line)
{
    auto result = VTTCue::setLine(line);
    if (!result.hasException())
        m_useDefaultPosition = false;
    return result;
}

ExceptionOr<void> TextTrackCueGeneric::setPosition(const LineAndPositionSetting& position)
{
    auto result = VTTCue::setPosition(position);
    if (!result.hasException())
        m_useDefaultPosition = false;
    return result;
}

void TextTrackCueGeneric::setFontSize(int fontSize, const IntSize& videoSize, bool important)
{
    if (!fontSize)
        return;
    
    if (important || !hasDisplayTree() || !baseFontSizeRelativeToVideoHeight()) {
        VTTCue::setFontSize(fontSize, videoSize, important);
        return;
    }

    double size = videoSize.height() * baseFontSizeRelativeToVideoHeight() / 100;
    if (fontSizeMultiplier())
        size *= fontSizeMultiplier() / 100;
    if (auto* displayTree = displayTreeInternal())
        displayTree->setInlineStyleProperty(CSSPropertyFontSize, lround(size), CSSUnitType::CSS_PX);
}

bool TextTrackCueGeneric::cueContentsMatch(const TextTrackCue& otherTextTrackCue) const
{
    auto& other = downcast<TextTrackCueGeneric>(otherTextTrackCue);
    return VTTCue::cueContentsMatch(other)
        && m_baseFontSizeRelativeToVideoHeight == other.m_baseFontSizeRelativeToVideoHeight
        && m_fontSizeMultiplier == other.m_fontSizeMultiplier
        && m_fontName == other.m_fontName
        && m_foregroundColor == other.m_foregroundColor
        && m_backgroundColor == other.m_backgroundColor;
}

bool TextTrackCueGeneric::isOrderedBefore(const TextTrackCue* that) const
{
    if (VTTCue::isOrderedBefore(that))
        return true;

    if (is<TextTrackCueGeneric>(*that) && startTime() == that->startTime() && endTime() == that->endTime()) {
        // Further order generic cues by their calculated line value.
        auto thisPosition = getPositionCoordinates();
        auto thatPosition = downcast<TextTrackCueGeneric>(*that).getPositionCoordinates();
        return thisPosition.second > thatPosition.second || (thisPosition.second == thatPosition.second && thisPosition.first < thatPosition.first);
    }

    return false;
}

bool TextTrackCueGeneric::isPositionedAbove(const TextTrackCue* that) const
{
    if (is<TextTrackCueGeneric>(*that)) {
        if (startTime() == that->startTime() && endTime() == that->endTime()) {
            // Further order generic cues by their calculated line value.
            auto thisPosition = getPositionCoordinates();
            auto thatPosition = downcast<TextTrackCueGeneric>(*that).getPositionCoordinates();
            return thisPosition.second > thatPosition.second || (thisPosition.second == thatPosition.second && thisPosition.first < thatPosition.first);
        }
        return startTime() > that->startTime();
    }

    return VTTCue::isOrderedBefore(that);
}

void TextTrackCueGeneric::toJSON(JSON::Object& object) const
{
    VTTCue::toJSON(object);

    if (m_foregroundColor.isValid())
        object.setString("foregroundColor"_s, serializationForHTML(m_foregroundColor));
    if (m_backgroundColor.isValid())
        object.setString("backgroundColor"_s, serializationForHTML(m_backgroundColor));
    if (m_highlightColor.isValid())
        object.setString("highlightColor"_s, serializationForHTML(m_highlightColor));
    if (m_baseFontSizeRelativeToVideoHeight)
        object.setDouble("relativeFontSize"_s, m_baseFontSizeRelativeToVideoHeight);
    if (m_fontSizeMultiplier)
        object.setDouble("fontSizeMultiplier"_s, m_fontSizeMultiplier);
    if (!m_fontName.isEmpty())
        object.setString("font"_s, m_fontName);
}

} // namespace WebCore

#endif
