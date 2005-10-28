/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#ifndef RENDERSTYLE_H
#define RENDERSTYLE_H

/*
 * WARNING:
 * --------
 *
 * The order of the values in the enums have to agree with the order specified
 * in cssvalues.in, otherwise some optimizations in the parser will fail,
 * and produce invaliud results.
 */

#include <qcolor.h>
#include <qfont.h>
#include <qfontmetrics.h>
#include <qlist.h>
#include <qpalette.h>
#include <qapplication.h>

#include "misc/khtmllayout.h"
#include "misc/shared.h"
#include "rendering/font.h"

#include <assert.h>

#define SET_VAR(group,variable,value) \
    if (!(group->variable == value)) \
        group.access()->variable = value;

class RenderArena;

namespace DOM {
    class DOMStringImpl;
    class ShadowValueImpl;
}

namespace khtml {

    class CachedImage;
    class CachedObject;

template <class DATA>
class DataRef
{
public:

    DataRef()
    {
	data=0;
    }
    DataRef( const DataRef<DATA> &d )
    {
    	data = d.data;
	data->ref();
    }

    ~DataRef()
    {
    	if(data) data->deref();
    }

    const DATA* operator->() const
    {
    	return data;
    }

    const DATA* get() const
    {
    	return data;
    }


    DATA* access()
    {
    	if (!data->hasOneRef())
	{
	    data->deref();
	    data = new DATA(*data);
	    data->ref();
	}
	return data;
    }

    void init()
    {
    	data = new DATA;
	data->ref();
    }

    DataRef<DATA>& operator=(const DataRef<DATA>& d)
    {
    	if (data==d.data)
	    return *this;
    	if (data)
    	    data->deref();
    	data = d.data;

	data->ref();

	return *this;
    }

    bool operator == ( const DataRef<DATA> &o ) const {
	return (*data == *(o.data) );
    }
    bool operator != ( const DataRef<DATA> &o ) const {
	return (*data != *(o.data) );
    }

private:
    DATA* data;
};

enum PseudoState { PseudoUnknown, PseudoNone, PseudoAnyLink, PseudoLink, PseudoVisited};

//------------------------------------------------

//------------------------------------------------
// Box model attributes. Not inherited.

struct LengthBox
{
    LengthBox()
    {
    }
    LengthBox( LengthType t )
	: left( t ), right ( t ), top( t ), bottom( t ) {}

    Length left;
    Length right;
    Length top;
    Length bottom;
    Length& operator=(Length& len)
    {
    	left=len;
	right=len;
	top=len;
	bottom=len;
	return len;
    }

    bool operator==(const LengthBox& o) const
    {
    	return left==o.left && right==o.right && top==o.top && bottom==o.bottom;
    }


    bool nonZero() const { return left.value!=0 || right.value!=0 || top.value!=0 || bottom.value!=0; }
};



enum EPosition {
    STATIC, RELATIVE, ABSOLUTE, FIXED
};

enum EFloat {
    FNONE = 0, FLEFT, FRIGHT
};


//------------------------------------------------
// Border attributes. Not inherited.

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order!
enum EBorderStyle {
    BNONE, BHIDDEN, INSET, GROOVE, RIDGE, OUTSET, DOTTED, DASHED, SOLID, DOUBLE
};

class BorderValue
{
public:
    BorderValue() {
	width = 3; // medium is default value
        style = BNONE;
    }

    QColor color;
    unsigned short width : 12;
    EBorderStyle style : 4;

    bool nonZero(bool checkStyle = true) const {
        return width != 0 && (!checkStyle || style != BNONE);
    }

    bool isTransparent() const {
        return color.isValid() && qAlpha(color.rgb()) == 0;
    }
    
    bool operator==(const BorderValue& o) const
    {
    	return width==o.width && style==o.style && color==o.color;
    }

    bool operator!=(const BorderValue& o) const
    {
        return !(*this == o);
    }
};

class OutlineValue : public BorderValue
{
public:
    OutlineValue()
    {
        _offset = 0;
        _auto = false;
    }
    
    bool operator==(const OutlineValue& o) const
    {
    	return width==o.width && style==o.style && color==o.color && _offset == o._offset && _auto == o._auto;
    }
    
    bool operator!=(const OutlineValue& o) const
    {
        return !(*this == o);
    }
    
    int _offset;
    bool _auto;
};

enum EBorderPrecedence { BOFF, BTABLE, BCOLGROUP, BCOL, BROWGROUP, BROW, BCELL };

struct CollapsedBorderValue
{
    CollapsedBorderValue() :border(0), precedence(BOFF) {}
    CollapsedBorderValue(const BorderValue* b, EBorderPrecedence p) :border(b), precedence(p) {}
    
    int width() const { return border && border->nonZero() ? border->width : 0; }
    EBorderStyle style() const { return border ? border->style : BHIDDEN; }
    bool exists() const { return border; }
    QColor color() const { return border ? border->color : QColor(); }
    bool isTransparent() const { return border ? border->isTransparent() : true; }
    
    bool operator==(const CollapsedBorderValue& o) const
    {
        if (!border) return !o.border;
        if (!o.border) return false;
        return *border == *o.border && precedence == o.precedence;
    }
    
    const BorderValue* border;
    EBorderPrecedence precedence;    
};

enum EBorderImageRule {
    BI_STRETCH, BI_ROUND, BI_REPEAT
};

class BorderImage
{
public:
    BorderImage() :m_image(0), m_horizontalRule(BI_STRETCH), m_verticalRule(BI_STRETCH) {}
    BorderImage(CachedImage* image, LengthBox slices, EBorderImageRule h, EBorderImageRule v) 
      :m_image(image), m_slices(slices), m_horizontalRule(h), m_verticalRule(v) {}

    bool operator==(const BorderImage& o) const
    {
        return m_image == o.m_image && m_slices == o.m_slices && m_horizontalRule == o.m_horizontalRule &&
               m_verticalRule == o.m_verticalRule;
    }

    bool hasImage() const { return m_image != 0; }
    CachedImage* image() const { return m_image; }

    CachedImage* m_image;
    LengthBox m_slices;
    EBorderImageRule m_horizontalRule : 2;
    EBorderImageRule m_verticalRule : 2;
};

class BorderData
{
public:
    BorderValue left;
    BorderValue right;
    BorderValue top;
    BorderValue bottom;
    
    BorderImage image;

    QSize topLeft;
    QSize topRight;
    QSize bottomLeft;
    QSize bottomRight;

    bool hasBorder() const
    {
        bool haveImage = image.hasImage();
    	return left.nonZero(!haveImage) || right.nonZero(!haveImage) || top.nonZero(!haveImage) || bottom.nonZero(!haveImage);
    }

    bool hasBorderRadius() const {
        if (topLeft.width() > 0)
            return true;
        if (topRight.width() > 0)
            return true;
        if (bottomLeft.width() > 0)
            return true;
        if (bottomRight.width() > 0)
            return true;
        return false;
    }
    
    unsigned short borderLeftWidth() const {
        if (!image.hasImage() && (left.style == BNONE || left.style == BHIDDEN))
            return 0; 
        return left.width;
    }
    
    unsigned short borderRightWidth() const {
        if (!image.hasImage() && (right.style == BNONE || right.style == BHIDDEN))
            return 0;
        return right.width;
    }
    
    unsigned short borderTopWidth() const {
        if (!image.hasImage() && (top.style == BNONE || top.style == BHIDDEN))
            return 0;
        return top.width;
    }
    
    unsigned short borderBottomWidth() const {
        if (!image.hasImage() && (bottom.style == BNONE || bottom.style == BHIDDEN))
            return 0;
        return bottom.width;
    }
    
    bool operator==(const BorderData& o) const
    {
    	return left == o.left && right == o.right && top == o.top && bottom == o.bottom && image == o.image &&
               topLeft == o.topLeft && topRight == o.topRight && bottomLeft == o.bottomLeft && bottomRight == o.bottomRight;
    }
    
    bool operator!=(const BorderData& o) const {
        return !(*this == o);
    }
};

enum EMarginCollapse { MCOLLAPSE, MSEPARATE, MDISCARD };

class StyleSurroundData : public Shared<StyleSurroundData>
{
public:
    StyleSurroundData();
    StyleSurroundData(const StyleSurroundData& o);
    
    bool operator==(const StyleSurroundData& o) const;
    bool operator!=(const StyleSurroundData& o) const {
        return !(*this == o);
    }

    LengthBox offset;
    LengthBox margin;
    LengthBox padding;
    BorderData border;
};


//------------------------------------------------
// Box attributes. Not inherited.

enum EBoxSizing { CONTENT_BOX, BORDER_BOX };

class StyleBoxData : public Shared<StyleBoxData>
{
public:
    StyleBoxData();
    StyleBoxData(const StyleBoxData& o);

