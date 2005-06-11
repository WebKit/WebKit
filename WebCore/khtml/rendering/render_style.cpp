/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004 Apple Computer, Inc.
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

#include "xml/dom_stringimpl.h"

#include "render_style.h"
#include "css/cssstyleselector.h"
#include "render_arena.h"

#include "kdebug.h"

using namespace khtml;

using DOM::DOMStringImpl;
using DOM::DOMString;

StyleSurroundData::StyleSurroundData()
    : margin( Fixed ), padding( Variable )
{
}

StyleSurroundData::StyleSurroundData(const StyleSurroundData& o )
    : Shared<StyleSurroundData>(),
      offset( o.offset ), margin( o.margin ), padding( o.padding ),
      border( o.border )
{
}

bool StyleSurroundData::operator==(const StyleSurroundData& o) const
{
    return offset==o.offset && margin==o.margin &&
	padding==o.padding && border==o.border;
}

StyleBoxData::StyleBoxData()
    : z_index( 0 ), z_auto(true)
{
    // Initialize our min/max widths/heights.
    min_width = min_height = RenderStyle::initialMinSize();
    max_width = max_height = RenderStyle::initialMaxSize();
}

StyleBoxData::StyleBoxData(const StyleBoxData& o )
    : Shared<StyleBoxData>(),
      width( o.width ), height( o.height ),
      min_width( o.min_width ), max_width( o.max_width ),
      min_height ( o.min_height ), max_height( o.max_height ),
      z_index( o.z_index ), z_auto( o.z_auto )
{
}

bool StyleBoxData::operator==(const StyleBoxData& o) const
{
    return
	    width == o.width &&
	    height == o.height &&
	    min_width == o.min_width &&
	    max_width == o.max_width &&
	    min_height == o.min_height &&
	    max_height == o.max_height &&
	    z_index == o.z_index &&
        z_auto == o.z_auto;
}


StyleVisualData::StyleVisualData()
      : hasClip(false), 
      textDecoration(RenderStyle::initialTextDecoration()), 
      colspan( 1 ), counter_increment( 0 ), counter_reset( 0 )
#if !APPLE_CHANGES
      , palette( QApplication::palette() )
#endif
{
}

StyleVisualData::~StyleVisualData() {
}

StyleVisualData::StyleVisualData(const StyleVisualData& o )
    : Shared<StyleVisualData>(),
      clip( o.clip ), hasClip( o.hasClip ), textDecoration(o.textDecoration), colspan( o.colspan ),
      counter_increment( o.counter_increment ), counter_reset( o.counter_reset )
#if !APPLE_CHANGES
      , palette( o.palette )
#endif
{
}

BackgroundLayer::BackgroundLayer()
:m_image(RenderStyle::initialBackgroundImage()),
 m_bgAttachment(RenderStyle::initialBackgroundAttachment()),
 m_bgRepeat(RenderStyle::initialBackgroundRepeat()),
 m_next(0)
{
    m_imageSet = m_attachmentSet = m_repeatSet = m_xPosSet = m_yPosSet = false;     
}

BackgroundLayer::BackgroundLayer(const BackgroundLayer& o)
{
    m_next = o.m_next ? new BackgroundLayer(*o.m_next) : 0;
    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_bgAttachment = o.m_bgAttachment;
    m_bgRepeat = o.m_bgRepeat;
    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_repeatSet = o.m_repeatSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
}

BackgroundLayer::~BackgroundLayer()
{
    delete m_next;
}

BackgroundLayer& BackgroundLayer::operator=(const BackgroundLayer& o) {
    if (m_next != o.m_next) {
        delete m_next;
        m_next = o.m_next ? new BackgroundLayer(*o.m_next) : 0;
    }
    
    m_image = o.m_image;
    m_xPosition = o.m_xPosition;
    m_yPosition = o.m_yPosition;
    m_bgAttachment = o.m_bgAttachment;
    m_bgRepeat = o.m_bgRepeat;
    
    m_imageSet = o.m_imageSet;
    m_attachmentSet = o.m_attachmentSet;
    m_repeatSet = o.m_repeatSet;
    m_xPosSet = o.m_xPosSet;
    m_yPosSet = o.m_yPosSet;
    
    return *this;
}

