/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef RenderStyle_h
#define RenderStyle_h

/*
 * WARNING:
 * --------
 *
 * The order of the values in the enums have to agree with the order specified
 * in CSSValueKeywords.in, otherwise some optimizations in the parser will fail,
 * and produce invalid results.
 */

#include "AffineTransform.h"
#include "CSSHelper.h"
#include "CSSImageGeneratorValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSReflectionDirection.h"
#include "CSSValueList.h"
#include "Color.h"
#include "DataRef.h"
#include "FloatPoint.h"
#include "Font.h"
#include "GraphicsTypes.h"
#include "IntRect.h"
#include "Length.h"
#include "Pair.h"
#include "TextDirection.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

#if ENABLE(SVG)
#include "SVGRenderStyle.h"
#endif

template<typename T, typename U> inline bool compareEqual(const T& t, const U& u) { return t == static_cast<T>(u); }

#define SET_VAR(group, variable, value) \
    if (!compareEqual(group->variable, value)) \
        group.access()->variable = value;

namespace WebCore {

using std::max;

class CSSStyleSelector;
class CSSValueList;
class CachedImage;
class CachedResource;
class CursorList;
class KeyframeList;
class Pair;
class RenderArena;
class ShadowValue;
class StringImpl;

struct CursorData;

enum PseudoState { PseudoUnknown, PseudoNone, PseudoAnyLink, PseudoLink, PseudoVisited};

//------------------------------------------------

//------------------------------------------------
// Box model attributes. Not inherited.

struct LengthBox {
    LengthBox() { }
    LengthBox(LengthType t)
        : left(t), right(t), top(t), bottom(t) { }

    Length left;
    Length right;
    Length top;
    Length bottom;

    LengthBox& operator=(const Length& len)
    {
        left = len;
        right = len;
        top = len;
        bottom = len;
        return *this;
    }

    bool operator==(const LengthBox& o) const
    {
        return left == o.left && right == o.right && top == o.top && bottom == o.bottom;
    }

    bool operator!=(const LengthBox& o) const
    {
        return !(*this == o);
    }

    bool nonZero() const { return !(left.isZero() && right.isZero() && top.isZero() && bottom.isZero()); }
};

enum EPosition {
    StaticPosition, RelativePosition, AbsolutePosition, FixedPosition
};

enum EFloat {
    FNONE = 0, FLEFT, FRIGHT
};

typedef void* WrappedImagePtr;

class StyleImage : public RefCounted<StyleImage>
{
public:
    virtual ~StyleImage() { }

    bool operator==(const StyleImage& other)
    {
        return data() == other.data();
    }
    
    virtual PassRefPtr<CSSValue> cssValue() = 0;

    virtual bool canRender(float multiplier) const { return true; }
    virtual bool isLoaded() const { return true; }
    virtual bool errorOccurred() const { return false; }
    virtual IntSize imageSize(const RenderObject*, float multiplier) const = 0;
    virtual bool imageHasRelativeWidth() const = 0;
    virtual bool imageHasRelativeHeight() const = 0;
    virtual bool usesImageContainerSize() const = 0;
    virtual void setImageContainerSize(const IntSize&) = 0;
    virtual void addClient(RenderObject*) = 0;
    virtual void removeClient(RenderObject*) = 0;
    virtual Image* image(RenderObject*, const IntSize&) const = 0;
    virtual WrappedImagePtr data() const = 0;
    virtual bool isCachedImage() const { return false; }
    virtual bool isGeneratedImage() const { return false; }
    
protected:
    StyleImage() { }
};

class StyleCachedImage : public StyleImage
{
public:
    static PassRefPtr<StyleCachedImage> create(CachedImage* image) { return adoptRef(new StyleCachedImage(image)); }
    virtual WrappedImagePtr data() const { return m_image; }

    virtual bool isCachedImage() const { return true; }
    
    virtual PassRefPtr<CSSValue> cssValue();
    
    CachedImage* cachedImage() const { return m_image; }

    virtual bool canRender(float multiplier) const;
    virtual bool isLoaded() const;
    virtual bool errorOccurred() const;
    virtual IntSize imageSize(const RenderObject*, float multiplier) const;
    virtual bool imageHasRelativeWidth() const;
    virtual bool imageHasRelativeHeight() const;
    virtual bool usesImageContainerSize() const;
    virtual void setImageContainerSize(const IntSize&);
    virtual void addClient(RenderObject*);
    virtual void removeClient(RenderObject*);
    virtual Image* image(RenderObject*, const IntSize&) const;
    
private:
    StyleCachedImage(CachedImage* image)
        : m_image(image)
    {
    }
    
    CachedImage* m_image;
};

class StyleGeneratedImage : public StyleImage
{
public:
    static PassRefPtr<StyleGeneratedImage> create(CSSImageGeneratorValue* val, bool fixedSize)
    {
        return adoptRef(new StyleGeneratedImage(val, fixedSize));
    }

    virtual WrappedImagePtr data() const { return m_generator; }

    virtual bool isGeneratedImage() const { return true; }
    
    virtual PassRefPtr<CSSValue> cssValue();

    virtual IntSize imageSize(const RenderObject*, float multiplier) const;
    virtual bool imageHasRelativeWidth() const { return !m_fixedSize; }
    virtual bool imageHasRelativeHeight() const { return !m_fixedSize; }
    virtual bool usesImageContainerSize() const { return !m_fixedSize; }
    virtual void setImageContainerSize(const IntSize&);
    virtual void addClient(RenderObject*);
    virtual void removeClient(RenderObject*);
    virtual Image* image(RenderObject*, const IntSize&) const;
    
private:
    StyleGeneratedImage(CSSImageGeneratorValue* val, bool fixedSize)
        : m_generator(val)
        , m_fixedSize(fixedSize)
    {
    }
    
    CSSImageGeneratorValue* m_generator; // The generator holds a reference to us.
    IntSize m_containerSize;
    bool m_fixedSize;
};

//------------------------------------------------
// Border attributes. Not inherited.

// These have been defined in the order of their precedence for border-collapsing. Do
// not change this order!
enum EBorderStyle {
    BNONE, BHIDDEN, INSET, GROOVE, RIDGE, OUTSET, DOTTED, DASHED, SOLID, DOUBLE
};

class BorderValue {
public:
    BorderValue() {
        width = 3; // medium is default value
        m_style = BNONE;
    }

    Color color;
    unsigned width : 12;
    unsigned m_style : 4; // EBorderStyle 

    EBorderStyle style() const { return static_cast<EBorderStyle>(m_style); }
    
    bool nonZero(bool checkStyle = true) const {
        return width != 0 && (!checkStyle || m_style != BNONE);
    }

    bool isTransparent() const {
        return color.isValid() && color.alpha() == 0;
    }

    bool isVisible(bool checkStyle = true) const {
        return nonZero(checkStyle) && !isTransparent() && (!checkStyle || m_style != BHIDDEN);
    }

    bool operator==(const BorderValue& o) const
    {
        return width == o.width && m_style == o.m_style && color == o.color;
    }

    bool operator!=(const BorderValue& o) const
    {
        return !(*this == o);
    }
};

class OutlineValue : public BorderValue {
public:
    OutlineValue()
    {
        _offset = 0;
        _auto = false;
    }
    
    bool operator==(const OutlineValue& o) const
    {
        return width == o.width && m_style == o.m_style && color == o.color && _offset == o._offset && _auto == o._auto;
    }
    
    bool operator!=(const OutlineValue& o) const
    {
        return !(*this == o);
    }
    
    int _offset;
    bool _auto;
};

enum EBorderPrecedence { BOFF, BTABLE, BCOLGROUP, BCOL, BROWGROUP, BROW, BCELL };

struct CollapsedBorderValue {
    CollapsedBorderValue() : border(0), precedence(BOFF) {}
    CollapsedBorderValue(const BorderValue* b, EBorderPrecedence p) :border(b), precedence(p) {}
    
    int width() const { return border && border->nonZero() ? border->width : 0; }
    EBorderStyle style() const { return border ? border->style() : BHIDDEN; }
    bool exists() const { return border; }
    Color color() const { return border ? border->color : Color(); }
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

enum ENinePieceImageRule {
    StretchImageRule, RoundImageRule, RepeatImageRule
};

class NinePieceImage {
public:
    NinePieceImage() :m_image(0), m_horizontalRule(StretchImageRule), m_verticalRule(StretchImageRule) {}
    NinePieceImage(StyleImage* image, LengthBox slices, ENinePieceImageRule h, ENinePieceImageRule v) 
      :m_image(image), m_slices(slices), m_horizontalRule(h), m_verticalRule(v) {}

    bool operator==(const NinePieceImage& o) const;
    bool operator!=(const NinePieceImage& o) const { return !(*this == o); }

    bool hasImage() const { return m_image != 0; }
    StyleImage* image() const { return m_image.get(); }
    
    ENinePieceImageRule horizontalRule() const { return static_cast<ENinePieceImageRule>(m_horizontalRule); }
    ENinePieceImageRule verticalRule() const { return static_cast<ENinePieceImageRule>(m_verticalRule); }
    
    RefPtr<StyleImage> m_image;
    LengthBox m_slices;
    unsigned m_horizontalRule : 2; // ENinePieceImageRule
    unsigned m_verticalRule : 2; // ENinePieceImageRule
};

class BorderData {
public:
    BorderValue left;
    BorderValue right;
    BorderValue top;
    BorderValue bottom;
    
    NinePieceImage image;

    IntSize topLeft;
    IntSize topRight;
    IntSize bottomLeft;
    IntSize bottomRight;

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
        if (!image.hasImage() && (left.style() == BNONE || left.style() == BHIDDEN))
            return 0; 
        return left.width;
    }
    
    unsigned short borderRightWidth() const {
        if (!image.hasImage() && (right.style() == BNONE || right.style() == BHIDDEN))
            return 0;
        return right.width;
    }
    
    unsigned short borderTopWidth() const {
        if (!image.hasImage() && (top.style() == BNONE || top.style() == BHIDDEN))
            return 0;
        return top.width;
    }
    
