/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
              (C) 2004, 2005 Rob Buis <buis@kde.org>

    Based on khtml code by:
    Copyright (C) 2000-2003 Lars Knoll (knoll@kde.org)
              (C) 2000 Antti Koivisto (koivisto@kde.org)
              (C) 2000-2003 Dirk Mueller (mueller@kde.org)
              (C) 2002-2003 Apple Computer, Inc.

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_RenderStyle_H
#define KDOM_RenderStyle_H

#include <qapplication.h>

#include <kdom/css/RenderStyleDefs.h>
#include <kdom/css/CSSValueListImpl.h>

namespace KDOM
{
    class RenderStyle : public Shared
    {
    public:
        // static pseudo styles. Dynamic ones are produced on the fly.
        enum PseudoId
        {
            NOPSEUDO,
            FIRST_LINE,
            FIRST_LETTER,
            BEFORE,
            AFTER,
            SELECTION
        };

        enum Diff
        {
            Equal,
            NonVisible = Equal,
            Visible,
            Position,
            Layout,
            CbLayout
        };

    public:
        RenderStyle();
        RenderStyle(bool); // Used to create the default style.
        RenderStyle(const RenderStyle &other);
        virtual ~RenderStyle();

        static void cleanup();

        virtual void inheritFrom(const RenderStyle *inheritParent);

        virtual RenderStyle *getPseudoStyle(PseudoId pi);
        virtual RenderStyle *addPseudoStyle(PseudoId pi);
        virtual void removePseudoStyle(PseudoId pid);

        // Helpers
        PseudoId styleType() { return noninherited_flags.f._styleType; }

        bool affectedByHoverRules() const { return noninherited_flags.f._affectedByHover; }
        bool affectedByActiveRules() const { return noninherited_flags.f._affectedByActive; }

        void setAffectedByHoverRules(bool b) { noninherited_flags.f._affectedByHover = b; }
        void setAffectedByActiveRules(bool b) { noninherited_flags.f._affectedByActive = b; }

        virtual bool equals(RenderStyle *other) const;
        virtual bool inheritedNotEqual(RenderStyle *other) const;

        virtual RenderStyle::Diff diff(const RenderStyle *other) const;

        // CSS2 Properties
        RS_DEFINE_ATTRIBUTE(EClear, Clear, clear, CL_NONE)
        RS_DEFINE_ATTRIBUTE(EDisplay, Display, display, DS_INLINE)
        RS_DEFINE_ATTRIBUTE(EPosition, Position, position, PS_STATIC)
        RS_DEFINE_ATTRIBUTE(EFloat, Floating, floating, FL_NONE)
        RS_DEFINE_ATTRIBUTE(EOverflow, Overflow, overflow, OF_VISIBLE)
        RS_DEFINE_ATTRIBUTE(ETableLayout, TableLayout, tableLayout, TL_AUTO)
        RS_DEFINE_ATTRIBUTE(EVerticalAlign, VerticalAlign, verticalAlign, VA_BASELINE)
        RS_DEFINE_ATTRIBUTE(EUnicodeBidi, UnicodeBidi, unicodeBidi, UB_NORMAL)
        RS_DEFINE_ATTRIBUTE(EBackgroundRepeat, BackgroundRepeat, backgroundRepeat, BR_REPEAT)
        RS_DEFINE_ATTRIBUTE(bool, BackgroundAttachment, backgroundAttachment, true)
        RS_DEFINE_ATTRIBUTE(EPageBreak, PageBreakBefore, pageBreakBefore, PB_AUTO)
        RS_DEFINE_ATTRIBUTE(EPageBreak, PageBreakAfter, pageBreakAfter, PB_AUTO)

