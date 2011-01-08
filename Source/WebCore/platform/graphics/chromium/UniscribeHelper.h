/*
 * Copyright (c) 2006, 2007, 2008, 2009, Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// A wrapper around Uniscribe that provides a reasonable API.

#ifndef UniscribeHelper_h
#define UniscribeHelper_h

#include <windows.h>
#include <usp10.h>
#include <map>

#include <unicode/uchar.h>
#include <wtf/Vector.h>

class UniscribeTest_TooBig_Test;  // A gunit test for UniscribeHelper.

namespace WebCore {

class GraphicsContext;

#define UNISCRIBE_HELPER_STACK_RUNS 8
#define UNISCRIBE_HELPER_STACK_CHARS 32

// This object should be safe to create & destroy frequently, as long as the
// caller preserves the script_cache when possible (this data may be slow to
// compute).
//
// This object is "kind of large" (~1K) because it reserves a lot of space for
// working with to avoid expensive heap operations. Therefore, not only should
// you not worry about creating and destroying it, you should try to not keep
// them around.
class UniscribeHelper {
public:
    // Initializes this Uniscribe run with the text pointed to by |run| with
    // |length|. The input is NOT null terminated.
    //
    // The is_rtl flag should be set if the input script is RTL. It is assumed
    // that the caller has already divided up the input text (using ICU, for
    // example) into runs of the same direction of script. This avoids
    // disagreements between the caller and Uniscribe later (see FillItems).
    //
    // A script cache should be provided by the caller that is initialized to
    // NULL. When the caller is done with the cache (it may be stored between
    // runs as long as it is used consistently with the same HFONT), it should
    // call ScriptFreeCache().
    UniscribeHelper(const UChar* input,
                    int inputLength,
                    bool isRtl,
                    HFONT,
                    SCRIPT_CACHE*,
                    SCRIPT_FONTPROPERTIES*);

    virtual ~UniscribeHelper();

    // Sets Uniscribe's directional override flag. False by default.
    bool directionalOverride() const
    {
        return m_directionalOverride;
    }
    void setDirectionalOverride(bool override)
    {
        m_directionalOverride = override;
    }

    // Set's Uniscribe's no-ligate override flag. False by default.
    bool inhibitLigate() const
    {
        return m_inhibitLigate;
    }
    void setInhibitLigate(bool inhibit)
    {
        m_inhibitLigate = inhibit;
    }

    // Set letter spacing. We will try to insert this much space between
    // graphemes (one or more glyphs perceived as a single unit by ordinary
    // users of a script). Positive values increase letter spacing, negative
    // values decrease it. 0 by default.
    int letterSpacing() const
    {
        return m_letterSpacing;
    }
    void setLetterSpacing(int letterSpacing)
    {
        m_letterSpacing = letterSpacing;
    }

    // Set the width of a standard space character. We use this to normalize
    // space widths. Windows will make spaces after Hindi characters larger than
    // other spaces. A space_width of 0 means to use the default space width.
    //
    // Must be set before Init() is called.
    int spaceWidth() const
    {
        return m_spaceWidth;
    }
    void setSpaceWidth(int spaceWidth)
    {
        m_spaceWidth = spaceWidth;
    }

    // Set word spacing. We will try to insert this much extra space between
    // each word in the input (beyond whatever whitespace character separates
    // words). Positive values lead to increased letter spacing, negative values
    // decrease it. 0 by default.
    //
    // Must be set before Init() is called.
    int wordSpacing() const
    {
        return m_wordSpacing;
    }
    void setWordSpacing(int wordSpacing)
    {
        m_wordSpacing = wordSpacing;
    }

    void setAscent(int ascent)
    {
        m_ascent = ascent;
    }

    // When set to true, this class is used only to look up glyph
    // indices for a range of Unicode characters without glyph placement.
    // By default, it's false. This should be set to true when this
    // class is used for glyph index look-up for non-BMP characters
    // in GlyphPageNodeChromiumWin.cpp.
    void setDisableFontFallback(bool disableFontFallback)
    {
        m_disableFontFallback = true;
    }

    // You must call this after setting any options but before doing any
    // other calls like asking for widths or drawing.
    void init()
    {
        initWithOptionalLengthProtection(true);
    }

    // Returns the total width in pixels of the text run.
    int width() const;

    // Call to justify the text, with the amount of space that should be ADDED
    // to get the desired width that the column should be justified to.
    // Normally, spaces are inserted, but for Arabic there will be kashidas
    // (extra strokes) inserted instead.
    //
    // This function MUST be called AFTER Init().
    void justify(int additionalSpace);

    // Computes the given character offset into a pixel offset of the beginning
    // of that character.
    int characterToX(int offset) const;

    // Converts the given pixel X position into a logical character offset into
    // the run. For positions appearing before the first character, this will
    // return -1.
    int xToCharacter(int x) const;

    // Draws the given characters to (x, y) in the given DC. The font will be
    // handled by this function, but the font color and other attributes should
    // be pre-set.
    //
    // The y position is the upper left corner, NOT the baseline.
    void draw(GraphicsContext* graphicsContext, HDC dc, int x, int y, int from,
              int to);

    // Returns the first glyph assigned to the character at the given offset.
    // This function is used to retrieve glyph information when Uniscribe is
    // being used to generate glyphs for non-complex, non-BMP (above U+FFFF)
    // characters. These characters are not otherwise special and have no
    // complex shaping rules, so we don't otherwise need Uniscribe, except
    // Uniscribe is the only way to get glyphs for non-BMP characters.
    //
    // Returns 0 if there is no glyph for the given character.
    WORD firstGlyphForCharacter(int charOffset) const;

protected:
    // Backend for init. The flag allows the unit test to specify whether we
    // should fail early for very long strings like normal, or try to pass the
    // long string to Uniscribe. The latter provides a way to force failure of
    // shaping.
    void initWithOptionalLengthProtection(bool lengthProtection);

    // Tries to preload the font when the it is not accessible.
    // This is the default implementation and it does not do anything.
    virtual void tryToPreloadFont(HFONT) {}

private:
    friend class UniscribeTest_TooBig_Test;

    // An array corresponding to each item in runs_ containing information
    // on each of the glyphs that were generated. Like runs_, this is in
    // reading order. However, for rtl text, the characters within each
    // item will be reversed.
    struct Shaping {
        Shaping()
            : m_prePadding(0)
            , m_hfont(NULL)
            , m_scriptCache(NULL)
            , m_ascentOffset(0) {
            m_abc.abcA = 0;
            m_abc.abcB = 0;
            m_abc.abcC = 0;
        }

        // Returns the number of glyphs (which will be drawn to the screen)
        // in this run.
        int glyphLength() const
        {
            return static_cast<int>(m_glyphs.size());
        }

        // Returns the number of characters (that we started with) in this run.
        int charLength() const
        {
            return static_cast<int>(m_logs.size());
        }

        // Returns the advance array that should be used when measuring glyphs.
        // The returned pointer will indicate an array with glyph_length()
        // elements and the advance that should be used for each one. This is
        // either the real advance, or the justified advances if there is one,
        // and is the array we want to use for measurement.
        const int* effectiveAdvances() const
        {
            if (m_advance.size() == 0)
                return 0;
            if (m_justify.size() == 0)
                return &m_advance[0];
            return &m_justify[0];
        }

        // This is the advance amount of space that we have added to the
        // beginning of the run. It is like the ABC's |A| advance but one that
        // we create and must handle internally whenever computing with pixel
        // offsets.
        int m_prePadding;

        // Glyph indices in the font used to display this item. These indices
        // are in screen order.
        Vector<WORD, UNISCRIBE_HELPER_STACK_CHARS> m_glyphs;

        // For each input character, this tells us the first glyph index it
        // generated. This is the only array with size of the input chars.
        //
        // All offsets are from the beginning of this run. Multiple characters
        // can generate one glyph, in which case there will be adjacent
        // duplicates in this list. One character can also generate multiple
        // glyphs, in which case there will be skipped indices in this list.
        Vector<WORD, UNISCRIBE_HELPER_STACK_CHARS> m_logs;

        // Flags and such for each glyph.
        Vector<SCRIPT_VISATTR, UNISCRIBE_HELPER_STACK_CHARS> m_visualAttributes;

        // Horizontal advances for each glyph listed above, this is basically
        // how wide each glyph is.
        Vector<int, UNISCRIBE_HELPER_STACK_CHARS> m_advance;

        // This contains glyph offsets, from the nominal position of a glyph.
        // It is used to adjust the positions of multiple combining characters
        // around/above/below base characters in a context-sensitive manner so
        // that they don't bump against each other and the base character.
        Vector<GOFFSET, UNISCRIBE_HELPER_STACK_CHARS> m_offsets;

        // Filled by a call to Justify, this is empty for nonjustified text.
        // If nonempty, this contains the array of justify characters for each
        // character as returned by ScriptJustify.
        //
        // This is the same as the advance array, but with extra space added
        // for some characters. The difference between a glyph's |justify|
        // width and it's |advance| width is the extra space added.
        Vector<int, UNISCRIBE_HELPER_STACK_CHARS> m_justify;

        // Sizing information for this run. This treats the entire run as a
        // character with a preceeding advance, width, and ending advance.  The
        // B width is the sum of the |advance| array, and the A and C widths
        // are any extra spacing applied to each end.
        //
        // It is unclear from the documentation what this actually means. From
        // experimentation, it seems that the sum of the character advances is
        // always the sum of the ABC values, and I'm not sure what you're
        // supposed to do with the ABC values.
        ABC m_abc;

        // Pointers to windows font data used to render this run.
        HFONT m_hfont;
        SCRIPT_CACHE* m_scriptCache;

        // Ascent offset between the ascent of the primary font
        // and that of the fallback font. The offset needs to be applied,
        // when drawing a string, to align multiple runs rendered with
        // different fonts.
        int m_ascentOffset;
    };

    // Computes the runs_ array from the text run.
    void fillRuns();

    // Computes the shapes_ array given an runs_ array already filled in.
    void fillShapes();

    // Fills in the screen_order_ array (see below).
    void fillScreenOrder();

    // Called to update the glyph positions based on the current spacing
    // options that are set.
    void applySpacing();

    // Normalizes all advances for spaces to the same width. This keeps windows
    // from making spaces after Hindi characters larger, which is then
    // inconsistent with our meaure of the width since WebKit doesn't include
    // spaces in text-runs sent to uniscribe unless white-space:pre.
    void adjustSpaceAdvances();

    // Returns the total width of a single item.
    int advanceForItem(int) const;

    // Shapes a run (pointed to by |input|) using |hfont| first.
    // Tries a series of fonts specified retrieved with NextWinFontData
    // and finally a font covering characters in |*input|. A string pointed
    // by |input| comes from ScriptItemize and is supposed to contain
    // characters belonging to a single script aside from characters common to
    // all scripts (e.g. space).
    bool shape(const UChar* input, int itemLength, int numGlyphs, SCRIPT_ITEM& run, Shaping&);

    // Gets Windows font data for the next best font to try in the list
    // of fonts. When there's no more font available, returns false
    // without touching any of out params. Need to call ResetFontIndex
    // to start scanning of the font list from the beginning.
    virtual bool nextWinFontData(HFONT*, SCRIPT_CACHE**, SCRIPT_FONTPROPERTIES**, int* ascent)
    {
        return false;
    }

    // Resets the font index to the first in the list of fonts to try after the
    // primaryFont turns out not to work. With fontIndex reset,
    // NextWinFontData scans fallback fonts from the beginning.
    virtual void resetFontIndex() {}

    // The input data for this run of Uniscribe. See the constructor.
    const UChar* m_input;
    const int m_inputLength;
    const bool m_isRtl;

    // Windows font data for the primary font. In a sense, m_logfont and m_style
    // are redundant because m_hfont contains all the information. However,
    // invoking GetObject, everytime we need the height and the style, is rather
    // expensive so that we cache them. Would it be better to add getter and
    // (virtual) setter for the height and the style of the primary font,
    // instead of m_logfont? Then, a derived class ctor can set m_ascent,
    // m_height and m_style if they're known. Getters for them would have to
    // 'infer' their values from m_hfont ONLY when they're not set.
    HFONT m_hfont;
    SCRIPT_CACHE* m_scriptCache;
    SCRIPT_FONTPROPERTIES* m_fontProperties;
    int m_ascent;
    LOGFONT m_logfont;
    int m_style;

    // Options, see the getters/setters above.
    bool m_directionalOverride;
    bool m_inhibitLigate;
    int m_letterSpacing;
    int m_spaceWidth;
    int m_wordSpacing;
    bool m_disableFontFallback;

    // Uniscribe breaks the text into Runs. These are one length of text that is
    // in one script and one direction. This array is in reading order.
    Vector<SCRIPT_ITEM, UNISCRIBE_HELPER_STACK_RUNS> m_runs;

    Vector<Shaping, UNISCRIBE_HELPER_STACK_RUNS> m_shapes;

    // This is a mapping between reading order and screen order for the items.
    // Uniscribe's items array are in reading order. For right-to-left text,
    // or mixed (although WebKit's |TextRun| should really be only one
    // direction), this makes it very difficult to compute character offsets
    // and positions. This list is in screen order from left to right, and
    // gives the index into the |m_runs| and |m_shapes| arrays of each
    // subsequent item.
    Vector<int, UNISCRIBE_HELPER_STACK_RUNS> m_screenOrder;
};

}  // namespace WebCore

#endif  // UniscribeHelper_h