    unsigned short borderBottomWidth() const {
        if (!image.hasImage() && (bottom.style() == BNONE || bottom.style() == BHIDDEN))
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

class StyleSurroundData : public RefCounted<StyleSurroundData> {
public:
    static PassRefPtr<StyleSurroundData> create() { return adoptRef(new StyleSurroundData); }
    PassRefPtr<StyleSurroundData> copy() const { return adoptRef(new StyleSurroundData(*this)); }
    
    bool operator==(const StyleSurroundData& o) const;
    bool operator!=(const StyleSurroundData& o) const {
        return !(*this == o);
    }

    LengthBox offset;
    LengthBox margin;
    LengthBox padding;
    BorderData border;
    
private:
    StyleSurroundData();
    StyleSurroundData(const StyleSurroundData&);    
};


//------------------------------------------------
// Box attributes. Not inherited.

enum EBoxSizing { CONTENT_BOX, BORDER_BOX };

class StyleBoxData : public RefCounted<StyleBoxData> {
public:
    static PassRefPtr<StyleBoxData> create() { return adoptRef(new StyleBoxData); }
    PassRefPtr<StyleBoxData> copy() const { return adoptRef(new StyleBoxData(*this)); }

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
    unsigned boxSizing : 1; // EBoxSizing
    
private:
    StyleBoxData();
    StyleBoxData(const StyleBoxData&);
};

#if ENABLE(DASHBOARD_SUPPORT)
//------------------------------------------------
// Dashboard region attributes. Not inherited.

struct StyleDashboardRegion {
    String label;
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

   bool operator!=(const StyleDashboardRegion& o) const
   {
       return !(*this == o);
   }
};
#endif

//------------------------------------------------
// Random visual rendering model attributes. Not inherited.

enum EOverflow {
    OVISIBLE, OHIDDEN, OSCROLL, OAUTO, OOVERLAY, OMARQUEE
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

class StyleVisualData : public RefCounted<StyleVisualData> {
public:
    static PassRefPtr<StyleVisualData> create() { return adoptRef(new StyleVisualData); }
    PassRefPtr<StyleVisualData> copy() const { return adoptRef(new StyleVisualData(*this)); }
    ~StyleVisualData();

    bool operator==(const StyleVisualData& o) const {
        return ( clip == o.clip &&
                 hasClip == o.hasClip &&
                 counterIncrement == o.counterIncrement &&
                 counterReset == o.counterReset &&
                 textDecoration == o.textDecoration &&
                 m_zoom == o.m_zoom);
    }
    bool operator!=(const StyleVisualData& o) const { return !(*this == o); }

    LengthBox clip;
    bool hasClip : 1;
    unsigned textDecoration : 4; // Text decorations defined *only* by this element.
    
    short counterIncrement; // ok, so these are not visual mode specific
    short counterReset;     // can't go to inherited, since these are not inherited

    float m_zoom;

private:
    StyleVisualData();
    StyleVisualData(const StyleVisualData&);    
};

//------------------------------------------------
enum EFillBox {
    BorderFillBox, PaddingFillBox, ContentFillBox, TextFillBox
};

enum EFillRepeat {
    RepeatFill, RepeatXFill, RepeatYFill, NoRepeatFill
};

struct LengthSize {
    Length width;
    Length height;
    
    LengthSize()
    {}
    
    LengthSize(const Length& w, const Length& h)
    : width(w)
    , height(h)
    {}
};

enum EFillLayerType {
    BackgroundFillLayer, MaskFillLayer
};

struct FillLayer {
public:
    FillLayer(EFillLayerType);
    ~FillLayer();

    StyleImage* image() const { return m_image.get(); }
    Length xPosition() const { return m_xPosition; }
    Length yPosition() const { return m_yPosition; }
    bool attachment() const { return m_attachment; }
    EFillBox clip() const { return static_cast<EFillBox>(m_clip); }
    EFillBox origin() const { return static_cast<EFillBox>(m_origin); }
    EFillRepeat repeat() const { return static_cast<EFillRepeat>(m_repeat); }
    CompositeOperator composite() const { return static_cast<CompositeOperator>(m_composite); }
    LengthSize size() const { return m_size; }

    const FillLayer* next() const { return m_next; }
    FillLayer* next() { return m_next; }

    bool isImageSet() const { return m_imageSet; }
    bool isXPositionSet() const { return m_xPosSet; }
    bool isYPositionSet() const { return m_yPosSet; }
    bool isAttachmentSet() const { return m_attachmentSet; }
    bool isClipSet() const { return m_clipSet; }
    bool isOriginSet() const { return m_originSet; }
    bool isRepeatSet() const { return m_repeatSet; }
    bool isCompositeSet() const { return m_compositeSet; }
    bool isSizeSet() const { return m_sizeSet; }
    
    void setImage(StyleImage* i) { m_image = i; m_imageSet = true; }
    void setXPosition(const Length& l) { m_xPosition = l; m_xPosSet = true; }
    void setYPosition(const Length& l) { m_yPosition = l; m_yPosSet = true; }
    void setAttachment(bool b) { m_attachment = b; m_attachmentSet = true; }
    void setClip(EFillBox b) { m_clip = b; m_clipSet = true; }
    void setOrigin(EFillBox b) { m_origin = b; m_originSet = true; }
    void setRepeat(EFillRepeat r) { m_repeat = r; m_repeatSet = true; }
    void setComposite(CompositeOperator c) { m_composite = c; m_compositeSet = true; }
    void setSize(const LengthSize& b) { m_size = b; m_sizeSet = true; }
    
    void clearImage() { m_imageSet = false; }
    void clearXPosition() { m_xPosSet = false; }
    void clearYPosition() { m_yPosSet = false; }
    void clearAttachment() { m_attachmentSet = false; }
    void clearClip() { m_clipSet = false; }
    void clearOrigin() { m_originSet = false; }
    void clearRepeat() { m_repeatSet = false; }
    void clearComposite() { m_compositeSet = false; }
    void clearSize() { m_sizeSet = false; }

    void setNext(FillLayer* n) { if (m_next != n) { delete m_next; m_next = n; } }

    FillLayer& operator=(const FillLayer& o);    
    FillLayer(const FillLayer& o);

    bool operator==(const FillLayer& o) const;
    bool operator!=(const FillLayer& o) const {
        return !(*this == o);
    }

    bool containsImage(StyleImage* s) const {
        if (!s)
            return false;
        if (m_image && *s == *m_image)
            return true;
        if (m_next)
            return m_next->containsImage(s);
        return false;
    }
    
    bool hasImage() const {
        if (m_image)
            return true;
        return m_next ? m_next->hasImage() : false;
    }
    bool hasFixedImage() const {
        if (m_image && !m_attachment)
            return true;
        return m_next ? m_next->hasFixedImage() : false;
    }

    EFillLayerType type() const { return static_cast<EFillLayerType>(m_type); }

    void fillUnsetProperties();
    void cullEmptyLayers();

    static bool initialFillAttachment(EFillLayerType) { return true; }
    static EFillBox initialFillClip(EFillLayerType) { return BorderFillBox; }
    static EFillBox initialFillOrigin(EFillLayerType type) { return type == BackgroundFillLayer ? PaddingFillBox : BorderFillBox; }
    static EFillRepeat initialFillRepeat(EFillLayerType) { return RepeatFill; }
    static CompositeOperator initialFillComposite(EFillLayerType) { return CompositeSourceOver; }
    static LengthSize initialFillSize(EFillLayerType) { return LengthSize(); }
    static Length initialFillXPosition(EFillLayerType type) { return Length(0.0, Percent); }
    static Length initialFillYPosition(EFillLayerType type) { return Length(0.0, Percent); }
    static StyleImage* initialFillImage(EFillLayerType) { return 0; }

private:
    FillLayer() { }

public:
    RefPtr<StyleImage> m_image;

    Length m_xPosition;
    Length m_yPosition;

    bool m_attachment : 1;
    unsigned m_clip : 2; // EFillBox
    unsigned m_origin : 2; // EFillBox
    unsigned m_repeat : 2; // EFillRepeat
    unsigned m_composite : 4; // CompositeOperator

    LengthSize m_size;

    bool m_imageSet : 1;
    bool m_attachmentSet : 1;
    bool m_clipSet : 1;
    bool m_originSet : 1;
    bool m_repeatSet : 1;
    bool m_xPosSet : 1;
    bool m_yPosSet : 1;
    bool m_compositeSet : 1;
    bool m_sizeSet : 1;
    
    unsigned m_type : 1; // EFillLayerType

    FillLayer* m_next;
};

class StyleBackgroundData : public RefCounted<StyleBackgroundData> {
public:
    static PassRefPtr<StyleBackgroundData> create() { return adoptRef(new StyleBackgroundData); }
    PassRefPtr<StyleBackgroundData> copy() const { return adoptRef(new StyleBackgroundData(*this)); }
    ~StyleBackgroundData() {}

    bool operator==(const StyleBackgroundData& o) const;
    bool operator!=(const StyleBackgroundData &o) const {
        return !(*this == o);
    }

    FillLayer m_background;
    Color m_color;
    OutlineValue m_outline;
    
private:
    StyleBackgroundData();
    StyleBackgroundData(const StyleBackgroundData& o );    
};

//------------------------------------------------
// CSS3 Marquee Properties

enum EMarqueeBehavior { MNONE, MSCROLL, MSLIDE, MALTERNATE };
enum EMarqueeDirection { MAUTO = 0, MLEFT = 1, MRIGHT = -1, MUP = 2, MDOWN = -2, MFORWARD = 3, MBACKWARD = -3 };

class StyleMarqueeData : public RefCounted<StyleMarqueeData> {
public:
    static PassRefPtr<StyleMarqueeData> create() { return adoptRef(new StyleMarqueeData); }
    PassRefPtr<StyleMarqueeData> copy() const { return adoptRef(new StyleMarqueeData(*this)); }
    
    bool operator==(const StyleMarqueeData& o) const;
    bool operator!=(const StyleMarqueeData& o) const {
        return !(*this == o);
    }
    
    Length increment;
    int speed;
    
    int loops; // -1 means infinite.
    
    unsigned behavior : 2; // EMarqueeBehavior 
    EMarqueeDirection direction : 3; // not unsigned because EMarqueeDirection has negative values
    
private:
    StyleMarqueeData();
    StyleMarqueeData(const StyleMarqueeData&);
};
  
// CSS3 Multi Column Layout

class StyleMultiColData : public RefCounted<StyleMultiColData> {
public:
    static PassRefPtr<StyleMultiColData> create() { return adoptRef(new StyleMultiColData); }
    PassRefPtr<StyleMultiColData> copy() const { return adoptRef(new StyleMultiColData(*this)); }
    
    bool operator==(const StyleMultiColData& o) const;
    bool operator!=(const StyleMultiColData &o) const {
        return !(*this == o);
    }

    unsigned short ruleWidth() const {
        if (m_rule.style() == BNONE || m_rule.style() == BHIDDEN)
            return 0; 
        return m_rule.width;
    }

    float m_width;
    unsigned short m_count;
    float m_gap;
    BorderValue m_rule;
    
    bool m_autoWidth : 1;
    bool m_autoCount : 1;
    bool m_normalGap : 1;
    unsigned m_breakBefore : 2; // EPageBreak
    unsigned m_breakAfter : 2; // EPageBreak
    unsigned m_breakInside : 2; // EPageBreak
    
private:
    StyleMultiColData();
    StyleMultiColData(const StyleMultiColData&);
};

// CSS Transforms (may become part of CSS3)

class TransformOperation : public RefCounted<TransformOperation> {
public:
    virtual ~TransformOperation() { }
    
    virtual bool operator==(const TransformOperation&) const = 0;
    bool operator!=(const TransformOperation& o) const { return !(*this == o); }

    virtual void apply(AffineTransform&, const IntSize& borderBoxSize) = 0;
    
    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false) = 0;
    
    virtual bool isScaleOperation() const { return false; }
    virtual bool isRotateOperation() const { return false; }
    virtual bool isSkewOperation() const { return false; }
    virtual bool isTranslateOperation() const { return false; }
    virtual bool isMatrixOperation() const { return false; }
};

class ScaleTransformOperation : public TransformOperation {
public:
    static PassRefPtr<ScaleTransformOperation> create(double sx, double sy)
    {
        return adoptRef(new ScaleTransformOperation(sx, sy));
    }

    virtual bool isScaleOperation() const { return true; }

    virtual bool operator==(const TransformOperation& o) const
    {
        if (o.isScaleOperation()) {
            const ScaleTransformOperation* s = static_cast<const ScaleTransformOperation*>(&o);
            return m_x == s->m_x && m_y == s->m_y;
        }
        return false;
    }

    virtual void apply(AffineTransform& transform, const IntSize& borderBoxSize)
    {
        transform.scale(m_x, m_y);
    }

    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false);

private:
    ScaleTransformOperation(double sx, double sy)
        : m_x(sx), m_y(sy)
    {
    }
        
    double m_x;
    double m_y;
};

class RotateTransformOperation : public TransformOperation {
public:
    static PassRefPtr<RotateTransformOperation> create(double angle)
    {
        return adoptRef(new RotateTransformOperation(angle));
    }

    virtual bool isRotateOperation() const { return true; }

    virtual bool operator==(const TransformOperation& o) const
    {
        if (o.isRotateOperation()) {
            const RotateTransformOperation* r = static_cast<const RotateTransformOperation*>(&o);
            return m_angle == r->m_angle;
        }
        return false;
    }
    
    virtual void apply(AffineTransform& transform, const IntSize& borderBoxSize)
    {
        transform.rotate(m_angle);
    }

    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false);
    
private:
    RotateTransformOperation(double angle)
        : m_angle(angle)
    {
    }

    double m_angle;
};

class SkewTransformOperation : public TransformOperation {
public:
    static PassRefPtr<SkewTransformOperation> create(double angleX, double angleY)
    {
        return adoptRef(new SkewTransformOperation(angleX, angleY));
    }

    virtual bool isSkewOperation() const { return true; }

    virtual bool operator==(const TransformOperation& o) const
    {
        if (!o.isSkewOperation())
            return false;
        const SkewTransformOperation* s = static_cast<const SkewTransformOperation*>(&o);
        return m_angleX == s->m_angleX && m_angleY == s->m_angleY;
    }

    virtual void apply(AffineTransform& transform, const IntSize& borderBoxSize)
    {
        transform.skew(m_angleX, m_angleY);
    }

    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false);
    
private:
    SkewTransformOperation(double angleX, double angleY)
        : m_angleX(angleX), m_angleY(angleY)
    {
    }
    
    double m_angleX;
    double m_angleY;
};

class TranslateTransformOperation : public TransformOperation {
public:
    static PassRefPtr<TranslateTransformOperation> create(const Length& tx, const Length& ty)
    {
        return adoptRef(new TranslateTransformOperation(tx, ty));
    }

    virtual bool isTranslateOperation() const { return true; }

