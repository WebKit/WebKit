/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "CachedImage.h"
#include "CounterContent.h"
#include "FontSelector.h"
#include "RenderArena.h"
#include "RenderObject.h"
#include "StyleImage.h"
#include <wtf/StdLibExtras.h>
#include <algorithm>

namespace WebCore {

inline RenderStyle* defaultStyle()
{
    static RenderStyle* s_defaultStyle = RenderStyle::createDefaultStyle().releaseRef();
    return s_defaultStyle;
}

PassRefPtr<RenderStyle> RenderStyle::create()
{
    return adoptRef(new RenderStyle());
}

PassRefPtr<RenderStyle> RenderStyle::createDefaultStyle()
{
    return adoptRef(new RenderStyle(true));
}

PassRefPtr<RenderStyle> RenderStyle::clone(const RenderStyle* other)
{
    return adoptRef(new RenderStyle(*other));
}

RenderStyle::RenderStyle()
    : m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
    , box(defaultStyle()->box)
    , visual(defaultStyle()->visual)
    , background(defaultStyle()->background)
    , surround(defaultStyle()->surround)
    , rareNonInheritedData(defaultStyle()->rareNonInheritedData)
    , rareInheritedData(defaultStyle()->rareInheritedData)
    , inherited(defaultStyle()->inherited)
#if ENABLE(SVG)
    , m_svgStyle(defaultStyle()->m_svgStyle)
#endif
{
    setBitDefaults(); // Would it be faster to copy this from the default style?
}

RenderStyle::RenderStyle(bool)
    : m_pseudoState(PseudoUnknown)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
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
    rareNonInheritedData.access()->m_transform.init();
    rareInheritedData.init();
    inherited.init();

#if ENABLE(SVG)
    m_svgStyle.init();
#endif
}

RenderStyle::RenderStyle(const RenderStyle& o)
    : RefCounted<RenderStyle>()
    , m_pseudoState(o.m_pseudoState)
    , m_affectedByAttributeSelectors(false)
    , m_unique(false)
    , m_affectedByEmpty(false)
    , m_emptyState(false)
    , m_childrenAffectedByFirstChildRules(false)
    , m_childrenAffectedByLastChildRules(false)
    , m_childrenAffectedByDirectAdjacentRules(false)
    , m_childrenAffectedByForwardPositionalRules(false)
    , m_childrenAffectedByBackwardPositionalRules(false)
    , m_firstChildState(false)
    , m_lastChildState(false)
    , m_childIndex(0)
    , box(o.box)
    , visual(o.visual)
    , background(o.background)
    , surround(o.surround)
    , rareNonInheritedData(o.rareNonInheritedData)
    , rareInheritedData(o.rareInheritedData)
    , inherited(o.inherited)
#if ENABLE(SVG)
    , m_svgStyle(o.m_svgStyle)
#endif
    , inherited_flags(o.inherited_flags)
    , noninherited_flags(o.noninherited_flags)
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
    return this != CSSStyleSelector::styleNotYetAvailable();
}

static inline int pseudoBit(PseudoId pseudo)
{
    return 1 << (pseudo - 1);
}

bool RenderStyle::hasPseudoStyle(PseudoId pseudo) const
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    return pseudoBit(pseudo) & noninherited_flags._pseudoBits;
}

void RenderStyle::setHasPseudoStyle(PseudoId pseudo)
{
    ASSERT(pseudo > NOPSEUDO);
    ASSERT(pseudo < FIRST_INTERNAL_PSEUDOID);
    noninherited_flags._pseudoBits |= pseudoBit(pseudo);
}

RenderStyle* RenderStyle::getCachedPseudoStyle(PseudoId pid)
{
    if (!m_cachedPseudoStyle || styleType() != NOPSEUDO)
        return 0;
    RenderStyle* ps = m_cachedPseudoStyle.get();
    while (ps && ps->styleType() != pid)
        ps = ps->m_cachedPseudoStyle.get();
    return ps;
}

