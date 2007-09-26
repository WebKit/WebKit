/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "RenderStyle.h"

#include "CSSStyleSelector.h"
#include "RenderArena.h"

namespace WebCore {

static RenderStyle* defaultStyle;

StyleSurroundData::StyleSurroundData()
    : margin(Fixed), padding(Auto)
{
}

StyleSurroundData::StyleSurroundData(const StyleSurroundData& o)
    : Shared<StyleSurroundData>()
    , offset(o.offset)
    , margin(o.margin)
    , padding(o.padding)
    , border(o.border)
{
}

bool StyleSurroundData::operator==(const StyleSurroundData& o) const
{
    return offset == o.offset && margin == o.margin && padding == o.padding && border == o.border;
}

StyleBoxData::StyleBoxData()
    : z_index(0), z_auto(true), boxSizing(CONTENT_BOX)
{
    // Initialize our min/max widths/heights.
    min_width = min_height = RenderStyle::initialMinSize();
    max_width = max_height = RenderStyle::initialMaxSize();
}

StyleBoxData::StyleBoxData(const StyleBoxData& o)
    : Shared<StyleBoxData>()
    , width(o.width)
    , height(o.height)
    , min_width(o.min_width)
    , max_width(o.max_width)
    , min_height(o.min_height)
    , max_height(o.max_height)
    , z_index(o.z_index)
    , z_auto(o.z_auto)
    , boxSizing(o.boxSizing)
{
}

bool StyleBoxData::operator==(const StyleBoxData& o) const
{
    return width == o.width &&
           height == o.height &&
           min_width == o.min_width &&
           max_width == o.max_width &&
           min_height == o.min_height &&
           max_height == o.max_height &&
           z_index == o.z_index &&
           z_auto == o.z_auto &&
           boxSizing == o.boxSizing;
}

StyleVisualData::StyleVisualData()
    : hasClip(false)
    , textDecoration(RenderStyle::initialTextDecoration())
    , counterIncrement(0)
    , counterReset(0)
{
}

StyleVisualData::~StyleVisualData()
{
}

StyleVisualData::StyleVisualData(const StyleVisualData& o)
    : Shared<StyleVisualData>()
    , clip(o.clip)
    , hasClip(o.hasClip)
    , textDecoration(o.textDecoration)
    , counterIncrement(o.counterIncrement)
    , counterReset(o.counterReset)
{
}

BackgroundLayer::BackgroundLayer()
    : m_image(RenderStyle::initialBackgroundImage())
    , m_xPosition(RenderStyle::initialBackgroundXPosition())
    , m_yPosition(RenderStyle::initialBackgroundYPosition())
    , m_bgAttachment(RenderStyle::initialBackgroundAttachment())
    , m_bgClip(RenderStyle::initialBackgroundClip())
    , m_bgOrigin(RenderStyle::initialBackgroundOrigin())
    , m_bgRepeat(RenderStyle::initialBackgroundRepeat())
    , m_bgComposite(RenderStyle::initialBackgroundComposite())
    , m_backgroundSize(RenderStyle::initialBackgroundSize())
    , m_imageSet(false)
    , m_attachmentSet(false)
    , m_clipSet(false)
    , m_originSet(false)
    , m_repeatSet(false)
    , m_xPosSet(false)
    , m_yPosSet(false)
    , m_compositeSet(false)
    , m_backgroundSizeSet(false)
    , m_next(0)
{
}

BackgroundLayer::BackgroundLayer(const BackgroundLayer& o)
    : m_image(o.m_image)
    , m_xPosition(o.m_xPosition)
    , m_yPosition(o.m_yPosition)
    , m_bgAttachment(o.m_bgAttachment)
    , m_bgClip(o.m_bgClip)
    , m_bgOrigin(o.m_bgOrigin)
    , m_bgRepeat(o.m_bgRepeat)
    , m_bgComposite(o.m_bgComposite)
    , m_backgroundSize(o.m_backgroundSize)
    , m_imageSet(o.m_imageSet)
    , m_attachmentSet(o.m_attachmentSet)
    , m_clipSet(o.m_clipSet)
    , m_originSet(o.m_originSet)
    , m_repeatSet(o.m_repeatSet)
    , m_xPosSet(o.m_xPosSet)
    , m_yPosSet(o.m_yPosSet)
    , m_compositeSet(o.m_compositeSet)
    , m_backgroundSizeSet(o.m_backgroundSizeSet)
    , m_next(o.m_next ? new BackgroundLayer(*o.m_next) : 0)
{
}

BackgroundLayer::~BackgroundLayer()
{
    delete m_next;
}

BackgroundLayer& BackgroundLayer::operator=(const BackgroundLayer& o)
{
    if (m_next != o.m_next) {
        delete m_next;
        m_next = o.m_next ? new BackgroundLayer(*o.m_next) : 0;
    }

    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_bgAttachment = o.m_bgAttachment;
    m_bgClip = o.m_bgClip;
    m_bgComposite = o.m_bgComposite;
    m_bgOrigin = o.m_bgOrigin;
    m_bgRepeat = o.m_bgRepeat;
    m_backgroundSize = o.m_backgroundSize;

    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_clipSet = o.m_clipSet;
    m_compositeSet = o.m_compositeSet;
    m_originSet = o.m_originSet;
    m_repeatSet = o.m_repeatSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
    m_backgroundSizeSet = o.m_backgroundSizeSet;

    return *this;
}

bool BackgroundLayer::operator==(const BackgroundLayer& o) const
{
    // We do not check the "isSet" booleans for each property, since those are only used during initial construction
    // to propagate patterns into layers.  All layer comparisons happen after values have all been filled in anyway.
    return m_image == o.m_image && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition &&
           m_bgAttachment == o.m_bgAttachment && m_bgClip == o.m_bgClip && 
           m_bgComposite == o.m_bgComposite && m_bgOrigin == o.m_bgOrigin && m_bgRepeat == o.m_bgRepeat &&
           m_backgroundSize.width == o.m_backgroundSize.width && m_backgroundSize.height == o.m_backgroundSize.height && 
           ((m_next && o.m_next) ? *m_next == *o.m_next : m_next == o.m_next);
}

void BackgroundLayer::fillUnsetProperties()
{
    BackgroundLayer* curr;
    for (curr = this; curr && curr->isBackgroundImageSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_image = pattern->m_image;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundXPositionSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_xPosition = pattern->m_xPosition;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundYPositionSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_yPosition = pattern->m_yPosition;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundAttachmentSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgAttachment = pattern->m_bgAttachment;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundClipSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgClip = pattern->m_bgClip;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundCompositeSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgComposite = pattern->m_bgComposite;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundOriginSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgOrigin = pattern->m_bgOrigin;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }

    for (curr = this; curr && curr->isBackgroundRepeatSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_bgRepeat = pattern->m_bgRepeat;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
    
    for (curr = this; curr && curr->isBackgroundSizeSet(); curr = curr->next());
    if (curr && curr != this) {
        // We need to fill in the remaining values with the pattern specified.
        for (BackgroundLayer* pattern = this; curr; curr = curr->next()) {
            curr->m_backgroundSize = pattern->m_backgroundSize;
            pattern = pattern->next();
            if (pattern == curr || !pattern)
                pattern = this;
        }
    }
}

void BackgroundLayer::cullEmptyLayers()
{
    BackgroundLayer *next;
    for (BackgroundLayer *p = this; p; p = next) {
        next = p->m_next;
        if (next && !next->isBackgroundImageSet() &&
            !next->isBackgroundXPositionSet() && !next->isBackgroundYPositionSet() &&
            !next->isBackgroundAttachmentSet() && !next->isBackgroundClipSet() &&
            !next->isBackgroundCompositeSet() && !next->isBackgroundOriginSet() &&
            !next->isBackgroundRepeatSet() && !next->isBackgroundSizeSet()) {
            delete next;
            p->m_next = 0;
            break;
        }
    }
}

StyleBackgroundData::StyleBackgroundData()
{
}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& o)
    : Shared<StyleBackgroundData>(), m_background(o.m_background), m_outline(o.m_outline)
{
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& o) const
{
    return m_background == o.m_background && m_color == o.m_color && m_outline == o.m_outline;
}

StyleMarqueeData::StyleMarqueeData()
    : increment(RenderStyle::initialMarqueeIncrement())
    , speed(RenderStyle::initialMarqueeSpeed())
    , loops(RenderStyle::initialMarqueeLoopCount())
    , behavior(RenderStyle::initialMarqueeBehavior())
    , direction(RenderStyle::initialMarqueeDirection())
{
}

StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData& o)
    : Shared<StyleMarqueeData>()
    , increment(o.increment)
    , speed(o.speed)
    , loops(o.loops)
    , behavior(o.behavior)
    , direction(o.direction) 
{
}