bool BackgroundLayer::operator==(const BackgroundLayer& o) const {
    return m_image == o.m_image && m_xPosition == o.m_xPosition && m_yPosition == o.m_yPosition &&
           m_bgAttachment == o.m_bgAttachment && m_bgRepeat == o.m_bgRepeat && 
           m_imageSet == o.m_imageSet && m_attachmentSet == o.m_attachmentSet && m_repeatSet == o.m_repeatSet &&
           m_xPosSet == o.m_xPosSet && m_yPosSet == o.m_yPosSet &&
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
}

void BackgroundLayer::cullEmptyLayers()
{
    BackgroundLayer *next;
    for (BackgroundLayer *p = this; p; p = next) {
        next = p->m_next;
        if (next && !next->isBackgroundImageSet() &&
            !next->isBackgroundXPositionSet() && !next->isBackgroundYPositionSet() &&
            !next->isBackgroundAttachmentSet() && !next->isBackgroundRepeatSet()) {
            delete next;
            p->m_next = 0;
            break;
        }
    }
}

StyleBackgroundData::StyleBackgroundData()
{}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& o)
    : Shared<StyleBackgroundData>(), m_background(o.m_background), m_outline(o.m_outline)
{}

bool StyleBackgroundData::operator==(const StyleBackgroundData& o) const
{
    return m_background == o.m_background && m_color == o.m_color && m_outline == o.m_outline;
}

StyleMarqueeData::StyleMarqueeData()
{
    increment = RenderStyle::initialMarqueeIncrement();
    speed = RenderStyle::initialMarqueeSpeed();
    direction = RenderStyle::initialMarqueeDirection();
    behavior = RenderStyle::initialMarqueeBehavior();
    loops = RenderStyle::initialMarqueeLoopCount();
}

StyleMarqueeData::StyleMarqueeData(const StyleMarqueeData& o)
:Shared<StyleMarqueeData>(), increment(o.increment), speed(o.speed), loops(o.loops),
 behavior(o.behavior), direction(o.direction) 
{}

bool StyleMarqueeData::operator==(const StyleMarqueeData& o) const
{
    return (increment == o.increment && speed == o.speed && direction == o.direction &&
            behavior == o.behavior && loops == o.loops);
}

StyleFlexibleBoxData::StyleFlexibleBoxData()
: Shared<StyleFlexibleBoxData>()
{
    flex = RenderStyle::initialBoxFlex();
    flex_group = RenderStyle::initialBoxFlexGroup();
    ordinal_group = RenderStyle::initialBoxOrdinalGroup();
    align = RenderStyle::initialBoxAlign();
    pack = RenderStyle::initialBoxPack();
    orient = RenderStyle::initialBoxOrient();
    lines = RenderStyle::initialBoxLines();
}

