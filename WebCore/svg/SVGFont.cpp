/**
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#if ENABLE(SVG_FONTS)
#include "Font.h"

#include "CSSFontSelector.h"
#include "GraphicsContext.h"
#include "RenderObject.h"
#include "SimpleFontData.h"
#include "SVGFontData.h"
#include "SVGGlyphElement.h"
#include "SVGFontElement.h"
#include "SVGFontFaceElement.h"
#include "SVGMissingGlyphElement.h"
#include "SVGPaintServer.h"
#include "SVGPaintServerSolid.h"
#include "XMLNames.h"

using namespace WTF::Unicode;

namespace WebCore {

static inline float convertEmUnitToPixel(float fontSize, float unitsPerEm, float value)
{
    if (unitsPerEm == 0.0f)
        return 0.0f;

    return value * fontSize / unitsPerEm;
}

static inline bool isVerticalWritingMode(const SVGRenderStyle* style)
{
    return style->writingMode() == WM_TBRL || style->writingMode() == WM_TB; 
}

// Helper functions to determine the arabic character forms (initial, medial, terminal, isolated)
enum ArabicCharShapingMode {
    SNone = 0,
    SRight = 1,
    SDual = 2
};

static const ArabicCharShapingMode s_arabicCharShapingMode[222] = {
    SRight, SRight, SRight, SRight, SDual , SRight, SDual , SRight, SDual , SDual , SDual , SDual , SDual , SRight,                 /* 0x0622 - 0x062F */
    SRight, SRight, SRight, SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SNone , SNone , SNone , SNone , SNone , /* 0x0630 - 0x063F */
    SNone , SDual , SDual , SDual , SDual , SDual , SDual , SRight, SDual , SDual , SNone , SNone , SNone , SNone , SNone , SNone , /* 0x0640 - 0x064F */
    SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , /* 0x0650 - 0x065F */
    SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , /* 0x0660 - 0x066F */
    SNone , SRight, SRight, SRight, SNone , SRight, SRight, SRight, SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , /* 0x0670 - 0x067F */
    SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, /* 0x0680 - 0x068F */
    SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SDual , SDual , SDual , SDual , SDual , SDual , /* 0x0690 - 0x069F */
    SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , /* 0x06A0 - 0x06AF */
    SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , SDual , /* 0x06B0 - 0x06BF */
    SRight, SDual , SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SRight, SDual , SRight, SDual , SRight, /* 0x06C0 - 0x06CF */
    SDual , SDual , SRight, SRight, SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , /* 0x06D0 - 0x06DF */
    SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , /* 0x06E0 - 0x06EF */
    SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SNone , SDual , SDual , SDual , SNone , SNone , SNone   /* 0x06F0 - 0x06FF */
};

static inline SVGGlyphIdentifier::ArabicForm processArabicFormDetection(const UChar& curChar, bool& lastCharShapesRight, SVGGlyphIdentifier::ArabicForm* prevForm)
{
    SVGGlyphIdentifier::ArabicForm curForm;

    ArabicCharShapingMode shapingMode = SNone;
    if (curChar >= 0x0622 && curChar <= 0x06FF)
        shapingMode = s_arabicCharShapingMode[curChar - 0x0622];

    // Use a simple state machine to identify the actual arabic form
    // It depends on the order of the arabic form enum:
    // enum ArabicForm { None = 0, Isolated, Terminal, Initial, Medial };

    if (lastCharShapesRight && shapingMode == SDual) {
        if (prevForm) {
            int correctedForm = (int) *prevForm + 1;
            ASSERT(correctedForm >= SVGGlyphIdentifier::None && correctedForm <= SVGGlyphIdentifier::Medial);
            *prevForm = static_cast<SVGGlyphIdentifier::ArabicForm>(correctedForm);
        }

        curForm = SVGGlyphIdentifier::Initial;
    } else
        curForm = shapingMode == SNone ? SVGGlyphIdentifier::None : SVGGlyphIdentifier::Isolated;

    lastCharShapesRight = shapingMode != SNone;
    return curForm;
}