    virtual bool operator==(const TransformOperation& o) const
    {
        if (!o.isTranslateOperation())
            return false;
        const TranslateTransformOperation* t = static_cast<const TranslateTransformOperation*>(&o);
        return m_x == t->m_x && m_y == t->m_y;
    }

    virtual void apply(AffineTransform& transform, const IntSize& borderBoxSize)
    {
        transform.translate(m_x.calcFloatValue(borderBoxSize.width()), m_y.calcFloatValue(borderBoxSize.height()));
    }

    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false);

private:
    TranslateTransformOperation(const Length& tx, const Length& ty)
        : m_x(tx), m_y(ty)
    {
    }

    Length m_x;
    Length m_y;
};

class MatrixTransformOperation : public TransformOperation {
public:
    static PassRefPtr<MatrixTransformOperation> create(double a, double b, double c, double d, double e, double f)
    {
        return adoptRef(new MatrixTransformOperation(a, b, c, d, e, f));
    }

    virtual bool isMatrixOperation() const { return true; }

    virtual bool operator==(const TransformOperation& o) const
    {
        if (o.isMatrixOperation()) {
            const MatrixTransformOperation* m = static_cast<const MatrixTransformOperation*>(&o);
            return m_a == m->m_a && m_b == m->m_b && m_c == m->m_c && m_d == m->m_d && m_e == m->m_e && m_f == m->m_f;
        }
        return false;
    }

    virtual void apply(AffineTransform& transform, const IntSize& borderBoxSize)
    {
        AffineTransform matrix(m_a, m_b, m_c, m_d, m_e, m_f);
        transform = matrix * transform;
    }

    virtual TransformOperation* blend(const TransformOperation* from, double progress, bool blendToIdentity = false);
    
private:
    MatrixTransformOperation(double a, double b, double c, double d, double e, double f)
        : m_a(a), m_b(b), m_c(c), m_d(d), m_e(e), m_f(f)
    {
    }
    
    double m_a;
    double m_b;
    double m_c;
    double m_d;
    double m_e;
    double m_f;
};

class TransformOperations {
public:
    bool operator==(const TransformOperations&) const;
    bool operator!=(const TransformOperations& o) const {
        return !(*this == o);
    }
    
    bool isEmpty() const { return m_operations.isEmpty(); }
    size_t size() const { return m_operations.size(); }
    const RefPtr<TransformOperation>& operator[](size_t i) const { return m_operations.at(i); }

    void append(const RefPtr<TransformOperation>& op) { return m_operations.append(op); }

private:
    Vector<RefPtr<TransformOperation> > m_operations;
};

class StyleTransformData : public RefCounted<StyleTransformData> {
public:
    static PassRefPtr<StyleTransformData> create() { return adoptRef(new StyleTransformData); }
    PassRefPtr<StyleTransformData> copy() const { return adoptRef(new StyleTransformData(*this)); }

    bool operator==(const StyleTransformData&) const;
    bool operator!=(const StyleTransformData& o) const {
        return !(*this == o);
    }

    TransformOperations m_operations;
    Length m_x;
    Length m_y;

private:
    StyleTransformData();
    StyleTransformData(const StyleTransformData&);
};

//------------------------------------------------
// CSS3 Flexible Box Properties

enum EBoxAlignment { BSTRETCH, BSTART, BCENTER, BEND, BJUSTIFY, BBASELINE };
enum EBoxOrient { HORIZONTAL, VERTICAL };
enum EBoxLines { SINGLE, MULTIPLE };
enum EBoxDirection { BNORMAL, BREVERSE };

class StyleFlexibleBoxData : public RefCounted<StyleFlexibleBoxData> {
public:
    static PassRefPtr<StyleFlexibleBoxData> create() { return adoptRef(new StyleFlexibleBoxData); }
    PassRefPtr<StyleFlexibleBoxData> copy() const { return adoptRef(new StyleFlexibleBoxData(*this)); }
    
    bool operator==(const StyleFlexibleBoxData& o) const;
    bool operator!=(const StyleFlexibleBoxData &o) const {
        return !(*this == o);
    }

    float flex;
    unsigned int flex_group;
    unsigned int ordinal_group;

    unsigned align : 3; // EBoxAlignment
    unsigned pack: 3; // EBoxAlignment
    unsigned orient: 1; // EBoxOrient
    unsigned lines : 1; // EBoxLines
    
private:
    StyleFlexibleBoxData();
    StyleFlexibleBoxData(const StyleFlexibleBoxData&);
};

// This struct holds information about shadows for the text-shadow and box-shadow properties.
struct ShadowData {
    ShadowData(int _x, int _y, int _blur, const Color& _color)
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
    Color color;
    ShadowData* next;
};

#if ENABLE(XBL)
struct BindingURI {
    BindingURI(StringImpl*);
    ~BindingURI();

    BindingURI* copy();

    bool operator==(const BindingURI& o) const;
    bool operator!=(const BindingURI& o) const {
        return !(*this == o);
    }
    
    BindingURI* next() { return m_next; }
    void setNext(BindingURI* n) { m_next = n; }
    
    StringImpl* uri() { return m_uri; }
    
    BindingURI* m_next;
    StringImpl* m_uri;
};
#endif

//------------------------------------------------

enum ETextSecurity {
    TSNONE, TSDISC, TSCIRCLE, TSSQUARE
};

// CSS3 User Modify Properties

enum EUserModify {
    READ_ONLY, READ_WRITE, READ_WRITE_PLAINTEXT_ONLY
};

// CSS3 User Drag Values

enum EUserDrag {
    DRAG_AUTO, DRAG_NONE, DRAG_ELEMENT
};

// CSS3 User Select Values

enum EUserSelect {
    SELECT_NONE, SELECT_TEXT
};

// Word Break Values. Matches WinIE, rather than CSS3

enum EWordBreak {
    NormalWordBreak, BreakAllWordBreak, BreakWordBreak
};

enum EWordWrap {
    NormalWordWrap, BreakWordWrap
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

enum EResize {
    RESIZE_NONE, RESIZE_BOTH, RESIZE_HORIZONTAL, RESIZE_VERTICAL
};

enum EAppearance {
    NoAppearance, CheckboxAppearance, RadioAppearance, PushButtonAppearance, SquareButtonAppearance, ButtonAppearance,
    ButtonBevelAppearance, DefaultButtonAppearance, ListboxAppearance, ListItemAppearance, 
    MediaFullscreenButtonAppearance, MediaMuteButtonAppearance, MediaPlayButtonAppearance,
    MediaSeekBackButtonAppearance, MediaSeekForwardButtonAppearance, MediaSliderAppearance, MediaSliderThumbAppearance,
    MenulistAppearance, MenulistButtonAppearance, MenulistTextAppearance, MenulistTextFieldAppearance,
    ScrollbarButtonUpAppearance, ScrollbarButtonDownAppearance, 
    ScrollbarButtonLeftAppearance, ScrollbarButtonRightAppearance,
    ScrollbarTrackHorizontalAppearance, ScrollbarTrackVerticalAppearance,
    ScrollbarThumbHorizontalAppearance, ScrollbarThumbVerticalAppearance,
    ScrollbarGripperHorizontalAppearance, ScrollbarGripperVerticalAppearance,
    SliderHorizontalAppearance, SliderVerticalAppearance, SliderThumbHorizontalAppearance,
    SliderThumbVerticalAppearance, CaretAppearance, SearchFieldAppearance, SearchFieldDecorationAppearance,
    SearchFieldResultsDecorationAppearance, SearchFieldResultsButtonAppearance,
    SearchFieldCancelButtonAppearance, TextFieldAppearance, TextAreaAppearance
};

enum EListStyleType {
     DISC, CIRCLE, SQUARE, LDECIMAL, DECIMAL_LEADING_ZERO,
     LOWER_ROMAN, UPPER_ROMAN, LOWER_GREEK,
     LOWER_ALPHA, LOWER_LATIN, UPPER_ALPHA, UPPER_LATIN,
     HEBREW, ARMENIAN, GEORGIAN, CJK_IDEOGRAPHIC,
     HIRAGANA, KATAKANA, HIRAGANA_IROHA, KATAKANA_IROHA, LNONE
};

struct CounterDirectives {
    CounterDirectives() : m_reset(false), m_increment(false) { }

    bool m_reset;
    int m_resetValue;
    bool m_increment;
    int m_incrementValue;
};

bool operator==(const CounterDirectives&, const CounterDirectives&);
inline bool operator!=(const CounterDirectives& a, const CounterDirectives& b) { return !(a == b); }

typedef HashMap<RefPtr<AtomicStringImpl>, CounterDirectives> CounterDirectiveMap;

class CounterContent {
public:
    CounterContent(const AtomicString& identifier, EListStyleType style, const AtomicString& separator)
        : m_identifier(identifier), m_listStyle(style), m_separator(separator)
    {
    }

    const AtomicString& identifier() const { return m_identifier; }
    EListStyleType listStyle() const { return m_listStyle; }
    const AtomicString& separator() const { return m_separator; }

private:
    AtomicString m_identifier;
    EListStyleType m_listStyle;
    AtomicString m_separator;
};

enum ContentType {
    CONTENT_NONE, CONTENT_OBJECT, CONTENT_TEXT, CONTENT_COUNTER
};

struct ContentData : Noncopyable {
    ContentData() : m_type(CONTENT_NONE), m_next(0) { }
    ~ContentData() { clear(); }

    void clear();

    ContentType m_type;
    union {
        StyleImage* m_image;
        StringImpl* m_text;
        CounterContent* m_counter;
    } m_content;
    ContentData* m_next;
};

enum EBorderFit { BorderFitBorder, BorderFitLines };

enum ETimingFunctionType { LinearTimingFunction, CubicBezierTimingFunction };

struct TimingFunction
{
    TimingFunction()
    : m_type(CubicBezierTimingFunction)
    , m_x1(.25)
    , m_y1(.1)
    , m_x2(.25)
    , m_y2(1.0)
    {}

    TimingFunction(ETimingFunctionType timingFunction, double x1 = .0, double y1 = .0, double x2 = .0, double y2 = .0)
    : m_type(timingFunction)
    , m_x1(x1)
    , m_y1(y1)
    , m_x2(x2)
    , m_y2(y2)
    {
    }

    bool operator==(const TimingFunction& o) const { return m_type == o.m_type && m_x1 == o.m_x1 && m_y1 == o.m_y1 && m_x2 == o.m_x2 && m_y2 == o.m_y2; }

    double x1() const { return m_x1; }
    double y1() const { return m_y1; }
    double x2() const { return m_x2; }
    double y2() const { return m_y2; }

    ETimingFunctionType type() const { return m_type; }

private:
    ETimingFunctionType m_type;

    double m_x1;
    double m_y1;
    double m_x2;
    double m_y2;
};

enum EAnimPlayState {
    AnimPlayStatePlaying = 0x0,
    AnimPlayStatePaused = 0x1
};

class Animation : public RefCounted<Animation> {
public:

    static PassRefPtr<Animation> create() { return adoptRef(new Animation); };
    
    bool isDelaySet() const { return m_delaySet; }
    bool isDirectionSet() const { return m_directionSet; }
    bool isDurationSet() const { return m_durationSet; }
    bool isIterationCountSet() const { return m_iterationCountSet; }
    bool isNameSet() const { return m_nameSet; }
    bool isPlayStateSet() const { return m_playStateSet; }
    bool isPropertySet() const { return m_propertySet; }
    bool isTimingFunctionSet() const { return m_timingFunctionSet; }

    // Flags this to be the special "none" animation (animation-name: none)
    bool isNoneAnimation() const { return m_isNone; }
    // We can make placeholder Animation objects to keep the comma-separated lists
    // of properties in sync. isValidAnimation means this is not a placeholder.
    bool isValidAnimation() const { return !m_isNone && !m_name.isEmpty(); }

    bool isEmpty() const
    {
        return (!m_directionSet && !m_durationSet && !m_nameSet && !m_playStateSet && 
               !m_iterationCountSet && !m_delaySet && !m_timingFunctionSet && !m_propertySet);
    }

    bool isEmptyOrZeroDuration() const
    {
        return isEmpty() || (m_duration == 0 && m_delay <= 0);
    }

    void clearDelay() { m_delaySet = false; }
    void clearDirection() { m_directionSet = false; }
    void clearDuration() { m_durationSet = false; }
    void clearIterationCount() { m_iterationCountSet = false; }
    void clearName() { m_nameSet = false; }
    void clearPlayState() { m_playStateSet = AnimPlayStatePlaying; }
    void clearProperty() { m_propertySet = false; }
    void clearTimingFunction() { m_timingFunctionSet = false; }

