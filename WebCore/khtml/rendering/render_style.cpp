/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
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

#include "xml/dom_stringimpl.h"

#include "render_style.h"
#include "css/cssstyleselector.h"

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
    min_width.type = Fixed;
    min_width.value = 0;
    min_height.type = Fixed;
    min_height.value = 0;
    max_width.type = Fixed;
    max_width.value = UNDEFINED;
    max_height.type = Fixed;
    max_height.value = UNDEFINED;
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
    : hasClip(false), textDecoration(TDNONE), colspan( 1 ), counter_increment( 0 ), counter_reset( 0 ),
      palette( QApplication::palette() )
{
}

StyleVisualData::~StyleVisualData() {
}

StyleVisualData::StyleVisualData(const StyleVisualData& o )
    : Shared<StyleVisualData>(),
      clip( o.clip ), hasClip( o.hasClip ), textDecoration(o.textDecoration), colspan( o.colspan ),
      counter_increment( o.counter_increment ), counter_reset( o.counter_reset ),
      palette( o.palette )
{
}



StyleBackgroundData::StyleBackgroundData()
    : image( 0 )
{
}

StyleBackgroundData::StyleBackgroundData(const StyleBackgroundData& o )
    : Shared<StyleBackgroundData>(),
      color( o.color ), image( o.image ),
      x_position( o.x_position ), y_position( o.y_position ),
      outline( o.outline )
{
}

bool StyleBackgroundData::operator==(const StyleBackgroundData& o) const
{
    return
	color == o.color &&
	image == o.image &&
	x_position == o.x_position &&
	y_position == o.y_position &&
	outline == o.outline;
}

StyleFlexibleBoxData::StyleFlexibleBoxData()
: Shared<StyleFlexibleBoxData>()
{
    flex = 0.0f;
    flex_group = 1;
    ordinal_group = 1;
    align = BSTRETCH;
    pack = BSTART;
    orient = HORIZONTAL;
    lines = SINGLE;
    flexed_height = -1;
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
    flexed_height = o.flexed_height;
}