static Vector<SVGGlyphIdentifier::ArabicForm> charactersWithArabicForm(const String& input, bool rtl)
{
    Vector<SVGGlyphIdentifier::ArabicForm> forms;
    unsigned int length = input.length();

    bool containsArabic = false;
    for (unsigned int i = 0; i < length; ++i) {
        if (isArabicChar(input[i])) {
            containsArabic = true;
            break;
        }
    }

    if (!containsArabic)
        return forms;

    bool lastCharShapesRight = false;

    // Start identifying arabic forms
    if (rtl)
        for (int i = length - 1; i >= 0; --i)
            forms.prepend(processArabicFormDetection(input[i], lastCharShapesRight, forms.isEmpty() ? 0 : &forms.first()));
    else
        for (unsigned int i = 0; i < length; ++i)
            forms.append(processArabicFormDetection(input[i], lastCharShapesRight, forms.isEmpty() ? 0 : &forms.last()));

    return forms;
}

static inline bool isCompatibleArabicForm(const SVGGlyphIdentifier& identifier, const Vector<SVGGlyphIdentifier::ArabicForm>& chars, unsigned int startPosition, unsigned int endPosition)
{
    if (chars.isEmpty())
        return true;

    Vector<SVGGlyphIdentifier::ArabicForm>::const_iterator it = chars.begin() + startPosition;
    Vector<SVGGlyphIdentifier::ArabicForm>::const_iterator end = chars.begin() + endPosition;

    ASSERT(end <= chars.end());
    for (; it != end; ++it) {
        if ((*it) != identifier.arabicForm && (*it) != SVGGlyphIdentifier::None)
            return false;
    }

    return true;
}

static inline bool isCompatibleGlyph(const SVGGlyphIdentifier& identifier, bool isVerticalText, const String& language,
                                     const Vector<SVGGlyphIdentifier::ArabicForm>& chars, unsigned int startPosition, unsigned int endPosition)
{
    bool valid = true;

    // Check wheter orientation if glyph fits within the request
    switch (identifier.orientation) {
    case SVGGlyphIdentifier::Vertical:
        valid = isVerticalText;
        break;
    case SVGGlyphIdentifier::Horizontal:
        valid = !isVerticalText;
        break;
    case SVGGlyphIdentifier::Both:
        break;
    }

    if (!valid)
        return false;

    // Check wheter languages are compatible
    if (!identifier.languages.isEmpty()) {
        // This glyph exists only in certain languages, if we're not specifying a
        // language on the referencing element we're unable to use this glyph.
        if (language.isEmpty())
            return false;

        // Split subcode from language, if existant.
        String languagePrefix;

        int subCodeSeparator = language.find('-');
        if (subCodeSeparator != -1)
            languagePrefix = language.left(subCodeSeparator);

        Vector<String>::const_iterator it = identifier.languages.begin();
        Vector<String>::const_iterator end = identifier.languages.end();

        bool found = false;
        for (; it != end; ++it) {
            String cur = (*it);

            if (cur == language || cur == languagePrefix) {
                found = true;
                break;
            }
        }

        if (!found)
            return false;
    }

    // Check wheter arabic form is compatible
    return isCompatibleArabicForm(identifier, chars, startPosition, endPosition);
}

static inline const SVGFontData* svgFontAndFontFaceElementForFontData(const SimpleFontData* fontData, SVGFontFaceElement*& fontFace, SVGFontElement*& font)
{
    ASSERT(fontData->isCustomFont());
    ASSERT(fontData->isSVGFont());

    const SVGFontData* svgFontData = static_cast<const SVGFontData*>(fontData->svgFontData());

    fontFace = svgFontData->svgFontFaceElement();
    ASSERT(fontFace);

    font = fontFace->associatedFontElement();
    return svgFontData;
}

// Helper class to walk a text run. Lookup a SVGGlyphIdentifier for each character
// - also respecting possibly defined ligatures - and invoke a callback for each found glyph.
template<typename SVGTextRunData>
struct SVGTextRunWalker {
    typedef bool (*SVGTextRunWalkerCallback)(const SVGGlyphIdentifier&, SVGTextRunData&);
    typedef void (*SVGTextRunWalkerMissingGlyphCallback)(const TextRun&, SVGTextRunData&);