    // copy and assignment
//    StyleBoxData(const StyleBoxData &other);
//    const StyleBoxData &operator = (const StyleBoxData &other);

    bool operator==(const StyleBoxData& o) const;
    bool operator!=(const StyleBoxData& o) const {
        return !(*this == o);
    }

    Length width;
    Length height;

    Length min_width;
    Length max_width;

    Length min_height;
    Length max_height;

    Length vertical_align;

    int z_index;
    bool z_auto : 1;
    EBoxSizing boxSizing : 1;
};

//------------------------------------------------
// Dashboard region attributes. Not inherited.

#if APPLE_CHANGES
struct StyleDashboardRegion
{
    QString label;
    LengthBox offset;
    int type;
    
    enum {
        None,
        Circle,
        Rectangle
    };

    bool operator==(const StyleDashboardRegion& o) const
    {
        return type == o.type && offset == o.offset && label == o.label;
    }
};
#endif

//------------------------------------------------
// Random visual rendering model attributes. Not inherited.

enum EOverflow {
    OVISIBLE, OHIDDEN, OSCROLL, OAUTO, OMARQUEE, OOVERLAY
};

enum EVerticalAlign {
    BASELINE, MIDDLE, SUB, SUPER, TEXT_TOP,
    TEXT_BOTTOM, TOP, BOTTOM, BASELINE_MIDDLE, LENGTH
};

enum EClear{
    CNONE = 0, CLEFT = 1, CRIGHT = 2, CBOTH = 3
};

enum ETableLayout {
    TAUTO, TFIXED
};

enum EUnicodeBidi {
    UBNormal, Embed, Override
};

class StyleVisualData : public Shared<StyleVisualData>
{
public:
    StyleVisualData();
    ~StyleVisualData();
    StyleVisualData(const StyleVisualData& o );

    bool operator==( const StyleVisualData &o ) const {
	return ( clip == o.clip &&
                 hasClip == o.hasClip &&
		 colspan == o.colspan &&
		 counter_increment == o.counter_increment &&
		 counter_reset == o.counter_reset &&
#if !APPLE_CHANGES
		 palette == o.palette &&
#endif
                 textDecoration == o.textDecoration);
    }
    bool operator!=( const StyleVisualData &o ) const {
        return !(*this == o);
    }

    LengthBox clip;
    bool hasClip : 1;
    int textDecoration : 4; // Text decorations defined *only* by this element.
    
    short colspan; // for html, not a css2 attribute

    short counter_increment; //ok, so these are not visual mode spesific
    short counter_reset;     //can't go to inherited, since these are not inherited

#if !APPLE_CHANGES
    QPalette palette;      //widget styling with IE attributes
#endif
};

//------------------------------------------------
enum EBackgroundBox {
    BGBORDER, BGPADDING, BGCONTENT
};

enum EBackgroundRepeat {
    REPEAT, REPEAT_X, REPEAT_Y, NO_REPEAT
};

struct BackgroundLayer {
public:
    BackgroundLayer();
    ~BackgroundLayer();

    CachedImage* backgroundImage() const { return m_image; }
    Length backgroundXPosition() const { return m_xPosition; }
    Length backgroundYPosition() const { return m_yPosition; }
    bool backgroundAttachment() const { return m_bgAttachment; }
    EBackgroundBox backgroundClip() const { return m_bgClip; }
    EBackgroundBox backgroundOrigin() const { return m_bgOrigin; }
    EBackgroundRepeat backgroundRepeat() const { return m_bgRepeat; }

    BackgroundLayer* next() const { return m_next; }
    BackgroundLayer* next() { return m_next; }

    bool isBackgroundImageSet() const { return m_imageSet; }
    bool isBackgroundXPositionSet() const { return m_xPosSet; }
    bool isBackgroundYPositionSet() const { return m_yPosSet; }
    bool isBackgroundAttachmentSet() const { return m_attachmentSet; }
    bool isBackgroundClipSet() const { return m_clipSet; }
    bool isBackgroundOriginSet() const { return m_originSet; }
    bool isBackgroundRepeatSet() const { return m_repeatSet; }
    
    void setBackgroundImage(CachedImage* i) { m_image = i; m_imageSet = true; }
    void setBackgroundXPosition(const Length& l) { m_xPosition = l; m_xPosSet = true; }
    void setBackgroundYPosition(const Length& l) { m_yPosition = l; m_yPosSet = true; }
    void setBackgroundAttachment(bool b) { m_bgAttachment = b; m_attachmentSet = true; }
    void setBackgroundClip(EBackgroundBox b) { m_bgClip = b; m_clipSet = true; }
    void setBackgroundOrigin(EBackgroundBox b) { m_bgOrigin = b; m_originSet = true; }
    void setBackgroundRepeat(EBackgroundRepeat r) { m_bgRepeat = r; m_repeatSet = true; }
    
    void clearBackgroundImage() { m_imageSet = false; }
    void clearBackgroundXPosition() { m_xPosSet = false; }
    void clearBackgroundYPosition() { m_yPosSet = false; }
    void clearBackgroundAttachment() { m_attachmentSet = false; }
    void clearBackgroundClip() { m_clipSet = false; }
    void clearBackgroundOrigin() { m_originSet = false; }
    void clearBackgroundRepeat() { m_repeatSet = false; }

    void setNext(BackgroundLayer* n) { if (m_next != n) { delete m_next; m_next = n; } }

    BackgroundLayer& operator=(const BackgroundLayer& o);    
    BackgroundLayer(const BackgroundLayer& o);

    bool operator==(const BackgroundLayer& o) const;
    bool operator!=(const BackgroundLayer& o) const {
        return !(*this == o);
    }

    bool containsImage(CachedImage* c) const { if (c == m_image) return true; if (m_next) return m_next->containsImage(c); return false; }
    
    bool hasImage() const {
        if (m_image)
            return true;
        return m_next ? m_next->hasImage() : false;
    }
    bool hasFixedImage() const {
        if (m_image && !m_bgAttachment)
            return true;
        return m_next ? m_next->hasFixedImage() : false;
    }

    void fillUnsetProperties();
    void cullEmptyLayers();

    CachedImage* m_image;

    Length m_xPosition;
    Length m_yPosition;

    bool m_bgAttachment : 1;
    EBackgroundBox m_bgClip : 2;
    EBackgroundBox m_bgOrigin : 2;
    EBackgroundRepeat m_bgRepeat : 2;

    bool m_imageSet : 1;
    bool m_attachmentSet : 1;
    bool m_clipSet : 1;
    bool m_originSet : 1;
    bool m_repeatSet : 1;
    bool m_xPosSet : 1;
    bool m_yPosSet : 1;

    BackgroundLayer* m_next;
};

class StyleBackgroundData : public Shared<StyleBackgroundData>
{
public:
    StyleBackgroundData();
    ~StyleBackgroundData() {}
    StyleBackgroundData(const StyleBackgroundData& o );

    bool operator==(const StyleBackgroundData& o) const;
    bool operator!=(const StyleBackgroundData &o) const {
	return !(*this == o);
    }

    BackgroundLayer m_background;
    QColor m_color;
    OutlineValue m_outline;
};

//------------------------------------------------
// CSS3 Marquee Properties

enum EMarqueeBehavior { MNONE, MSCROLL, MSLIDE, MALTERNATE, MUNFURL };
enum EMarqueeDirection { MAUTO = 0, MLEFT = 1, MRIGHT = -1, MUP = 2, MDOWN = -2, MFORWARD = 3, MBACKWARD = -3 };

class StyleMarqueeData : public Shared<StyleMarqueeData>
{
public:
    StyleMarqueeData();
    StyleMarqueeData(const StyleMarqueeData& o);
    
    bool operator==(const StyleMarqueeData& o) const;
    bool operator!=(const StyleMarqueeData& o) const {
        return !(*this == o);
    }
    
    Length increment;
    int speed;
    
    int loops; // -1 means infinite.
    
    EMarqueeBehavior behavior : 3;
    EMarqueeDirection direction : 3;
};
    
//------------------------------------------------
// CSS3 Flexible Box Properties

enum EBoxAlignment { BSTRETCH, BSTART, BCENTER, BEND, BJUSTIFY, BBASELINE };
enum EBoxOrient { HORIZONTAL, VERTICAL };
enum EBoxLines { SINGLE, MULTIPLE };
enum EBoxDirection { BNORMAL, BREVERSE };

class StyleFlexibleBoxData : public Shared<StyleFlexibleBoxData>
{
public:
    StyleFlexibleBoxData();
    StyleFlexibleBoxData(const StyleFlexibleBoxData& o);

    bool operator==(const StyleFlexibleBoxData& o) const;
    bool operator!=(const StyleFlexibleBoxData &o) const {
        return !(*this == o);
    }

    float flex;
    unsigned int flex_group;
    unsigned int ordinal_group;