StyleFlexibleBoxData::StyleFlexibleBoxData(const StyleFlexibleBoxData& o)
: Shared<StyleFlexibleBoxData>()
{
    flex = o.flex;
    flex_group = o.flex_group;
    ordinal_group = o.ordinal_group;
    align = o.align;
    pack = o.pack;
    orient = o.orient;
    lines = o.lines;
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return flex == o.flex && flex_group == o.flex_group &&
           ordinal_group == o.ordinal_group && align == o.align &&
           pack == o.pack && orient == o.orient && lines == o.lines;
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData()
:Shared<StyleCSS3NonInheritedData>(), 
#if APPLE_CHANGES
lineClamp(RenderStyle::initialLineClamp()),
#endif
opacity(RenderStyle::initialOpacity()),
userDrag(RenderStyle::initialUserDrag()),
userSelect(RenderStyle::initialUserSelect()),
textOverflow(RenderStyle::initialTextOverflow()),
marginTopCollapse(MCOLLAPSE),
marginBottomCollapse(MCOLLAPSE)
#ifndef KHTML_NO_XBL
, bindingURI(0)
#endif
{
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData& o)
:Shared<StyleCSS3NonInheritedData>(), 
#if APPLE_CHANGES
lineClamp(o.lineClamp),
#endif
opacity(o.opacity), flexibleBox(o.flexibleBox), marquee(o.marquee),
userDrag(o.userDrag), userSelect(o.userSelect), textOverflow(o.textOverflow),
marginTopCollapse(o.marginTopCollapse), marginBottomCollapse(o.marginBottomCollapse)
{
#ifndef KHTML_NO_XBL
    bindingURI = o.bindingURI ? o.bindingURI->copy() : 0;
#endif
}

StyleCSS3NonInheritedData::~StyleCSS3NonInheritedData()
{
#ifndef KHTML_NO_XBL
    delete bindingURI;
#endif
}

#ifndef KHTML_NO_XBL
bool StyleCSS3NonInheritedData::bindingsEquivalent(const StyleCSS3NonInheritedData& o) const
{
    if (this == &o) return true;
    if (!bindingURI && o.bindingURI || bindingURI && !o.bindingURI)
        return false;
    if (bindingURI && o.bindingURI && (*bindingURI != *o.bindingURI))
        return false;
    return true;
}
#endif

bool StyleCSS3NonInheritedData::operator==(const StyleCSS3NonInheritedData& o) const
{
    return opacity == o.opacity && flexibleBox == o.flexibleBox && marquee == o.marquee &&
           userDrag == o.userDrag && userSelect == o.userSelect && textOverflow == o.textOverflow &&
           marginTopCollapse == o.marginTopCollapse && marginBottomCollapse == o.marginBottomCollapse
#ifndef KHTML_NO_XBL
           && bindingsEquivalent(o)
#endif
#if APPLE_CHANGES
           && lineClamp == o.lineClamp && m_dashboardRegions == o.m_dashboardRegions
#endif
    ;
}

StyleCSS3InheritedData::StyleCSS3InheritedData()
:Shared<StyleCSS3InheritedData>(), textShadow(0), userModify(READ_ONLY), wordWrap(WBNORMAL), 
    nbspMode(NBNORMAL), khtmlLineBreak(LBNORMAL)
#if APPLE_CHANGES
, textSizeAdjust(RenderStyle::initialTextSizeAdjust())
#endif
{

}

StyleCSS3InheritedData::StyleCSS3InheritedData(const StyleCSS3InheritedData& o)
:Shared<StyleCSS3InheritedData>()
{
    textShadow = o.textShadow ? new ShadowData(*o.textShadow) : 0;
    userModify = o.userModify;
    wordWrap = o.wordWrap;
    nbspMode = o.nbspMode;
    khtmlLineBreak = o.khtmlLineBreak;
#if APPLE_CHANGES
    textSizeAdjust = o.textSizeAdjust;
#endif
}

StyleCSS3InheritedData::~StyleCSS3InheritedData()
{
    delete textShadow;
}

bool StyleCSS3InheritedData::operator==(const StyleCSS3InheritedData& o) const
{
    return (userModify == o.userModify) && shadowDataEquivalent(o) && (wordWrap == o.wordWrap) &&
        (nbspMode == o.nbspMode) && (khtmlLineBreak == o.khtmlLineBreak)
#if APPLE_CHANGES
            && (textSizeAdjust == o.textSizeAdjust)
#endif
    ;
}

bool StyleCSS3InheritedData::shadowDataEquivalent(const StyleCSS3InheritedData& o) const
{
    if (!textShadow && o.textShadow || textShadow && !o.textShadow)
        return false;
    if (textShadow && o.textShadow && (*textShadow != *o.textShadow))
        return false;
    return true;
}

StyleInheritedData::StyleInheritedData()
    : indent( RenderStyle::initialTextIndent() ), line_height( RenderStyle::initialLineHeight() ), 
      style_image( RenderStyle::initialListStyleImage() ),
      cursor_image( 0 ), font(), color( RenderStyle::initialColor() ), 
      horizontal_border_spacing( RenderStyle::initialHorizontalBorderSpacing() ), 
      vertical_border_spacing( RenderStyle::initialVerticalBorderSpacing() ),
      widows( RenderStyle::initialWidows() ), orphans( RenderStyle::initialOrphans() ),
      page_break_inside( RenderStyle::initialPageBreak() )
{
}

StyleInheritedData::~StyleInheritedData()
{
}

StyleInheritedData::StyleInheritedData(const StyleInheritedData& o )
    : Shared<StyleInheritedData>(),
      indent( o.indent ), line_height( o.line_height ), style_image( o.style_image ),
      cursor_image( o.cursor_image ), font( o.font ),
      color( o.color ),
      horizontal_border_spacing( o.horizontal_border_spacing ),
      vertical_border_spacing( o.vertical_border_spacing ),
      widows(o.widows), orphans(o.orphans), page_break_inside(o.page_break_inside)
{
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
	indent == o.indent &&
	line_height == o.line_height &&
	style_image == o.style_image &&
	cursor_image == o.cursor_image &&
	font == o.font &&
	color == o.color &&
        horizontal_border_spacing == o.horizontal_border_spacing &&
        vertical_border_spacing == o.vertical_border_spacing &&
        widows == o.widows &&
        orphans == o.orphans &&
        page_break_inside == o.page_break_inside;
}

// ----------------------------------------------------------

void* RenderStyle::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderStyle::operator delete(void* ptr, size_t sz)
{
    // Stash size where detach can find it.
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
    delete content;
    
    delete this;
    
    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t *)this, this);
}