    SVGTextRunWalker(const SVGFontData* fontData, SVGFontElement* fontElement, SVGTextRunData& data,
                     SVGTextRunWalkerCallback callback, SVGTextRunWalkerMissingGlyphCallback missingGlyphCallback)
        : m_fontData(fontData)
        , m_fontElement(fontElement)
        , m_walkerData(data)
        , m_walkerCallback(callback)
        , m_walkerMissingGlyphCallback(missingGlyphCallback)
    {
    }

    void walk(const TextRun& run, bool isVerticalText, const String& language, int from, int to)
    {
        // Should hold true for SVG text, otherwhise sth. is wrong
        ASSERT(to - from == run.length());

        int maximumHashKeyLength = m_fontElement->maximumHashKeyLength();
        ASSERT(maximumHashKeyLength >= 0);

        Vector<SVGGlyphIdentifier::ArabicForm> chars(charactersWithArabicForm(String(run.data(from), run.length()), run.rtl()));

        SVGGlyphIdentifier identifier;
        bool foundGlyph = false;
        int characterLookupRange;

        for (int i = from; i < to; ++i) {
            // If characterLookupRange is > 0, then the font defined ligatures (length of unicode property value > 1).
            // We have to check wheter the current character & the next character define a ligature. This needs to be
            // extended to the n-th next character (where n is 'characterLookupRange'), to check for any possible ligature.
            characterLookupRange = maximumHashKeyLength + i >= to ? to - i : maximumHashKeyLength;

            while (characterLookupRange > 0 && !foundGlyph) {
                String lookupString(run.data(run.rtl() ? run.length() - (i + characterLookupRange) : i), characterLookupRange);

                Vector<SVGGlyphIdentifier> glyphs = m_fontElement->glyphIdentifiersForString(lookupString);
                Vector<SVGGlyphIdentifier>::iterator it = glyphs.begin();
                Vector<SVGGlyphIdentifier>::iterator end = glyphs.end();

                for (; it != end; ++it) {
                    identifier = *it;

                    unsigned int startPosition = run.rtl() ? run.length() - (i + lookupString.length()) : i;
                    unsigned int endPosition = startPosition + lookupString.length();

                    if (identifier.isValid && isCompatibleGlyph(identifier, isVerticalText, language, chars, startPosition, endPosition)) {
                        ASSERT(characterLookupRange > 0);
                        i += characterLookupRange - 1;

                        foundGlyph = true;
                        SVGGlyphElement::inheritUnspecifiedAttributes(identifier, m_fontData);
                        break;
                    }
                }

                characterLookupRange--;
            }

            if (!foundGlyph) {
                if (SVGMissingGlyphElement* element = m_fontElement->firstMissingGlyphElement()) {
                    // <missing-glyph> element support
                    identifier = SVGGlyphElement::buildGenericGlyphIdentifier(element);
                    SVGGlyphElement::inheritUnspecifiedAttributes(identifier, m_fontData);
                } else {
                    // Fallback to system font fallback
                    TextRun subRun(run);
                    subRun.setText(subRun.data(i), 1);

                    (*m_walkerMissingGlyphCallback)(subRun, m_walkerData);
                    continue;
                }
            }

            if (!(*m_walkerCallback)(identifier, m_walkerData))
                break;

            foundGlyph = false;
        }
    }

private:
    const SVGFontData* m_fontData;
    SVGFontElement* m_fontElement;
    SVGTextRunData& m_walkerData;
    SVGTextRunWalkerCallback m_walkerCallback;
    SVGTextRunWalkerMissingGlyphCallback m_walkerMissingGlyphCallback;
};

// Callback & data structures to compute the width of text using SVG Fonts
struct SVGTextRunWalkerMeasuredLengthData {
    int at;
    int from;
    int to;

    float scale;
    float length;
    const Font* font;
};

bool floatWidthUsingSVGFontCallback(const SVGGlyphIdentifier& identifier, SVGTextRunWalkerMeasuredLengthData& data)
{
    if (data.at >= data.from && data.at < data.to)
        data.length += identifier.horizontalAdvanceX * data.scale;

    data.at++;
    return data.at < data.to;
}