bool StyleMarqueeData::operator==(const StyleMarqueeData& o) const
{
    return increment == o.increment && speed == o.speed && direction == o.direction &&
           behavior == o.behavior && loops == o.loops;
}

StyleFlexibleBoxData::StyleFlexibleBoxData()
    : flex(RenderStyle::initialBoxFlex())
    , flex_group(RenderStyle::initialBoxFlexGroup())
    , ordinal_group(RenderStyle::initialBoxOrdinalGroup())
    , align(RenderStyle::initialBoxAlign())
    , pack(RenderStyle::initialBoxPack())
    , orient(RenderStyle::initialBoxOrient())
    , lines(RenderStyle::initialBoxLines())
{
}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
    : Shared<StyleFlexibleBoxData>()
    , flex(o.flex)
    , flex_group(o.flex_group)
    , ordinal_group(o.ordinal_group)
    , align(o.align)
    , pack(o.pack)
    , orient(o.orient)
    , lines(o.lines)
{
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return flex == o.flex && flex_group == o.flex_group &&
           ordinal_group == o.ordinal_group && align == o.align &&
           pack == o.pack && orient == o.orient && lines == o.lines;
}

StyleMultiColData::StyleMultiColData()
    : m_width(0)
    , m_count(RenderStyle::initialColumnCount())
    , m_gap(0)
    , m_autoWidth(true)
    , m_autoCount(true)
    , m_normalGap(true)
    , m_breakBefore(RenderStyle::initialPageBreak())
    , m_breakAfter(RenderStyle::initialPageBreak())
    , m_breakInside(RenderStyle::initialPageBreak())
{}

StyleMultiColData::StyleMultiColData(const StyleMultiColData& o)
    : Shared<StyleMultiColData>()
    , m_width(o.m_width)
    , m_count(o.m_count)
    , m_gap(o.m_gap)
    , m_rule(o.m_rule)
    , m_autoWidth(o.m_autoWidth)
    , m_autoCount(o.m_autoCount)
    , m_normalGap(o.m_normalGap)
    , m_breakBefore(o.m_breakBefore)
    , m_breakAfter(o.m_breakAfter)
    , m_breakInside(o.m_breakInside)
{}