RenderStyle::RenderStyle()
:m_pseudoState(PseudoUnknown), m_affectedByAttributeSelectors(false)
{
    m_ref = 0;
    
    if (!_default)
	_default = ::new RenderStyle(true);

    box = _default->box;
    visual = _default->visual;
    background = _default->background;
    surround = _default->surround;
    css3NonInheritedData = _default->css3NonInheritedData;
    css3InheritedData = _default->css3InheritedData;
    
    inherited = _default->inherited;

    setBitDefaults();

    pseudoStyle = 0;
    content = 0;
}

RenderStyle::RenderStyle(bool)
:m_pseudoState(PseudoUnknown), m_affectedByAttributeSelectors(false)
{
    setBitDefaults();

    box.init();
    visual.init();
    background.init();
    surround.init();
    css3NonInheritedData.init();
    css3NonInheritedData.access()->flexibleBox.init();
    css3NonInheritedData.access()->marquee.init();
    css3InheritedData.init();
    inherited.init();

    pseudoStyle = 0;
    content = 0;
    m_ref = 1;
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : inherited_flags( o.inherited_flags ), noninherited_flags( o.noninherited_flags ),
      box( o.box ), visual( o.visual ), background( o.background ), surround( o.surround ),
      css3NonInheritedData( o.css3NonInheritedData ), css3InheritedData( o.css3InheritedData ),
      inherited( o.inherited ), pseudoStyle( 0 ), content( o.content ), m_pseudoState(o.m_pseudoState),
      m_affectedByAttributeSelectors(false)
{
    m_ref = 0;
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent)
{
    css3InheritedData = inheritParent->css3InheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
}

RenderStyle::~RenderStyle()
{
}

bool RenderStyle::operator==(const RenderStyle& o) const
{
// compare everything except the pseudoStyle pointer
    return (inherited_flags == o.inherited_flags &&
            noninherited_flags == o.noninherited_flags &&
	    box == o.box &&
            visual == o.visual &&
            background == o.background &&
            surround == o.surround &&
            css3NonInheritedData == o.css3NonInheritedData &&
            css3InheritedData == o.css3InheritedData &&
            inherited == o.inherited);
}

bool RenderStyle::isStyleAvailable() const
{
    return this != CSSStyleSelector::styleNotYetAvailable;
}

enum EPseudoBit { NO_BIT = 0x0, BEFORE_BIT = 0x1, AFTER_BIT = 0x2, FIRST_LINE_BIT = 0x4,
                  FIRST_LETTER_BIT = 0x8, SELECTION_BIT = 0x10, FIRST_LINE_INHERITED_BIT = 0x20 };

static int pseudoBit(RenderStyle::PseudoId pseudo)
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
        default:
            return NO_BIT;
    }
}

bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    return (pseudoBit(pseudo) & noninherited_flags._pseudoBits) != 0;
}

void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    noninherited_flags._pseudoBits |= pseudoBit(pseudo);
}

RenderStyle* RenderStyle::getPseudoStyle(PseudoId pid)
{
    RenderStyle *ps = 0;
    if (noninherited_flags._styleType==NOPSEUDO) {
	ps = pseudoStyle;
        while (ps) {
            if (ps->noninherited_flags._styleType==pid)
                    break;
    
            ps = ps->pseudoStyle;
        }
    }
    return ps;
}