    double delay() const { return m_delay; }
    bool direction() const { return m_direction; }
    double duration() const { return m_duration; }
    int iterationCount() const { return m_iterationCount; }
    const String& name() const { return m_name; }
    unsigned playState() const { return m_playState; }
    int property() const { return m_property; }
    const TimingFunction& timingFunction() const { return m_timingFunction; }

    const RefPtr<KeyframeList>& keyframeList() const { return m_keyframeList; }

    void setDelay(double c) { m_delay = c; m_delaySet = true; }
    void setDirection(bool d) { m_direction = d; m_directionSet = true; }
    void setDuration(double d) { ASSERT(d >= 0); m_duration = d; m_durationSet = true; }
    void setIterationCount(int c) { m_iterationCount = c; m_iterationCountSet = true; }
    void setName(const String& n) { m_name = n; m_nameSet = true; }
    void setPlayState(unsigned d) { m_playState = d; m_playStateSet = true; }
    void setProperty(int t) { m_property = t; m_propertySet = true; }
    void setTimingFunction(const TimingFunction& f) { m_timingFunction = f; m_timingFunctionSet = true; }

    void setIsNoneAnimation(bool n) { m_isNone = n; }

    void setAnimationKeyframe(const RefPtr<KeyframeList> keyframe)  { m_keyframeList = keyframe; }

    Animation& operator=(const Animation& o);

    // return true if all members of this class match (excluding m_next)
    bool animationsMatch(const Animation* t, bool matchPlayStates=true) const;

    // return true every Animation in the chain (defined by m_next) match 
    bool operator==(const Animation& o) const { return animationsMatch(&o); }
    bool operator!=(const Animation& o) const { return !(*this == o); }

private:
    Animation();
    Animation(const Animation& o);
    
    double m_delay;
    bool m_direction;
    double m_duration;
    int m_iterationCount;
    String m_name;
    int m_property;
    TimingFunction m_timingFunction;

    RefPtr<KeyframeList> m_keyframeList;

    unsigned m_playState     : 2;

    bool m_delaySet          : 1;
    bool m_directionSet      : 1;
    bool m_durationSet       : 1;
    bool m_iterationCountSet : 1;
    bool m_nameSet           : 1;
    bool m_playStateSet      : 1;
    bool m_propertySet       : 1;
    bool m_timingFunctionSet : 1;
    
    bool m_isNone            : 1;
};

class AnimationList : public Vector<RefPtr<Animation> >
{
public:
    void fillUnsetProperties();
    bool operator==(const AnimationList& o) const;
};    


class StyleReflection : public RefCounted<StyleReflection> {
public:
    static PassRefPtr<StyleReflection> create()
    {
        return adoptRef(new StyleReflection);
    }

    bool operator==(const StyleReflection& o) const
    {
        return m_direction == o.m_direction && m_offset == o.m_offset && m_mask == o.m_mask;
    }
    bool operator!=(const StyleReflection& o) const { return !(*this == o); }

    CSSReflectionDirection direction() const { return m_direction; }
    Length offset() const { return m_offset; }
    const NinePieceImage& mask() const { return m_mask; }

    void setDirection(CSSReflectionDirection dir) { m_direction = dir; }
    void setOffset(const Length& l) { m_offset = l; }
    void setMask(const NinePieceImage& image) { m_mask = image; }

private:
    StyleReflection()
        : m_direction(ReflectionBelow)
        , m_offset(0, Fixed)
    {
    }
    
    CSSReflectionDirection m_direction;
    Length m_offset;
    NinePieceImage m_mask;
};

// This struct is for rarely used non-inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
class StyleRareNonInheritedData : public RefCounted<StyleRareNonInheritedData> {
public:
    static PassRefPtr<StyleRareNonInheritedData> create() { return adoptRef(new StyleRareNonInheritedData); }
    PassRefPtr<StyleRareNonInheritedData> copy() const { return adoptRef(new StyleRareNonInheritedData(*this)); }
    ~StyleRareNonInheritedData();
    
#if ENABLE(XBL)
    bool bindingsEquivalent(const StyleRareNonInheritedData&) const;
#endif

    bool operator==(const StyleRareNonInheritedData&) const;
    bool operator!=(const StyleRareNonInheritedData& o) const { return !(*this == o); }
 
    bool shadowDataEquivalent(const StyleRareNonInheritedData& o) const;
    bool reflectionDataEquivalent(const StyleRareNonInheritedData& o) const;
    bool animationDataEquivalent(const StyleRareNonInheritedData&) const;
    bool transitionDataEquivalent(const StyleRareNonInheritedData&) const;
    void updateKeyframes(const CSSStyleSelector* styleSelector);

    int lineClamp; // An Apple extension.
#if ENABLE(DASHBOARD_SUPPORT)
    Vector<StyleDashboardRegion> m_dashboardRegions;
#endif
    float opacity; // Whether or not we're transparent.

    DataRef<StyleFlexibleBoxData> flexibleBox; // Flexible box properties 
    DataRef<StyleMarqueeData> marquee; // Marquee properties
    DataRef<StyleMultiColData> m_multiCol; //  CSS3 multicol properties
    DataRef<StyleTransformData> m_transform; // Transform properties (rotate, scale, skew, etc.)

    OwnPtr<ContentData> m_content;
    OwnPtr<CounterDirectiveMap> m_counterDirectives;

    unsigned userDrag : 2; // EUserDrag
    bool textOverflow : 1; // Whether or not lines that spill out should be truncated with "..."
    unsigned marginTopCollapse : 2; // EMarginCollapse
    unsigned marginBottomCollapse : 2; // EMarginCollapse
    unsigned matchNearestMailBlockquoteColor : 1; // EMatchNearestMailBlockquoteColor, FIXME: This property needs to be eliminated. It should never have been added.
    unsigned m_appearance : 6; // EAppearance
    unsigned m_borderFit : 1; // EBorderFit
    OwnPtr<ShadowData> m_boxShadow;  // For box-shadow decorations.
    
    RefPtr<StyleReflection> m_boxReflect;

    OwnPtr<AnimationList> m_animations;
    OwnPtr<AnimationList> m_transitions;

    FillLayer m_mask;
    NinePieceImage m_maskBoxImage;

#if ENABLE(XBL)
    OwnPtr<BindingURI> bindingURI; // The XBL binding URI list.
#endif
    
private:
    StyleRareNonInheritedData();
    StyleRareNonInheritedData(const StyleRareNonInheritedData&);
};

// This struct is for rarely used inherited CSS3, CSS2, and WebKit-specific properties.
// By grouping them together, we save space, and only allocate this object when someone
// actually uses one of these properties.
class StyleRareInheritedData : public RefCounted<StyleRareInheritedData> {
public:
    static PassRefPtr<StyleRareInheritedData> create() { return adoptRef(new StyleRareInheritedData); }
    PassRefPtr<StyleRareInheritedData> copy() const { return adoptRef(new StyleRareInheritedData(*this)); }
    ~StyleRareInheritedData();

    bool operator==(const StyleRareInheritedData& o) const;
    bool operator!=(const StyleRareInheritedData &o) const {
        return !(*this == o);
    }
    bool shadowDataEquivalent(const StyleRareInheritedData&) const;

    Color textStrokeColor;
    float textStrokeWidth;
    Color textFillColor;

    ShadowData* textShadow; // Our text shadow information for shadowed text drawing.
    AtomicString highlight; // Apple-specific extension for custom highlight rendering.
    unsigned textSecurity : 2; // ETextSecurity
    unsigned userModify : 2; // EUserModify (editing)
    unsigned wordBreak : 2; // EWordBreak
    unsigned wordWrap : 1; // EWordWrap 
    unsigned nbspMode : 1; // ENBSPMode
    unsigned khtmlLineBreak : 1; // EKHTMLLineBreak
    bool textSizeAdjust : 1; // An Apple extension.
    unsigned resize : 2; // EResize
    unsigned userSelect : 1;  // EUserSelect
    
private:
    StyleRareInheritedData();
    StyleRareInheritedData(const StyleRareInheritedData&);
};

//------------------------------------------------
// Inherited attributes.
//

enum EWhiteSpace {
    NORMAL, PRE, PRE_WRAP, PRE_LINE, NOWRAP, KHTML_NOWRAP
};

enum ETextAlign {
    TAAUTO, LEFT, RIGHT, CENTER, JUSTIFY, WEBKIT_LEFT, WEBKIT_RIGHT, WEBKIT_CENTER
};

enum ETextTransform {
    CAPITALIZE, UPPERCASE, LOWERCASE, TTNONE
};

enum ETextDecoration {
    TDNONE = 0x0 , UNDERLINE = 0x1, OVERLINE = 0x2, LINE_THROUGH= 0x4, BLINK = 0x8
};

enum EPageBreak {
    PBAUTO, PBALWAYS, PBAVOID
};

class StyleInheritedData : public RefCounted<StyleInheritedData> {
public:
    static PassRefPtr<StyleInheritedData> create() { return adoptRef(new StyleInheritedData); }
    PassRefPtr<StyleInheritedData> copy() const { return adoptRef(new StyleInheritedData(*this)); }
    ~StyleInheritedData();
    
    bool operator==(const StyleInheritedData& o) const;
    bool operator != ( const StyleInheritedData &o ) const {
        return !(*this == o);
    }

    Length indent;
    // could be packed in a short but doesn't
    // make a difference currently because of padding
    Length line_height;

    RefPtr<StyleImage> list_style_image;
    RefPtr<CursorList> cursorData;

    Font font;
    Color color;
    
    float m_effectiveZoom;

    short horizontal_border_spacing;
    short vertical_border_spacing;
    
    // Paged media properties.
    short widows;
    short orphans;
    unsigned page_break_inside : 2; // EPageBreak
    
private:
    StyleInheritedData();
    StyleInheritedData(const StyleInheritedData&);
};


enum EEmptyCell {
    SHOW, HIDE
};

enum ECaptionSide {
    CAPTOP, CAPBOTTOM, CAPLEFT, CAPRIGHT
};

enum EListStylePosition { OUTSIDE, INSIDE };

enum EVisibility { VISIBLE, HIDDEN, COLLAPSE };

enum ECursor {
    CURSOR_AUTO, CURSOR_CROSS, CURSOR_DEFAULT, CURSOR_POINTER, CURSOR_MOVE, CURSOR_VERTICAL_TEXT, CURSOR_CELL, CURSOR_CONTEXT_MENU,
    CURSOR_ALIAS, CURSOR_PROGRESS, CURSOR_NO_DROP, CURSOR_NOT_ALLOWED, CURSOR_WEBKIT_ZOOM_IN, CURSOR_WEBKIT_ZOOM_OUT,
    CURSOR_E_RESIZE, CURSOR_NE_RESIZE, CURSOR_NW_RESIZE, CURSOR_N_RESIZE, CURSOR_SE_RESIZE, CURSOR_SW_RESIZE,
    CURSOR_S_RESIZE, CURSOR_W_RESIZE, CURSOR_EW_RESIZE, CURSOR_NS_RESIZE, CURSOR_NESW_RESIZE, CURSOR_NWSE_RESIZE,
    CURSOR_COL_RESIZE, CURSOR_ROW_RESIZE, CURSOR_TEXT, CURSOR_WAIT, CURSOR_HELP, CURSOR_ALL_SCROLL, 
    CURSOR_COPY, CURSOR_NONE
};

struct CursorData {
    CursorData()
        : cursorImage(0)
    {}
    
    bool operator==(const CursorData& o) const {
        return hotSpot == o.hotSpot && cursorImage == o.cursorImage;
    }
    bool operator!=(const CursorData& o) const { return !(*this == o); }

    IntPoint hotSpot; // for CSS3 support
    CachedImage* cursorImage; // weak pointer, the CSSValueImage takes care of deleting cursorImage
};

class CursorList : public RefCounted<CursorList> {
public:
    static PassRefPtr<CursorList> create()
    {
        return adoptRef(new CursorList);
    }
    
    const CursorData& operator[](int i) const {
        return m_vector[i];
    }

    bool operator==(const CursorList& o) const { return m_vector == o.m_vector; }
    bool operator!=(const CursorList& o) const { return m_vector != o.m_vector; }

    size_t size() const { return m_vector.size(); }
    void append(const CursorData& cursorData) { m_vector.append(cursorData); }

private:
    CursorList() { }