RenderStyle* RenderStyle::addCachedPseudoStyle(PassRefPtr<RenderStyle> pseudo)
{
    if (!pseudo)
        return 0;
    pseudo->m_cachedPseudoStyle = m_cachedPseudoStyle;
    m_cachedPseudoStyle = pseudo;
    return m_cachedPseudoStyle.get();
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

static bool positionedObjectMoved(const LengthBox& a, const LengthBox& b)
{
    // If any unit types are different, then we can't guarantee
    // that this was just a movement.
    if (a.left().type() != b.left().type() ||
        a.right().type() != b.right().type() ||
        a.top().type() != b.top().type() ||
        a.bottom().type() != b.bottom().type())
        return false;

    // Only one unit can be non-auto in the horizontal direction and
    // in the vertical direction.  Otherwise the adjustment of values
    // is changing the size of the box.
    if (!a.left().isIntrinsicOrAuto() && !a.right().isIntrinsicOrAuto())
        return false;
    if (!a.top().isIntrinsicOrAuto() && !a.bottom().isIntrinsicOrAuto())
        return false;

    // One of the units is fixed or percent in both directions and stayed
    // that way in the new style.  Therefore all we are doing is moving.
    return true;
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
StyleDifference RenderStyle::diff(const RenderStyle* other, unsigned& changedContextSensitiveProperties) const
{
    changedContextSensitiveProperties = ContextSensitivePropertyNone;

#if ENABLE(SVG)
    // This is horribly inefficient.  Eventually we'll have to integrate
    // this more directly by calling: Diff svgDiff = svgStyle->diff(other)
    // and then checking svgDiff and returning from the appropriate places below.
    if (m_svgStyle != other->m_svgStyle)
        return StyleDifferenceLayout;
#endif

    if (box->width != other->box->width ||
        box->min_width != other->box->min_width ||
        box->max_width != other->box->max_width ||
        box->height != other->box->height ||
        box->min_height != other->box->min_height ||
        box->max_height != other->box->max_height)
        return StyleDifferenceLayout;

    if (box->vertical_align != other->box->vertical_align || noninherited_flags._vertical_align != other->noninherited_flags._vertical_align)
        return StyleDifferenceLayout;

    if (box->boxSizing != other->box->boxSizing)
        return StyleDifferenceLayout;

    if (surround->margin != other->surround->margin)
        return StyleDifferenceLayout;

    if (surround->padding != other->surround->padding)
        return StyleDifferenceLayout;

    if (rareNonInheritedData.get() != other->rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_appearance != other->rareNonInheritedData->m_appearance ||
            rareNonInheritedData->marginTopCollapse != other->rareNonInheritedData->marginTopCollapse ||
            rareNonInheritedData->marginBottomCollapse != other->rareNonInheritedData->marginBottomCollapse ||
            rareNonInheritedData->lineClamp != other->rareNonInheritedData->lineClamp ||
            rareNonInheritedData->textOverflow != other->rareNonInheritedData->textOverflow)
            return StyleDifferenceLayout;

        if (rareNonInheritedData->flexibleBox.get() != other->rareNonInheritedData->flexibleBox.get() &&
            *rareNonInheritedData->flexibleBox.get() != *other->rareNonInheritedData->flexibleBox.get())
            return StyleDifferenceLayout;

        if (!rareNonInheritedData->shadowDataEquivalent(*other->rareNonInheritedData.get()))
            return StyleDifferenceLayout;

        if (!rareNonInheritedData->reflectionDataEquivalent(*other->rareNonInheritedData.get()))
            return StyleDifferenceLayout;

        if (rareNonInheritedData->m_multiCol.get() != other->rareNonInheritedData->m_multiCol.get() &&
            *rareNonInheritedData->m_multiCol.get() != *other->rareNonInheritedData->m_multiCol.get())
            return StyleDifferenceLayout;

        if (rareNonInheritedData->m_transform.get() != other->rareNonInheritedData->m_transform.get() &&
            *rareNonInheritedData->m_transform.get() != *other->rareNonInheritedData->m_transform.get()) {
#if USE(ACCELERATED_COMPOSITING)
            changedContextSensitiveProperties |= ContextSensitivePropertyTransform;
            // Don't return; keep looking for another change
#else
            return StyleDifferenceLayout;
#endif
        }

#if !USE(ACCELERATED_COMPOSITING)
        if (rareNonInheritedData.get() != other->rareNonInheritedData.get()) {
            if (rareNonInheritedData->m_transformStyle3D != other->rareNonInheritedData->m_transformStyle3D ||
                rareNonInheritedData->m_backfaceVisibility != other->rareNonInheritedData->m_backfaceVisibility ||
                rareNonInheritedData->m_perspective != other->rareNonInheritedData->m_perspective ||
                rareNonInheritedData->m_perspectiveOriginX != other->rareNonInheritedData->m_perspectiveOriginX ||
                rareNonInheritedData->m_perspectiveOriginY != other->rareNonInheritedData->m_perspectiveOriginY)
                return StyleDifferenceLayout;
        }
#endif

#if ENABLE(DASHBOARD_SUPPORT)
        // If regions change, trigger a relayout to re-calc regions.
        if (rareNonInheritedData->m_dashboardRegions != other->rareNonInheritedData->m_dashboardRegions)
            return StyleDifferenceLayout;
#endif
    }

    if (rareInheritedData.get() != other->rareInheritedData.get()) {
        if (rareInheritedData->highlight != other->rareInheritedData->highlight ||
            rareInheritedData->textSizeAdjust != other->rareInheritedData->textSizeAdjust ||
            rareInheritedData->wordBreak != other->rareInheritedData->wordBreak ||
            rareInheritedData->wordWrap != other->rareInheritedData->wordWrap ||
            rareInheritedData->nbspMode != other->rareInheritedData->nbspMode ||
            rareInheritedData->khtmlLineBreak != other->rareInheritedData->khtmlLineBreak ||
            rareInheritedData->textSecurity != other->rareInheritedData->textSecurity)
            return StyleDifferenceLayout;

        if (!rareInheritedData->shadowDataEquivalent(*other->rareInheritedData.get()))
            return StyleDifferenceLayout;

        if (textStrokeWidth() != other->textStrokeWidth())
            return StyleDifferenceLayout;
    }

    if (inherited->indent != other->inherited->indent ||
        inherited->line_height != other->inherited->line_height ||
        inherited->list_style_image != other->inherited->list_style_image ||
        inherited->font != other->inherited->font ||
        inherited->horizontal_border_spacing != other->inherited->horizontal_border_spacing ||
        inherited->vertical_border_spacing != other->inherited->vertical_border_spacing ||
        inherited_flags._box_direction != other->inherited_flags._box_direction ||
        inherited_flags._visuallyOrdered != other->inherited_flags._visuallyOrdered ||
        inherited_flags._htmlHacks != other->inherited_flags._htmlHacks ||
        noninherited_flags._position != other->noninherited_flags._position ||
        noninherited_flags._floating != other->noninherited_flags._floating ||
        noninherited_flags._originalDisplay != other->noninherited_flags._originalDisplay)
        return StyleDifferenceLayout;


    if (((int)noninherited_flags._effectiveDisplay) >= TABLE) {
        if (inherited_flags._border_collapse != other->inherited_flags._border_collapse ||
            inherited_flags._empty_cells != other->inherited_flags._empty_cells ||
            inherited_flags._caption_side != other->inherited_flags._caption_side ||
            noninherited_flags._table_layout != other->noninherited_flags._table_layout)
            return StyleDifferenceLayout;

        // In the collapsing border model, 'hidden' suppresses other borders, while 'none'
        // does not, so these style differences can be width differences.
        if (inherited_flags._border_collapse &&
            ((borderTopStyle() == BHIDDEN && other->borderTopStyle() == BNONE) ||
             (borderTopStyle() == BNONE && other->borderTopStyle() == BHIDDEN) ||
             (borderBottomStyle() == BHIDDEN && other->borderBottomStyle() == BNONE) ||
             (borderBottomStyle() == BNONE && other->borderBottomStyle() == BHIDDEN) ||
             (borderLeftStyle() == BHIDDEN && other->borderLeftStyle() == BNONE) ||
             (borderLeftStyle() == BNONE && other->borderLeftStyle() == BHIDDEN) ||
             (borderRightStyle() == BHIDDEN && other->borderRightStyle() == BNONE) ||
             (borderRightStyle() == BNONE && other->borderRightStyle() == BHIDDEN)))
            return StyleDifferenceLayout;
    }

    if (noninherited_flags._effectiveDisplay == LIST_ITEM) {
        if (inherited_flags._list_style_type != other->inherited_flags._list_style_type ||
            inherited_flags._list_style_position != other->inherited_flags._list_style_position)
            return StyleDifferenceLayout;
    }

    if (inherited_flags._text_align != other->inherited_flags._text_align ||
        inherited_flags._text_transform != other->inherited_flags._text_transform ||
        inherited_flags._direction != other->inherited_flags._direction ||
        inherited_flags._white_space != other->inherited_flags._white_space ||
        noninherited_flags._clear != other->noninherited_flags._clear)
        return StyleDifferenceLayout;

    // Overflow returns a layout hint.
    if (noninherited_flags._overflowX != other->noninherited_flags._overflowX ||
        noninherited_flags._overflowY != other->noninherited_flags._overflowY)
        return StyleDifferenceLayout;

    // If our border widths change, then we need to layout.  Other changes to borders
    // only necessitate a repaint.
    if (borderLeftWidth() != other->borderLeftWidth() ||
        borderTopWidth() != other->borderTopWidth() ||
        borderBottomWidth() != other->borderBottomWidth() ||
        borderRightWidth() != other->borderRightWidth())
        return StyleDifferenceLayout;

    // If the counter directives change, trigger a relayout to re-calculate counter values and rebuild the counter node tree.
    const CounterDirectiveMap* mapA = rareNonInheritedData->m_counterDirectives.get();
    const CounterDirectiveMap* mapB = other->rareNonInheritedData->m_counterDirectives.get();
    if (!(mapA == mapB || (mapA && mapB && *mapA == *mapB)))
        return StyleDifferenceLayout;
    if (visual->counterIncrement != other->visual->counterIncrement ||
        visual->counterReset != other->visual->counterReset)
        return StyleDifferenceLayout;

    if (inherited->m_effectiveZoom != other->inherited->m_effectiveZoom)
        return StyleDifferenceLayout;

    // Make sure these left/top/right/bottom checks stay below all layout checks and above
    // all visible checks.
    if (position() != StaticPosition) {
        if (surround->offset != other->surround->offset) {
             // Optimize for the case where a positioned layer is moving but not changing size.
            if (position() == AbsolutePosition && positionedObjectMoved(surround->offset, other->surround->offset))
                return StyleDifferenceLayoutPositionedMovementOnly;

            // FIXME: We will need to do a bit of work in RenderObject/Box::setStyle before we
            // can stop doing a layout when relative positioned objects move.  In particular, we'll need
            // to update scrolling positions and figure out how to do a repaint properly of the updated layer.
            //if (other->position() == RelativePosition)
            //    return RepaintLayer;
            //else
                return StyleDifferenceLayout;
        } else if (box->z_index != other->box->z_index || box->z_auto != other->box->z_auto ||
                 visual->clip != other->visual->clip || visual->hasClip != other->visual->hasClip)
            return StyleDifferenceRepaintLayer;
    }

    if (rareNonInheritedData->opacity != other->rareNonInheritedData->opacity) {
#if USE(ACCELERATED_COMPOSITING)
        changedContextSensitiveProperties |= ContextSensitivePropertyOpacity;
        // Don't return; keep looking for another change.
#else
        return StyleDifferenceRepaintLayer;
#endif
    }

    if (rareNonInheritedData->m_mask != other->rareNonInheritedData->m_mask ||
        rareNonInheritedData->m_maskBoxImage != other->rareNonInheritedData->m_maskBoxImage)
        return StyleDifferenceRepaintLayer;

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
        return StyleDifferenceRepaint;

#if USE(ACCELERATED_COMPOSITING)
    if (rareNonInheritedData.get() != other->rareNonInheritedData.get()) {
        if (rareNonInheritedData->m_transformStyle3D != other->rareNonInheritedData->m_transformStyle3D ||
            rareNonInheritedData->m_backfaceVisibility != other->rareNonInheritedData->m_backfaceVisibility ||
            rareNonInheritedData->m_perspective != other->rareNonInheritedData->m_perspective ||
            rareNonInheritedData->m_perspectiveOriginX != other->rareNonInheritedData->m_perspectiveOriginX ||
            rareNonInheritedData->m_perspectiveOriginY != other->rareNonInheritedData->m_perspectiveOriginY)
            return StyleDifferenceRecompositeLayer;
    }
#endif

    // Cursors are not checked, since they will be set appropriately in response to mouse events,
    // so they don't need to cause any repaint or layout.

    // Animations don't need to be checked either.  We always set the new style on the RenderObject, so we will get a chance to fire off
    // the resulting transition properly.
    return StyleDifferenceEqual;
}

void RenderStyle::setClip(Length top, Length right, Length bottom, Length left)
{
    StyleVisualData* data = visual.access();
    data->clip.m_top = top;
    data->clip.m_right = right;
    data->clip.m_bottom = bottom;
    data->clip.m_left = left;
}

void RenderStyle::addCursor(CachedImage* image, const IntPoint& hotSpot)
{
    CursorData data;
    data.cursorImage = image;
    data.hotSpot = hotSpot;
    if (!inherited.access()->cursorData)
        inherited.access()->cursorData = CursorList::create();
    inherited.access()->cursorData->append(data);
}

void RenderStyle::setCursorList(PassRefPtr<CursorList> other)
{
    inherited.access()->cursorData = other;
}

void RenderStyle::clearCursorList()
{
    if (inherited->cursorData)
        inherited.access()->cursorData = 0;
}

void RenderStyle::clearContent()
{
    if (rareNonInheritedData->m_content)
        rareNonInheritedData->m_content->clear();
}

void RenderStyle::setContent(PassRefPtr<StyleImage> image, bool add)
{
    if (!image)
        return; // The object is null. Nothing to do. Just bail.

    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->next())
        lastContent = lastContent->next();

    bool reuseContent = !add;
    ContentData* newContentData;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->setNext(newContentData);
    else
        content.set(newContentData);

    newContentData->setImage(image);
}