        RS_DEFINE_ATTRIBUTE_INHERITED(EDirection, Direction, direction, DI_LTR)
        RS_DEFINE_ATTRIBUTE_INHERITED(EVisibility, Visibility, visibility, VS_VISIBLE)
        RS_DEFINE_ATTRIBUTE_INHERITED(EWhiteSpace, WhiteSpace, whiteSpace, WS_NORMAL)
        RS_DEFINE_ATTRIBUTE_INHERITED(ETextAlign, TextAlign, textAlign, TA_AUTO)
        RS_DEFINE_ATTRIBUTE_INHERITED(ETextTransform, TextTransform, textTransform, TT_NONE)
        RS_DEFINE_ATTRIBUTE_INHERITED(ETextDecoration, TextDecoration, textDecoration, TD_NONE)
        RS_DEFINE_ATTRIBUTE_INHERITED(bool, BorderCollapse, borderCollapse, false)
        RS_DEFINE_ATTRIBUTE_INHERITED(EEmptyCell, EmptyCells, emptyCells, EC_SHOW)
        RS_DEFINE_ATTRIBUTE_INHERITED(ECaptionSide, CaptionSide, captionSide, CS_TOP)
        RS_DEFINE_ATTRIBUTE_INHERITED(EListStyleType, ListStyleType, listStyleType, LT_DISC)
        RS_DEFINE_ATTRIBUTE_INHERITED(EListStylePosition, ListStylePosition, listStylePosition, LP_OUTSIDE)
        RS_DEFINE_ATTRIBUTE_INHERITED(ECursor, Cursor, cursor, CS_AUTO)
        RS_DEFINE_ATTRIBUTE_INHERITED(EUserInput, UserInput, userInput, UI_NONE)

        RS_DEFINE_ATTRIBUTE(bool, HasClip, hasClip, false)

        // CSS2/3 Properties (using DataRef's)

