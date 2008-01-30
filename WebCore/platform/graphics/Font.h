/*
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006, 2007 Apple Computer, Inc.
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

#ifndef Font_h
#define Font_h

#include "FontDescription.h"
#include <wtf/HashMap.h>

#if PLATFORM(QT)
#include <QtGui/qfont.h>
#include <QtGui/qfontmetrics.h>
#endif

namespace WebCore {

class FloatPoint;
class FloatRect;
class FontData;
class FontFallbackList;
class FontPlatformData;
class FontSelector;
class GlyphBuffer;
class GlyphPageTreeNode;
class GraphicsContext;
class IntPoint;
class RenderObject;
class SimpleFontData;
class SVGPaintServer;

struct GlyphData;

class TextRun {
public:
    TextRun(const UChar* c, int len, bool allowTabs = false, int xpos = 0, int padding = 0, bool rtl = false, bool directionalOverride = false,
              bool applyRunRounding = true, bool applyWordRounding = true)
        : m_characters(c)
        , m_len(len)
        , m_allowTabs(allowTabs)
        , m_xpos(xpos)
        , m_padding(padding)
        , m_rtl(rtl)
        , m_directionalOverride(directionalOverride)
        , m_applyRunRounding(applyRunRounding)
        , m_applyWordRounding(applyWordRounding)
        , m_disableSpacing(false)
#if ENABLE(SVG_FONTS)
        , m_referencingRenderObject(0)
        , m_activePaintServer(0)
#endif
    {
    }

    TextRun(const String& s, bool allowTabs = false, int xpos = 0, int padding = 0, bool rtl = false, bool directionalOverride = false,
              bool applyRunRounding = true, bool applyWordRounding = true)
        : m_characters(s.characters())
        , m_len(s.length())
        , m_allowTabs(allowTabs)
        , m_xpos(xpos)
        , m_padding(padding)
        , m_rtl(rtl)
        , m_directionalOverride(directionalOverride)
        , m_applyRunRounding(applyRunRounding)
        , m_applyWordRounding(applyWordRounding)
        , m_disableSpacing(false)
#if ENABLE(SVG_FONTS)
        , m_referencingRenderObject(0)
        , m_activePaintServer(0)
#endif
    {
    }

    const UChar operator[](int i) const { return m_characters[i]; }
    const UChar* data(int i) const { return &m_characters[i]; }

    const UChar* characters() const { return m_characters; }
    int length() const { return m_len; }

    void setText(const UChar* c, int len) { m_characters = c; m_len = len; }

    bool allowTabs() const { return m_allowTabs; }
    int xPos() const { return m_xpos; }
    int padding() const { return m_padding; }
    bool rtl() const { return m_rtl; }
    bool ltr() const { return !m_rtl; }
    bool directionalOverride() const { return m_directionalOverride; }
    bool applyRunRounding() const { return m_applyRunRounding; }
    bool applyWordRounding() const { return m_applyWordRounding; }
    bool spacingDisabled() const { return m_disableSpacing; }

    void disableSpacing() { m_disableSpacing = true; }
    void disableRoundingHacks() { m_applyRunRounding = m_applyWordRounding = false; }
    void setRTL(bool b) { m_rtl = b; }
    void setDirectionalOverride(bool override) { m_directionalOverride = override; }

#if ENABLE(SVG_FONTS)
    RenderObject* referencingRenderObject() const { return m_referencingRenderObject; }
    void setReferencingRenderObject(RenderObject* object) { m_referencingRenderObject = object; }

    SVGPaintServer* activePaintServer() const { return m_activePaintServer; }
    void setActivePaintServer(SVGPaintServer* object) { m_activePaintServer = object; }
#endif

private:
    const UChar* m_characters;
    int m_len;

    bool m_allowTabs;
    int m_xpos;
    int m_padding;
    bool m_rtl;
    bool m_directionalOverride;
    bool m_applyRunRounding;
    bool m_applyWordRounding;
    bool m_disableSpacing;

#if ENABLE(SVG_FONTS)
    RenderObject* m_referencingRenderObject;
    SVGPaintServer* m_activePaintServer;
#endif
};

class Font {
public:
    Font();
    Font(const FontDescription&, short letterSpacing, short wordSpacing);
#if !PLATFORM(QT)
    Font(const FontPlatformData&, bool isPrinting); // This constructor is only used if the platform wants to start with a native font.
#endif
    ~Font();

    Font(const Font&);
    Font& operator=(const Font&);

    bool operator==(const Font& other) const;
    bool operator!=(const Font& other) const {
        return !(*this == other);
    }

    const FontDescription& fontDescription() const { return m_fontDescription; }

    int pixelSize() const { return fontDescription().computedPixelSize(); }
    float size() const { return fontDescription().computedSize(); }

    void update(PassRefPtr<FontSelector>) const;

    void drawText(GraphicsContext*, const TextRun&, const FloatPoint&, int from = 0, int to = -1) const;

    int width(const TextRun&) const;
    float floatWidth(const TextRun&) const;

    int offsetForPosition(const TextRun&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForText(const TextRun&, const IntPoint&, int h, int from = 0, int to = -1) const;

    bool isSmallCaps() const { return m_fontDescription.smallCaps(); }

    short wordSpacing() const { return m_wordSpacing; }
    short letterSpacing() const { return m_letterSpacing; }
#if !PLATFORM(QT)
    void setWordSpacing(short s) { m_wordSpacing = s; }
    void setLetterSpacing(short s) { m_letterSpacing = s; }
#else
    void setWordSpacing(short s);
    void setLetterSpacing(short s);
#endif
    bool isFixedPitch() const;
    bool isPrinterFont() const { return m_fontDescription.usePrinterFont(); }
    
    FontRenderingMode renderingMode() const { return m_fontDescription.renderingMode(); }

    FontFamily& firstFamily() { return m_fontDescription.firstFamily(); }
    const FontFamily& family() const { return m_fontDescription.family(); }

    bool italic() const { return m_fontDescription.italic(); }
    unsigned weight() const { return m_fontDescription.weight(); }
    bool bold() const { return m_fontDescription.bold(); }

#if !PLATFORM(QT)
    bool isPlatformFont() const { return m_isPlatformFont; }
#endif

#if PLATFORM(QT)
    inline const QFont &font() const { return m_font; }
    inline const QFont &scFont() const { return m_scFont; }
#endif

    // Metrics that we query the FontFallbackList for.
    int ascent() const;
    int descent() const;
    int height() const { return ascent() + descent(); }
    int lineSpacing() const;
    float xHeight() const;
    unsigned unitsPerEm() const;
    int spaceWidth() const;
    int tabWidth() const { return 8 * spaceWidth(); }

#if !PLATFORM(QT)
    const SimpleFontData* primaryFont() const {
        if (!m_cachedPrimaryFont)
            cachePrimaryFont();
        return m_cachedPrimaryFont;
    }

    const FontData* fontDataAt(unsigned) const;
    const GlyphData& glyphDataForCharacter(UChar32, bool mirror, bool forceSmallCaps = false) const;
    // Used for complex text, and does not utilize the glyph map cache.
    const FontData* fontDataForCharacters(const UChar*, int length) const;

private:
    bool canUseGlyphCache(const TextRun&) const;
    void drawSimpleText(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
#if ENABLE(SVG_FONTS)
    void drawTextUsingSVGFont(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
    float floatWidthUsingSVGFont(const TextRun&) const;
    FloatRect selectionRectForTextUsingSVGFont(const TextRun&, const IntPoint&, int h, int from, int to) const;
    int offsetForPositionForTextUsingSVGFont(const TextRun&, int position, bool includePartialGlyphs) const;
#endif
    void drawGlyphs(GraphicsContext*, const SimpleFontData*, const GlyphBuffer&, int from, int to, const FloatPoint&) const;
    void drawGlyphBuffer(GraphicsContext*, const GlyphBuffer&, const TextRun&, const FloatPoint&) const;
    void drawComplexText(GraphicsContext*, const TextRun&, const FloatPoint&, int from, int to) const;
    float floatWidthForSimpleText(const TextRun&, GlyphBuffer*) const;
    float floatWidthForComplexText(const TextRun&) const;
    int offsetForPositionForSimpleText(const TextRun&, int position, bool includePartialGlyphs) const;
    int offsetForPositionForComplexText(const TextRun&, int position, bool includePartialGlyphs) const;
    FloatRect selectionRectForSimpleText(const TextRun&, const IntPoint&, int h, int from, int to) const;
    FloatRect selectionRectForComplexText(const TextRun&, const IntPoint&, int h, int from, int to) const;
    void cachePrimaryFont() const;
#endif
    friend struct WidthIterator;

    // Useful for debugging the different font rendering code paths.
public:
#if !PLATFORM(QT)
    enum CodePath { Auto, Simple, Complex };
    static void setCodePath(CodePath);
    static CodePath codePath;

    static const uint8_t gRoundingHackCharacterTable[256];
    static bool isRoundingHackCharacter(UChar32 c)
    {
        return (((c & ~0xFF) == 0 && gRoundingHackCharacterTable[c]));
    }

    FontSelector* fontSelector() const;
#endif
    static bool treatAsSpace(UChar c) { return c == ' ' || c == '\t' || c == '\n' || c == 0x00A0; }
    static bool treatAsZeroWidthSpace(UChar c) { return c < 0x20 || (c >= 0x7F && c < 0xA0) || c == 0x200e || c == 0x200f || c >= 0x202a && c <= 0x202e; }
private:
    FontDescription m_fontDescription;
#if !PLATFORM(QT)
    mutable RefPtr<FontFallbackList> m_fontList;
    mutable HashMap<int, GlyphPageTreeNode*> m_pages;
    mutable GlyphPageTreeNode* m_pageZero;
    mutable const SimpleFontData* m_cachedPrimaryFont;
#endif
    short m_letterSpacing;
    short m_wordSpacing;
#if !PLATFORM(QT)
    bool m_isPlatformFont;
#else
    QFont m_font;
    QFont m_scFont;
    int m_spaceWidth;
#endif
};

}

#endif