    EBoxAlignment align : 3;
    EBoxAlignment pack: 3;
    EBoxOrient orient: 1;
    EBoxLines lines : 1;
};

// This struct holds information about shadows for the text-shadow and box-shadow properties.
struct ShadowData {
    ShadowData(int _x, int _y, int _blur, const QColor& _color)
    :x(_x), y(_y), blur(_blur), color(_color), next(0) {}
    ShadowData(const ShadowData& o);
    
    ~ShadowData() { delete next; }

    bool operator==(const ShadowData& o) const;
    bool operator!=(const ShadowData &o) const {
        return !(*this == o);
    }
    
    int x;
    int y;
    int blur;
    QColor color;
    ShadowData* next;
};

#ifndef KHTML_NO_XBL
struct BindingURI {
    BindingURI(DOM::DOMStringImpl*);
    ~BindingURI();

    BindingURI* copy();

    bool operator==(const BindingURI& o) const;
    bool operator!=(const BindingURI& o) const {
        return !(*this == o);
    }
    
    BindingURI* next() { return m_next; }
    void setNext(BindingURI* n) { m_next = n; }
    
    DOM::DOMStringImpl* uri() { return m_uri; }
    
    BindingURI* m_next;
    DOM::DOMStringImpl* m_uri;
};
#endif

//------------------------------------------------
// CSS3 User Modify Properties

enum EUserModify {
    READ_ONLY, READ_WRITE
};

// CSS3 User Drag Values

enum EUserDrag {
    DRAG_AUTO, DRAG_NONE, DRAG_ELEMENT
};

// CSS3 User Select Values

enum EUserSelect {
    SELECT_AUTO, SELECT_NONE, SELECT_TEXT, SELECT_IGNORE
};

// Word Break Values. Matches WinIE, rather than CSS3

enum EWordWrap {
    WBNORMAL, BREAK_WORD
};

enum ENBSPMode {
    NBNORMAL, SPACE
};

enum EKHTMLLineBreak {
    LBNORMAL, AFTER_WHITE_SPACE
};

enum EMatchNearestMailBlockquoteColor {
    BCNORMAL, MATCH
};

enum EAppearance {
    NoAppearance, CheckboxAppearance, RadioAppearance, PushButtonAppearance, SquareButtonAppearance, ButtonAppearance,
    ButtonBevelAppearance, ListboxAppearance, ListItemAppearance, MenulistAppearance,
    MenulistButtonAppearance, MenulistTextAppearance, MenulistTextFieldAppearance,
    ScrollbarButtonUpAppearance, ScrollbarButtonDownAppearance, 
    ScrollbarButtonLeftAppearance, ScrollbarButtonRightAppearance,
    ScrollbarTrackHorizontalAppearance, ScrollbarTrackVerticalAppearance,
    ScrollbarThumbHorizontalAppearance, ScrollbarThumbVerticalAppearance,
    ScrollbarGripperHorizontalAppearance, ScrollbarGripperVerticalAppearance,
    SliderHorizontalAppearance, SliderVerticalAppearance, SliderThumbHorizontalAppearance,
    SliderThumbVerticalAppearance, CaretAppearance, SearchFieldAppearance, SearchFieldResultsAppearance,
    SearchFieldCloseAppearance, TextFieldAppearance
};

// This struct is for rarely used non-inherited CSS3 properties.  By grouping them together,
// we save space, and only allocate this object when someone actually uses
// a non-inherited CSS3 property.
class StyleCSS3NonInheritedData : public Shared<StyleCSS3NonInheritedData>
{
public:
    StyleCSS3NonInheritedData();
    ~StyleCSS3NonInheritedData();
    StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData& o);

#ifndef KHTML_NO_XBL
    bool bindingsEquivalent(const StyleCSS3NonInheritedData& o) const;
#endif

    bool operator==(const StyleCSS3NonInheritedData& o) const;
    bool operator!=(const StyleCSS3NonInheritedData &o) const {
        return !(*this == o);
    }
    
#if APPLE_CHANGES
    int lineClamp;         // An Apple extension.  Not really CSS3 but not worth making a new struct over.
    QValueList<StyleDashboardRegion> m_dashboardRegions;
#endif
    float opacity;         // Whether or not we're transparent.
    DataRef<StyleFlexibleBoxData> flexibleBox; // Flexible box properties 
    DataRef<StyleMarqueeData> marquee; // Marquee properties
    EUserDrag userDrag : 2; // Whether or not a drag can be initiated by this element.
    EUserSelect userSelect : 2;  // Whether or not the element is selectable.
    bool textOverflow : 1; // Whether or not lines that spill out should be truncated with "..."
    EMarginCollapse marginTopCollapse : 2;
    EMarginCollapse marginBottomCollapse : 2;
    EMatchNearestMailBlockquoteColor matchNearestMailBlockquoteColor : 1;    

    EAppearance m_appearance : 5;

#ifndef KHTML_NO_XBL
    BindingURI* bindingURI; // The XBL binding URI list.
#endif
};

// This struct is for rarely used inherited CSS3 properties.  By grouping them together,
// we save space, and only allocate this object when someone actually uses
// an inherited CSS3 property.
class StyleCSS3InheritedData : public Shared<StyleCSS3InheritedData>
{
public:
    StyleCSS3InheritedData();
    ~StyleCSS3InheritedData();
    StyleCSS3InheritedData(const StyleCSS3InheritedData& o);

    bool operator==(const StyleCSS3InheritedData& o) const;
    bool operator!=(const StyleCSS3InheritedData &o) const {
        return !(*this == o);
    }
    bool shadowDataEquivalent(const StyleCSS3InheritedData& o) const;

    ShadowData* textShadow;  // Our text shadow information for shadowed text drawing.
    EUserModify userModify : 2; // Flag used for editing state
    EWordWrap wordWrap : 1;    // Flag used for word wrap
    ENBSPMode nbspMode : 1;    
    EKHTMLLineBreak khtmlLineBreak : 1;    
#if APPLE_CHANGES
    bool textSizeAdjust : 1;    // An Apple extension.  Not really CSS3 but not worth making a new struct over.
#endif
    
private:
    StyleCSS3InheritedData &operator=(const StyleCSS3InheritedData &);
};

//------------------------------------------------
// Inherited attributes.
//

enum EWhiteSpace {
    NORMAL, PRE, NOWRAP, KHTML_NOWRAP
};

enum ETextAlign {
    TAAUTO, LEFT, RIGHT, CENTER, JUSTIFY, KHTML_LEFT, KHTML_RIGHT, KHTML_CENTER
};

enum ETextTransform {
    CAPITALIZE, UPPERCASE, LOWERCASE, TTNONE
};

enum EDirection {
    LTR, RTL
};

enum ETextDecoration {
    TDNONE = 0x0 , UNDERLINE = 0x1, OVERLINE = 0x2, LINE_THROUGH= 0x4, BLINK = 0x8
};

enum EPageBreak {
    PBAUTO, PBALWAYS, PBAVOID
};

class StyleInheritedData : public Shared<StyleInheritedData>
{
public:
    StyleInheritedData();
    ~StyleInheritedData();
    StyleInheritedData(const StyleInheritedData& o );

    bool operator==(const StyleInheritedData& o) const;
    bool operator != ( const StyleInheritedData &o ) const {
	return !(*this == o);
    }

    Length indent;
    // could be packed in a short but doesn't
    // make a difference currently because of padding
    Length line_height;

    CachedImage *style_image;
    CachedImage *cursor_image;

    khtml::Font font;
    QColor color;
    
    short horizontal_border_spacing;
    short vertical_border_spacing;
    
    // Paged media properties.
    short widows;
    short orphans;
    EPageBreak page_break_inside : 2;
};


enum EEmptyCell {
    SHOW, HIDE
};

enum ECaptionSide
{
    CAPTOP, CAPBOTTOM, CAPLEFT, CAPRIGHT
};


enum EListStyleType {
     DISC, CIRCLE, SQUARE, LDECIMAL, DECIMAL_LEADING_ZERO,
     LOWER_ROMAN, UPPER_ROMAN, LOWER_GREEK,
     LOWER_ALPHA, LOWER_LATIN, UPPER_ALPHA, UPPER_LATIN,
     HEBREW, ARMENIAN, GEORGIAN, CJK_IDEOGRAPHIC,
     HIRAGANA, KATAKANA, HIRAGANA_IROHA, KATAKANA_IROHA, LNONE
};

enum EListStylePosition { OUTSIDE, INSIDE };

enum EVisibility { VISIBLE, HIDDEN, COLLAPSE };

enum ECursor {
    CURSOR_AUTO, CURSOR_CROSS, CURSOR_DEFAULT, CURSOR_POINTER, CURSOR_MOVE,
    CURSOR_E_RESIZE, CURSOR_NE_RESIZE, CURSOR_NW_RESIZE, CURSOR_N_RESIZE, CURSOR_SE_RESIZE, CURSOR_SW_RESIZE,
    CURSOR_S_RESIZE, CURSOR_W_RESIZE, CURSOR_TEXT, CURSOR_WAIT, CURSOR_HELP
};