        RS_DEFINE_ATTRIBUTE_DATAREF(Length, surround, offset.left, Left, left)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, surround, offset.right, Right, right)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, surround, offset.top, Top, top)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, surround, offset.bottom, Bottom, bottom)
        RS_DEFINE_ATTRIBUTE_DATAREF(const BorderValue &, surround, border.left, BorderLeft, borderLeft)
        RS_DEFINE_ATTRIBUTE_DATAREF(const BorderValue &, surround, border.right, BorderRight, borderRight)
        RS_DEFINE_ATTRIBUTE_DATAREF(const BorderValue &, surround, border.top, BorderTop, borderTop)
        RS_DEFINE_ATTRIBUTE_DATAREF(const BorderValue &, surround, border.bottom, BorderBottom, borderBottom)
        RS_DEFINE_ATTRIBUTE_DATAREF(EBorderStyle, surround, border.left.style, BorderLeftStyle, borderLeftStyle)
        RS_DEFINE_ATTRIBUTE_DATAREF(EBorderStyle, surround, border.right.style, BorderRightStyle, borderRightStyle)
        RS_DEFINE_ATTRIBUTE_DATAREF(EBorderStyle, surround, border.top.style, BorderTopStyle, borderTopStyle)
        RS_DEFINE_ATTRIBUTE_DATAREF(EBorderStyle, surround, border.bottom.style, BorderBottomStyle, borderBottomStyle)
        RS_DEFINE_ATTRIBUTE_DATAREF(const QColor &, surround, border.left.color, BorderLeftColor, borderLeftColor)
        RS_DEFINE_ATTRIBUTE_DATAREF(const QColor &, surround, border.right.color, BorderRightColor, borderRightColor)
        RS_DEFINE_ATTRIBUTE_DATAREF(const QColor &, surround, border.top.color, BorderTopColor, borderTopColor)
        RS_DEFINE_ATTRIBUTE_DATAREF(const QColor &, surround, border.bottom.color, BorderBottomColor, borderBottomColor)
        RS_DEFINE_ATTRIBUTE_DATAREF(unsigned short, surround, border.left.width, BorderLeftWidth, borderLeftWidth)
        RS_DEFINE_ATTRIBUTE_DATAREF(unsigned short, surround, border.right.width, BorderRightWidth, borderRightWidth)
        RS_DEFINE_ATTRIBUTE_DATAREF(unsigned short, surround, border.top.width, BorderTopWidth, borderTopWidth)
        RS_DEFINE_ATTRIBUTE_DATAREF(unsigned short, surround, border.bottom.width, BorderBottomWidth, borderBottomWidth)

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, margin.left, MarginLeft, marginLeft, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, margin.right, MarginRight, marginRight, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, margin.top, MarginTop, marginTop, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, margin.bottom, MarginBottom, marginBottom, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, padding.left, PaddingLeft, paddingLeft, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, padding.right, PaddingRight, paddingRight, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, padding.top, PaddingTop, paddingTop, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, surround, padding.bottom, PaddingBottom, paddingBottom, Length(LT_FIXED))

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, width, Width, width, Length())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, height, Height, height, Length())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, minWidth, MinWidth, minWidth, Length(0, LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, minHeight, MinHeight, minHeight, Length(0, LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, maxWidth, MaxWidth, maxWidth, Length(-1, LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, box, maxHeight, MaxHeight, maxHeight, Length(-1, LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, box, verticalAlign, VerticalAlignLength, verticalAlignLength)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(EBoxSizing, box, boxSizing, BoxSizing, boxSizing, BS_CONTENT_BOX)
        RS_DEFINE_ATTRIBUTE_DATAREF(signed int, box, zIndex, ZIndex, zIndex)
        RS_DEFINE_ATTRIBUTE_DATAREF(bool, box, zAuto, AutoZIndex, autoZIndex)

        RS_DEFINE_ATTRIBUTE_DATAREF(QColor, background, color, BackgroundColor, backgroundColor)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(CachedImage *, background, image, BackgroundImage, backgroundImage, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, background, xPosition, BackgroundXPosition, backgroundXPosition, Length())
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, background, yPosition, BackgroundYPosition, backgroundYPosition, Length())
        RS_DEFINE_ATTRIBUTE_DATAREF(EBorderStyle, background, outline.style, OutlineStyle, outlineStyle)
        RS_DEFINE_ATTRIBUTE_DATAREF(QColor, background, outline.color, OutlineColor, outlineColor)

        RS_DEFINE_ATTRIBUTE_DATAREF(Length, visual, clip.left, ClipLeft, clipLeft)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, visual, clip.right, ClipRight, clipRight)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, visual, clip.top, ClipTop, clipTop)
        RS_DEFINE_ATTRIBUTE_DATAREF(Length, visual, clip.bottom, ClipBottom, clipBottom)

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, css3NonInheritedData.access()->marquee, increment, MarqueeIncrement, marqueeIncrement, Length(6, LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(int, css3NonInheritedData.access()->marquee, speed, MarqueeSpeed, marqueeSpeed, 85)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(EMarqueeDirection, css3NonInheritedData.access()->marquee, direction, MarqueeDirection, marqueeDirection, MD_AUTO)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(EMarqueeBehavior, css3NonInheritedData.access()->marquee, behavior, MarqueeBehavior, marqueeBehavior, MB_SCROLL)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(int, css3NonInheritedData.access()->marquee, loops, MarqueeLoopCount, marqueeLoopCount, -1)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(float, css3NonInheritedData, opacity, Opacity, opacity, 1.0)

        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(QColor, inherited, color, Color, color, QColor(Qt::black))
        RS_DEFINE_ATTRIBUTE_DATAREF(QColor, inherited, decorationColor, TextDecorationColor, textDecorationColor)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(int, inherited, font.m_wordSpacing, WordSpacing, wordSpacing, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(int, inherited, font.m_letterSpacing, LetterSpacing, letterSpacing, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, inherited, indent, TextIndent, textIndent, Length(LT_FIXED))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(Length, inherited, lineHeight, LineHeight, lineHeight, Length(-100, LT_PERCENT))
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(short, inherited, borderHSpacing, BorderHSpacing, borderHSpacing, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(short, inherited, borderVSpacing, BorderVSpacing, borderVSpacing, 0)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(short, inherited, widows, Widows, widows, 2)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(short, inherited, orphans, Orphans, orphans, 2)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(EPageBreak, inherited, pageBreakInside, PageBreakInside, pageBreakInside, PB_AUTO)
        RS_DEFINE_ATTRIBUTE_DATAREF_WITH_INITIAL(CachedImage *, inherited, styleImage, ListStyleImage, listStyleImage, 0)

        // Special cases
        void setHasAutoZIndex() { RS_SET_VARIABLE(box, zAuto, true ); }

        bool isFloating() const { return !(noninherited_flags.f._floating == FL_NONE); }

        bool hidesOverflow() const { return overflow() != OF_VISIBLE; }
        bool scrollsOverflow() const { return overflow() == OF_SCROLL || overflow() == OF_AUTO; }

        static Length initialSize() { return Length(); }
        static Length initialOffset() { return Length(); }
        static Length initialMargin() { return Length(LT_FIXED); }
        static Length initialPadding() { return Length(LT_VARIABLE); }
        static Length initialMinSize() { return Length(0, LT_FIXED); }
        static Length initialMaxSize() { return Length(UNDEFINED, LT_FIXED); }    

        static QuotesValueImpl *initialQuotes() { return 0; }

        static int initialOutlineOffset() { return 0; }
        static EBorderStyle initialBorderStyle() { return BS_NONE; }
        static unsigned short initialBorderWidth() { return 3; }

        void resetOutline() { RS_SET_VARIABLE(background, outline, OutlineValue()) }
        void resetMargin() { RS_SET_VARIABLE(surround, margin, LengthBox(LT_FIXED)) }
        void resetPadding() { RS_SET_VARIABLE(surround, padding, LengthBox(LT_VARIABLE)) }

        // Text helpers
        bool autoWrap() const
        {
            if(whiteSpace() == WS_NORMAL || whiteSpace() == WS_PRE_WRAP || whiteSpace() == WS_PRE_LINE)
                return true;

            // nowrap | pre
            return false;
        }

        bool preserveLF() const
        {
            if(whiteSpace() == WS_PRE || whiteSpace() == WS_PRE_WRAP || whiteSpace() == WS_PRE_LINE)
                return true;

            // normal | nowrap
            return false;
        }

        bool preserveWS() const
        {
            if(whiteSpace() == WS_PRE || whiteSpace() == WS_PRE_WRAP)
                return true;

            // normal | nowrap | pre-line
            return false;
        }

        ShadowData *textShadow() const { return css3InheritedData->textShadow; }
        void setTextShadow(ShadowData *val, bool add = false);

        // Palette handling
        QPalette palette() const { return visual->palette; }

        void setPaletteColor(QPalette::ColorGroup g, QColorGroup::ColorRole r, const QColor &c)
        {
            visual.access()->palette.setColor(g, r, c);
        }

        void resetPalette() // Called when the desktop color scheme changes.
        {
            const_cast<StyleVisualData *>(visual.get())->palette = QApplication::palette();
        }

        // Border helpers
        bool borderLeftIsTransparent() const { return surround->border.left.isTransparent(); }
        bool borderRightIsTransparent() const { return surround->border.right.isTransparent(); }
        bool borderTopIsTransparent() const { return surround->border.top.isTransparent(); }
        bool borderBottomIsTransparent() const { return surround->border.bottom.isTransparent(); }

        bool hasBorder() const { return surround->border.hasBorder(); }
        bool hasMargin() const { return surround->margin.nonZero(); }
        bool hasOffset() const { return surround->offset.nonZero(); }

        void resetBorderTop() { RS_SET_VARIABLE(surround, border.top, BorderValue()) }
        void resetBorderRight() { RS_SET_VARIABLE(surround, border.right, BorderValue()) }
        void resetBorderBottom() { RS_SET_VARIABLE(surround, border.bottom, BorderValue()) }
        void resetBorderLeft() { RS_SET_VARIABLE(surround, border.left, BorderValue()) }

        // Font helpers
        const QFont &font() const { return inherited->font.m_font; }
        const QFontMetrics &fontMetrics() const { return inherited->font.m_fontMetrics; }

        // use with care. call font->update() after modifications
        const Font &htmlFont() { return inherited->font; }
        
        bool setFontDef(const FontDef &v)
        {
            // bah, this doesn't compare pointers. broken! (Dirk)
            if(!(inherited->font.fontDef() == v))
            {
                inherited.access()->font = Font(v);
                return true;
            }
            
            return false;
        }

        // text decorations
        unsigned int textDecorationComputed() const { return visual->textDecoration; }
        void setTextDecorationComputed(unsigned int v) { RS_SET_VARIABLE(visual, textDecoration, v) }

        void addToTextDecoration(int v) { inherited_flags.f._textDecoration |= v; }

        // outline width
        unsigned short outlineWidth() const
        {
            if(background->outline.style == BS_NONE || background->outline.style == BS_HIDDEN)
                return 0;

            return background->outline.width;
        }

        void setOutlineWidth(unsigned short v) { RS_SET_VARIABLE(background, outline.width, v) }

        // outline offset
        int outlineOffset() const
        {
            if(background->outline.style == BS_NONE || background->outline.style == BS_HIDDEN)
                return 0;

            return background->outline._offset;
        }

        void setOutlineOffset(int v) { RS_SET_VARIABLE(background, outline._offset, v) }

        // clipping helper
        void setClip(Length top, Length right, Length bottom, Length left);

        QuotesValueImpl *quotes() const { return inherited->quotes; }
        void setQuotes(QuotesValueImpl *q);

        QString openQuote(int level) const;
        QString closeQuote(int level) const;

        // CSS3 content stuff
        ContentData *contentData() const { return content; }

        bool contentDataEquivalent(RenderStyle *otherStyle);

        void setContent(DOMStringImpl *s, bool add);
        void setContent(CachedObject *o, bool add);
        void setContent(CounterImpl *c, bool add);
        void setContent(EQuoteContent q, bool add);

        // CSS3 counter-reset/counter-increment stuff
        CSSValueListImpl *counterReset() const { return counter_reset; }
        CSSValueListImpl *counterIncrement() const { return counter_increment; }

        bool counterDataEquivalent(RenderStyle *otherStyle);

        void setCounterReset(CSSValueListImpl *v);
        void setCounterIncrement(CSSValueListImpl *v);

        void addCounterReset(CounterActImpl *c);
        void addCounterIncrement(CounterActImpl *c);

        bool hasCounterReset(DOMStringImpl *c) const;
        bool hasCounterIncrement(DOMStringImpl *c) const;

        short counterReset(DOMStringImpl *c) const;
        short counterIncrement(DOMStringImpl *c) const;

        // pseudo style (needs to be accessed by 'CSSStyleSelector')
        RenderStyle *pseudoStyle;

        // Dumping helper (used by regression testing tool)
        QString createDiff(const RenderStyle &parent) const;

    protected:
        // inherit
        struct InheritedFlags
        {
            // 64 bit inherited, don't add to the struct, or the operator will break.
            bool operator==(const InheritedFlags &other) const { return _iflags == other._iflags; }
            bool operator!=(const InheritedFlags &other) const { return _iflags != other._iflags; }

            union
            {
                struct
                {
                    EEmptyCell _emptyCells : 1 ;
                    ECaptionSide _captionSide : 2;
                    EListStyleType _listStyleType : 6 ;
                    EListStylePosition _listStylePosition :1;

                    EVisibility _visibility : 2;
                    ETextAlign _textAlign : 4;
                    ETextTransform _textTransform : 2;
                    unsigned _textDecoration : 4;
                    ECursor _cursor : 5;

                    EDirection _direction : 1;
                    bool _borderCollapse : 1 ;
                    EWhiteSpace _whiteSpace : 3;

                    EUserInput _userInput : 2;

                    unsigned int unused : 30;
                } f;
                Q_UINT64 _iflags;
            };
        } inherited_flags;

        // don't inherit
        struct NonInheritedFlags
        {
            // 64 bit non-inherited, don't add to the struct, or the operator will break.
            bool operator==(const NonInheritedFlags &other) const { return _niflags == other._niflags; }
            bool operator!=(const NonInheritedFlags &other) const { return _niflags != other._niflags; }

            union
            {
                struct
                {
                    EDisplay _display : 5;
                    EBackgroundRepeat _backgroundRepeat : 2;
                    bool _backgroundAttachment : 1;
                    EOverflow _overflow : 4 ;
                    EVerticalAlign _verticalAlign : 4;
                    EClear _clear : 2;
                    EPosition _position : 2;
                    EFloat _floating : 2;
                    ETableLayout _tableLayout : 1;

                    EPageBreak _pageBreakBefore : 2;
                    EPageBreak _pageBreakAfter : 2;

                    // Internal
                    PseudoId _styleType : 4;
                    bool _affectedByHover : 1;
                    bool _affectedByActive : 1;
                    bool _hasClip : 1;
                    EUnicodeBidi _unicodeBidi : 2;

                    unsigned int unused : 28;
                } f;
                Q_UINT64 _niflags;
            };
        } noninherited_flags;

        // non-inherited attributes
        DataRef<StyleBoxData> box;
        DataRef<StyleVisualData> visual;
        DataRef<StyleBackgroundData> background;
        DataRef<StyleSurroundData> surround;
        DataRef<StyleCSS3NonInheritedData> css3NonInheritedData;

        // inherited attributes
        DataRef<StyleInheritedData> inherited;
        DataRef<StyleCSS3InheritedData> css3InheritedData;

        // static default style
        static RenderStyle *s_defaultStyle;

        // added this here, so we can get rid of the vptr in this class.
        // makes up for the same size.
        ContentData *content;
        CSSValueListImpl *counter_reset;
        CSSValueListImpl *counter_increment;

    private:
        RenderStyle(const RenderStyle *) : Shared() { }

        void setBitDefaults()
        {
            inherited_flags.f._emptyCells = initialEmptyCells();
            inherited_flags.f._captionSide = initialCaptionSide();
            inherited_flags.f._listStyleType = initialListStyleType();
            inherited_flags.f._listStylePosition = initialListStylePosition();
            inherited_flags.f._visibility = initialVisibility();
            inherited_flags.f._textAlign = initialTextAlign();
            inherited_flags.f._textTransform = initialTextTransform();
            inherited_flags.f._textDecoration = initialTextDecoration();
            inherited_flags.f._cursor = initialCursor();
            inherited_flags.f._direction = initialDirection();
            inherited_flags.f._borderCollapse = initialBorderCollapse();
            inherited_flags.f._whiteSpace = initialWhiteSpace();
            inherited_flags.f._userInput = UI_NONE;
            inherited_flags.f.unused = 0;

            noninherited_flags._niflags = 0L; // for safety: without this, the equality method sometimes
                                              // makes use of uninitialised bits according to valgrind

            noninherited_flags.f._display = initialDisplay();
            noninherited_flags.f._backgroundRepeat = initialBackgroundRepeat();
            noninherited_flags.f._backgroundAttachment = initialBackgroundAttachment();
            noninherited_flags.f._overflow = initialOverflow();
            noninherited_flags.f._verticalAlign = initialVerticalAlign();
            noninherited_flags.f._clear = initialClear();
            noninherited_flags.f._position = initialPosition();
            noninherited_flags.f._floating = initialFloating();
            noninherited_flags.f._tableLayout = initialTableLayout();
            noninherited_flags.f._pageBreakBefore = initialPageBreakBefore();
            noninherited_flags.f._pageBreakAfter = initialPageBreakAfter();
            noninherited_flags.f._styleType = NOPSEUDO;
            noninherited_flags.f._affectedByHover = false;
            noninherited_flags.f._affectedByActive = false;
            noninherited_flags.f._hasClip = false;
            noninherited_flags.f._unicodeBidi = initialUnicodeBidi();
            noninherited_flags.f.unused = 0;
        }
    };
};

#endif

// vim:ts=4:noet