    Vector<CursorData> m_vector;
};

//------------------------------------------------

enum EDisplay {
    INLINE, BLOCK, LIST_ITEM, RUN_IN, COMPACT, INLINE_BLOCK,
    TABLE, INLINE_TABLE, TABLE_ROW_GROUP,
    TABLE_HEADER_GROUP, TABLE_FOOTER_GROUP, TABLE_ROW,
    TABLE_COLUMN_GROUP, TABLE_COLUMN, TABLE_CELL,
    TABLE_CAPTION, BOX, INLINE_BOX, NONE
};

class RenderStyle {
    friend class CSSStyleSelector;

public:
    // static pseudo styles. Dynamic ones are produced on the fly.
    enum PseudoId { NOPSEUDO, FIRST_LINE, FIRST_LETTER, BEFORE, AFTER, SELECTION, FIRST_LINE_INHERITED, FILE_UPLOAD_BUTTON, SLIDER_THUMB, 
                    SEARCH_CANCEL_BUTTON, SEARCH_DECORATION, SEARCH_RESULTS_DECORATION, SEARCH_RESULTS_BUTTON, MEDIA_CONTROLS_PANEL,
                    MEDIA_CONTROLS_PLAY_BUTTON, MEDIA_CONTROLS_MUTE_BUTTON, MEDIA_CONTROLS_TIMELINE, MEDIA_CONTROLS_TIME_DISPLAY,
                    MEDIA_CONTROLS_SEEK_BACK_BUTTON, MEDIA_CONTROLS_SEEK_FORWARD_BUTTON , MEDIA_CONTROLS_FULLSCREEN_BUTTON };
    static const int FIRST_INTERNAL_PSEUDOID = FILE_UPLOAD_BUTTON;

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

// !START SYNC!: Keep this in sync with the copy constructor in RenderStyle.cpp

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

        unsigned _empty_cells : 1; // EEmptyCell 
        unsigned _caption_side : 2; // ECaptionSide
        unsigned _list_style_type : 5 ; // EListStyleType
        unsigned _list_style_position : 1; // EListStylePosition
        unsigned _visibility : 2; // EVisibility
        unsigned _text_align : 3; // ETextAlign
        unsigned _text_transform : 2; // ETextTransform
        unsigned _text_decorations : 4;
        unsigned _cursor_style : 6; // ECursor
        unsigned _direction : 1; // TextDirection
        bool _border_collapse : 1 ;
        unsigned _white_space : 3; // EWhiteSpace
        unsigned _box_direction : 1; // EBoxDirection (CSS3 box_direction property, flexible box layout module)
        
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
            (_overflowX == other._overflowX) &&
            (_overflowY == other._overflowY) &&
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
        
        unsigned _effectiveDisplay : 5; // EDisplay
        unsigned _originalDisplay : 5; // EDisplay
        unsigned _overflowX : 3; // EOverflow
        unsigned _overflowY : 3; // EOverflow
        unsigned _vertical_align : 4; // EVerticalAlign
        unsigned _clear : 2; // EClear
        unsigned _position : 2; // EPosition
        unsigned _floating : 2; // EFloat
        unsigned _table_layout : 1; // ETableLayout
        
        unsigned _page_break_before : 2; // EPageBreak
        unsigned _page_break_after : 2; // EPageBreak

        unsigned _styleType : 5; // PseudoId
        bool _affectedByHover : 1;
        bool _affectedByActive : 1;
        bool _affectedByDrag : 1;
        unsigned _pseudoBits : 6;
        unsigned _unicodeBidi : 2; // EUnicodeBidi
    } noninherited_flags;

// non-inherited attributes
    DataRef<StyleBoxData> box;
    DataRef<StyleVisualData> visual;
    DataRef<StyleBackgroundData> background;
    DataRef<StyleSurroundData> surround;
    DataRef<StyleRareNonInheritedData> rareNonInheritedData;

// inherited attributes
    DataRef<StyleRareInheritedData> rareInheritedData;
    DataRef<StyleInheritedData> inherited;
    
// list of associated pseudo styles
    RenderStyle* pseudoStyle;

    unsigned m_pseudoState : 3; // PseudoState
    bool m_affectedByAttributeSelectors : 1;
    bool m_unique : 1;
    
    // Bits for dynamic child matching.
    bool m_affectedByEmpty : 1;
    bool m_emptyState : 1;
    
    // We optimize for :first-child and :last-child.  The other positional child selectors like nth-child or
    // *-child-of-type, we will just give up and re-evaluate whenever children change at all.
    bool m_childrenAffectedByFirstChildRules : 1;
    bool m_childrenAffectedByLastChildRules  : 1;
    bool m_childrenAffectedByDirectAdjacentRules : 1;
    bool m_childrenAffectedByForwardPositionalRules : 1;
    bool m_childrenAffectedByBackwardPositionalRules : 1;
    bool m_firstChildState : 1;
    bool m_lastChildState : 1;
    unsigned m_childIndex : 18; // Plenty of bits to cache an index.

    int m_ref;
    
#if ENABLE(SVG)
    DataRef<SVGRenderStyle> m_svgStyle;
#endif
    
// !END SYNC!

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
        inherited_flags._visuallyOrdered = initialVisuallyOrdered();
        inherited_flags._htmlHacks=false;
        inherited_flags._box_direction = initialBoxDirection();
        inherited_flags._force_backgrounds_to_white = false;
        
        noninherited_flags._effectiveDisplay = noninherited_flags._originalDisplay = initialDisplay();
        noninherited_flags._overflowX = initialOverflowX();
        noninherited_flags._overflowY = initialOverflowY();
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

    PseudoId styleType() { return  static_cast<PseudoId>(noninherited_flags._styleType); }
    void setStyleType(PseudoId styleType) { noninherited_flags._styleType = styleType; }

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
    bool        hasPadding() const { return surround->padding.nonZero(); }
    bool        hasOffset() const { return surround->offset.nonZero(); }