void RenderStyle::setContent(PassRefPtr<StringImpl> s, bool add)
{
    if (!s)
        return; // The string is null. Nothing to do. Just bail.

    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->next())
        lastContent = lastContent->next();

    bool reuseContent = !add;
    if (add && lastContent) {
        if (lastContent->isText()) {
            // We can augment the existing string and share this ContentData node.
            String newStr = lastContent->text();
            newStr.append(s.get());
            lastContent->setText(newStr.impl());
            return;
        }
    }

    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->setNext(newContentData);
    else
        content.set(newContentData);

    newContentData->setText(s);
}

void RenderStyle::setContent(CounterContent* c, bool add)
{
    if (!c)
        return;

    OwnPtr<ContentData>& content = rareNonInheritedData.access()->m_content;
    ContentData* lastContent = content.get();
    while (lastContent && lastContent->next())
        lastContent = lastContent->next();

    bool reuseContent = !add;
    ContentData* newContentData = 0;
    if (reuseContent && content) {
        content->clear();
        newContentData = content.release();
    } else
        newContentData = new ContentData;

    if (lastContent && !reuseContent)
        lastContent->setNext(newContentData);
    else
        content.set(newContentData);

    newContentData->setCounter(c);
}

void RenderStyle::applyTransform(TransformationMatrix& transform, const IntSize& borderBoxSize, ApplyTransformOrigin applyOrigin) const
{
    // transform-origin brackets the transform with translate operations.
    // Optimize for the case where the only transform is a translation, since the transform-origin is irrelevant
    // in that case.
    bool applyTransformOrigin = false;
    unsigned s = rareNonInheritedData->m_transform->m_operations.operations().size();
    unsigned i;
    if (applyOrigin == IncludeTransformOrigin) {
        for (i = 0; i < s; i++) {
            TransformOperation::OperationType type = rareNonInheritedData->m_transform->m_operations.operations()[i]->getOperationType();
            if (type != TransformOperation::TRANSLATE_X &&
                    type != TransformOperation::TRANSLATE_Y &&
                    type != TransformOperation::TRANSLATE && 
                    type != TransformOperation::TRANSLATE_Z && 
                    type != TransformOperation::TRANSLATE_3D
                    ) {
                applyTransformOrigin = true;
                break;
            }
        }
    }

    if (applyTransformOrigin) {
        transform.translate3d(transformOriginX().calcFloatValue(borderBoxSize.width()), transformOriginY().calcFloatValue(borderBoxSize.height()), transformOriginZ());
    }

    for (i = 0; i < s; i++)
        rareNonInheritedData->m_transform->m_operations.operations()[i]->apply(transform, borderBoxSize);

    if (applyTransformOrigin) {
        transform.translate3d(-transformOriginX().calcFloatValue(borderBoxSize.width()), -transformOriginY().calcFloatValue(borderBoxSize.height()), -transformOriginZ());
    }
}