bool StyleFlexibleBoxData::operator==(const StyleFlexibleBoxData& o) const
{
    return flex == o.flex && flex_group == o.flex_group &&
           ordinal_group == o.ordinal_group && align == o.align &&
           pack == o.pack && orient == o.orient && lines == o.lines &&
           flexed_height == o.flexed_height;
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData()
:Shared<StyleCSS3NonInheritedData>(), opacity(1.0f)
{
    
}

StyleCSS3NonInheritedData::StyleCSS3NonInheritedData(const StyleCSS3NonInheritedData& o)
:Shared<StyleCSS3NonInheritedData>(), opacity(o.opacity), flexibleBox(o.flexibleBox)
{
}

bool StyleCSS3NonInheritedData::operator==(const StyleCSS3NonInheritedData& o) const
{
    return opacity == o.opacity && flexibleBox == o.flexibleBox;
}

StyleCSS3InheritedData::StyleCSS3InheritedData()
:Shared<StyleCSS3InheritedData>(), textShadow(0)
{

}

StyleCSS3InheritedData::StyleCSS3InheritedData(const StyleCSS3InheritedData& o)
:Shared<StyleCSS3InheritedData>()
{
    textShadow = o.textShadow ? new ShadowData(*o.textShadow) : 0;
}

bool StyleCSS3InheritedData::operator==(const StyleCSS3InheritedData& o) const
{
    return shadowDataEquivalent(o);
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
    : indent( Fixed ), line_height( -100, Percent ), style_image( 0 ),
      cursor_image( 0 ), font(), color( Qt::black ), border_spacing( 0 )
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
      border_spacing( o.border_spacing )
{
}

bool StyleInheritedData::operator==(const StyleInheritedData& o) const
{
    return
	indent == o.indent &&
	line_height == o.line_height &&
	border_spacing == o.border_spacing &&
	style_image == o.style_image &&
	cursor_image == o.cursor_image &&
	font == o.font &&
	color == o.color;

    // doesn't work because structs are not packed
    //return memcmp(this, &o, sizeof(*this))==0;
}

RenderStyle::RenderStyle()
{
//    counter++;
    if (!_default)
	_default = new RenderStyle(true);

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
{
    setBitDefaults();

    box.init();
    visual.init();
    background.init();
    surround.init();
    css3NonInheritedData.init();
    css3NonInheritedData.access()->flexibleBox.init();
    css3InheritedData.init();
    inherited.init();

    pseudoStyle = 0;
    content = 0;
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : Shared<RenderStyle>(),
      inherited_flags( o.inherited_flags ), noninherited_flags( o.noninherited_flags ),
      box( o.box ), visual( o.visual ), background( o.background ), surround( o.surround ),
      css3NonInheritedData( o.css3NonInheritedData ), css3InheritedData( o.css3InheritedData ),
      inherited( o.inherited ), pseudoStyle( 0 ), content( o.content )
{
}

void RenderStyle::inheritFrom(const RenderStyle* inheritParent)
{
    css3InheritedData = inheritParent->css3InheritedData;
    inherited = inheritParent->inherited;
    inherited_flags = inheritParent->inherited_flags;
}

RenderStyle::~RenderStyle()
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
        prev->deref();
    }
    delete content;
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

RenderStyle* RenderStyle::addPseudoStyle(PseudoId pid)
{
    RenderStyle *ps = getPseudoStyle(pid);

    if (!ps)
    {
        ps = new RenderStyle(); // So that noninherited flags are reset.
        ps->ref();
        ps->noninherited_flags._styleType = pid;
        ps->pseudoStyle = pseudoStyle;

        pseudoStyle = ps;
    }

    return ps;
}

void RenderStyle::removePseudoStyle(PseudoId pid)
{
    RenderStyle *ps = pseudoStyle;
    RenderStyle *prev = this;

    while (ps) {
        if (ps->noninherited_flags._styleType==pid) {
            prev->pseudoStyle = ps->pseudoStyle;
            ps->deref();
            return;
        }
        prev = ps;
        ps = ps->pseudoStyle;
    }
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

    if ( *box.get() != *other->box.get() ||
        *surround.get() != *other->surround.get() ||
         *css3NonInheritedData->flexibleBox.get() != *other->css3NonInheritedData->flexibleBox.get() ||
        !(inherited->indent == other->inherited->indent) ||
        !(inherited->line_height == other->inherited->line_height) ||
        !(inherited->style_image == other->inherited->style_image) ||
        !(inherited->cursor_image == other->inherited->cursor_image) ||
        !(inherited->font == other->inherited->font) ||
        !(inherited->border_spacing == other->inherited->border_spacing) ||
        !(inherited_flags._box_direction == other->inherited_flags._box_direction) ||
        !(inherited_flags._visuallyOrdered == other->inherited_flags._visuallyOrdered) ||
        !(inherited_flags._htmlHacks == other->inherited_flags._htmlHacks) ||
        !(noninherited_flags._position == other->noninherited_flags._position) ||
        !(noninherited_flags._floating == other->noninherited_flags._floating) ||
        !(noninherited_flags._originalDisplay == other->noninherited_flags._originalDisplay) ||
         visual->colspan != other->visual->colspan ||
         visual->counter_increment != other->visual->counter_increment ||
         visual->counter_reset != other->visual->counter_reset)
        return CbLayout;
   
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
        return CbLayout;
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
	 !(inherited_flags._font_variant == other->inherited_flags._font_variant) ||
	 !(noninherited_flags._clear == other->noninherited_flags._clear)
	)
	return Layout;

// only for inline:
//     EVerticalAlign _vertical_align : 4;

    if ( !(noninherited_flags._effectiveDisplay == INLINE) &&
         !(noninherited_flags._vertical_align == other->noninherited_flags._vertical_align))
        return Layout;

    // Visible:
// 	EVisibility _visibility : 2;
//     EOverflow _overflow : 4 ;
//     EBackgroundRepeat _bg_repeat : 2;
//     bool _bg_attachment : 1;
// 	int _text_decoration : 4;
//     DataRef<StyleBackgroundData> background;
    if (inherited->color != other->inherited->color ||
        !(inherited_flags._visibility == other->inherited_flags._visibility) ||
        !(noninherited_flags._overflow == other->noninherited_flags._overflow) ||
        !(noninherited_flags._bg_repeat == other->noninherited_flags._bg_repeat) ||
        !(noninherited_flags._bg_attachment == other->noninherited_flags._bg_attachment) ||
        !(inherited_flags._text_decorations == other->inherited_flags._text_decorations) ||
        *background.get() != *other->background.get() ||
        !(visual->clip == other->visual->clip) ||
        visual->hasClip != other->visual->hasClip ||
        visual->textDecoration != other->visual->textDecoration ||
        css3NonInheritedData->opacity != other->css3NonInheritedData->opacity ||
        !css3InheritedData->shadowDataEquivalent(*other->css3InheritedData.get()) ||
        !(visual->palette == other->visual->palette)
	)
        return Visible;

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

void RenderStyle::setPaletteColor(QPalette::ColorGroup g, QColorGroup::ColorRole r, const QColor& c)
{
    visual.access()->palette.setColor(g,r,c);
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
    if (add) {
        if (!lastContent)
            return; // Something's wrong.  We had no previous content, and we should have.

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