bool StyleMultiColData::operator==(const StyleMultiColData& o) const
{
    return m_width == o.m_width && m_count == o.m_count && m_gap == o.m_gap &&
           m_rule == o.m_rule && m_breakBefore == o.m_breakBefore && 
           m_autoWidth == o.m_autoWidth && m_autoCount == o.m_autoCount && m_normalGap == o.m_normalGap &&
           m_breakAfter == o.m_breakAfter && m_breakInside == o.m_breakInside;
}

StyleRareNonInheritedData::StyleRareNonInheritedData()
    : lineClamp(RenderStyle::initialLineClamp())
    , opacity(RenderStyle::initialOpacity())
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(RenderStyle::initialUserDrag())
    , textOverflow(RenderStyle::initialTextOverflow())
    , marginTopCollapse(MCOLLAPSE)
    , marginBottomCollapse(MCOLLAPSE)
    , matchNearestMailBlockquoteColor(RenderStyle::initialMatchNearestMailBlockquoteColor())
    , m_appearance(RenderStyle::initialAppearance())
    , m_borderFit(RenderStyle::initialBorderFit())
    , m_boxShadow(0)
#if ENABLE(XBL)
    , bindingURI(0)
#endif
{
}

StyleRareNonInheritedData::StyleRareNonInheritedData(const StyleRareNonInheritedData& o)
    : Shared<StyleRareNonInheritedData>()
    , lineClamp(o.lineClamp)
    , opacity(o.opacity)
    , flexibleBox(o.flexibleBox)
    , marquee(o.marquee)
    , m_multiCol(o.m_multiCol)
    , m_content(0)
    , m_counterDirectives(0)
    , userDrag(o.userDrag)
    , textOverflow(o.textOverflow)
    , marginTopCollapse(o.marginTopCollapse)
    , marginBottomCollapse(o.marginBottomCollapse)
    , matchNearestMailBlockquoteColor(o.matchNearestMailBlockquoteColor)
    , m_appearance(o.m_appearance)
    , m_borderFit(o.m_borderFit)
    , m_boxShadow(o.m_boxShadow ? new ShadowData(*o.m_boxShadow) : 0)
#if ENABLE(XBL)
    , bindingURI(o.bindingURI ? o.bindingURI->copy() : 0)
#endif
{
}

StyleRareNonInheritedData::~StyleRareNonInheritedData()
{
    delete m_content;
    delete m_counterDirectives;
    delete m_boxShadow;
#if ENABLE(XBL)
    delete bindingURI;
#endif
}

#if ENABLE(XBL)
bool StyleRareNonInheritedData::bindingsEquivalent(const StyleRareNonInheritedData& o) const
{
    if (this == &o) return true;
    if (!bindingURI && o.bindingURI || bindingURI && !o.bindingURI)
        return false;
    if (bindingURI && o.bindingURI && (*bindingURI != *o.bindingURI))
        return false;
    return true;
}
#endif

bool StyleRareNonInheritedData::operator==(const StyleRareNonInheritedData& o) const
{
    return lineClamp == o.lineClamp
        && m_dashboardRegions == o.m_dashboardRegions
        && opacity == o.opacity
        && flexibleBox == o.flexibleBox
        && marquee == o.marquee
        && m_multiCol == o.m_multiCol
        && m_content == o.m_content
        && m_counterDirectives == o.m_counterDirectives
        && userDrag == o.userDrag
        && textOverflow == o.textOverflow
        && marginTopCollapse == o.marginTopCollapse
        && marginBottomCollapse == o.marginBottomCollapse
        && matchNearestMailBlockquoteColor == o.matchNearestMailBlockquoteColor
        && m_appearance == o.m_appearance
        && m_borderFit == o.m_borderFit
        && shadowDataEquivalent(o)
#if ENABLE(XBL)
        && bindingsEquivalent(o)
#endif
        ;
}

bool StyleRareNonInheritedData::shadowDataEquivalent(const StyleRareNonInheritedData& o) const
{
    if (!m_boxShadow && o.m_boxShadow || m_boxShadow && !o.m_boxShadow)
        return false;
    if (m_boxShadow && o.m_boxShadow && (*m_boxShadow != *o.m_boxShadow))
        return false;
    return true;
}

StyleRareInheritedData::StyleRareInheritedData()
    : textStrokeWidth(RenderStyle::initialTextStrokeWidth())
    , textShadow(0)
    , textSecurity(RenderStyle::initialTextSecurity())
    , userModify(READ_ONLY)
    , wordBreak(RenderStyle::initialWordBreak())
    , wordWrap(RenderStyle::initialWordWrap())
    , nbspMode(NBNORMAL)
    , khtmlLineBreak(LBNORMAL)
    , textSizeAdjust(RenderStyle::initialTextSizeAdjust())
    , resize(RenderStyle::initialResize())
    , userSelect(RenderStyle::initialUserSelect())
{
}

StyleRareInheritedData::StyleRareInheritedData(const StyleRareInheritedData& o)
    : Shared<StyleRareInheritedData>()
    , textStrokeColor(o.textStrokeColor)
    , textStrokeWidth(o.textStrokeWidth)
    , textFillColor(o.textFillColor)
    , textShadow(o.textShadow ? new ShadowData(*o.textShadow) : 0)
    , highlight(o.highlight)
    , textSecurity(o.textSecurity)
    , userModify(o.userModify)
    , wordBreak(o.wordBreak)
    , wordWrap(o.wordWrap)
    , nbspMode(o.nbspMode)
    , khtmlLineBreak(o.khtmlLineBreak)
    , textSizeAdjust(o.textSizeAdjust)
    , resize(o.resize)
    , userSelect(o.userSelect)
{
}