enum ContentType {
    CONTENT_NONE, CONTENT_OBJECT, CONTENT_TEXT, CONTENT_COUNTER
};

struct ContentData {
    ContentData() :_contentType(CONTENT_NONE), _nextContent(0) {}
    ~ContentData();
    void clearContent();

    ContentType contentType() { return _contentType; }

    DOM::DOMStringImpl* contentText() { if (contentType() == CONTENT_TEXT) return _content.text; return 0; }
    CachedObject* contentObject() { if (contentType() == CONTENT_OBJECT) return _content.object; return 0; }
    
    ContentType _contentType;

    union {
        CachedObject* object;
        DOM::DOMStringImpl* text;
        // counters...
    } _content ;

    ContentData* _nextContent;
};

//------------------------------------------------

enum EDisplay {
    INLINE, BLOCK, LIST_ITEM, RUN_IN, COMPACT, INLINE_BLOCK,
    TABLE, INLINE_TABLE, TABLE_ROW_GROUP,
    TABLE_HEADER_GROUP, TABLE_FOOTER_GROUP, TABLE_ROW,
    TABLE_COLUMN_GROUP, TABLE_COLUMN, TABLE_CELL,
    TABLE_CAPTION, BOX, INLINE_BOX, NONE
};

class RenderStyle
{
    friend class CSSStyleSelector;
public:
    static void cleanup();

    // static pseudo styles. Dynamic ones are produced on the fly.
    enum PseudoId { NOPSEUDO, FIRST_LINE, FIRST_LETTER, BEFORE, AFTER, SELECTION, FIRST_LINE_INHERITED };

    void ref() { m_ref++;  }
    void deref(RenderArena* arena) { 
	if (m_ref) m_ref--; 
	if (!m_ref)
	    arenaDelete(arena);
    }
    bool hasOneRef() { return m_ref==1; }
    int refCount() const { return m_ref; }
    
    // Overloaded new operator.  Derived classes must override operator new
    // in order to allocate out of the RenderArena.
    void* operator new(size_t sz, RenderArena* renderArena) throw();    
    
    // Overridden to prevent the normal delete from being called.
    void operator delete(void* ptr, size_t sz);
    
private:
    void arenaDelete(RenderArena *arena);

protected:

// !START SYNC!: Keep this in sync with the copy constructor in render_style.cpp

    // inherit
    struct InheritedFlags {
    	bool operator==( const InheritedFlags &other ) const {
	    return (_empty_cells == other._empty_cells) &&
                   (_caption_side == other._caption_side) &&
                   (_list_style_type == other._list_style_type) &&
                   (_list_style_position == other._list_style_position) &&
                   (_visibility == other._visibility) &&
                   (_text_align == other._text_align) &&
                   (_text_transform == other._text_transform) &&
                   (_text_decorations == other._text_decorations) &&
                   (_text_transform == other._text_transform) &&
                   (_cursor_style == other._cursor_style) &&
                   (_direction == other._direction) &&
                   (_border_collapse == other._border_collapse) &&
                   (_white_space == other._white_space) &&
                   (_box_direction == other._box_direction) &&
                   (_visuallyOrdered == other._visuallyOrdered) &&
                   (_htmlHacks == other._htmlHacks) &&
                   (_force_backgrounds_to_white == other._force_backgrounds_to_white);
	}
        
	bool operator!=( const InheritedFlags &other ) const {
            return !(*this == other);
	}

	EEmptyCell _empty_cells : 1 ;
	ECaptionSide _caption_side : 2;
	EListStyleType _list_style_type : 5 ;
	EListStylePosition _list_style_position :1;
	EVisibility _visibility : 2;
	ETextAlign _text_align : 4;
	ETextTransform _text_transform : 2;
	int _text_decorations : 4;
	ECursor _cursor_style : 4;
	EDirection _direction : 1;
	bool _border_collapse : 1 ;
	EWhiteSpace _white_space : 2;
	EBoxDirection _box_direction : 1; // CSS3 box_direction property (flexible box layout module)
              // non CSS2 inherited
              bool _visuallyOrdered : 1;
              bool _htmlHacks :1;
              bool _force_backgrounds_to_white : 1;
    } inherited_flags;

// don't inherit
    struct NonInheritedFlags {
        bool operator==( const NonInheritedFlags &other ) const {
            return (_effectiveDisplay == other._effectiveDisplay) &&
            (_originalDisplay == other._originalDisplay) &&
            (_bg_repeat == other._bg_repeat) &&
            (_overflow == other._overflow) &&
            (_vertical_align == other._vertical_align) &&
            (_clear == other._clear) &&
            (_position == other._position) &&
            (_floating == other._floating) &&
            (_table_layout == other._table_layout) &&
            (_page_break_before == other._page_break_before) &&
            (_page_break_after == other._page_break_after) &&
            (_styleType == other._styleType) &&
            (_affectedByHover == other._affectedByHover) &&
            (_affectedByActive == other._affectedByActive) &&
            (_affectedByDrag == other._affectedByDrag) &&
            (_pseudoBits == other._pseudoBits) &&
            (_unicodeBidi == other._unicodeBidi);
	}

        bool operator!=( const NonInheritedFlags &other ) const {
            return !(*this == other);
        }
        
        EDisplay _effectiveDisplay : 5;
        EDisplay _originalDisplay : 5;
        EBackgroundRepeat _bg_repeat : 2;
        EOverflow _overflow : 4 ;
        EVerticalAlign _vertical_align : 4;
        EClear _clear : 2;
        EPosition _position : 2;
        EFloat _floating : 2;
        ETableLayout _table_layout : 1;
        
        EPageBreak _page_break_before : 2;
        EPageBreak _page_break_after : 2;

        PseudoId _styleType : 3;
        bool _affectedByHover : 1;
        bool _affectedByActive : 1;
        bool _affectedByDrag : 1;
        int _pseudoBits : 6;
        EUnicodeBidi _unicodeBidi : 2;
    } noninherited_flags;

// non-inherited attributes
    DataRef<StyleBoxData> box;
    DataRef<StyleVisualData> visual;
    DataRef<StyleBackgroundData> background;
    DataRef<StyleSurroundData> surround;
    DataRef<StyleCSS3NonInheritedData> css3NonInheritedData;

// inherited attributes
    DataRef<StyleCSS3InheritedData> css3InheritedData;
    DataRef<StyleInheritedData> inherited;
    
// list of associated pseudo styles
    RenderStyle* pseudoStyle;
    
    // added this here, so we can get rid of the vptr in this class.
    // makes up for the same size.
    ContentData *content;

    PseudoState m_pseudoState : 3;
    bool m_affectedByAttributeSelectors : 1;
    
    int m_ref;
    
// !END SYNC!

// static default style
    static RenderStyle* _default;

protected:
    void setBitDefaults()
    {
	inherited_flags._empty_cells = initialEmptyCells();
	inherited_flags._caption_side = initialCaptionSide();
	inherited_flags._list_style_type = initialListStyleType();
	inherited_flags._list_style_position = initialListStylePosition();
	inherited_flags._visibility = initialVisibility();
	inherited_flags._text_align = initialTextAlign();
	inherited_flags._text_transform = initialTextTransform();
	inherited_flags._text_decorations = initialTextDecoration();
	inherited_flags._cursor_style = initialCursor();
	inherited_flags._direction = initialDirection();
	inherited_flags._border_collapse = initialBorderCollapse();
	inherited_flags._white_space = initialWhiteSpace();
	inherited_flags._visuallyOrdered = false;
	inherited_flags._htmlHacks=false;
        inherited_flags._box_direction = initialBoxDirection();
        inherited_flags._force_backgrounds_to_white = false;
        
	noninherited_flags._effectiveDisplay = noninherited_flags._originalDisplay = initialDisplay();
	noninherited_flags._overflow = initialOverflow();
	noninherited_flags._vertical_align = initialVerticalAlign();
	noninherited_flags._clear = initialClear();
	noninherited_flags._position = initialPosition();
	noninherited_flags._floating = initialFloating();
	noninherited_flags._table_layout = initialTableLayout();
        noninherited_flags._page_break_before = initialPageBreak();
        noninherited_flags._page_break_after = initialPageBreak();
	noninherited_flags._styleType = NOPSEUDO;
        noninherited_flags._affectedByHover = false;
        noninherited_flags._affectedByActive = false;
        noninherited_flags._affectedByDrag = false;
        noninherited_flags._pseudoBits = 0;
	noninherited_flags._unicodeBidi = initialUnicodeBidi();
    }

public:

    RenderStyle();
    // used to create the default style.
    RenderStyle(bool);
    RenderStyle(const RenderStyle&);

    ~RenderStyle();