#if ENABLE(XBL)
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

    val->next = rareData->textShadow;
    rareData->textShadow = val;
}

void RenderStyle::setBoxShadow(ShadowData* shadowData, bool add)
{
    StyleRareNonInheritedData* rareData = rareNonInheritedData.access();
    if (!add) {
        rareData->m_boxShadow.set(shadowData);
        return;
    }

    shadowData->next = rareData->m_boxShadow.release();
    rareData->m_boxShadow.set(shadowData);
}

const CounterDirectiveMap* RenderStyle::counterDirectives() const
{
    return rareNonInheritedData->m_counterDirectives.get();
}

CounterDirectiveMap& RenderStyle::accessCounterDirectives()
{
    OwnPtr<CounterDirectiveMap>& map = rareNonInheritedData.access()->m_counterDirectives;
    if (!map)
        map.set(new CounterDirectiveMap);
    return *map.get();
}

#if ENABLE(DASHBOARD_SUPPORT)
const Vector<StyleDashboardRegion>& RenderStyle::initialDashboardRegions()
{
    DEFINE_STATIC_LOCAL(Vector<StyleDashboardRegion>, emptyList, ());
    return emptyList;
}

const Vector<StyleDashboardRegion>& RenderStyle::noneDashboardRegions()
{
    DEFINE_STATIC_LOCAL(Vector<StyleDashboardRegion>, noneList, ());
    static bool noneListInitialized = false;

    if (!noneListInitialized) {
        StyleDashboardRegion region;
        region.label = "";
        region.offset.m_top  = Length();
        region.offset.m_right = Length();
        region.offset.m_bottom = Length();
        region.offset.m_left = Length();
        region.type = StyleDashboardRegion::None;
        noneList.append(region);
        noneListInitialized = true;
    }
    return noneList;
}
#endif