void floatWidthMissingGlyphCallback(const TextRun& run, SVGTextRunWalkerMeasuredLengthData& data)
{
    // Handle system font fallback
    FontDescription fontDescription(data.font->fontDescription());
    fontDescription.setFamily(FontFamily());
    Font font(fontDescription, 0, 0); // spacing handled by SVG text code.
    font.update(data.font->fontSelector());

    data.length += font.floatWidth(run);
}

static float floatWidthOfSubStringUsingSVGFont(const Font* font, const TextRun& run, int from, int to)
{
    int newFrom = to > from ? from : to;
    int newTo = to > from ? to : from;

    from = newFrom;
    to = newTo;

    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    if (const SVGFontData* fontData = svgFontAndFontFaceElementForFontData(font->primaryFont(), fontFaceElement, fontElement)) {
        if (!fontElement)
            return 0.0f;

        SVGTextRunWalkerMeasuredLengthData data;

        data.font = font;
        data.at = from;
        data.from = from;
        data.to = to;
        data.scale = convertEmUnitToPixel(font->size(), fontFaceElement->unitsPerEm(), 1.0f);
        data.length = 0.0f;

        String language;
        bool isVerticalText = false; // Holds true for HTML text

        // TODO: language matching & svg glyphs should be possible for HTML text, too.
        if (RenderObject* renderObject = run.referencingRenderObject()) {
            isVerticalText = isVerticalWritingMode(renderObject->style()->svgStyle());

            if (SVGElement* element = static_cast<SVGElement*>(renderObject->element()))
                language = element->getAttribute(XMLNames::langAttr);
        }

        SVGTextRunWalker<SVGTextRunWalkerMeasuredLengthData> runWalker(fontData, fontElement, data, floatWidthUsingSVGFontCallback, floatWidthMissingGlyphCallback);
        runWalker.walk(run, isVerticalText, language, 0, run.length());
        return data.length;
    }

    return 0.0f;
}

float Font::floatWidthUsingSVGFont(const TextRun& run) const
{
    return floatWidthOfSubStringUsingSVGFont(this, run, 0, run.length());
}

// Callback & data structures to draw text using SVG Fonts
struct SVGTextRunWalkerDrawTextData {
    float scale;
    bool isVerticalText;

    float xStartOffset;
    FloatPoint currentPoint;
    FloatPoint glyphOrigin;

    GraphicsContext* context;
    RenderObject* renderObject;

    SVGPaintServer* activePaintServer;
};

bool drawTextUsingSVGFontCallback(const SVGGlyphIdentifier& identifier, SVGTextRunWalkerDrawTextData& data)
{
    // TODO: Support arbitary SVG content as glyph (currently limited to <glyph d="..."> situations)
    if (!identifier.pathData.isEmpty()) {
        data.context->save();

        if (data.isVerticalText) {
            data.glyphOrigin.setX(identifier.verticalOriginX * data.scale);
            data.glyphOrigin.setY(identifier.verticalOriginY * data.scale);
        }

        data.context->translate(data.xStartOffset + data.currentPoint.x() + data.glyphOrigin.x(), data.currentPoint.y() + data.glyphOrigin.y());
        data.context->scale(FloatSize(data.scale, -data.scale));

        data.context->beginPath();
        data.context->addPath(identifier.pathData);

        SVGPaintTargetType targetType = data.context->textDrawingMode() == cTextStroke ? ApplyToStrokeTargetType : ApplyToFillTargetType;
        if (data.activePaintServer->setup(data.context, data.renderObject, targetType)) {
            // Spec: Any properties specified on a text elements which represents a length, such as the
            // 'stroke-width' property, might produce surprising results since the length value will be
            // processed in the coordinate system of the glyph. (TODO: What other lengths? miter-limit? dash-offset?)
            if (targetType == ApplyToStrokeTargetType && data.scale != 0.0f)
                data.context->setStrokeThickness(data.context->strokeThickness() / data.scale);

            data.activePaintServer->renderPath(data.context, data.renderObject, targetType);
            data.activePaintServer->teardown(data.context, data.renderObject, targetType);
        }

        data.context->restore();
    }

    if (data.isVerticalText)
        data.currentPoint.move(0.0f, identifier.verticalAdvanceY * data.scale);
    else
        data.currentPoint.move(identifier.horizontalAdvanceX * data.scale, 0.0f);

    return true;
}