StyleRareInheritedData::~StyleRareInheritedData()
{
    delete textShadow;
}

bool StyleRareInheritedData::operator==(const StyleRareInheritedData& o) const
{
    return textStrokeColor == o.textStrokeColor
        && textStrokeWidth == o.textStrokeWidth
        && textFillColor == o.textFillColor
        && shadowDataEquivalent(o)
        && highlight == o.highlight
        && textSecurity == o.textSecurity
        && userModify == o.userModify
        && wordBreak == o.wordBreak
        && wordWrap == o.wordWrap
        && nbspMode == o.nbspMode
        && khtmlLineBreak == o.khtmlLineBreak
        && textSizeAdjust == o.textSizeAdjust
        && userSelect == o.userSelect;
}

bool StyleRareInheritedData::shadowDataEquivalent(const StyleRareInheritedData& o) const
{
    if (!textShadow && o.textShadow || textShadow && !o.textShadow)
        return false;
    if (textShadow && o.textShadow && (*textShadow != *o.textShadow))
        return false;
    return true;
}

StyleInheritedData::StyleInheritedData()
    : indent(RenderStyle::initialTextIndent()), line_height(RenderStyle::initialLineHeight()), 
      style_image(RenderStyle::initialListStyleImage()),
      color(RenderStyle::initialColor()), 
      horizontal_border_spacing(RenderStyle::initialHorizontalBorderSpacing()), 
      vertical_border_spacing(RenderStyle::initialVerticalBorderSpacing()),
      widows(RenderStyle::initialWidows()), orphans(RenderStyle::initialOrphans()),
      page_break_inside(RenderStyle::initialPageBreak())
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o)
    : Shared<StyleInheritedData>(),
      indent( o.indent ), line_height( o.line_height ), style_image( o.style_image ),
      cursorData(o.cursorData),
      font( o.font ), color( o.color ),
      horizontal_border_spacing( o.horizontal_border_spacing ),
      vertical_border_spacing( o.vertical_border_spacing ),
      widows(o.widows), orphans(o.orphans), page_break_inside(o.page_break_inside)
{
}

static bool cursorDataEqvuialent(const CursorList* c1, const CursorList* c2)
{
    if (c1 == c2)
        return true;
    if (!c1 && c2 || c1 && !c2)
        return false;
    return (*c1 == *c2);
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
        indent == o.indent &&
        line_height == o.line_height &&
        style_image == o.style_image &&
        cursorDataEqvuialent(cursorData.get(), o.cursorData.get()) &&
        font == o.font &&
        color == o.color &&
        horizontal_border_spacing == o.horizontal_border_spacing &&
        vertical_border_spacing == o.vertical_border_spacing &&
        widows == o.widows &&
        orphans == o.orphans &&
        page_break_inside == o.page_break_inside;
}

bool CursorList::operator==(const CursorList& other) const
{
    // If the lists aren't the same size, then they can't be equivalent.
    if (size() != other.size())
        return false;
        
    for (unsigned i = 0; i < size(); i++) {
        if (m_vector[i] != other.m_vector[i])
            return false;
    }
    
    return true;
}


static inline bool operator!=(const CounterContent& a, const CounterContent& b)
{
    return a.identifier() != b.identifier()
        || a.listStyle() != b.listStyle()
        || a.separator() != b.separator();
}

// ----------------------------------------------------------

void* RenderStyle::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderStyle::operator delete(void* ptr, size_t sz)
{
    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

void RenderStyle::arenaDelete(RenderArena *arena)
{
    RenderStyle *ps = pseudoStyle;
    RenderStyle *prev = 0;
    
    while (ps) {
        prev = ps;
        ps = ps->pseudoStyle;
        // to prevent a double deletion.
        // this works only because the styles below aren't really shared
        // Dirk said we need another construct as soon as these are shared
        prev->pseudoStyle = 0;
        prev->deref(arena);
    }
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)this, this);
}

inline RenderStyle *initDefaultStyle()
{
    if (!defaultStyle)
        defaultStyle = ::new RenderStyle(true);
    return defaultStyle;
}

RenderStyle::RenderStyle()
    : box(initDefaultStyle()->box)
    , visual(defaultStyle->visual)
    , background(defaultStyle->background)
    , surround(defaultStyle->surround)
    , rareNonInheritedData(defaultStyle->rareNonInheritedData)
    , rareInheritedData(defaultStyle->rareInheritedData)
    , inherited(defaultStyle->inherited)
    , pseudoStyle(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(0)
#if ENABLE(SVG)
    , m_svgStyle(defaultStyle->m_svgStyle)
#endif
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
}