void RenderStyle::addPseudoStyle(RenderStyle* pseudo)
{
    if (!pseudo) return;
    pseudo->ref();
    pseudo->pseudoStyle = pseudoStyle;
    pseudoStyle = pseudo;
}

bool RenderStyle::inheritedNotEqual( RenderStyle *other ) const
{
    return inherited_flags != other->inherited_flags ||
           inherited != other->inherited ||
           css3InheritedData != other->css3InheritedData;
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
RenderStyle::Diff RenderStyle::diff( const RenderStyle *other ) const
{
    // we anyway assume they are the same
// 	EDisplay _effectiveDisplay : 5;

    // NonVisible:
// 	ECursor _cursor_style : 4;

// ### this needs work to know more exactly if we need a relayout
//     or just a repaint

// non-inherited attributes
//     DataRef<StyleBoxData> box;
//     DataRef<StyleVisualData> visual;
//     DataRef<StyleSurroundData> surround;

// inherited attributes
//     DataRef<StyleInheritedData> inherited;

    if ( box->width != other->box->width ||
         box->min_width != other->box->min_width ||
         box->max_width != other->box->max_width ||
         box->height != other->box->height ||
         box->min_height != other->box->min_height ||
         box->max_height != other->box->max_height ||
         box->vertical_align != other->box->vertical_align ||
         !(surround->margin == other->surround->margin) ||
         !(surround->padding == other->surround->padding) ||
         css3NonInheritedData->marginTopCollapse != other->css3NonInheritedData->marginTopCollapse ||
         css3NonInheritedData->marginBottomCollapse != other->css3NonInheritedData->marginBottomCollapse ||
         *css3NonInheritedData->flexibleBox.get() != *other->css3NonInheritedData->flexibleBox.get() ||
#if APPLE_CHANGES
         (css3NonInheritedData->lineClamp != other->css3NonInheritedData->lineClamp) ||
         (css3InheritedData->textSizeAdjust != other->css3InheritedData->textSizeAdjust) ||
#endif
         (css3InheritedData->wordWrap != other->css3InheritedData->wordWrap) ||
         (css3InheritedData->nbspMode != other->css3InheritedData->nbspMode) ||
         (css3InheritedData->khtmlLineBreak != other->css3InheritedData->khtmlLineBreak) ||
        !(inherited->indent == other->inherited->indent) ||
        !(inherited->line_height == other->inherited->line_height) ||
        !(inherited->style_image == other->inherited->style_image) ||
        !(inherited->cursor_image == other->inherited->cursor_image) ||
        !(inherited->font == other->inherited->font) ||
        !(inherited->horizontal_border_spacing == other->inherited->horizontal_border_spacing) ||
        !(inherited->vertical_border_spacing == other->inherited->vertical_border_spacing) ||
        !(inherited_flags._box_direction == other->inherited_flags._box_direction) ||
        !(inherited_flags._visuallyOrdered == other->inherited_flags._visuallyOrdered) ||
        !(inherited_flags._htmlHacks == other->inherited_flags._htmlHacks) ||
        !(noninherited_flags._position == other->noninherited_flags._position) ||
        !(noninherited_flags._floating == other->noninherited_flags._floating) ||
        !(noninherited_flags._originalDisplay == other->noninherited_flags._originalDisplay) ||
         visual->colspan != other->visual->colspan ||
         visual->counter_increment != other->visual->counter_increment ||
         visual->counter_reset != other->visual->counter_reset ||
         css3NonInheritedData->textOverflow != other->css3NonInheritedData->textOverflow)
        return Layout;
   
    // changes causing Layout changes:

// only for tables:
// 	_border_collapse
// 	EEmptyCell _empty_cells : 2 ;
// 	ECaptionSide _caption_side : 2;
//     ETableLayout _table_layout : 1;
//     EPosition _position : 2;
//     EFloat _floating : 2;
    if ( ((int)noninherited_flags._effectiveDisplay) >= TABLE ) {
	// Stupid gcc gives a compile error on
	// a != other->b if a and b are bitflags. Using
	// !(a== other->b) instead.
	if ( !(inherited_flags._border_collapse == other->inherited_flags._border_collapse) ||
	     !(inherited_flags._empty_cells == other->inherited_flags._empty_cells) ||
	     !(inherited_flags._caption_side == other->inherited_flags._caption_side) ||
	     !(noninherited_flags._table_layout == other->noninherited_flags._table_layout))
        return Layout;
    }

// only for lists:
// 	EListStyleType _list_style_type : 5 ;
// 	EListStylePosition _list_style_position :1;
    if (noninherited_flags._effectiveDisplay == LIST_ITEM ) {
	if ( !(inherited_flags._list_style_type == other->inherited_flags._list_style_type) ||
	     !(inherited_flags._list_style_position == other->inherited_flags._list_style_position) )
	    return Layout;
    }

// ### These could be better optimised
// 	ETextAlign _text_align : 3;
// 	ETextTransform _text_transform : 4;
// 	EDirection _direction : 1;
// 	EWhiteSpace _white_space : 2;
// 	EFontVariant _font_variant : 1;
//     EClear _clear : 2;
    if ( !(inherited_flags._text_align == other->inherited_flags._text_align) ||
	 !(inherited_flags._text_transform == other->inherited_flags._text_transform) ||
	 !(inherited_flags._direction == other->inherited_flags._direction) ||
	 !(inherited_flags._white_space == other->inherited_flags._white_space) ||
	 !(noninherited_flags._clear == other->noninherited_flags._clear)
	)
	return Layout;

// only for inline:
//     EVerticalAlign _vertical_align : 4;

    if ( !(noninherited_flags._effectiveDisplay == INLINE) &&
         !(noninherited_flags._vertical_align == other->noninherited_flags._vertical_align))
        return Layout;

    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other->borderLeftWidth() ||
        borderTopWidth() != other->borderTopWidth() ||
        borderBottomWidth() != other->borderBottomWidth() ||
        borderRightWidth() != other->borderRightWidth())
        return Layout;

#if APPLE_CHANGES
    // If regions change trigger a relayout to re-calc regions.
    if (!(css3NonInheritedData->m_dashboardRegions == other->css3NonInheritedData->m_dashboardRegions))
        return Layout;
#endif

    // Make sure these left/top/right/bottom checks stay below all layout checks and above
    // all visible checks.
    if (other->position() != STATIC) {
        if (!(surround->offset == other->surround->offset)) {
            // FIXME: We will need to do a bit of work in RenderObject/Box::setStyle before we
            // can stop doing a layout when relative positioned objects move.  In particular, we'll need
            // to update scrolling positions and figure out how to do a repaint properly of the updated layer.
            //if (other->position() == RELATIVE)
            //    return RepaintLayer;
            //else
                return Layout;
        }
        else if (box->z_index != other->box->z_index || box->z_auto != other->box->z_auto ||
                 !(visual->clip == other->visual->clip) || visual->hasClip != other->visual->hasClip)
            return RepaintLayer;
    }

    if (css3NonInheritedData->opacity != other->css3NonInheritedData->opacity)
        return RepaintLayer;

    // Repaint:
// 	EVisibility _visibility : 2;
//     EOverflow _overflow : 4 ;
// 	int _text_decoration : 4;
//     DataRef<StyleBackgroundData> background;
    if (inherited->color != other->inherited->color ||
        !(inherited_flags._visibility == other->inherited_flags._visibility) ||
        !(noninherited_flags._overflow == other->noninherited_flags._overflow) ||
        !(inherited_flags._text_decorations == other->inherited_flags._text_decorations) ||
        !(inherited_flags._force_backgrounds_to_white == other->inherited_flags._force_backgrounds_to_white) ||
        !(surround->border == other->surround->border) ||
        *background.get() != *other->background.get() ||
        visual->textDecoration != other->visual->textDecoration ||
        !css3InheritedData->shadowDataEquivalent(*other->css3InheritedData.get()) ||
        css3InheritedData->userModify != other->css3InheritedData->userModify ||
        css3NonInheritedData->userSelect != other->css3NonInheritedData->userSelect ||
        css3NonInheritedData->userDrag != other->css3NonInheritedData->userDrag
#if !APPLE_CHANGES
        || !(visual->palette == other->visual->palette)
#endif
	)
        return Repaint;

    return Equal;
}