    bool hasBackground() const { if (backgroundColor().isValid() && backgroundColor().alpha() > 0)
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

    EDisplay    display() const { return static_cast<EDisplay>(noninherited_flags._effectiveDisplay); }
    EDisplay    originalDisplay() const { return static_cast<EDisplay>(noninherited_flags._originalDisplay); }
    
    Length      left() const {  return surround->offset.left; }
    Length      right() const {  return surround->offset.right; }
    Length      top() const {  return surround->offset.top; }
    Length      bottom() const {  return surround->offset.bottom; }

    EPosition   position() const { return  static_cast<EPosition>(noninherited_flags._position); }
    EFloat      floating() const { return  static_cast<EFloat>(noninherited_flags._floating); }

    Length      width() const { return box->width; }
    Length      height() const { return box->height; }
    Length      minWidth() const { return box->min_width; }
    Length      maxWidth() const { return box->max_width; }
    Length      minHeight() const { return box->min_height; }
    Length      maxHeight() const { return box->max_height; }

    const BorderData& border() const { return surround->border; }
    const BorderValue& borderLeft() const { return surround->border.left; }
    const BorderValue& borderRight() const { return surround->border.right; }
    const BorderValue& borderTop() const { return surround->border.top; }
    const BorderValue& borderBottom() const { return surround->border.bottom; }

    const NinePieceImage& borderImage() const { return surround->border.image; }

    const IntSize& borderTopLeftRadius() const { return surround->border.topLeft; }
    const IntSize& borderTopRightRadius() const { return surround->border.topRight; }
    const IntSize& borderBottomLeftRadius() const { return surround->border.bottomLeft; }
    const IntSize& borderBottomRightRadius() const { return surround->border.bottomRight; }
    bool hasBorderRadius() const { return surround->border.hasBorderRadius(); }

    unsigned short  borderLeftWidth() const { return surround->border.borderLeftWidth(); }
    EBorderStyle    borderLeftStyle() const { return surround->border.left.style(); }
    const Color&  borderLeftColor() const { return surround->border.left.color; }
    bool borderLeftIsTransparent() const { return surround->border.left.isTransparent(); }
    unsigned short  borderRightWidth() const { return surround->border.borderRightWidth(); }
    EBorderStyle    borderRightStyle() const {  return surround->border.right.style(); }
    const Color&   borderRightColor() const {  return surround->border.right.color; }
    bool borderRightIsTransparent() const { return surround->border.right.isTransparent(); }
    unsigned short  borderTopWidth() const { return surround->border.borderTopWidth(); }
    EBorderStyle    borderTopStyle() const { return surround->border.top.style(); }
    const Color&  borderTopColor() const {  return surround->border.top.color; }
    bool borderTopIsTransparent() const { return surround->border.top.isTransparent(); }
    unsigned short  borderBottomWidth() const { return surround->border.borderBottomWidth(); }
    EBorderStyle    borderBottomStyle() const {  return surround->border.bottom.style(); }
    const Color&   borderBottomColor() const {  return surround->border.bottom.color; }
    bool borderBottomIsTransparent() const { return surround->border.bottom.isTransparent(); }
    
    unsigned short outlineSize() const { return max(0, outlineWidth() + outlineOffset()); }
    unsigned short outlineWidth() const {
        if (background->m_outline.style() == BNONE)
            return 0;
        return background->m_outline.width;
    }
    bool hasOutline() const { return outlineWidth() > 0 && outlineStyle() > BHIDDEN; }
    EBorderStyle   outlineStyle() const {  return background->m_outline.style(); }
    bool outlineStyleIsAuto() const { return background->m_outline._auto; }
    const Color &  outlineColor() const {  return background->m_outline.color; }

    EOverflow overflowX() const { return static_cast<EOverflow>(noninherited_flags._overflowX); }
    EOverflow overflowY() const { return static_cast<EOverflow>(noninherited_flags._overflowY); }

    EVisibility visibility() const { return static_cast<EVisibility>(inherited_flags._visibility); }
    EVerticalAlign verticalAlign() const { return  static_cast<EVerticalAlign>(noninherited_flags._vertical_align); }
    Length verticalAlignLength() const { return box->vertical_align; }

    Length clipLeft() const { return visual->clip.left; }
    Length clipRight() const { return visual->clip.right; }
    Length clipTop() const { return visual->clip.top; }
    Length clipBottom() const { return visual->clip.bottom; }
    LengthBox clip() const { return visual->clip; }
    bool hasClip() const { return visual->hasClip; }
    
    EUnicodeBidi unicodeBidi() const { return static_cast<EUnicodeBidi>(noninherited_flags._unicodeBidi); }

    EClear clear() const { return static_cast<EClear>(noninherited_flags._clear); }
    ETableLayout tableLayout() const { return static_cast<ETableLayout>(noninherited_flags._table_layout); }

    const Font& font() { return inherited->font; }
    const FontDescription& fontDescription() { return inherited->font.fontDescription(); }
    int fontSize() const { return inherited->font.pixelSize(); }

    const Color & color() const { return inherited->color; }
    Length textIndent() const { return inherited->indent; }
    ETextAlign textAlign() const { return static_cast<ETextAlign>(inherited_flags._text_align); }
    ETextTransform textTransform() const { return static_cast<ETextTransform>(inherited_flags._text_transform); }
    int textDecorationsInEffect() const { return inherited_flags._text_decorations; }
    int textDecoration() const { return visual->textDecoration; }
    int wordSpacing() const { return inherited->font.wordSpacing(); }
    int letterSpacing() const { return inherited->font.letterSpacing(); }

    float zoom() const { return visual->m_zoom; }
    float effectiveZoom() const {  return inherited->m_effectiveZoom; }
    
    TextDirection direction() const { return static_cast<TextDirection>(inherited_flags._direction); }
    Length lineHeight() const { return inherited->line_height; }

    EWhiteSpace whiteSpace() const { return static_cast<EWhiteSpace>(inherited_flags._white_space); }
    static bool autoWrap(EWhiteSpace ws) {
        // Nowrap and pre don't automatically wrap.
        return ws != NOWRAP && ws != PRE;
    }
    bool autoWrap() const {
        return autoWrap(whiteSpace());
    }
    static bool preserveNewline(EWhiteSpace ws) {
        // Normal and nowrap do not preserve newlines.
        return ws != NORMAL && ws != NOWRAP;
    }
    bool preserveNewline() const {
        return preserveNewline(whiteSpace());
    }
    static bool collapseWhiteSpace(EWhiteSpace ws) {
        // Pre and prewrap do not collapse whitespace.
        return ws != PRE && ws != PRE_WRAP;
    }
    bool collapseWhiteSpace() const {
        return collapseWhiteSpace(whiteSpace());
    }
    bool isCollapsibleWhiteSpace(UChar c) const {
        switch (c) {
            case ' ':
            case '\t':
                return collapseWhiteSpace();
            case '\n':
                return !preserveNewline();
        }
        return false;
    }
    bool breakOnlyAfterWhiteSpace() const {
        return whiteSpace() == PRE_WRAP || khtmlLineBreak() == AFTER_WHITE_SPACE;
    }
    bool breakWords() const {
        return wordBreak() == BreakWordBreak || wordWrap() == BreakWordWrap;
    }

    const Color & backgroundColor() const { return background->m_color; }
    StyleImage* backgroundImage() const { return background->m_background.m_image.get(); }
    EFillRepeat backgroundRepeat() const { return static_cast<EFillRepeat>(background->m_background.m_repeat); }
    CompositeOperator backgroundComposite() const { return static_cast<CompositeOperator>(background->m_background.m_composite); }
    bool backgroundAttachment() const { return background->m_background.m_attachment; }
    EFillBox backgroundClip() const { return static_cast<EFillBox>(background->m_background.m_clip); }
    EFillBox backgroundOrigin() const { return static_cast<EFillBox>(background->m_background.m_origin); }
    Length backgroundXPosition() const { return background->m_background.m_xPosition; }
    Length backgroundYPosition() const { return background->m_background.m_yPosition; }
    LengthSize backgroundSize() const { return background->m_background.m_size; }
    FillLayer* accessBackgroundLayers() { return &(background.access()->m_background); }
    const FillLayer* backgroundLayers() const { return &(background->m_background); }

    StyleImage* maskImage() const { return rareNonInheritedData->m_mask.m_image.get(); }
    EFillRepeat maskRepeat() const { return static_cast<EFillRepeat>(rareNonInheritedData->m_mask.m_repeat); }
    CompositeOperator maskComposite() const { return static_cast<CompositeOperator>(rareNonInheritedData->m_mask.m_composite); }
    bool maskAttachment() const { return rareNonInheritedData->m_mask.m_attachment; }
    EFillBox maskClip() const { return static_cast<EFillBox>(rareNonInheritedData->m_mask.m_clip); }
    EFillBox maskOrigin() const { return static_cast<EFillBox>(rareNonInheritedData->m_mask.m_origin); }
    Length maskXPosition() const { return rareNonInheritedData->m_mask.m_xPosition; }
    Length maskYPosition() const { return rareNonInheritedData->m_mask.m_yPosition; }
    LengthSize maskSize() const { return rareNonInheritedData->m_mask.m_size; }
    FillLayer* accessMaskLayers() { return &(rareNonInheritedData.access()->m_mask); }
    const FillLayer* maskLayers() const { return &(rareNonInheritedData->m_mask); }
    const NinePieceImage& maskBoxImage() const { return rareNonInheritedData->m_maskBoxImage; }

    // returns true for collapsing borders, false for separate borders
    bool borderCollapse() const { return inherited_flags._border_collapse; }
    short horizontalBorderSpacing() const { return inherited->horizontal_border_spacing; }
    short verticalBorderSpacing() const { return inherited->vertical_border_spacing; }
    EEmptyCell emptyCells() const { return static_cast<EEmptyCell>(inherited_flags._empty_cells); }
    ECaptionSide captionSide() const { return static_cast<ECaptionSide>(inherited_flags._caption_side); }

    short counterIncrement() const { return visual->counterIncrement; }
    short counterReset() const { return visual->counterReset; }

    EListStyleType listStyleType() const { return static_cast<EListStyleType>(inherited_flags._list_style_type); }
    StyleImage* listStyleImage() const { return inherited->list_style_image.get(); }
    EListStylePosition listStylePosition() const { return static_cast<EListStylePosition>(inherited_flags._list_style_position); }

    Length marginTop() const { return surround->margin.top; }
    Length marginBottom() const {  return surround->margin.bottom; }
    Length marginLeft() const {  return surround->margin.left; }
    Length marginRight() const {  return surround->margin.right; }

    Length paddingTop() const {  return surround->padding.top; }
    Length paddingBottom() const {  return surround->padding.bottom; }
    Length paddingLeft() const { return surround->padding.left; }
    Length paddingRight() const {  return surround->padding.right; }

    ECursor cursor() const { return static_cast<ECursor>(inherited_flags._cursor_style); }
    
    CursorList* cursors() const { return inherited->cursorData.get(); }

    short widows() const { return inherited->widows; }
    short orphans() const { return inherited->orphans; }
    EPageBreak pageBreakInside() const { return static_cast<EPageBreak>(inherited->page_break_inside); }
    EPageBreak pageBreakBefore() const { return static_cast<EPageBreak>(noninherited_flags._page_break_before); }
    EPageBreak pageBreakAfter() const { return static_cast<EPageBreak>(noninherited_flags._page_break_after); }
    
    // CSS3 Getter Methods
#if ENABLE(XBL)
    BindingURI* bindingURIs() const { return rareNonInheritedData->bindingURI; }
#endif
    int outlineOffset() const { 
        if (background->m_outline.style() == BNONE)
            return 0;
        return background->m_outline._offset;
    }
    ShadowData* textShadow() const { return rareInheritedData->textShadow; }
    const Color& textStrokeColor() const { return rareInheritedData->textStrokeColor; }
    float textStrokeWidth() const { return rareInheritedData->textStrokeWidth; }
    const Color& textFillColor() const { return rareInheritedData->textFillColor; }
    float opacity() const { return rareNonInheritedData->opacity; }
    EAppearance appearance() const { return static_cast<EAppearance>(rareNonInheritedData->m_appearance); }
    EBoxAlignment boxAlign() const { return static_cast<EBoxAlignment>(rareNonInheritedData->flexibleBox->align); }
    EBoxDirection boxDirection() const { return static_cast<EBoxDirection>(inherited_flags._box_direction); }
    float boxFlex() { return rareNonInheritedData->flexibleBox->flex; }
    unsigned int boxFlexGroup() const { return rareNonInheritedData->flexibleBox->flex_group; }
    EBoxLines boxLines() { return static_cast<EBoxLines>(rareNonInheritedData->flexibleBox->lines); }
    unsigned int boxOrdinalGroup() const { return rareNonInheritedData->flexibleBox->ordinal_group; }
    EBoxOrient boxOrient() const { return static_cast<EBoxOrient>(rareNonInheritedData->flexibleBox->orient); }
    EBoxAlignment boxPack() const { return static_cast<EBoxAlignment>(rareNonInheritedData->flexibleBox->pack); }
    ShadowData* boxShadow() const { return rareNonInheritedData->m_boxShadow.get(); }
    StyleReflection* boxReflect() const { return rareNonInheritedData->m_boxReflect.get(); }
    EBoxSizing boxSizing() const { return static_cast<EBoxSizing>(box->boxSizing); }
    Length marqueeIncrement() const { return rareNonInheritedData->marquee->increment; }
    int marqueeSpeed() const { return rareNonInheritedData->marquee->speed; }
    int marqueeLoopCount() const { return rareNonInheritedData->marquee->loops; }
    EMarqueeBehavior marqueeBehavior() const { return static_cast<EMarqueeBehavior>(rareNonInheritedData->marquee->behavior); }
    EMarqueeDirection marqueeDirection() const { return static_cast<EMarqueeDirection>(rareNonInheritedData->marquee->direction); }
    EUserModify userModify() const { return static_cast<EUserModify>(rareInheritedData->userModify); }
    EUserDrag userDrag() const { return static_cast<EUserDrag>(rareNonInheritedData->userDrag); }
    EUserSelect userSelect() const { return static_cast<EUserSelect>(rareInheritedData->userSelect); }
    bool textOverflow() const { return rareNonInheritedData->textOverflow; }
    EMarginCollapse marginTopCollapse() const { return static_cast<EMarginCollapse>(rareNonInheritedData->marginTopCollapse); }
    EMarginCollapse marginBottomCollapse() const { return static_cast<EMarginCollapse>(rareNonInheritedData->marginBottomCollapse); }
    EWordBreak wordBreak() const { return static_cast<EWordBreak>(rareInheritedData->wordBreak); }
    EWordWrap wordWrap() const { return static_cast<EWordWrap>(rareInheritedData->wordWrap); }
    ENBSPMode nbspMode() const { return static_cast<ENBSPMode>(rareInheritedData->nbspMode); }
    EKHTMLLineBreak khtmlLineBreak() const { return static_cast<EKHTMLLineBreak>(rareInheritedData->khtmlLineBreak); }
    EMatchNearestMailBlockquoteColor matchNearestMailBlockquoteColor() const { return static_cast<EMatchNearestMailBlockquoteColor>(rareNonInheritedData->matchNearestMailBlockquoteColor); }
    const AtomicString& highlight() const { return rareInheritedData->highlight; }
    EBorderFit borderFit() const { return static_cast<EBorderFit>(rareNonInheritedData->m_borderFit); }
    EResize resize() const { return static_cast<EResize>(rareInheritedData->resize); }
    float columnWidth() const { return rareNonInheritedData->m_multiCol->m_width; }
    bool hasAutoColumnWidth() const { return rareNonInheritedData->m_multiCol->m_autoWidth; }
    unsigned short columnCount() const { return rareNonInheritedData->m_multiCol->m_count; }
    bool hasAutoColumnCount() const { return rareNonInheritedData->m_multiCol->m_autoCount; }
    float columnGap() const { return rareNonInheritedData->m_multiCol->m_gap; }
    bool hasNormalColumnGap() const { return rareNonInheritedData->m_multiCol->m_normalGap; }
    const Color& columnRuleColor() const { return rareNonInheritedData->m_multiCol->m_rule.color; }
    EBorderStyle columnRuleStyle() const { return rareNonInheritedData->m_multiCol->m_rule.style(); }
    unsigned short columnRuleWidth() const { return rareNonInheritedData->m_multiCol->ruleWidth(); }
    bool columnRuleIsTransparent() const { return rareNonInheritedData->m_multiCol->m_rule.isTransparent(); }
    EPageBreak columnBreakBefore() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakBefore); }
    EPageBreak columnBreakInside() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakInside); }
    EPageBreak columnBreakAfter() const { return static_cast<EPageBreak>(rareNonInheritedData->m_multiCol->m_breakAfter); }
    const TransformOperations& transform() const { return rareNonInheritedData->m_transform->m_operations; }
    Length transformOriginX() const { return rareNonInheritedData->m_transform->m_x; }
    Length transformOriginY() const { return rareNonInheritedData->m_transform->m_y; }
    bool hasTransform() const { return !rareNonInheritedData->m_transform->m_operations.isEmpty(); }
    void applyTransform(AffineTransform&, const IntSize& borderBoxSize, bool includeTransformOrigin = true) const;
    bool hasMask() const { return rareNonInheritedData->m_mask.hasImage() || rareNonInheritedData->m_maskBoxImage.hasImage(); }
    // End CSS3 Getters

    // Apple-specific property getter methods
    const AnimationList* animations() const { return rareNonInheritedData->m_animations.get(); }
    const AnimationList* transitions() const { return rareNonInheritedData->m_transitions.get(); }

    AnimationList* accessAnimations();
    AnimationList* accessTransitions();

    bool hasAnimations() const { return rareNonInheritedData->m_animations && rareNonInheritedData->m_animations->size() > 0; }
    bool hasTransitions() const { return rareNonInheritedData->m_transitions && rareNonInheritedData->m_transitions->size() > 0; }

    // return the first found Animation (including 'all' transitions)
    const Animation* transitionForProperty(int property);

    int lineClamp() const { return rareNonInheritedData->lineClamp; }
    bool textSizeAdjust() const { return rareInheritedData->textSizeAdjust; }
    ETextSecurity textSecurity() const { return static_cast<ETextSecurity>(rareInheritedData->textSecurity); }

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

#if ENABLE(DASHBOARD_SUPPORT)
    Vector<StyleDashboardRegion> dashboardRegions() const { return rareNonInheritedData->m_dashboardRegions; }
    void setDashboardRegions(Vector<StyleDashboardRegion> regions) { SET_VAR(rareNonInheritedData,m_dashboardRegions,regions); }
    void setDashboardRegion(int type, const String& label, Length t, Length r, Length b, Length l, bool append) {
        StyleDashboardRegion region;
        region.label = label;
        region.offset.top  = t;
        region.offset.right = r;
        region.offset.bottom = b;
        region.offset.left = l;
        region.type = type;
        if (!append)
            rareNonInheritedData.access()->m_dashboardRegions.clear();
        rareNonInheritedData.access()->m_dashboardRegions.append(region);
    }