RenderStyle::RenderStyle(bool)
    : pseudoStyle(0)
    , m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(1)
{
    setBitDefaults();

    box.init();
    visual.init();
    background.init();
    surround.init();
    rareNonInheritedData.init();
    rareNonInheritedData.access()->flexibleBox.init();
    rareNonInheritedData.access()->marquee.init();
    rareNonInheritedData.access()->m_multiCol.init();
    rareInheritedData.init();
    inherited.init();
    
#if ENABLE(SVG)
    m_svgStyle.init();
#endif
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
    , box(o.box)
    , visual(o.visual)
    , background(o.background)
    , surround(o.surround)
    , rareNonInheritedData(o.rareNonInheritedData)
    , rareInheritedData(o.rareInheritedData)
    , inherited(o.inherited)
    , pseudoStyle(0)
    , m_pseudoState(o.m_pseudoState)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_ref(0)
#if ENABLE(SVG)
    , m_svgStyle(o.m_svgStyle)
#endif
{
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent)
{
    rareInheritedData = inheritParent->rareInheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
#if ENABLE(SVG)
    if (m_svgStyle != inheritParent->m_svgStyle)
        m_svgStyle.access()->inheritFrom(inheritParent->m_svgStyle.get());
#endif
}

RenderStyle::~RenderStyle()
{
}

bool RenderStyle::operator==(const RenderStyle& o) const
{
    // compare everything except the pseudoStyle pointer
    return inherited_flags == o.inherited_flags &&
            noninherited_flags == o.noninherited_flags &&
            box == o.box &&
            visual == o.visual &&
            background == o.background &&
            surround == o.surround &&
            rareNonInheritedData == o.rareNonInheritedData &&
            rareInheritedData == o.rareInheritedData &&
            inherited == o.inherited
#if ENABLE(SVG)
            && m_svgStyle == o.m_svgStyle
#endif
            ;
}

bool RenderStyle::isStyleAvailable() const
{
    return this != CSSStyleSelector::styleNotYetAvailable;
}

enum EPseudoBit { NO_BIT = 0x0, BEFORE_BIT = 0x1, AFTER_BIT = 0x2, FIRST_LINE_BIT = 0x4,
                  FIRST_LETTER_BIT = 0x8, SELECTION_BIT = 0x10, FIRST_LINE_INHERITED_BIT = 0x20,
                  FILE_UPLOAD_BUTTON_BIT = 0x40, SLIDER_THUMB_BIT = 0x80, SEARCH_CANCEL_BUTTON_BIT = 0x100, SEARCH_DECORATION_BIT = 0x200, 
                  SEARCH_RESULTS_DECORATION_BIT = 0x400, SEARCH_RESULTS_BUTTON_BIT = 0x800 };

static inline int pseudoBit(RenderStyle::PseudoId pseudo)
{
    switch (pseudo) {
        case RenderStyle::BEFORE:
            return BEFORE_BIT;
        case RenderStyle::AFTER:
            return AFTER_BIT;
        case RenderStyle::FIRST_LINE:
            return FIRST_LINE_BIT;
        case RenderStyle::FIRST_LETTER:
            return FIRST_LETTER_BIT;
        case RenderStyle::SELECTION:
            return SELECTION_BIT;
        case RenderStyle::FIRST_LINE_INHERITED:
            return FIRST_LINE_INHERITED_BIT;
        case RenderStyle::FILE_UPLOAD_BUTTON:
            return FILE_UPLOAD_BUTTON_BIT;
        case RenderStyle::SLIDER_THUMB:
            return SLIDER_THUMB_BIT;
        case RenderStyle::SEARCH_CANCEL_BUTTON:
            return SEARCH_CANCEL_BUTTON_BIT;        
        case RenderStyle::SEARCH_DECORATION:
            return SEARCH_DECORATION_BIT;
        case RenderStyle::SEARCH_RESULTS_DECORATION:
            return SEARCH_RESULTS_DECORATION_BIT;
        case RenderStyle::SEARCH_RESULTS_BUTTON:
            return SEARCH_RESULTS_BUTTON_BIT;
        default:
            return NO_BIT;
    }
}

bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    return pseudoBit(pseudo) & noninherited_flags._pseudoBits;
}

void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    noninherited_flags._pseudoBits |= pseudoBit(pseudo);
}

RenderStyle* RenderStyle::getPseudoStyle(PseudoId pid)
{
    if (!pseudoStyle || styleType() != NOPSEUDO)
        return 0;
    RenderStyle* ps = pseudoStyle;
    while (ps && ps->styleType() != pid)
        ps = ps->pseudoStyle;
    return ps;
}

void RenderStyle::addPseudoStyle(RenderStyle* pseudo)
{
    if (!pseudo)
        return;
    pseudo->ref();
    pseudo->pseudoStyle = pseudoStyle;
    pseudoStyle = pseudo;
}

bool RenderStyle::inheritedNotEqual(RenderStyle* other) const
{
    return inherited_flags != other->inherited_flags ||
           inherited != other->inherited ||
#if ENABLE(SVG)
           m_svgStyle->inheritedNotEqual(other->m_svgStyle.get()) ||
#endif
           rareInheritedData != other->rareInheritedData;
}