RenderStyle* RenderStyle::_default = 0;
//int RenderStyle::counter = 0;
//int SharedData::counter = 0;

void RenderStyle::cleanup()
{
    delete _default;
    _default = 0;
//    counter = 0;
//    SharedData::counter = 0;
}

#if !APPLE_CHANGES

void RenderStyle::setPaletteColor(QPalette::ColorGroup g, QColorGroup::ColorRole r, const QColor& c)
{
    visual.access()->palette.setColor(g,r,c);
}

#endif

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

bool RenderStyle::contentDataEquivalent(RenderStyle* otherStyle)
{
    ContentData* c1 = content;
    ContentData* c2 = otherStyle->content;

    while (c1 && c2) {
        if (c1->_contentType != c2->_contentType)
            return false;
        if (c1->_contentType == CONTENT_TEXT) {
            DOMString c1Str(c1->_content.text);
            DOMString c2Str(c2->_content.text);
            if (c1Str != c2Str)
                return false;
        }
        else if (c1->_contentType == CONTENT_OBJECT) {
            if (c1->_content.object != c2->_content.object)
                return false;
        }

        c1 = c1->_nextContent;
        c2 = c2->_nextContent;
    }

    return !c1 && !c2;
}

void RenderStyle::setContent(CachedObject* o, bool add)
{
    if (!o)
        return; // The object is null. Nothing to do. Just bail.

    ContentData* lastContent = content;
    while (lastContent && lastContent->_nextContent)
        lastContent = lastContent->_nextContent;

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clearContent();
        newContentData = content;
    }
    else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->_nextContent = newContentData;
    else
        content = newContentData;

    //    o->ref();
    newContentData->_content.object = o;
    newContentData->_contentType = CONTENT_OBJECT;
}