    void inheritFrom(const RenderStyle* inheritParent);

    PseudoId styleType() { return  noninherited_flags._styleType; }

    RenderStyle* getPseudoStyle(PseudoId pi);
    void addPseudoStyle(RenderStyle* pseudo);

    bool affectedByHoverRules() const { return  noninherited_flags._affectedByHover; }
    bool affectedByActiveRules() const { return  noninherited_flags._affectedByActive; }
    bool affectedByDragRules() const { return  noninherited_flags._affectedByDrag; }

    void setAffectedByHoverRules(bool b) {  noninherited_flags._affectedByHover = b; }
    void setAffectedByActiveRules(bool b) {  noninherited_flags._affectedByActive = b; }
    void setAffectedByDragRules(bool b) {  noninherited_flags._affectedByDrag = b; }
 
    bool operator==(const RenderStyle& other) const;
    bool        isFloating() const { return !(noninherited_flags._floating == FNONE); }
    bool        hasMargin() const { return surround->margin.nonZero(); }
    bool        hasBorder() const { return surround->border.hasBorder(); }
    bool        hasOffset() const { return surround->offset.nonZero(); }

    bool hasBackground() const { if (backgroundColor().isValid() && qAlpha(backgroundColor().rgb()) > 0)
                                    return true;
                                 return background->m_background.hasImage(); }
    bool hasFixedBackgroundImage() const { return background->m_background.hasFixedImage(); }
    bool hasAppearance() const { return appearance() != NoAppearance; }

    bool visuallyOrdered() const { return inherited_flags._visuallyOrdered; }
    void setVisuallyOrdered(bool b) {  inherited_flags._visuallyOrdered = b; }

    bool isStyleAvailable() const;
    
    bool hasPseudoStyle(PseudoId pseudo) const;
    void setHasPseudoStyle(PseudoId pseudo);
    
// attribute getter methods

    EDisplay 	display() const { return noninherited_flags._effectiveDisplay; }
    EDisplay    originalDisplay() const { return noninherited_flags._originalDisplay; }
    
    Length  	left() const {  return surround->offset.left; }
    Length  	right() const {  return surround->offset.right; }
    Length  	top() const {  return surround->offset.top; }
    Length  	bottom() const {  return surround->offset.bottom; }

    EPosition 	position() const { return  noninherited_flags._position; }
    EFloat  	floating() const { return  noninherited_flags._floating; }

    Length  	width() const { return box->width; }
    Length  	height() const { return box->height; }
    Length  	minWidth() const { return box->min_width; }
    Length  	maxWidth() const { return box->max_width; }
    Length  	minHeight() const { return box->min_height; }
    Length  	maxHeight() const { return box->max_height; }

    const BorderData& border() const { return surround->border; }
    const BorderValue& borderLeft() const { return surround->border.left; }
    const BorderValue& borderRight() const { return surround->border.right; }
    const BorderValue& borderTop() const { return surround->border.top; }
    const BorderValue& borderBottom() const { return surround->border.bottom; }

    const BorderImage& borderImage() const { return surround->border.image; }

    QSize borderTopLeftRadius() const { return surround->border.topLeft; }
    QSize borderTopRightRadius() const { return surround->border.topRight; }
    QSize borderBottomLeftRadius() const { return surround->border.bottomLeft; }
    QSize borderBottomRightRadius() const { return surround->border.bottomRight; }
    bool hasBorderRadius() const { return surround->border.hasBorderRadius(); }

    unsigned short  borderLeftWidth() const { return surround->border.borderLeftWidth(); }
    EBorderStyle    borderLeftStyle() const { return surround->border.left.style; }
    const QColor &  borderLeftColor() const { return surround->border.left.color; }
    bool borderLeftIsTransparent() const { return surround->border.left.isTransparent(); }
    unsigned short  borderRightWidth() const { return surround->border.borderRightWidth(); }
    EBorderStyle    borderRightStyle() const {  return surround->border.right.style; }
    const QColor &  	    borderRightColor() const {  return surround->border.right.color; }
    bool borderRightIsTransparent() const { return surround->border.right.isTransparent(); }
    unsigned short  borderTopWidth() const { return surround->border.borderTopWidth(); }
    EBorderStyle    borderTopStyle() const {return surround->border.top.style; }
    const QColor &  borderTopColor() const {  return surround->border.top.color; }
    bool borderTopIsTransparent() const { return surround->border.top.isTransparent(); }
    unsigned short  borderBottomWidth() const { return surround->border.borderBottomWidth(); }
    EBorderStyle    borderBottomStyle() const {  return surround->border.bottom.style; }
    const QColor &  	    borderBottomColor() const {  return surround->border.bottom.color; }
    bool borderBottomIsTransparent() const { return surround->border.bottom.isTransparent(); }
    
    unsigned short outlineSize() const { return outlineWidth() + outlineOffset(); }
    unsigned short outlineWidth() const { if (background->m_outline.style == BNONE || background->m_outline.style == BHIDDEN) return 0; return background->m_outline.width; }
    EBorderStyle    outlineStyle() const {  return background->m_outline.style; }
    bool outlineStyleIsAuto() const { return background->m_outline._auto; }
    const QColor &  	    outlineColor() const {  return background->m_outline.color; }

    EOverflow overflow() const { return  noninherited_flags._overflow; }
    EVisibility visibility() const { return inherited_flags._visibility; }
    EVerticalAlign verticalAlign() const { return  noninherited_flags._vertical_align; }
    Length verticalAlignLength() const { return box->vertical_align; }

    Length clipLeft() const { return visual->clip.left; }
    Length clipRight() const { return visual->clip.right; }
    Length clipTop() const { return visual->clip.top; }
    Length clipBottom() const { return visual->clip.bottom; }
    LengthBox clip() const { return visual->clip; }
    bool hasClip() const { return visual->hasClip; }
    
    EUnicodeBidi unicodeBidi() const { return noninherited_flags._unicodeBidi; }

    EClear clear() const { return  noninherited_flags._clear; }
    ETableLayout tableLayout() const { return  noninherited_flags._table_layout; }

    short colSpan() const { return visual->colspan; }

    const QFont & font() const { return inherited->font.f; }
    int fontSize() const { return inherited->font.f.pixelSize(); }
    // use with care. call font->update() after modifications
    const Font &htmlFont() { return inherited->font; }
    const QFontMetrics & fontMetrics() const { return inherited->font.fm; }

    const QColor & color() const { return inherited->color; }
    Length textIndent() const { return inherited->indent; }
    ETextAlign textAlign() const { return inherited_flags._text_align; }
    ETextTransform textTransform() const { return inherited_flags._text_transform; }
    int textDecorationsInEffect() const { return inherited_flags._text_decorations; }
    int textDecoration() const { return visual->textDecoration; }
    int wordSpacing() const { return inherited->font.wordSpacing; }
    int letterSpacing() const { return inherited->font.letterSpacing; }

    EDirection direction() const { return inherited_flags._direction; }
    Length lineHeight() const { return inherited->line_height; }

    EWhiteSpace whiteSpace() const { return inherited_flags._white_space; }

    const QColor & backgroundColor() const { return background->m_color; }
    CachedImage *backgroundImage() const { return background->m_background.m_image; }
    EBackgroundRepeat backgroundRepeat() const { return background->m_background.m_bgRepeat; }
    bool backgroundAttachment() const { return background->m_background.m_bgAttachment; }
    EBackgroundBox backgroundClip() const { return background->m_background.m_bgClip; }
    EBackgroundBox backgroundOrigin() const { return background->m_background.m_bgOrigin; }
    Length backgroundXPosition() const { return background->m_background.m_xPosition; }
    Length backgroundYPosition() const { return background->m_background.m_yPosition; }
    BackgroundLayer* accessBackgroundLayers() { return &(background.access()->m_background); }
    const BackgroundLayer* backgroundLayers() const { return &(background->m_background); }

    // returns true for collapsing borders, false for separate borders
    bool borderCollapse() const { return inherited_flags._border_collapse; }
    short horizontalBorderSpacing() const { return inherited->horizontal_border_spacing; }
    short verticalBorderSpacing() const { return inherited->vertical_border_spacing; }
    EEmptyCell emptyCells() const { return inherited_flags._empty_cells; }
    ECaptionSide captionSide() const { return inherited_flags._caption_side; }

    short counterIncrement() const { return visual->counter_increment; }
    short counterReset() const { return visual->counter_reset; }

    EListStyleType listStyleType() const { return inherited_flags._list_style_type; }
    CachedImage *listStyleImage() const { return inherited->style_image; }
    EListStylePosition listStylePosition() const { return inherited_flags._list_style_position; }

    Length marginTop() const { return surround->margin.top; }
    Length marginBottom() const {  return surround->margin.bottom; }
    Length marginLeft() const {  return surround->margin.left; }
    Length marginRight() const {  return surround->margin.right; }