/*
  compares two styles. The result gives an idea of the action that
  needs to be taken when replacing the old style with a new one.

  CbLayout: The containing block of the object needs a relayout.
  Layout: the RenderObject needs a relayout after the style change
  Visible: The change is visible, but no relayout is needed
  NonVisible: The object does need neither repaint nor relayout after
       the change.

  ### TODO:
  A lot can be optimised here based on the display type, lots of
  optimisations are unimplemented, and currently result in the
  worst case result causing a relayout of the containing block.
*/
RenderStyle::Diff RenderStyle::diff(const RenderStyle* other) const
{
#if ENABLE(SVG)
    // This is horribly inefficient.  Eventually we'll have to integrate
    // this more directly by calling: Diff svgDiff = svgStyle->diff(other)
    // and then checking svgDiff and returning from the appropriate places below.
    if (m_svgStyle != other->m_svgStyle)
        return Layout;
#endif

    if (box->width != other->box->width ||
        box->min_width != other->box->min_width ||
        box->max_width != other->box->max_width ||
        box->height != other->box->height ||
        box->min_height != other->box->min_height ||
        box->max_height != other->box->max_height)
        return Layout;
    
    if (box->vertical_align != other->box->vertical_align || noninherited_flags._vertical_align != other->noninherited_flags._vertical_align)
        return Layout;
    
    if (box->boxSizing != other->box->boxSizing)
        return Layout;
    
    if (surround->margin != other->surround->margin)
        return Layout;
        
    if (surround->padding != other->surround->padding)
        return Layout;
    
    if (rareNonInheritedData.get() != other->rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_appearance != other->rareNonInheritedData->m_appearance ||
            rareNonInheritedData->marginTopCollapse != other->rareNonInheritedData->marginTopCollapse ||
            rareNonInheritedData->marginBottomCollapse != other->rareNonInheritedData->marginBottomCollapse ||
            rareNonInheritedData->lineClamp != other->rareNonInheritedData->lineClamp ||
            rareNonInheritedData->textOverflow != other->rareNonInheritedData->textOverflow)
            return Layout;
        
        if (rareNonInheritedData->flexibleBox.get() != other->rareNonInheritedData->flexibleBox.get() &&
            *rareNonInheritedData->flexibleBox.get() != *other->rareNonInheritedData->flexibleBox.get())
            return Layout;
        
        if (!rareNonInheritedData->shadowDataEquivalent(*other->rareNonInheritedData.get()))
            return Layout;

        if (rareNonInheritedData->m_multiCol.get() != other->rareNonInheritedData->m_multiCol.get() &&
            *rareNonInheritedData->m_multiCol.get() != *other->rareNonInheritedData->m_multiCol.get())
            return Layout;
            
        // If regions change trigger a relayout to re-calc regions.
        if (rareNonInheritedData->m_dashboardRegions != other->rareNonInheritedData->m_dashboardRegions)
            return Layout;
    }

    if (rareInheritedData.get() != other->rareInheritedData.get()) {
        if (rareInheritedData->highlight != other->rareInheritedData->highlight ||
            rareInheritedData->textSizeAdjust != other->rareInheritedData->textSizeAdjust ||
            rareInheritedData->wordBreak != other->rareInheritedData->wordBreak ||
            rareInheritedData->wordWrap != other->rareInheritedData->wordWrap ||
            rareInheritedData->nbspMode != other->rareInheritedData->nbspMode ||
            rareInheritedData->khtmlLineBreak != other->rareInheritedData->khtmlLineBreak ||
            rareInheritedData->textSecurity != other->rareInheritedData->textSecurity)
            return Layout;
        
        if (!rareInheritedData->shadowDataEquivalent(*other->rareInheritedData.get()))
            return Layout;
            
        if (textStrokeWidth() != other->textStrokeWidth())
            return Layout;
    }

    if (inherited->indent != other->inherited->indent ||
        inherited->line_height != other->inherited->line_height ||
        inherited->style_image != other->inherited->style_image ||
        inherited->font != other->inherited->font ||
        inherited->horizontal_border_spacing != other->inherited->horizontal_border_spacing ||
        inherited->vertical_border_spacing != other->inherited->vertical_border_spacing ||
        inherited_flags._box_direction != other->inherited_flags._box_direction ||
        inherited_flags._visuallyOrdered != other->inherited_flags._visuallyOrdered ||
        inherited_flags._htmlHacks != other->inherited_flags._htmlHacks ||
        noninherited_flags._position != other->noninherited_flags._position ||
        noninherited_flags._floating != other->noninherited_flags._floating ||
        noninherited_flags._originalDisplay != other->noninherited_flags._originalDisplay)
        return Layout;

   
    if (((int)noninherited_flags._effectiveDisplay) >= TABLE) {
        if (inherited_flags._border_collapse != other->inherited_flags._border_collapse ||
            inherited_flags._empty_cells != other->inherited_flags._empty_cells ||
            inherited_flags._caption_side != other->inherited_flags._caption_side ||
            noninherited_flags._table_layout != other->noninherited_flags._table_layout)
            return Layout;
        
        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse &&
            (borderTopStyle() == BHIDDEN && other->borderTopStyle() == BNONE ||
             borderTopStyle() == BNONE && other->borderTopStyle() == BHIDDEN ||
             borderBottomStyle() == BHIDDEN && other->borderBottomStyle() == BNONE ||
             borderBottomStyle() == BNONE && other->borderBottomStyle() == BHIDDEN ||
             borderLeftStyle() == BHIDDEN && other->borderLeftStyle() == BNONE ||
             borderLeftStyle() == BNONE && other->borderLeftStyle() == BHIDDEN ||
             borderRightStyle() == BHIDDEN && other->borderRightStyle() == BNONE ||
             borderRightStyle() == BNONE && other->borderRightStyle() == BHIDDEN))
            return Layout;
    }

    if (noninherited_flags._effectiveDisplay == LIST_ITEM) {
        if (inherited_flags._list_style_type != other->inherited_flags._list_style_type ||
            inherited_flags._list_style_position != other->inherited_flags._list_style_position)
            return Layout;
    }

    if (inherited_flags._text_align != other->inherited_flags._text_align ||
        inherited_flags._text_transform != other->inherited_flags._text_transform ||
        inherited_flags._direction != other->inherited_flags._direction ||
        inherited_flags._white_space != other->inherited_flags._white_space ||
        noninherited_flags._clear != other->noninherited_flags._clear)
        return Layout;

    // Overflow returns a layout hint.
    if (noninherited_flags._overflowX != other->noninherited_flags._overflowX ||
        noninherited_flags._overflowY != other->noninherited_flags._overflowY)
        return Layout;
 
    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other->borderLeftWidth() ||
        borderTopWidth() != other->borderTopWidth() ||
        borderBottomWidth() != other->borderBottomWidth() ||
        borderRightWidth() != other->borderRightWidth())
        return Layout;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    const CounterDirectiveMap* mapA = rareNonInheritedData->m_counterDirectives;
    const CounterDirectiveMap* mapB = other->rareNonInheritedData->m_counterDirectives;
    if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
        return Layout;
    if (visual->counterIncrement != other->visual->counterIncrement ||
        visual->counterReset != other->visual->counterReset)
        return Layout;

    // Make sure these left/top/right/bottom checks stay below all layout checks and above
    // all visible checks.
    if (other->position() != StaticPosition) {
        if (surround->offset != other->surround->offset) {
            // FIXME: We will need to do a bit of work in RenderObject/Box::setStyle before we
            // can stop doing a layout when relative positioned objects move.  In particular, we'll need
            // to update scrolling positions and figure out how to do a repaint properly of the updated layer.
            //if (other->position() == RelativePosition)
            //    return RepaintLayer;
            //else
                return Layout;
        }
        else if (box->z_index != other->box->z_index || box->z_auto != other->box->z_auto ||
                 visual->clip != other->visual->clip || visual->hasClip != other->visual->hasClip)
            return RepaintLayer;
    }

    if (rareNonInheritedData->opacity != other->rareNonInheritedData->opacity)
        return RepaintLayer;

    if (inherited->color != other->inherited->color ||
        inherited_flags._visibility != other->inherited_flags._visibility ||
        inherited_flags._text_decorations != other->inherited_flags._text_decorations ||
        inherited_flags._force_backgrounds_to_white != other->inherited_flags._force_backgrounds_to_white ||
        surround->border != other->surround->border ||
        *background.get() != *other->background.get() ||
        visual->textDecoration != other->visual->textDecoration ||
        rareInheritedData->userModify != other->rareInheritedData->userModify ||
        rareInheritedData->userSelect != other->rareInheritedData->userSelect ||
        rareNonInheritedData->userDrag != other->rareNonInheritedData->userDrag ||
        rareNonInheritedData->m_borderFit != other->rareNonInheritedData->m_borderFit ||
        rareInheritedData->textFillColor != other->rareInheritedData->textFillColor ||
        rareInheritedData->textStrokeColor != other->rareInheritedData->textStrokeColor)
        return Repaint;

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any repaint or layout.

    return Equal;
}