#endif

    void resetBorder() { resetBorderImage(); resetBorderTop(); resetBorderRight(); resetBorderBottom(); resetBorderLeft(); resetBorderRadius(); }
    void resetBorderTop() { SET_VAR(surround, border.top, BorderValue()) }
    void resetBorderRight() { SET_VAR(surround, border.right, BorderValue()) }
    void resetBorderBottom() { SET_VAR(surround, border.bottom, BorderValue()) }
    void resetBorderLeft() { SET_VAR(surround, border.left, BorderValue()) }
    void resetBorderImage() { SET_VAR(surround, border.image, NinePieceImage()) }
    void resetBorderRadius() { resetBorderTopLeftRadius(); resetBorderTopRightRadius(); resetBorderBottomLeftRadius(); resetBorderBottomRightRadius(); }
    void resetBorderTopLeftRadius() { SET_VAR(surround, border.topLeft, initialBorderRadius()) }
    void resetBorderTopRightRadius() { SET_VAR(surround, border.topRight, initialBorderRadius()) }
    void resetBorderBottomLeftRadius() { SET_VAR(surround, border.bottomLeft, initialBorderRadius()) }
    void resetBorderBottomRightRadius() { SET_VAR(surround, border.bottomRight, initialBorderRadius()) }
    
    void resetOutline() { SET_VAR(background, m_outline, OutlineValue()) }
    
    void setBackgroundColor(const Color& v)    { SET_VAR(background, m_color, v) }

    void setBorderImage(const NinePieceImage& b)   { SET_VAR(surround, border.image, b) }

    void setBorderTopLeftRadius(const IntSize& s) { SET_VAR(surround, border.topLeft, s) }
    void setBorderTopRightRadius(const IntSize& s) { SET_VAR(surround, border.topRight, s) }
    void setBorderBottomLeftRadius(const IntSize& s) { SET_VAR(surround, border.bottomLeft, s) }
    void setBorderBottomRightRadius(const IntSize& s) { SET_VAR(surround, border.bottomRight, s) }
    void setBorderRadius(const IntSize& s) { 
        setBorderTopLeftRadius(s); setBorderTopRightRadius(s); setBorderBottomLeftRadius(s); setBorderBottomRightRadius(s);
    }

    void setBorderLeftWidth(unsigned short v)   {  SET_VAR(surround, border.left.width, v) }
    void setBorderLeftStyle(EBorderStyle v)     {  SET_VAR(surround, border.left.m_style, v) }
    void setBorderLeftColor(const Color & v)   {  SET_VAR(surround, border.left.color, v) }
    void setBorderRightWidth(unsigned short v)  {  SET_VAR(surround, border.right.width, v) }
    void setBorderRightStyle(EBorderStyle v)    {  SET_VAR(surround, border.right.m_style, v) }
    void setBorderRightColor(const Color & v)  {  SET_VAR(surround, border.right.color, v) }
    void setBorderTopWidth(unsigned short v)    {  SET_VAR(surround, border.top.width, v) }
    void setBorderTopStyle(EBorderStyle v)      {  SET_VAR(surround, border.top.m_style, v) }
    void setBorderTopColor(const Color & v)    {  SET_VAR(surround, border.top.color, v) }    
    void setBorderBottomWidth(unsigned short v) {  SET_VAR(surround, border.bottom.width, v) }
    void setBorderBottomStyle(EBorderStyle v)   {  SET_VAR(surround, border.bottom.m_style, v) }
    void setBorderBottomColor(const Color & v) {  SET_VAR(surround, border.bottom.color, v) }
    void setOutlineWidth(unsigned short v) {  SET_VAR(background, m_outline.width, v) }
    void setOutlineStyle(EBorderStyle v, bool isAuto = false)   
    {  
        SET_VAR(background, m_outline.m_style, v)
        SET_VAR(background, m_outline._auto, isAuto)
    }
    void setOutlineColor(const Color & v) {  SET_VAR(background,m_outline.color,v) }

    void setOverflowX(EOverflow v) { noninherited_flags._overflowX = v; }
    void setOverflowY(EOverflow v) { noninherited_flags._overflowY = v; }
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

    bool setFontDescription(const FontDescription& v) {
        if (inherited->font.fontDescription() != v) {
            inherited.access()->font = Font(v, inherited->font.letterSpacing(), inherited->font.wordSpacing());
            return true;
        }
        return false;
    }

    void setColor(const Color & v) { SET_VAR(inherited,color,v) }
    void setTextIndent(Length v) { SET_VAR(inherited,indent,v) }
    void setTextAlign(ETextAlign v) { inherited_flags._text_align = v; }
    void setTextTransform(ETextTransform v) { inherited_flags._text_transform = v; }
    void addToTextDecorationsInEffect(int v) { inherited_flags._text_decorations |= v; }
    void setTextDecorationsInEffect(int v) { inherited_flags._text_decorations = v; }
    void setTextDecoration(int v) { SET_VAR(visual, textDecoration, v); }
    void setDirection(TextDirection v) { inherited_flags._direction = v; }
    void setLineHeight(Length v) { SET_VAR(inherited,line_height,v) }
    void setZoom(float f) { SET_VAR(visual, m_zoom, f); setEffectiveZoom(effectiveZoom() * zoom()); }
    void setEffectiveZoom(float f) { SET_VAR(inherited, m_effectiveZoom, f) }

    void setWhiteSpace(EWhiteSpace v) { inherited_flags._white_space = v; }

    void setWordSpacing(int v) { inherited.access()->font.setWordSpacing(v); }
    void setLetterSpacing(int v) { inherited.access()->font.setLetterSpacing(v); }

    void clearBackgroundLayers() { background.access()->m_background = FillLayer(BackgroundFillLayer); }
    void inheritBackgroundLayers(const FillLayer& parent) { background.access()->m_background = parent; }
    void adjustBackgroundLayers() {
        if (backgroundLayers()->next()) {
            accessBackgroundLayers()->cullEmptyLayers();
            accessBackgroundLayers()->fillUnsetProperties();
        }
    }

    void clearMaskLayers() { rareNonInheritedData.access()->m_mask = FillLayer(MaskFillLayer); }
    void inheritMaskLayers(const FillLayer& parent) { rareNonInheritedData.access()->m_mask = parent; }
    void adjustMaskLayers() {
        if (maskLayers()->next()) {
            accessMaskLayers()->cullEmptyLayers();
            accessMaskLayers()->fillUnsetProperties();
        }
    }
    void setMaskBoxImage(const NinePieceImage& b)   { SET_VAR(rareNonInheritedData, m_maskBoxImage, b) }

    void setBorderCollapse(bool collapse) { inherited_flags._border_collapse = collapse; }
    void setHorizontalBorderSpacing(short v) { SET_VAR(inherited,horizontal_border_spacing,v) }
    void setVerticalBorderSpacing(short v) { SET_VAR(inherited,vertical_border_spacing,v) }
    void setEmptyCells(EEmptyCell v) { inherited_flags._empty_cells = v; }
    void setCaptionSide(ECaptionSide v) { inherited_flags._caption_side = v; }

    void setCounterIncrement(short v) {  SET_VAR(visual,counterIncrement,v) }
    void setCounterReset(short v) {  SET_VAR(visual,counterReset,v) }

    void setListStyleType(EListStyleType v) { inherited_flags._list_style_type = v; }
    void setListStyleImage(StyleImage* v) { if (inherited->list_style_image != v) inherited.access()->list_style_image = v; }
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
    void addCursor(CachedImage*, const IntPoint& = IntPoint());
    void setCursorList(PassRefPtr<CursorList>);
    void clearCursorList();

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
#if ENABLE(XBL)
    void deleteBindingURIs() { 
        SET_VAR(rareNonInheritedData, bindingURI, (BindingURI*) 0);
    }
    void inheritBindingURIs(BindingURI* other) {
        SET_VAR(rareNonInheritedData, bindingURI, other->copy());
    }
    void addBindingURI(StringImpl* uri);
#endif
    void setOutlineOffset(int v) { SET_VAR(background, m_outline._offset, v) }
    void setTextShadow(ShadowData* val, bool add=false);
    void setTextStrokeColor(const Color& c) { SET_VAR(rareInheritedData, textStrokeColor, c) }
    void setTextStrokeWidth(float w) { SET_VAR(rareInheritedData, textStrokeWidth, w) }
    void setTextFillColor(const Color& c) { SET_VAR(rareInheritedData, textFillColor, c) }
    void setOpacity(float f) { SET_VAR(rareNonInheritedData, opacity, f); }
    void setAppearance(EAppearance a) { SET_VAR(rareNonInheritedData, m_appearance, a); }
    void setBoxAlign(EBoxAlignment a) { SET_VAR(rareNonInheritedData.access()->flexibleBox, align, a); }
    void setBoxDirection(EBoxDirection d) { inherited_flags._box_direction = d; }
    void setBoxFlex(float f) { SET_VAR(rareNonInheritedData.access()->flexibleBox, flex, f); }
    void setBoxFlexGroup(unsigned int fg) { SET_VAR(rareNonInheritedData.access()->flexibleBox, flex_group, fg); }
    void setBoxLines(EBoxLines l) { SET_VAR(rareNonInheritedData.access()->flexibleBox, lines, l); }
    void setBoxOrdinalGroup(unsigned int og) { SET_VAR(rareNonInheritedData.access()->flexibleBox, ordinal_group, og); }
    void setBoxOrient(EBoxOrient o) { SET_VAR(rareNonInheritedData.access()->flexibleBox, orient, o); }
    void setBoxPack(EBoxAlignment p) { SET_VAR(rareNonInheritedData.access()->flexibleBox, pack, p); }
    void setBoxShadow(ShadowData* val, bool add=false);
    void setBoxReflect(const PassRefPtr<StyleReflection>& reflect) { if (rareNonInheritedData->m_boxReflect != reflect) rareNonInheritedData.access()->m_boxReflect = reflect; }
    void setBoxSizing(EBoxSizing s) { SET_VAR(box, boxSizing, s); }
    void setMarqueeIncrement(const Length& f) { SET_VAR(rareNonInheritedData.access()->marquee, increment, f); }
    void setMarqueeSpeed(int f) { SET_VAR(rareNonInheritedData.access()->marquee, speed, f); }
    void setMarqueeDirection(EMarqueeDirection d) { SET_VAR(rareNonInheritedData.access()->marquee, direction, d); }
    void setMarqueeBehavior(EMarqueeBehavior b) { SET_VAR(rareNonInheritedData.access()->marquee, behavior, b); }
    void setMarqueeLoopCount(int i) { SET_VAR(rareNonInheritedData.access()->marquee, loops, i); }
    void setUserModify(EUserModify u) { SET_VAR(rareInheritedData, userModify, u); }
    void setUserDrag(EUserDrag d) { SET_VAR(rareNonInheritedData, userDrag, d); }
    void setUserSelect(EUserSelect s) { SET_VAR(rareInheritedData, userSelect, s); }
    void setTextOverflow(bool b) { SET_VAR(rareNonInheritedData, textOverflow, b); }
    void setMarginTopCollapse(EMarginCollapse c) { SET_VAR(rareNonInheritedData, marginTopCollapse, c); }
    void setMarginBottomCollapse(EMarginCollapse c) { SET_VAR(rareNonInheritedData, marginBottomCollapse, c); }
    void setWordBreak(EWordBreak b) { SET_VAR(rareInheritedData, wordBreak, b); }
    void setWordWrap(EWordWrap b) { SET_VAR(rareInheritedData, wordWrap, b); }
    void setNBSPMode(ENBSPMode b) { SET_VAR(rareInheritedData, nbspMode, b); }
    void setKHTMLLineBreak(EKHTMLLineBreak b) { SET_VAR(rareInheritedData, khtmlLineBreak, b); }
    void setMatchNearestMailBlockquoteColor(EMatchNearestMailBlockquoteColor c)  { SET_VAR(rareNonInheritedData, matchNearestMailBlockquoteColor, c); }
    void setHighlight(const AtomicString& h) { SET_VAR(rareInheritedData, highlight, h); }
    void setBorderFit(EBorderFit b) { SET_VAR(rareNonInheritedData, m_borderFit, b); }
    void setResize(EResize r) { SET_VAR(rareInheritedData, resize, r); }
    void setColumnWidth(float f) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoWidth, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_width, f); }
    void setHasAutoColumnWidth() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoWidth, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_width, 0); }
    void setColumnCount(unsigned short c) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoCount, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_count, c); }
    void setHasAutoColumnCount() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_autoCount, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_count, 0); }
    void setColumnGap(float f) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_normalGap, false); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_gap, f); }
    void setHasNormalColumnGap() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_normalGap, true); SET_VAR(rareNonInheritedData.access()->m_multiCol, m_gap, 0); }
    void setColumnRuleColor(const Color& c) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule.color, c); }
    void setColumnRuleStyle(EBorderStyle b) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule.m_style, b); }
    void setColumnRuleWidth(unsigned short w) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule.width, w); }
    void resetColumnRule() { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_rule, BorderValue()) }
    void setColumnBreakBefore(EPageBreak p) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakBefore, p); }
    void setColumnBreakInside(EPageBreak p) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakInside, p); }
    void setColumnBreakAfter(EPageBreak p) { SET_VAR(rareNonInheritedData.access()->m_multiCol, m_breakAfter, p); }
    void setTransform(const TransformOperations& ops) { SET_VAR(rareNonInheritedData.access()->m_transform, m_operations, ops); }
    void setTransformOriginX(Length l) { SET_VAR(rareNonInheritedData.access()->m_transform, m_x, l); }
    void setTransformOriginY(Length l) { SET_VAR(rareNonInheritedData.access()->m_transform, m_y, l); }
    // End CSS3 Setters

    // Apple-specific property setters
    void clearAnimations()
    {
        rareNonInheritedData.access()->m_animations.clear();
    }
    void clearTransitions()
    {
        rareNonInheritedData.access()->m_transitions.clear();
    }

    void inheritAnimations(const AnimationList* parent) { rareNonInheritedData.access()->m_animations.set(parent ? new AnimationList(*parent) : 0); }
    void inheritTransitions(const AnimationList* parent) { rareNonInheritedData.access()->m_transitions.set(parent ? new AnimationList(*parent) : 0); }
    void adjustAnimations();
    void adjustTransitions();
    void updateKeyframes(const CSSStyleSelector* styleSelector) { if (rareNonInheritedData.get()) rareNonInheritedData.access()->updateKeyframes(styleSelector); }

    void setLineClamp(int c) { SET_VAR(rareNonInheritedData, lineClamp, c); }
    void setTextSizeAdjust(bool b) { SET_VAR(rareInheritedData, textSizeAdjust, b); }
    void setTextSecurity(ETextSecurity aTextSecurity) { SET_VAR(rareInheritedData, textSecurity, aTextSecurity); } 