    Length paddingTop() const {  return surround->padding.top; }
    Length paddingBottom() const {  return surround->padding.bottom; }
    Length paddingLeft() const { return surround->padding.left; }
    Length paddingRight() const {  return surround->padding.right; }

    ECursor cursor() const { return inherited_flags._cursor_style; }
    
    CachedImage *cursorImage() const { return inherited->cursor_image; }

    short widows() const { return inherited->widows; }
    short orphans() const { return inherited->orphans; }
    EPageBreak pageBreakInside() const { return inherited->page_break_inside; }
    EPageBreak pageBreakBefore() const { return noninherited_flags._page_break_before; }
    EPageBreak pageBreakAfter() const { return noninherited_flags._page_break_after; }
    
    // CSS3 Getter Methods
#ifndef KHTML_NO_XBL
    BindingURI* bindingURIs() const { return css3NonInheritedData->bindingURI; }
#endif
    int outlineOffset() const { 
        if (background->m_outline.style == BNONE || background->m_outline.style == BHIDDEN) return 0; return background->m_outline._offset;
    }
    ShadowData* textShadow() const { return css3InheritedData->textShadow; }
    float opacity() const { return css3NonInheritedData->opacity; }
    EAppearance appearance() const { return css3NonInheritedData->m_appearance; }
    EBoxAlignment boxAlign() const { return css3NonInheritedData->flexibleBox->align; }
    EBoxDirection boxDirection() const { return inherited_flags._box_direction; }
    float boxFlex() { return css3NonInheritedData->flexibleBox->flex; }
    unsigned int boxFlexGroup() const { return css3NonInheritedData->flexibleBox->flex_group; }
    EBoxLines boxLines() { return css3NonInheritedData->flexibleBox->lines; }
    unsigned int boxOrdinalGroup() const { return css3NonInheritedData->flexibleBox->ordinal_group; }
    EBoxOrient boxOrient() const { return css3NonInheritedData->flexibleBox->orient; }
    EBoxAlignment boxPack() const { return css3NonInheritedData->flexibleBox->pack; }
    EBoxSizing boxSizing() const { return box->boxSizing; }
    Length marqueeIncrement() const { return css3NonInheritedData->marquee->increment; }
    int marqueeSpeed() const { return css3NonInheritedData->marquee->speed; }
    int marqueeLoopCount() const { return css3NonInheritedData->marquee->loops; }
    EMarqueeBehavior marqueeBehavior() const { return css3NonInheritedData->marquee->behavior; }
    EMarqueeDirection marqueeDirection() const { return css3NonInheritedData->marquee->direction; }
    EUserModify userModify() const { return css3InheritedData->userModify; }
    EUserDrag userDrag() const { return css3NonInheritedData->userDrag; }
    EUserSelect userSelect() const { return css3NonInheritedData->userSelect; }
    bool textOverflow() const { return css3NonInheritedData->textOverflow; }
    EMarginCollapse marginTopCollapse() const { return css3NonInheritedData->marginTopCollapse; }
    EMarginCollapse marginBottomCollapse() const { return css3NonInheritedData->marginBottomCollapse; }
    EWordWrap wordWrap() const { return css3InheritedData->wordWrap; }
    ENBSPMode nbspMode() const { return css3InheritedData->nbspMode; }
    EKHTMLLineBreak khtmlLineBreak() const { return css3InheritedData->khtmlLineBreak; }
    EMatchNearestMailBlockquoteColor matchNearestMailBlockquoteColor() const { return css3NonInheritedData->matchNearestMailBlockquoteColor; }
    // End CSS3 Getters

#if APPLE_CHANGES
    // Apple-specific property getter methods
    int lineClamp() const { return css3NonInheritedData->lineClamp; }
    bool textSizeAdjust() const { return css3InheritedData->textSizeAdjust; }
#endif

// attribute setter methods

    void setDisplay(EDisplay v) {  noninherited_flags._effectiveDisplay = v; }
    void setOriginalDisplay(EDisplay v) {  noninherited_flags._originalDisplay = v; }
    void setPosition(EPosition v) {  noninherited_flags._position = v; }
    void setFloating(EFloat v) {  noninherited_flags._floating = v; }

    void setLeft(Length v)  {  SET_VAR(surround,offset.left,v) }
    void setRight(Length v) {  SET_VAR(surround,offset.right,v) }
    void setTop(Length v)   {  SET_VAR(surround,offset.top,v) }
    void setBottom(Length v){  SET_VAR(surround,offset.bottom,v) }

    void setWidth(Length v)  { SET_VAR(box,width,v) }
    void setHeight(Length v) { SET_VAR(box,height,v) }

    void setMinWidth(Length v)  { SET_VAR(box,min_width,v) }
    void setMaxWidth(Length v)  { SET_VAR(box,max_width,v) }
    void setMinHeight(Length v) { SET_VAR(box,min_height,v) }
    void setMaxHeight(Length v) { SET_VAR(box,max_height,v) }

#if APPLE_CHANGES
    QValueList<StyleDashboardRegion> dashboardRegions() const { return css3NonInheritedData->m_dashboardRegions; }
    void setDashboardRegions(QValueList<StyleDashboardRegion> regions) { SET_VAR(css3NonInheritedData,m_dashboardRegions,regions); }
    void setDashboardRegion (int type, QString label, Length t, Length r, Length b, Length l, bool append) {
        StyleDashboardRegion region;
        region.label = label;
        region.offset.top  = t;
        region.offset.right = r;
        region.offset.bottom = b;
        region.offset.left = l;
        region.type = type;
        if (!append) {
            css3NonInheritedData.access()->m_dashboardRegions.clear ();
        }
        css3NonInheritedData.access()->m_dashboardRegions.append (region);
    }
#endif

    void resetBorder() { resetBorderImage(); resetBorderTop(); resetBorderRight(); resetBorderBottom(); resetBorderLeft(); resetBorderRadius(); }
    void resetBorderTop() { SET_VAR(surround, border.top, BorderValue()) }
    void resetBorderRight() { SET_VAR(surround, border.right, BorderValue()) }
    void resetBorderBottom() { SET_VAR(surround, border.bottom, BorderValue()) }
    void resetBorderLeft() { SET_VAR(surround, border.left, BorderValue()) }
    void resetBorderImage() { SET_VAR(surround, border.image, BorderImage()) }
    void resetBorderRadius() { resetBorderTopLeftRadius(); resetBorderTopRightRadius(); resetBorderBottomLeftRadius(); resetBorderBottomRightRadius(); }
    void resetBorderTopLeftRadius() { SET_VAR(surround, border.topLeft, initialBorderRadius()) }
    void resetBorderTopRightRadius() { SET_VAR(surround, border.topRight, initialBorderRadius()) }
    void resetBorderBottomLeftRadius() { SET_VAR(surround, border.bottomLeft, initialBorderRadius()) }
    void resetBorderBottomRightRadius() { SET_VAR(surround, border.bottomRight, initialBorderRadius()) }
    
    void resetOutline() { SET_VAR(background, m_outline, OutlineValue()) }
    
    void setBackgroundColor(const QColor& v)    { SET_VAR(background, m_color, v) }

    void setBorderImage(const BorderImage& b)   { SET_VAR(surround, border.image, b) }

    void setBorderTopLeftRadius(const QSize& s) { SET_VAR(surround, border.topLeft, s) }
    void setBorderTopRightRadius(const QSize& s) { SET_VAR(surround, border.topRight, s) }
    void setBorderBottomLeftRadius(const QSize& s) { SET_VAR(surround, border.bottomLeft, s) }
    void setBorderBottomRightRadius(const QSize& s) { SET_VAR(surround, border.bottomRight, s) }
    void setBorderRadius(const QSize& s) { 
        setBorderTopLeftRadius(s); setBorderTopRightRadius(s); setBorderBottomLeftRadius(s); setBorderBottomRightRadius(s);
    }

    void setBorderLeftWidth(unsigned short v)   {  SET_VAR(surround,border.left.width,v) }
    void setBorderLeftStyle(EBorderStyle v)     {  SET_VAR(surround,border.left.style,v) }
    void setBorderLeftColor(const QColor & v)   {  SET_VAR(surround,border.left.color,v) }
    void setBorderRightWidth(unsigned short v)  {  SET_VAR(surround,border.right.width,v) }
    void setBorderRightStyle(EBorderStyle v)    {  SET_VAR(surround,border.right.style,v) }
    void setBorderRightColor(const QColor & v)  {  SET_VAR(surround,border.right.color,v) }
    void setBorderTopWidth(unsigned short v)    {  SET_VAR(surround,border.top.width,v) }
    void setBorderTopStyle(EBorderStyle v)      {  SET_VAR(surround,border.top.style,v) }
    void setBorderTopColor(const QColor & v)    {  SET_VAR(surround,border.top.color,v) }    
    void setBorderBottomWidth(unsigned short v) {  SET_VAR(surround,border.bottom.width,v) }
    void setBorderBottomStyle(EBorderStyle v)   {  SET_VAR(surround,border.bottom.style,v) }
    void setBorderBottomColor(const QColor & v) {  SET_VAR(surround,border.bottom.color,v) }
    void setOutlineWidth(unsigned short v) {  SET_VAR(background,m_outline.width,v) }
    void setOutlineStyle(EBorderStyle v, bool isAuto = false)   
    {  
        SET_VAR(background,m_outline.style,v)
        SET_VAR(background,m_outline._auto, isAuto)
    }
    void setOutlineColor(const QColor & v) {  SET_VAR(background,m_outline.color,v) }