void RenderStyle::adjustBackgroundLayers()
{
    if (backgroundLayers()->next()) {
        // First we cull out layers that have no properties set.
        accessBackgroundLayers()->cullEmptyLayers();
        
        // Next we repeat patterns into layers that don't have some properties set.
        accessBackgroundLayers()->fillUnsetProperties();
    }
}

void RenderStyle::setClip( Length top, Length right, Length bottom, Length left )
{
    StyleVisualData *data = visual.access();
    data->clip.top = top;
    data->clip.right = right;
    data->clip.bottom = bottom;
    data->clip.left = left;
}

void RenderStyle::addCursor(CachedImage* image, const IntPoint& hotSpot)
{
    CursorData data;
    data.cursorImage = image;
    data.hotSpot = hotSpot;
    if (!inherited.access()->cursorData)
        inherited.access()->cursorData = new CursorList;
    inherited.access()->cursorData->append(data);
}

void RenderStyle::addSVGCursor(const String& fragmentId)
{
    CursorData data;
    data.cursorFragmentId = fragmentId;
    if (!inherited.access()->cursorData)
        inherited.access()->cursorData = new CursorList;
    inherited.access()->cursorData->append(data);
}

void RenderStyle::setCursorList(PassRefPtr<CursorList> other)
{
    inherited.access()->cursorData = other;
}

void RenderStyle::clearCursorList()
{
    inherited.access()->cursorData = new CursorList;
}

bool RenderStyle::contentDataEquivalent(const RenderStyle* otherStyle) const
{
    ContentData* c1 = rareNonInheritedData->m_content;
    ContentData* c2 = otherStyle->rareNonInheritedData->m_content;

    while (c1 && c2) {
        if (c1->m_type != c2->m_type)
            return false;

        switch (c1->m_type) {
            case CONTENT_NONE:
                break;
            case CONTENT_TEXT:
                if (!equal(c1->m_content.m_text, c2->m_content.m_text))
                    return false;
                break;
            case CONTENT_OBJECT:
                if (c1->m_content.m_object != c2->m_content.m_object)
                    return false;
                break;
            case CONTENT_COUNTER:
                if (*c1->m_content.m_counter != *c2->m_content.m_counter)
                    return false;
                break;
        }

        c1 = c1->m_next;
        c2 = c2->m_next;
    }

    return !c1 && !c2;
}

void RenderStyle::clearContent()
{
    if (rareNonInheritedData->m_content)
        rareNonInheritedData->m_content->clear();
}

void RenderStyle::setContent(CachedResource* o, bool add)
{
    if (!o)
        return; // The object is null. Nothing to do. Just bail.

    ContentData*& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content;
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content;
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content = newContentData;

    newContentData->m_content.m_object = o;
    newContentData->m_type = CONTENT_OBJECT;
}