void drawTextMissingGlyphCallback(const TextRun& run, SVGTextRunWalkerDrawTextData& data)
{
    // Handle system font fallback
    FontDescription fontDescription(data.context->font().fontDescription());
    fontDescription.setFamily(FontFamily());
    Font font(fontDescription, 0, 0); // spacing handled by SVG text code.
    font.update(data.context->font().fontSelector());

    font.drawText(data.context, run, data.currentPoint);

    if (data.isVerticalText)
        data.currentPoint.move(0.0f, font.floatWidth(run));
    else
        data.currentPoint.move(font.floatWidth(run), 0.0f);
}

void Font::drawTextUsingSVGFont(GraphicsContext* context, const TextRun& run, 
                                const FloatPoint& point, int from, int to) const
{
    SVGFontElement* fontElement = 0;
    SVGFontFaceElement* fontFaceElement = 0;

    if (const SVGFontData* fontData = svgFontAndFontFaceElementForFontData(primaryFont(), fontFaceElement, fontElement)) {
        if (!fontElement)
            return;

        SVGTextRunWalkerDrawTextData data;
        data.currentPoint = point;
        data.scale = convertEmUnitToPixel(size(), fontFaceElement->unitsPerEm(), 1.0f);

        // Required to be valid for SVG text only.
        data.renderObject = run.referencingRenderObject();
        data.activePaintServer = run.activePaintServer();

        // If renderObject is not set, we're dealing for HTML text rendered using SVG Fonts.
        if (!data.renderObject) {
            ASSERT(!data.activePaintServer);

            // TODO: We're only supporting simple filled HTML text so far.
            SVGPaintServerSolid* solidPaintServer = SVGPaintServer::sharedSolidPaintServer();
            solidPaintServer->setColor(context->fillColor());

            data.activePaintServer = solidPaintServer;
        }

        ASSERT(data.activePaintServer);

        data.isVerticalText = false;
        data.xStartOffset = floatWidthOfSubStringUsingSVGFont(this, run, run.rtl() ? to : 0, run.rtl() ? run.length() : from);
        data.glyphOrigin = FloatPoint();
        data.context = context;

        String language;

        // TODO: language matching & svg glyphs should be possible for HTML text, too.
        if (data.renderObject) {    
            data.isVerticalText = isVerticalWritingMode(data.renderObject->style()->svgStyle());    

            if (SVGElement* element = static_cast<SVGElement*>(data.renderObject->element()))
                language = element->getAttribute(XMLNames::langAttr);
        }
        
        if (!data.isVerticalText) {
            data.glyphOrigin.setX(fontData->horizontalOriginX() * data.scale);
            data.glyphOrigin.setY(fontData->horizontalOriginY() * data.scale);
        }

        SVGTextRunWalker<SVGTextRunWalkerDrawTextData> runWalker(fontData, fontElement, data, drawTextUsingSVGFontCallback, drawTextMissingGlyphCallback);
        runWalker.walk(run, data.isVerticalText, language, from, to);
    }
}

FloatRect Font::selectionRectForTextUsingSVGFont(const TextRun& run, const IntPoint& point, int height, int from, int to) const
{
    return FloatRect(point.x() + floatWidthOfSubStringUsingSVGFont(this, run, run.rtl() ? to : 0, run.rtl() ? run.length() : from),
                     point.y(), floatWidthOfSubStringUsingSVGFont(this, run, from, to), height);
}

int Font::offsetForPositionForTextUsingSVGFont(const TextRun&, int position, bool includePartialGlyphs) const
{
    // TODO: Fix text selection when HTML text is drawn using a SVG Font
    // We need to integrate the SVG text selection code in the offsetForPosition() framework.
    // This will also fix a major issue, that SVG Text code can't select arabic strings properly.
    return 0;
}

}

#endif