    void setOverflow(EOverflow v) {  noninherited_flags._overflow = v; }
    void setVisibility(EVisibility v) { inherited_flags._visibility = v; }
    void setVerticalAlign(EVerticalAlign v) { noninherited_flags._vertical_align = v; }
    void setVerticalAlignLength(Length l) { SET_VAR(box, vertical_align, l ) }

    void setHasClip(bool b = true) { SET_VAR(visual,hasClip,b) }
    void setClipLeft(Length v) { SET_VAR(visual,clip.left,v) }
    void setClipRight(Length v) { SET_VAR(visual,clip.right,v) }
    void setClipTop(Length v) { SET_VAR(visual,clip.top,v) }
    void setClipBottom(Length v) { SET_VAR(visual,clip.bottom,v) }
    void setClip( Length top, Length right, Length bottom, Length left );
    
    void setUnicodeBidi( EUnicodeBidi b ) { noninherited_flags._unicodeBidi = b; }

    void setClear(EClear v) {  noninherited_flags._clear = v; }
    void setTableLayout(ETableLayout v) {  noninherited_flags._table_layout = v; }
    void ssetColSpan(short v) { SET_VAR(visual,colspan,v) }

    bool setFontDef(const khtml::FontDef & v) {
        // bah, this doesn't compare pointers. broken! (Dirk)
        if (!(inherited->font.fontDef == v)) {
            inherited.access()->font = Font( v, inherited->font.letterSpacing, inherited->font.wordSpacing );
            return true;
        }
        return false;
    }

    void setColor(const QColor & v) { SET_VAR(inherited,color,v) }
    void setTextIndent(Length v) { SET_VAR(inherited,indent,v) }
    void setTextAlign(ETextAlign v) { inherited_flags._text_align = v; }
    void setTextTransform(ETextTransform v) { inherited_flags._text_transform = v; }
    void addToTextDecorationsInEffect(int v) { inherited_flags._text_decorations |= v; }
    void setTextDecorationsInEffect(int v) { inherited_flags._text_decorations = v; }
    void setTextDecoration(int v) { SET_VAR(visual, textDecoration, v); }
    void setDirection(EDirection v) { inherited_flags._direction = v; }
    void setLineHeight(Length v) { SET_VAR(inherited,line_height,v) }

    void setWhiteSpace(EWhiteSpace v) { inherited_flags._white_space = v; }

    void setWordSpacing(int v) { SET_VAR(inherited,font.wordSpacing,v) }
    void setLetterSpacing(int v) { SET_VAR(inherited,font.letterSpacing,v) }

    void clearBackgroundLayers() { background.access()->m_background = BackgroundLayer(); }
    void inheritBackgroundLayers(const BackgroundLayer& parent) { background.access()->m_background = parent; }
    void adjustBackgroundLayers();

    void setBorderCollapse(bool collapse) { inherited_flags._border_collapse = collapse; }
    void setHorizontalBorderSpacing(short v) { SET_VAR(inherited,horizontal_border_spacing,v) }
    void setVerticalBorderSpacing(short v) { SET_VAR(inherited,vertical_border_spacing,v) }
    void setEmptyCells(EEmptyCell v) { inherited_flags._empty_cells = v; }
    void setCaptionSide(ECaptionSide v) { inherited_flags._caption_side = v; }


    void setCounterIncrement(short v) {  SET_VAR(visual,counter_increment,v) }
    void setCounterReset(short v) {  SET_VAR(visual,counter_reset,v) }

    void setListStyleType(EListStyleType v) { inherited_flags._list_style_type = v; }
    void setListStyleImage(CachedImage *v) {  SET_VAR(inherited,style_image,v)}
    void setListStylePosition(EListStylePosition v) { inherited_flags._list_style_position = v; }

    void resetMargin() { SET_VAR(surround, margin, LengthBox(Fixed)) }
    void setMarginTop(Length v)     {  SET_VAR(surround,margin.top,v) }
    void setMarginBottom(Length v)  {  SET_VAR(surround,margin.bottom,v) }
    void setMarginLeft(Length v)    {  SET_VAR(surround,margin.left,v) }
    void setMarginRight(Length v)   {  SET_VAR(surround,margin.right,v) }

    void resetPadding() { SET_VAR(surround, padding, LengthBox(Auto)) }
    void setPaddingTop(Length v)    {  SET_VAR(surround,padding.top,v) }
    void setPaddingBottom(Length v) {  SET_VAR(surround,padding.bottom,v) }
    void setPaddingLeft(Length v)   {  SET_VAR(surround,padding.left,v) }
    void setPaddingRight(Length v)  {  SET_VAR(surround,padding.right,v) }

    void setCursor( ECursor c ) { inherited_flags._cursor_style = c; }
    void setCursorImage( CachedImage *v ) { SET_VAR(inherited,cursor_image,v) }

    bool forceBackgroundsToWhite() const { return inherited_flags._force_backgrounds_to_white; }
    void setForceBackgroundsToWhite(bool b=true) { inherited_flags._force_backgrounds_to_white = b; }

    bool htmlHacks() const { return inherited_flags._htmlHacks; }
    void setHtmlHacks(bool b=true) { inherited_flags._htmlHacks = b; }

    bool hasAutoZIndex() { return box->z_auto; }
    void setHasAutoZIndex() { SET_VAR(box, z_auto, true); SET_VAR(box, z_index, 0) }
    int zIndex() const { return box->z_index; }
    void setZIndex(int v) { SET_VAR(box, z_auto, false); SET_VAR(box, z_index, v) }

    void setWidows(short w) { SET_VAR(inherited, widows, w); }
    void setOrphans(short o) { SET_VAR(inherited, orphans, o); }
    void setPageBreakInside(EPageBreak b) { SET_VAR(inherited, page_break_inside, b); }
    void setPageBreakBefore(EPageBreak b) { noninherited_flags._page_break_before = b; }
    void setPageBreakAfter(EPageBreak b) { noninherited_flags._page_break_after = b; }
    