void RenderStyle::setContent(DOMStringImpl* s, bool add)
{
    if (!s)
        return; // The string is null. Nothing to do. Just bail.
    
    ContentData* lastContent = content;
    while (lastContent && lastContent->_nextContent)
        lastContent = lastContent->_nextContent;

    bool reuseContent = !add;
    if (add && lastContent) {
        if (lastContent->_contentType == CONTENT_TEXT) {
            // We can augment the existing string and share this ContentData node.
            DOMStringImpl* oldStr = lastContent->_content.text;
            DOMStringImpl* newStr = oldStr->copy();
            newStr->ref();
            oldStr->deref();
            newStr->append(s);
            lastContent->_content.text = newStr;
            return;
        }
    }

    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clearContent();
        newContentData = content;
    }
    else
        newContentData = new ContentData;
    
    if (lastContent && !reuseContent)
        lastContent->_nextContent = newContentData;
    else
        content = newContentData;
    
    newContentData->_content.text = s;
    newContentData->_content.text->ref();
    newContentData->_contentType = CONTENT_TEXT;
}

ContentData::~ContentData()
{
    clearContent();
}

void ContentData::clearContent()
{
    delete _nextContent;
    _nextContent = 0;
    
    switch (_contentType)
    {
        case CONTENT_OBJECT:
//            _content.object->deref();
            _content.object = 0;
            break;
        case CONTENT_TEXT:
            _content.text->deref();
            _content.text = 0;
        default:
            ;
    }
}

#ifndef KHTML_NO_XBL
BindingURI::BindingURI(DOM::DOMStringImpl* uri) 
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
    
    return DOMString(m_uri) == DOMString(o.m_uri);
}

void RenderStyle::addBindingURI(DOM::DOMStringImpl* uri)
{
    BindingURI* binding = new BindingURI(uri);
    if (!bindingURIs())
        SET_VAR(css3NonInheritedData, bindingURI, binding)
    else 
        for (BindingURI* b = bindingURIs(); b; b = b->next()) {
            if (!b->next())
                b->setNext(binding);
        }
}
#endif

void RenderStyle::setTextShadow(ShadowData* val, bool add)
{
    StyleCSS3InheritedData* css3Data = css3InheritedData.access(); 
    if (!add) {
        delete css3Data->textShadow;
        css3Data->textShadow = val;
        return;
    }

    ShadowData* last = css3Data->textShadow;
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

const QValueList<StyleDashboardRegion>& RenderStyle::initialDashboardRegions()
{ 
    static QValueList<StyleDashboardRegion> emptyList;
    return emptyList;
}

const QValueList<StyleDashboardRegion>& RenderStyle::noneDashboardRegions()
{ 
    static QValueList<StyleDashboardRegion> noneList;
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