void RenderStyle::adjustAnimations()
{
    AnimationList* animationList = rareNonInheritedData->m_animations.get();
    if (!animationList)
        return;

    // Get rid of empty animations and anything beyond them
    for (size_t i = 0; i < animationList->size(); ++i) {
        if (animationList->animation(i)->isEmpty()) {
            animationList->resize(i);
            break;
        }
    }

    if (animationList->isEmpty()) {
        clearAnimations();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    animationList->fillUnsetProperties();
}

void RenderStyle::adjustTransitions()
{
    AnimationList* transitionList = rareNonInheritedData->m_transitions.get();
    if (!transitionList)
        return;

    // Get rid of empty transitions and anything beyond them
    for (size_t i = 0; i < transitionList->size(); ++i) {
        if (transitionList->animation(i)->isEmpty()) {
            transitionList->resize(i);
            break;
        }
    }

    if (transitionList->isEmpty()) {
        clearTransitions();
        return;
    }

    // Repeat patterns into layers that don't have some properties set.
    transitionList->fillUnsetProperties();

    // Make sure there are no duplicate properties. This is an O(n^2) algorithm
    // but the lists tend to be very short, so it is probably ok
    for (size_t i = 0; i < transitionList->size(); ++i) {
        for (size_t j = i+1; j < transitionList->size(); ++j) {
            if (transitionList->animation(i)->property() == transitionList->animation(j)->property()) {
                // toss i
                transitionList->remove(i);
                j = i;
            }
        }
    }
}

AnimationList* RenderStyle::accessAnimations()
{
    if (!rareNonInheritedData.access()->m_animations)
        rareNonInheritedData.access()->m_animations.set(new AnimationList());
    return rareNonInheritedData->m_animations.get();
}

AnimationList* RenderStyle::accessTransitions()
{
    if (!rareNonInheritedData.access()->m_transitions)
        rareNonInheritedData.access()->m_transitions.set(new AnimationList());
    return rareNonInheritedData->m_transitions.get();
}

const Animation* RenderStyle::transitionForProperty(int property) const
{
    if (transitions()) {
        for (size_t i = 0; i < transitions()->size(); ++i) {
            const Animation* p = transitions()->animation(i);
            if (p->property() == cAnimateAll || p->property() == property) {
                return p;
            }
        }
    }
    return 0;
}

void RenderStyle::setBlendedFontSize(int size)
{
    FontDescription desc(fontDescription());
    desc.setSpecifiedSize(size);
    desc.setComputedSize(size);
    setFontDescription(desc);
    font().update(font().fontSelector());
}

} // namespace WebCore