    // CSS3 Setters
#ifndef KHTML_NO_XBL
    void deleteBindingURIs() { 
        delete css3NonInheritedData->bindingURI; 
        SET_VAR(css3NonInheritedData, bindingURI, 0);
    }
    void inheritBindingURIs(BindingURI* other) {
        SET_VAR(css3NonInheritedData, bindingURI, other->copy());
    }
    void addBindingURI(DOM::DOMStringImpl* uri);
#endif
    void setOutlineOffset(unsigned short v) { SET_VAR(background, m_outline._offset, v) }
    void setTextShadow(ShadowData* val, bool add=false);
    void setOpacity(float f) { SET_VAR(css3NonInheritedData, opacity, f); }
    void setAppearance(EAppearance a) { SET_VAR(css3NonInheritedData, m_appearance, a); }
    void setBoxAlign(EBoxAlignment a) { SET_VAR(css3NonInheritedData.access()->flexibleBox, align, a); }
    void setBoxDirection(EBoxDirection d) { inherited_flags._box_direction = d; }
    void setBoxFlex(float f) { SET_VAR(css3NonInheritedData.access()->flexibleBox, flex, f); }
    void setBoxFlexGroup(unsigned int fg) { SET_VAR(css3NonInheritedData.access()->flexibleBox, flex_group, fg); }
    void setBoxLines(EBoxLines l) { SET_VAR(css3NonInheritedData.access()->flexibleBox, lines, l); }
    void setBoxOrdinalGroup(unsigned int og) { SET_VAR(css3NonInheritedData.access()->flexibleBox, ordinal_group, og); }
    void setBoxOrient(EBoxOrient o) { SET_VAR(css3NonInheritedData.access()->flexibleBox, orient, o); }
    void setBoxPack(EBoxAlignment p) { SET_VAR(css3NonInheritedData.access()->flexibleBox, pack, p); }
    void setBoxSizing(EBoxSizing s) { SET_VAR(box, boxSizing, s); }
    void setMarqueeIncrement(const Length& f) { SET_VAR(css3NonInheritedData.access()->marquee, increment, f); }
    void setMarqueeSpeed(int f) { SET_VAR(css3NonInheritedData.access()->marquee, speed, f); }
    void setMarqueeDirection(EMarqueeDirection d) { SET_VAR(css3NonInheritedData.access()->marquee, direction, d); }
    void setMarqueeBehavior(EMarqueeBehavior b) { SET_VAR(css3NonInheritedData.access()->marquee, behavior, b); }
    void setMarqueeLoopCount(int i) { SET_VAR(css3NonInheritedData.access()->marquee, loops, i); }
    void setUserModify(EUserModify u) { SET_VAR(css3InheritedData, userModify, u); }
    void setUserDrag(EUserDrag d) { SET_VAR(css3NonInheritedData, userDrag, d); }
    void setUserSelect(EUserSelect s) { SET_VAR(css3NonInheritedData, userSelect, s); }
    void setTextOverflow(bool b) { SET_VAR(css3NonInheritedData, textOverflow, b); }
    void setMarginTopCollapse(EMarginCollapse c) { SET_VAR(css3NonInheritedData, marginTopCollapse, c); }
    void setMarginBottomCollapse(EMarginCollapse c) { SET_VAR(css3NonInheritedData, marginBottomCollapse, c); }
    void setWordWrap(EWordWrap b) { SET_VAR(css3InheritedData, wordWrap, b); }
    void setNBSPMode(ENBSPMode b) { SET_VAR(css3InheritedData, nbspMode, b); }
    void setKHTMLLineBreak(EKHTMLLineBreak b) { SET_VAR(css3InheritedData, khtmlLineBreak, b); }
    void setMatchNearestMailBlockquoteColor(EMatchNearestMailBlockquoteColor c)  { SET_VAR(css3NonInheritedData, matchNearestMailBlockquoteColor, c); }
    // End CSS3 Setters
   
#if APPLE_CHANGES
    // Apple-specific property setters
    void setLineClamp(int c) { SET_VAR(css3NonInheritedData, lineClamp, c); }
    void setTextSizeAdjust(bool b) { SET_VAR(css3InheritedData, textSizeAdjust, b); }
#endif

#if !APPLE_CHANGES
    QPalette palette() const { return visual->palette; }
    void setPaletteColor(QPalette::ColorGroup g, QColorGroup::ColorRole r, const QColor& c);
    void resetPalette() // Called when the desktop color scheme changes.
    {
        const_cast<StyleVisualData *>(visual.get())->palette = QApplication::palette();
    }
#endif

    ContentData* contentData() { return content; }
    bool contentDataEquivalent(RenderStyle* otherStyle);
    void setContent(DOM::DOMStringImpl* s, bool add = false);
    void setContent(CachedObject* o, bool add = false);

    bool inheritedNotEqual( RenderStyle *other ) const;

    // The difference between two styles.  The following values are used:
    // (1) Equal - The two styles are identical
    // (2) Repaint - The object just needs to be repainted.
    // (3) RepaintLayer - The layer and its descendant layers needs to be repainted.
    // (4) Layout - A layout is required.
    enum Diff { Equal, Repaint, RepaintLayer, Layout };
    Diff diff( const RenderStyle *other ) const;

    bool isDisplayReplacedType() {
        return display() == INLINE_BLOCK || display() == INLINE_BOX || display() == INLINE_TABLE;
    }
    bool isDisplayInlineType() {
        return display() == INLINE || isDisplayReplacedType();
    }
    bool isOriginalDisplayInlineType() {
        return originalDisplay() == INLINE || originalDisplay() == INLINE_BLOCK ||
               originalDisplay() == INLINE_BOX || originalDisplay() == INLINE_TABLE;
    }
    
    // To obtain at any time the pseudo state for a given link.
    PseudoState pseudoState() const { return m_pseudoState; }
    void setPseudoState(PseudoState s) { m_pseudoState = s; }
    
    // To tell if this style matched attribute selectors. This makes it impossible to share.
    bool affectedByAttributeSelectors() const { return m_affectedByAttributeSelectors; }
    void setAffectedByAttributeSelectors() { m_affectedByAttributeSelectors = true; }

    // Initial values for all the properties
    static bool initialBackgroundAttachment() { return true; }
    static EBackgroundBox initialBackgroundClip() { return BGBORDER; }
    static EBackgroundBox initialBackgroundOrigin() { return BGPADDING; }
    static EBackgroundRepeat initialBackgroundRepeat() { return REPEAT; }
    static bool initialBorderCollapse() { return false; }
    static EBorderStyle initialBorderStyle() { return BNONE; }
    static BorderImage initialBorderImage() { return BorderImage(); }
    static QSize initialBorderRadius() { return QSize(0,0); }
    static ECaptionSide initialCaptionSide() { return CAPTOP; }
    static EClear initialClear() { return CNONE; }
    static EDirection initialDirection() { return LTR; }
    static EDisplay initialDisplay() { return INLINE; }
    static EEmptyCell initialEmptyCells() { return SHOW; }
    static EFloat initialFloating() { return FNONE; }
    static EListStylePosition initialListStylePosition() { return OUTSIDE; }
    static EListStyleType initialListStyleType() { return DISC; }
    static EOverflow initialOverflow() { return OVISIBLE; }
    static EPageBreak initialPageBreak() { return PBAUTO; }
    static EPosition initialPosition() { return STATIC; }
    static ETableLayout initialTableLayout() { return TAUTO; }
    static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
    static ETextTransform initialTextTransform() { return TTNONE; }
    static EVisibility initialVisibility() { return VISIBLE; }
    static EWhiteSpace initialWhiteSpace() { return NORMAL; }
    static Length initialBackgroundXPosition() { return Length(); }
    static Length initialBackgroundYPosition() { return Length(); }
    static short initialHorizontalBorderSpacing() { return 0; }
    static short initialVerticalBorderSpacing() { return 0; }
    static ECursor initialCursor() { return CURSOR_AUTO; }
    static QColor initialColor() { return Qt::black; }
    static CachedImage* initialBackgroundImage() { return 0; }
    static CachedImage* initialListStyleImage() { return 0; }
    static unsigned short initialBorderWidth() { return 3; }
    static int initialLetterWordSpacing() { return 0; }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(0, Fixed); }
    static Length initialMaxSize() { return Length(UNDEFINED, Fixed); }
    static Length initialOffset() { return Length(); }
    static Length initialMargin() { return Length(Fixed); }
    static Length initialPadding() { return Length(Auto); }
    static Length initialTextIndent() { return Length(Fixed); }
    static EVerticalAlign initialVerticalAlign() { return BASELINE; }
    static int initialWidows() { return 2; }
    static int initialOrphans() { return 2; }
    static Length initialLineHeight() { return Length(-100, Percent); }
    static ETextAlign initialTextAlign() { return TAAUTO; }
    static ETextDecoration initialTextDecoration() { return TDNONE; }
    static int initialOutlineOffset() { return 0; }
    static float initialOpacity() { return 1.0f; }
    static EBoxAlignment initialBoxAlign() { return BSTRETCH; }
    static EBoxDirection initialBoxDirection() { return BNORMAL; }
    static EBoxLines initialBoxLines() { return SINGLE; }
    static EBoxOrient initialBoxOrient() { return HORIZONTAL; }
    static EBoxAlignment initialBoxPack() { return BSTART; }
    static float initialBoxFlex() { return 0.0f; }
    static int initialBoxFlexGroup() { return 1; }
    static int initialBoxOrdinalGroup() { return 1; }
    static EBoxSizing initialBoxSizing() { return CONTENT_BOX; }
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static Length initialMarqueeIncrement() { return Length(6, Fixed); }
    static EMarqueeBehavior initialMarqueeBehavior() { return MSCROLL; }
    static EMarqueeDirection initialMarqueeDirection() { return MAUTO; }
    static EUserModify initialUserModify() { return READ_ONLY; }
    static EUserDrag initialUserDrag() { return DRAG_AUTO; }
    static EUserSelect initialUserSelect() { return SELECT_AUTO; }
    static bool initialTextOverflow() { return false; }
    static EMarginCollapse initialMarginTopCollapse() { return MCOLLAPSE; }
    static EMarginCollapse initialMarginBottomCollapse() { return MCOLLAPSE; }
    static EWordWrap initialWordWrap() { return WBNORMAL; }
    static ENBSPMode initialNBSPMode() { return NBNORMAL; }
    static EKHTMLLineBreak initialKHTMLLineBreak() { return LBNORMAL; }
    static EMatchNearestMailBlockquoteColor initialMatchNearestMailBlockquoteColor() { return BCNORMAL; }
    static EAppearance initialAppearance() { return NoAppearance; }

#if APPLE_CHANGES
    // Keep these at the end.
    static int initialLineClamp() { return -1; }
    static bool initialTextSizeAdjust() { return true; }
    static const QValueList<StyleDashboardRegion>& initialDashboardRegions();
    static const QValueList<StyleDashboardRegion>& noneDashboardRegions();
#endif
};


} // namespace

#endif