void RenderStyle::setContent(StringImpl* s, bool add)
{
    if (!s)
        return; // The string is null. Nothing to do. Just bail.
    
    ContentData*& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content;
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    if (add && lastContent) {
        if (lastContent->m_type == CONTENT_TEXT) {
            // We can augment the existing string and share this ContentData node.
            StringImpl* oldStr = lastContent->m_content.m_text;
            StringImpl* newStr = oldStr->copy();
            newStr->ref();
            oldStr->deref();
            newStr->append(s);
            lastContent->m_content.m_text = newStr;
            return;
        }
    }

    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content;
    } else
        newContentData = new ContentData;
    
    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content = newContentData;
    
    newContentData->m_content.m_text = s;
    newContentData->m_content.m_text->ref();
    newContentData->m_type = CONTENT_TEXT;
}

void RenderStyle::setContent(CounterContent* c, bool add)
{
    if (!c)
        return;

    ContentData*& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content;
    while (lastContent && lastContent->m_next)
        lastContent = lastContent->m_next;

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content;
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->m_next = newContentData;
    else
        content = newContentData;

    newContentData->m_content.m_counter = c;
    newContentData->m_type = CONTENT_COUNTER;
}

void ContentData::clear()
{
    switch (m_type) {
        case CONTENT_NONE:
        case CONTENT_OBJECT:
            break;
        case CONTENT_TEXT:
            m_content.m_text->deref();
            break;
        case CONTENT_COUNTER:
            delete m_content.m_counter;
            break;
    }

    ContentData* n = m_next;
    m_type = CONTENT_NONE;
    m_next = 0;

    // Reverse the list so we can delete without recursing.
    ContentData* last = 0;
    ContentData* c;
    while ((c = n)) {
        n = c->m_next;
        c->m_next = last;
        last = c;
    }
    for (c = last; c; c = n) {
        n = c->m_next;
        c->m_next = 0;
        delete c;
    }
}

#if ENABLE(XBL)
BindingURI::BindingURI(StringImpl* uri) 
:m_next(0)
{ 
    m_uri = uri;
    if (uri) uri->ref();
}

BindingURI::~BindingURI()
{
    if (m_uri)
        m_uri->deref();
    delete m_next;
}

BindingURI* BindingURI::copy()
{
    BindingURI* newBinding = new BindingURI(m_uri);
    if (next()) {
        BindingURI* nextCopy = next()->copy();
        newBinding->setNext(nextCopy);
    }
    
    return newBinding;
}

bool BindingURI::operator==(const BindingURI& o) const
{
    if ((m_next && !o.m_next) || (!m_next && o.m_next) ||
        (m_next && o.m_next && *m_next != *o.m_next))
        return false;
    
    if (m_uri == o.m_uri)
        return true;
    if (!m_uri || !o.m_uri)
        return false;
    
    return String(m_uri) == String(o.m_uri);
}

void RenderStyle::addBindingURI(StringImpl* uri)
{
    BindingURI* binding = new BindingURI(uri);
    if (!bindingURIs())
        SET_VAR(rareNonInheritedData, bindingURI, binding)
    else 
        for (BindingURI* b = bindingURIs(); b; b = b->next()) {
            if (!b->next())
                b->setNext(binding);
        }
}
#endif

void RenderStyle::setTextShadow(ShadowData* val, bool add)
{
    StyleRareInheritedData* rareData = rareInheritedData.access(); 
    if (!add) {
        delete rareData->textShadow;
        rareData->textShadow = val;
        return;
    }

    ShadowData* last = rareData->textShadow;
    while (last->next) last = last->next;
    last->next = val;
}

void RenderStyle::setBoxShadow(ShadowData* val, bool add)
{
    StyleRareNonInheritedData* rareData = rareNonInheritedData.access(); 
    if (!add) {
        delete rareData->m_boxShadow;
        rareData->m_boxShadow = val;
        return;
    }

    ShadowData* last = rareData->m_boxShadow;
    while (last->next) last = last->next;
    last->next = val;
}

ShadowData::ShadowData(const ShadowData& o)
:x(o.x), y(o.y), blur(o.blur), color(o.color)
{
    next = o.next ? new ShadowData(*o.next) : 0;
}

bool ShadowData::operator==(const ShadowData& o) const
{
    if ((next && !o.next) || (!next && o.next) ||
        (next && o.next && *next != *o.next))
        return false;
    
    return x == o.x && y == o.y && blur == o.blur && color == o.color;
}

bool operator==(const CounterDirectives& a, const CounterDirectives& b)
{
    if (a.m_reset != b.m_reset || a.m_increment != b.m_increment)
        return false;
    if (a.m_reset && a.m_resetValue != b.m_resetValue)
        return false;
    if (a.m_increment && a.m_incrementValue != b.m_incrementValue)
        return false;
    return true;
}

const CounterDirectiveMap* RenderStyle::counterDirectives() const
{
    return rareNonInheritedData->m_counterDirectives;
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    CounterDirectiveMap*& map = rareNonInheritedData.access()->m_counterDirectives;
    if (!map)
        map = new CounterDirectiveMap;
    return *map;
}

const Vector<StyleDashboardRegion>& RenderStyle::initialDashboardRegions()
{ 
    static Vector<StyleDashboardRegion> emptyList;
    return emptyList;
}

const Vector<StyleDashboardRegion>& RenderStyle::noneDashboardRegions()
{ 
    static Vector<StyleDashboardRegion> noneList;
    static bool noneListInitialized = false;
    
    if (!noneListInitialized) {
        StyleDashboardRegion region;
        region.label = "";
        region.offset.top  = Length();
        region.offset.right = Length();
        region.offset.bottom = Length();
        region.offset.left = Length();
        region.type = StyleDashboardRegion::None;
        noneList.append (region);
        noneListInitialized = true;
    }
    return noneList;
}

}