#if ENABLE(SVG)
    const SVGRenderStyle* svgStyle() const { return m_svgStyle.get(); }
    SVGRenderStyle* accessSVGStyle() { return m_svgStyle.access(); }
#endif

    const ContentData* contentData() const { return rareNonInheritedData->m_content.get(); }
    bool contentDataEquivalent(const RenderStyle* otherStyle) const;
    void clearContent();
    void setContent(StringImpl*, bool add = false);
    void setContent(StyleImage*, bool add = false);
    void setContent(CounterContent*, bool add = false);

    const CounterDirectiveMap* counterDirectives() const;
    CounterDirectiveMap& accessCounterDirectives();

    bool inheritedNotEqual(RenderStyle* other) const;

    // The difference between two styles.  The following values are used:
    // (1) Equal - The two styles are identical
    // (2) Repaint - The object just needs to be repainted.
    // (3) RepaintLayer - The layer and its descendant layers needs to be repainted.
    // (4) Layout - A layout is required.
    enum Diff { Equal, Repaint, RepaintLayer, LayoutPositionedMovementOnly, Layout };
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
    PseudoState pseudoState() const { return static_cast<PseudoState>(m_pseudoState); }
    void setPseudoState(PseudoState s) { m_pseudoState = s; }
    
    // To tell if this style matched attribute selectors. This makes it impossible to share.
    bool affectedByAttributeSelectors() const { return m_affectedByAttributeSelectors; }
    void setAffectedByAttributeSelectors() { m_affectedByAttributeSelectors = true; }

    bool unique() const { return m_unique; }
    void setUnique() { m_unique = true; }
    
    // Methods for indicating the style is affected by dynamic updates (e.g., children changing, our position changing in our sibling list, etc.)
    bool affectedByEmpty() const { return m_affectedByEmpty; }
    bool emptyState() const { return m_emptyState; }
    void setEmptyState(bool b) { m_affectedByEmpty = true; m_unique = true; m_emptyState = b; }
    bool childrenAffectedByPositionalRules() const { return childrenAffectedByForwardPositionalRules() || childrenAffectedByBackwardPositionalRules(); }
    bool childrenAffectedByFirstChildRules() const { return m_childrenAffectedByFirstChildRules; }
    void setChildrenAffectedByFirstChildRules() { m_childrenAffectedByFirstChildRules = true; }
    bool childrenAffectedByLastChildRules() const { return m_childrenAffectedByLastChildRules; }
    void setChildrenAffectedByLastChildRules() { m_childrenAffectedByLastChildRules = true; }
    bool childrenAffectedByDirectAdjacentRules() const { return m_childrenAffectedByDirectAdjacentRules; }
    void setChildrenAffectedByDirectAdjacentRules() { m_childrenAffectedByDirectAdjacentRules = true; }
    bool childrenAffectedByForwardPositionalRules() const { return m_childrenAffectedByForwardPositionalRules; }
    void setChildrenAffectedByForwardPositionalRules() { m_childrenAffectedByForwardPositionalRules = true; }
    bool childrenAffectedByBackwardPositionalRules() const { return m_childrenAffectedByBackwardPositionalRules; }
    void setChildrenAffectedByBackwardPositionalRules() { m_childrenAffectedByBackwardPositionalRules = true; }
    bool firstChildState() const { return m_firstChildState; }
    void setFirstChildState() { m_firstChildState = true; }
    bool lastChildState() const { return m_lastChildState; }
    void setLastChildState() { m_lastChildState = true; }
    unsigned childIndex() const { return m_childIndex; }
    void setChildIndex(unsigned index) { m_childIndex = index; }

    // Initial values for all the properties
    static bool initialBorderCollapse() { return false; }
    static EBorderStyle initialBorderStyle() { return BNONE; }
    static NinePieceImage initialNinePieceImage() { return NinePieceImage(); }
    static IntSize initialBorderRadius() { return IntSize(0,0); }
    static ECaptionSide initialCaptionSide() { return CAPTOP; }
    static EClear initialClear() { return CNONE; }
    static TextDirection initialDirection() { return LTR; }
    static EDisplay initialDisplay() { return INLINE; }
    static EEmptyCell initialEmptyCells() { return SHOW; }
    static EFloat initialFloating() { return FNONE; }
    static EListStylePosition initialListStylePosition() { return OUTSIDE; }
    static EListStyleType initialListStyleType() { return DISC; }
    static EOverflow initialOverflowX() { return OVISIBLE; }
    static EOverflow initialOverflowY() { return OVISIBLE; }
    static EPageBreak initialPageBreak() { return PBAUTO; }
    static EPosition initialPosition() { return StaticPosition; }
    static ETableLayout initialTableLayout() { return TAUTO; }
    static EUnicodeBidi initialUnicodeBidi() { return UBNormal; }
    static ETextTransform initialTextTransform() { return TTNONE; }
    static EVisibility initialVisibility() { return VISIBLE; }
    static EWhiteSpace initialWhiteSpace() { return NORMAL; }
    static short initialHorizontalBorderSpacing() { return 0; }
    static short initialVerticalBorderSpacing() { return 0; }
    static ECursor initialCursor() { return CURSOR_AUTO; }
    static Color initialColor() { return Color::black; }
    static StyleImage* initialListStyleImage() { return 0; }
    static unsigned short initialBorderWidth() { return 3; }
    static int initialLetterWordSpacing() { return 0; }
    static Length initialSize() { return Length(); }
    static Length initialMinSize() { return Length(0, Fixed); }
    static Length initialMaxSize() { return Length(undefinedLength, Fixed); }
    static Length initialOffset() { return Length(); }
    static Length initialMargin() { return Length(Fixed); }
    static Length initialPadding() { return Length(Fixed); }
    static Length initialTextIndent() { return Length(Fixed); }
    static EVerticalAlign initialVerticalAlign() { return BASELINE; }
    static int initialWidows() { return 2; }
    static int initialOrphans() { return 2; }
    static Length initialLineHeight() { return Length(-100.0, Percent); }
    static ETextAlign initialTextAlign() { return TAAUTO; }
    static ETextDecoration initialTextDecoration() { return TDNONE; }
    static float initialZoom() { return 1.0f; }
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
    static StyleReflection* initialBoxReflect() { return 0; }
    static int initialMarqueeLoopCount() { return -1; }
    static int initialMarqueeSpeed() { return 85; }
    static Length initialMarqueeIncrement() { return Length(6, Fixed); }
    static EMarqueeBehavior initialMarqueeBehavior() { return MSCROLL; }
    static EMarqueeDirection initialMarqueeDirection() { return MAUTO; }
    static EUserModify initialUserModify() { return READ_ONLY; }
    static EUserDrag initialUserDrag() { return DRAG_AUTO; }
    static EUserSelect initialUserSelect() { return SELECT_TEXT; }
    static bool initialTextOverflow() { return false; }
    static EMarginCollapse initialMarginTopCollapse() { return MCOLLAPSE; }
    static EMarginCollapse initialMarginBottomCollapse() { return MCOLLAPSE; }
    static EWordBreak initialWordBreak() { return NormalWordBreak; }
    static EWordWrap initialWordWrap() { return NormalWordWrap; }
    static ENBSPMode initialNBSPMode() { return NBNORMAL; }
    static EKHTMLLineBreak initialKHTMLLineBreak() { return LBNORMAL; }
    static EMatchNearestMailBlockquoteColor initialMatchNearestMailBlockquoteColor() { return BCNORMAL; }
    static const AtomicString& initialHighlight() { return nullAtom; }
    static EBorderFit initialBorderFit() { return BorderFitBorder; }
    static EResize initialResize() { return RESIZE_NONE; }
    static EAppearance initialAppearance() { return NoAppearance; }
    static bool initialVisuallyOrdered() { return false; }
    static float initialTextStrokeWidth() { return 0; }
    static unsigned short initialColumnCount() { return 1; }
    static const TransformOperations& initialTransform() { static TransformOperations ops; return ops; }
    static Length initialTransformOriginX() { return Length(50.0, Percent); }
    static Length initialTransformOriginY() { return Length(50.0, Percent); }
    
    // Keep these at the end.
    static float initialAnimationDelay() { return 0; }
    static bool initialAnimationDirection() { return false; }
    static double initialAnimationDuration() { return 0; }
    static int initialAnimationIterationCount() { return 1; }
    static String initialAnimationName() { return String(); }
    static unsigned initialAnimationPlayState() { return AnimPlayStatePlaying; }
    static int initialAnimationProperty() { return cAnimateAll; }
    static TimingFunction initialAnimationTimingFunction() { return TimingFunction(); }
    static int initialLineClamp() { return -1; }
    static bool initialTextSizeAdjust() { return true; }
    static ETextSecurity initialTextSecurity() { return TSNONE; }
#if ENABLE(DASHBOARD_SUPPORT)
    static const Vector<StyleDashboardRegion>& initialDashboardRegions();
    static const Vector<StyleDashboardRegion>& noneDashboardRegions();
#endif
};

class KeyframeValue {
public:
    KeyframeValue() : key(-1) { }
    float key;
    RenderStyle style;
};

class KeyframeList : public RefCounted<KeyframeList> {
public:
    static PassRefPtr<KeyframeList> create(const AtomicString& inAnimName) { return adoptRef(new KeyframeList(inAnimName)); }

    bool operator==(const KeyframeList& o) const;
    bool operator!=(const KeyframeList& o) const { return !(*this == o); }
    
    const AtomicString& animationName() const { return m_animationName; }
    
    void insert(float inKey, const RenderStyle& inStyle);
    
    void addProperty(int prop) { m_properties.add(prop); }
    bool containsProperty(int prop) const { return m_properties.contains(prop); }
    HashSet<int>::const_iterator beginProperties() { return m_properties.begin(); }
    HashSet<int>::const_iterator endProperties() { return m_properties.end(); }
    
    void clear() { m_keyframes.clear(); m_properties.clear(); }
    bool isEmpty() const { return m_keyframes.isEmpty(); }
    Vector<KeyframeValue>::const_iterator beginKeyframes() const { return m_keyframes.begin(); }
    Vector<KeyframeValue>::const_iterator endKeyframes() const { return m_keyframes.end(); }

private:
    KeyframeList(const AtomicString& inAnimName)
    : m_animationName(inAnimName)
    {
        insert(0, RenderStyle());
        insert(1, RenderStyle());
    }
        
protected:
    AtomicString m_animationName;
    Vector<KeyframeValue> m_keyframes;
    HashSet<int> m_properties;       // the properties being animated
};

} // namespace WebCore

#endif
