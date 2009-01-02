/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2004 Allan Sandfeld Jensen (kde@carewolf.com)
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
#include "RenderObject.h"

#include "AXObjectCache.h"
#include "AffineTransform.h"
#include "AnimationController.h"
#include "CSSStyleSelector.h"
#include "CachedImage.h"
#include "Chrome.h"
#include "Document.h"
#include "Element.h"
#include "EventHandler.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLNames.h"
#include "HTMLOListElement.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "KURL.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "Position.h"
#include "RenderArena.h"
#include "RenderCounter.h"
#include "RenderFlexibleBox.h"
#include "RenderImageGeneratedContent.h"
#include "RenderInline.h"
#include "RenderListItem.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderView.h"
#include "SelectionController.h"
#include "TextResourceDecoder.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/RefCountedLeakCounter.h>

#if ENABLE(WML)
#include "WMLNames.h"
#endif

using namespace std;

namespace WebCore {

using namespace HTMLNames;

#ifndef NDEBUG
static void* baseOfRenderObjectBeingDeleted;
#endif

bool RenderObject::s_affectsParentBlock = false;

void* RenderObject::operator new(size_t sz, RenderArena* renderArena) throw()
{
    return renderArena->allocate(sz);
}

void RenderObject::operator delete(void* ptr, size_t sz)
{
    ASSERT(baseOfRenderObjectBeingDeleted == ptr);

    // Stash size where destroy can find it.
    *(size_t *)ptr = sz;
}

RenderObject* RenderObject::createObject(Node* node, RenderStyle* style)
{
    Document* doc = node->document();
    RenderArena* arena = doc->renderArena();

    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    const ContentData* contentData = style->contentData();
    if (contentData && !contentData->m_next && contentData->m_type == CONTENT_OBJECT && doc != node) {
        RenderImageGeneratedContent* image = new (arena) RenderImageGeneratedContent(node);
        image->setStyle(style);
        if (StyleImage* styleImage = contentData->m_content.m_image)
            image->setStyleImage(styleImage);
        return image;
    }

    RenderObject* o = 0;

    switch (style->display()) {
        case NONE:
            break;
        case INLINE:
            o = new (arena) RenderInline(node);
            break;
        case BLOCK:
            o = new (arena) RenderBlock(node);
            break;
        case INLINE_BLOCK:
            o = new (arena) RenderBlock(node);
            break;
        case LIST_ITEM:
            o = new (arena) RenderListItem(node);
            break;
        case RUN_IN:
        case COMPACT:
            o = new (arena) RenderBlock(node);
            break;
        case TABLE:
        case INLINE_TABLE:
            o = new (arena) RenderTable(node);
            break;
        case TABLE_ROW_GROUP:
        case TABLE_HEADER_GROUP:
        case TABLE_FOOTER_GROUP:
            o = new (arena) RenderTableSection(node);
            break;
        case TABLE_ROW:
            o = new (arena) RenderTableRow(node);
            break;
        case TABLE_COLUMN_GROUP:
        case TABLE_COLUMN:
            o = new (arena) RenderTableCol(node);
            break;
        case TABLE_CELL:
            o = new (arena) RenderTableCell(node);
            break;
        case TABLE_CAPTION:
            o = new (arena) RenderBlock(node);
            break;
        case BOX:
        case INLINE_BOX:
            o = new (arena) RenderFlexibleBox(node);
            break;
    }

    return o;
}

#ifndef NDEBUG 
static WTF::RefCountedLeakCounter renderObjectCounter("RenderObject");
#endif

RenderObject::RenderObject(Node* node)
    : CachedResourceClient()
    , m_style(0)
    , m_node(node)
    , m_parent(0)
    , m_previous(0)
    , m_next(0)
#ifndef NDEBUG
    , m_hasAXObject(false)
#endif
    , m_verticalPosition(PositionUndefined)
    , m_needsLayout(false)
    , m_needsPositionedMovementLayout(false)
    , m_normalChildNeedsLayout(false)
    , m_posChildNeedsLayout(false)
    , m_prefWidthsDirty(false)
    , m_floating(false)
    , m_positioned(false)
    , m_relPositioned(false)
    , m_paintBackground(false)
    , m_isAnonymous(node == node->document())
    , m_isText(false)
    , m_inline(true)
    , m_replaced(false)
    , m_isDragging(false)
    , m_hasLayer(false)
    , m_hasOverflowClip(false)
    , m_hasTransform(false)
    , m_hasReflection(false)
    , m_hasOverrideSize(false)
    , m_hasCounterNodeMap(false)
    , m_everHadLayout(false)
{
#ifndef NDEBUG
    renderObjectCounter.increment();
#endif
}

RenderObject::~RenderObject()
{
    ASSERT(!node() || documentBeingDestroyed() || !document()->frame()->view() || document()->frame()->view()->layoutRoot() != this);
#ifndef NDEBUG
    ASSERT(!m_hasAXObject);
    renderObjectCounter.decrement();
#endif
}

bool RenderObject::isDescendantOf(const RenderObject* obj) const
{
    for (const RenderObject* r = this; r; r = r->m_parent) {
        if (r == obj)
            return true;
    }
    return false;
}

bool RenderObject::isBody() const
{
    return node()->hasTagName(bodyTag);
}

bool RenderObject::isHR() const
{
    return element() && element()->hasTagName(hrTag);
}

bool RenderObject::isHTMLMarquee() const
{
    return element() && element()->renderer() == this && element()->hasTagName(marqueeTag);
}

bool RenderObject::canHaveChildren() const
{
    return false;
}

RenderFlow* RenderObject::continuation() const
{
    return 0;
}

bool RenderObject::isInlineContinuation() const
{
    return false;
}

void RenderObject::addChild(RenderObject*, RenderObject*)
{
    ASSERT_NOT_REACHED();
}

RenderObject* RenderObject::removeChildNode(RenderObject*, bool)
{
    ASSERT_NOT_REACHED();
    return 0;
}

void RenderObject::removeChild(RenderObject*)
{
    ASSERT_NOT_REACHED();
}

void RenderObject::moveChildNode(RenderObject*)
{
    ASSERT_NOT_REACHED();
}

void RenderObject::appendChildNode(RenderObject*, bool)
{
    ASSERT_NOT_REACHED();
}

void RenderObject::insertChildNode(RenderObject*, RenderObject*, bool)
{
    ASSERT_NOT_REACHED();
}

RenderObject* RenderObject::nextInPreOrder() const
{
    if (RenderObject* o = firstChild())
        return o;

    return nextInPreOrderAfterChildren();
}

RenderObject* RenderObject::nextInPreOrderAfterChildren() const
{
    RenderObject* o;
    if (!(o = nextSibling())) {
        o = parent();
        while (o && !o->nextSibling())
            o = o->parent();
        if (o)
            o = o->nextSibling();
    }

    return o;
}

RenderObject* RenderObject::nextInPreOrder(RenderObject* stayWithin) const
{
    if (RenderObject* o = firstChild())
        return o;

    return nextInPreOrderAfterChildren(stayWithin);
}

RenderObject* RenderObject::nextInPreOrderAfterChildren(RenderObject* stayWithin) const
{
    if (this == stayWithin)
        return 0;

    RenderObject* o;
    if (!(o = nextSibling())) {
        o = parent();
        while (o && !o->nextSibling()) {
            if (o == stayWithin)
                return 0;
            o = o->parent();
        }
        if (o)
            o = o->nextSibling();
    }

    return o;
}

RenderObject* RenderObject::previousInPreOrder() const
{
    if (RenderObject* o = previousSibling()) {
        while (o->lastChild())
            o = o->lastChild();
        return o;
    }

    return parent();
}

RenderObject* RenderObject::childAt(unsigned index) const
{
    RenderObject* child = firstChild();
    for (unsigned i = 0; child && i < index; i++)
        child = child->nextSibling();
    return child;
}

bool RenderObject::isEditable() const
{
    RenderText* textRenderer = 0;
    if (isText())
        textRenderer = static_cast<RenderText*>(const_cast<RenderObject*>(this));

    return style()->visibility() == VISIBLE &&
        element() && element()->isContentEditable() &&
        ((isBlockFlow() && !firstChild()) ||
        isReplaced() ||
        isBR() ||
        (textRenderer && textRenderer->firstTextBox()));
}

RenderObject* RenderObject::firstLeafChild() const
{
    RenderObject* r = firstChild();
    while (r) {
        RenderObject* n = 0;
        n = r->firstChild();
        if (!n)
            break;
        r = n;
    }
    return r;
}

RenderObject* RenderObject::lastLeafChild() const
{
    RenderObject* r = lastChild();
    while (r) {
        RenderObject* n = 0;
        n = r->lastChild();
        if (!n)
            break;
        r = n;
    }
    return r;
}

static void addLayers(RenderObject* obj, RenderLayer* parentLayer, RenderObject*& newObject,
                      RenderLayer*& beforeChild)
{
    if (obj->hasLayer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject.  We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = 0;
        }
        parentLayer->addChild(obj->layer(), beforeChild);
        return;
    }

    for (RenderObject* curr = obj->firstChild(); curr; curr = curr->nextSibling())
        addLayers(curr, parentLayer, newObject, beforeChild);
}

void RenderObject::addLayers(RenderLayer* parentLayer, RenderObject* newObject)
{
    if (!parentLayer)
        return;

    RenderObject* object = newObject;
    RenderLayer* beforeChild = 0;
    WebCore::addLayers(this, parentLayer, object, beforeChild);
}

void RenderObject::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->removeLayers(parentLayer);
}

void RenderObject::moveLayers(RenderLayer* oldParent, RenderLayer* newParent)
{
    if (!newParent)
        return;

    if (hasLayer()) {
        if (oldParent)
            oldParent->removeChild(layer());
        newParent->addChild(layer());
        return;
    }

    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->moveLayers(oldParent, newParent);
}

RenderLayer* RenderObject::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint,
                                         bool checkParent)
{
    // Error check the parent layer passed in.  If it's null, we can't find anything.
    if (!parentLayer)
        return 0;

    // Step 1: If our layer is a child of the desired parent, then return our layer.
    RenderLayer* ourLayer = layer();
    if (ourLayer && ourLayer->parent() == parentLayer)
        return ourLayer;

    // Step 2: If we don't have a layer, or our layer is the desired parent, then descend
    // into our siblings trying to find the next layer whose parent is the desired parent.
    if (!ourLayer || ourLayer == parentLayer) {
        for (RenderObject* curr = startPoint ? startPoint->nextSibling() : firstChild();
             curr; curr = curr->nextSibling()) {
            RenderLayer* nextLayer = curr->findNextLayer(parentLayer, 0, false);
            if (nextLayer)
                return nextLayer;
        }
    }

    // Step 3: If our layer is the desired parent layer, then we're finished.  We didn't
    // find anything.
    if (parentLayer == ourLayer)
        return 0;

    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && parent())
        return parent()->findNextLayer(parentLayer, this, true);

    return 0;
}

RenderLayer* RenderObject::enclosingLayer() const
{
    const RenderObject* curr = this;
    while (curr) {
        RenderLayer* layer = curr->layer();
        if (layer)
            return layer;
        curr = curr->parent();
    }
    return 0;
}

bool RenderObject::requiresLayer()
{
    return isRoot() || isPositioned() || isRelPositioned() || isTransparent() || hasOverflowClip() || hasTransform() || hasMask() || hasReflection();
}

RenderBlock* RenderObject::firstLineBlock() const
{
    return 0;
}

int RenderObject::offsetLeft() const
{
    RenderObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int x = xPos() - offsetPar->borderLeft();
    if (!isPositioned()) {
        if (isRelPositioned())
            x += static_cast<const RenderBox*>(this)->relativePositionOffsetX();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            x += curr->xPos();
            curr = curr->parent();
        }
        if (offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            x += offsetPar->xPos();
    }
    return x;
}

int RenderObject::offsetTop() const
{
    RenderObject* offsetPar = offsetParent();
    if (!offsetPar)
        return 0;
    int y = yPos() - borderTopExtra() - offsetPar->borderTop();
    if (!isPositioned()) {
        if (isRelPositioned())
            y += static_cast<const RenderBox*>(this)->relativePositionOffsetY();
        RenderObject* curr = parent();
        while (curr && curr != offsetPar) {
            if (!curr->isTableRow())
                y += curr->yPos();
            curr = curr->parent();
        }
        if (offsetPar->isBody() && !offsetPar->isRelPositioned() && !offsetPar->isPositioned())
            y += offsetPar->yPos();
    }
    return y;
}

RenderObject* RenderObject::offsetParent() const
{
    // FIXME: It feels like this function could almost be written using containing blocks.
    if (isBody())
        return 0;

    bool skipTables = isPositioned() || isRelPositioned();
    float currZoom = style()->effectiveZoom();
    RenderObject* curr = parent();
    while (curr && (!curr->element() ||
                    (!curr->isPositioned() && !curr->isRelPositioned() && !curr->isBody()))) {
        Node* element = curr->element();
        if (!skipTables && element) {
            bool isTableElement = element->hasTagName(tableTag) ||
                                  element->hasTagName(tdTag) ||
                                  element->hasTagName(thTag);

#if ENABLE(WML)
            if (!isTableElement && element->isWMLElement())
                isTableElement = element->hasTagName(WMLNames::tableTag) ||
                                 element->hasTagName(WMLNames::tdTag);
#endif

            if (isTableElement)
                break;
        }

        float newZoom = curr->style()->effectiveZoom();
        if (currZoom != newZoom)
            break;
        currZoom = newZoom;
        curr = curr->parent();
    }
    return curr;
}

int RenderObject::verticalScrollbarWidth() const
{
    return includeVerticalScrollbarSize() ? layer()->verticalScrollbarWidth() : 0;
}

int RenderObject::horizontalScrollbarHeight() const
{
    return includeHorizontalScrollbarSize() ? layer()->horizontalScrollbarHeight() : 0;
}

// More IE extensions.  clientWidth and clientHeight represent the interior of an object
// excluding border and scrollbar.
int RenderObject::clientWidth() const
{
    return width() - borderLeft() - borderRight() - verticalScrollbarWidth();
}

int RenderObject::clientHeight() const
{
    return height() - borderTop() - borderBottom() - horizontalScrollbarHeight();
}

// scrollWidth/scrollHeight will be the same as clientWidth/clientHeight unless the
// object has overflow:hidden/scroll/auto specified and also has overflow.
int RenderObject::scrollWidth() const
{
    return hasOverflowClip() ? layer()->scrollWidth() : overflowWidth();
}

int RenderObject::scrollHeight() const
{
    return hasOverflowClip() ? layer()->scrollHeight() : overflowHeight();
}

int RenderObject::scrollLeft() const
{
    return hasOverflowClip() ? layer()->scrollXOffset() : 0;
}

int RenderObject::scrollTop() const
{
    return hasOverflowClip() ? layer()->scrollYOffset() : 0;
}

void RenderObject::setScrollLeft(int newLeft)
{
    if (hasOverflowClip())
        layer()->scrollToXOffset(newLeft);
}

void RenderObject::setScrollTop(int newTop)
{
    if (hasOverflowClip())
        layer()->scrollToYOffset(newTop);
}

bool RenderObject::scroll(ScrollDirection direction, ScrollGranularity granularity, float multiplier)
{
    RenderLayer* l = layer();
    if (l && l->scroll(direction, granularity, multiplier))
        return true;
    RenderBlock* b = containingBlock();
    if (b && !b->isRenderView())
        return b->scroll(direction, granularity, multiplier);
    return false;
}
    
bool RenderObject::canBeProgramaticallyScrolled(bool scrollToAnchor) const
{
    if (!layer())
        return false;

    return (hasOverflowClip() && (scrollsOverflow() || (node() && node()->isContentEditable()))) || (node() && node()->isDocumentNode());
}

void RenderObject::autoscroll()
{
    layer()->autoscroll();
}

void RenderObject::panScroll(const IntPoint& source)
{
    layer()->panScrollFromPoint(source);
}

bool RenderObject::hasStaticX() const
{
    return (style()->left().isAuto() && style()->right().isAuto()) || style()->left().isStatic() || style()->right().isStatic();
}

bool RenderObject::hasStaticY() const
{
    return (style()->top().isAuto() && style()->bottom().isAuto()) || style()->top().isStatic();
}

void RenderObject::markAllDescendantsWithFloatsForLayout(RenderObject*)
{
}

void RenderObject::setPrefWidthsDirty(bool b, bool markParents)
{
    bool alreadyDirty = m_prefWidthsDirty;
    m_prefWidthsDirty = b;
    if (b && !alreadyDirty && markParents && (isText() || (style()->position() != FixedPosition && style()->position() != AbsolutePosition)))
        invalidateContainerPrefWidths();
}

void RenderObject::invalidateContainerPrefWidths()
{
    // In order to avoid pathological behavior when inlines are deeply nested, we do include them
    // in the chain that we mark dirty (even though they're kind of irrelevant).
    RenderObject* o = isTableCell() ? containingBlock() : container();
    while (o && !o->m_prefWidthsDirty) {
        o->m_prefWidthsDirty = true;
        if (o->style()->position() == FixedPosition || o->style()->position() == AbsolutePosition)
            // A positioned object has no effect on the min/max width of its containing block ever.
            // We can optimize this case and not go up any further.
            break;
        o = o->isTableCell() ? o->containingBlock() : o->container();
    }
}

void RenderObject::setNeedsLayout(bool b, bool markParents)
{
    bool alreadyNeededLayout = m_needsLayout;
    m_needsLayout = b;
    if (b) {
        if (!alreadyNeededLayout) {
            if (markParents)
                markContainingBlocksForLayout();
            if (hasLayer())
                layer()->setNeedsFullRepaint();
        }
    } else {
        m_everHadLayout = true;
        m_posChildNeedsLayout = false;
        m_normalChildNeedsLayout = false;
        m_needsPositionedMovementLayout = false;
    }
}

void RenderObject::setChildNeedsLayout(bool b, bool markParents)
{
    bool alreadyNeededLayout = m_normalChildNeedsLayout;
    m_normalChildNeedsLayout = b;
    if (b) {
        if (!alreadyNeededLayout && markParents)
            markContainingBlocksForLayout();
    } else {
        m_posChildNeedsLayout = false;
        m_normalChildNeedsLayout = false;
        m_needsPositionedMovementLayout = false;
    }
}

void RenderObject::setNeedsPositionedMovementLayout()
{
    bool alreadyNeededLayout = needsLayout();
    m_needsPositionedMovementLayout = true;
    if (!alreadyNeededLayout) {
        markContainingBlocksForLayout();
        if (hasLayer())
            layer()->setNeedsFullRepaint();
    }
}

static inline bool objectIsRelayoutBoundary(const RenderObject *obj) 
{
    // FIXME: In future it may be possible to broaden this condition in order to improve performance.
    // Table cells are excluded because even when their CSS height is fixed, their height()
    // may depend on their contents.
    return obj->isTextField() || obj->isTextArea()
        || obj->hasOverflowClip() && !obj->style()->width().isIntrinsicOrAuto() && !obj->style()->height().isIntrinsicOrAuto() && !obj->style()->height().isPercent() && !obj->isTableCell()
#if ENABLE(SVG)
           || obj->isSVGRoot()
#endif
           ;
}
    
void RenderObject::markContainingBlocksForLayout(bool scheduleRelayout, RenderObject* newRoot)
{
    ASSERT(!scheduleRelayout || !newRoot);

    RenderObject* o = container();
    RenderObject* last = this;

    while (o) {
        if (!last->isText() && (last->style()->position() == FixedPosition || last->style()->position() == AbsolutePosition)) {
            if (last->hasStaticY()) {
                RenderObject* parent = last->parent();
                if (!parent->normalChildNeedsLayout()) {
                    parent->setChildNeedsLayout(true, false);
                    if (parent != newRoot)
                        parent->markContainingBlocksForLayout(scheduleRelayout, newRoot);
                }
            }
            if (o->m_posChildNeedsLayout)
                return;
            o->m_posChildNeedsLayout = true;
        } else {
            if (o->m_normalChildNeedsLayout)
                return;
            o->m_normalChildNeedsLayout = true;
        }

        if (o == newRoot)
            return;

        last = o;
        if (scheduleRelayout && objectIsRelayoutBoundary(last))
            break;
        o = o->container();
    }

    if (scheduleRelayout)
        last->scheduleRelayout();
}

RenderBlock* RenderObject::containingBlock() const
{
    if (isTableCell()) {
        const RenderTableCell* cell = static_cast<const RenderTableCell*>(this);
        if (parent() && cell->section())
            return cell->table();
        return 0;
    }

    if (isRenderView())
        return const_cast<RenderBlock*>(static_cast<const RenderBlock*>(this));

    RenderObject* o = parent();
    if (!isText() && m_style->position() == FixedPosition) {
        while (o && !o->isRenderView() && !(o->hasTransform() && o->isRenderBlock()))
            o = o->parent();
    } else if (!isText() && m_style->position() == AbsolutePosition) {
        while (o && (o->style()->position() == StaticPosition || (o->isInline() && !o->isReplaced())) && !o->isRenderView() && !(o->hasTransform() && o->isRenderBlock())) {
            // For relpositioned inlines, we return the nearest enclosing block.  We don't try
            // to return the inline itself.  This allows us to avoid having a positioned objects
            // list in all RenderInlines and lets us return a strongly-typed RenderBlock* result
            // from this method.  The container() method can actually be used to obtain the
            // inline directly.
            if (o->style()->position() == RelativePosition && o->isInline() && !o->isReplaced())
                return o->containingBlock();
            o = o->parent();
        }
    } else {
        while (o && ((o->isInline() && !o->isReplaced()) || o->isTableRow() || o->isTableSection()
                     || o->isTableCol() || o->isFrameSet() || o->isMedia()
#if ENABLE(SVG)
                     || o->isSVGContainer() || o->isSVGRoot()
#endif
                     ))
            o = o->parent();
    }

    if (!o || !o->isRenderBlock())
        return 0; // Probably doesn't happen any more, but leave just in case. -dwh

    return static_cast<RenderBlock*>(o);
}

int RenderObject::containingBlockWidth() const
{
    // FIXME ?
    return containingBlock()->availableWidth();
}

int RenderObject::containingBlockHeight() const
{
    // FIXME ?
    return containingBlock()->contentHeight();
}

static bool mustRepaintFillLayers(const RenderObject* renderer, const FillLayer* layer)
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer->next())
        return true;

    // Make sure we have a valid image.
    StyleImage* img = layer->image();
    bool shouldPaintBackgroundImage = img && img->canRender(renderer->style()->effectiveZoom());

    // These are always percents or auto.
    if (shouldPaintBackgroundImage &&
        (!layer->xPosition().isZero() || !layer->yPosition().isZero() ||
         layer->size().width().isPercent() || layer->size().height().isPercent()))
        // The image will shift unpredictably if the size changes.
        return true;

    return false;
}

bool RenderObject::mustRepaintBackgroundOrBorder() const
{
    if (hasMask() && mustRepaintFillLayers(this, style()->maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!hasBoxDecorations())
        return false;

    if (mustRepaintFillLayers(this, style()->backgroundLayers()))
        return true;
     
    // Our fill layers are ok.  Let's check border.
    if (style()->hasBorder()) {
        // Border images are not ok.
        StyleImage* borderImage = style()->borderImage().image();
        bool shouldPaintBorderImage = borderImage && borderImage->canRender(style()->effectiveZoom());

        // If the image hasn't loaded, we're still using the normal border style.
        if (shouldPaintBorderImage && borderImage->isLoaded())
            return true;
    }

    return false;
}

void RenderObject::drawBorderArc(GraphicsContext* graphicsContext, int x, int y, float thickness, IntSize radius,
                                 int angleStart, int angleSpan, BorderSide s, Color c, const Color& textColor,
                                 EBorderStyle style, bool firstCorner)
{
    if ((style == DOUBLE && thickness / 2 < 3) || ((style == RIDGE || style == GROOVE) && thickness / 2 < 2))
        style = SOLID;

    if (!c.isValid()) {
        if (style == INSET || style == OUTSET || style == RIDGE || style == GROOVE)
            c.setRGB(238, 238, 238);
        else
            c = textColor;
    }

    switch (style) {
        case BNONE:
        case BHIDDEN:
            return;
        case DOTTED:
        case DASHED:
            graphicsContext->setStrokeColor(c);
            graphicsContext->setStrokeStyle(style == DOTTED ? DottedStroke : DashedStroke);
            graphicsContext->setStrokeThickness(thickness);
            graphicsContext->strokeArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), angleStart, angleSpan);
            break;
        case DOUBLE: {
            float third = thickness / 3.0f;
            float innerThird = (thickness + 1.0f) / 6.0f;
            int shiftForInner = static_cast<int>(innerThird * 2.5f);

            int outerY = y;
            int outerHeight = radius.height() * 2;
            int innerX = x + shiftForInner;
            int innerY = y + shiftForInner;
            int innerWidth = (radius.width() - shiftForInner) * 2;
            int innerHeight = (radius.height() - shiftForInner) * 2;
            if (innerThird > 1 && (s == BSTop || (firstCorner && (s == BSLeft || s == BSRight)))) {
                outerHeight += 2;
                innerHeight += 2;
            }

            graphicsContext->setStrokeStyle(SolidStroke);
            graphicsContext->setStrokeColor(c);
            graphicsContext->setStrokeThickness(third);
            graphicsContext->strokeArc(IntRect(x, outerY, radius.width() * 2, outerHeight), angleStart, angleSpan);
            graphicsContext->setStrokeThickness(innerThird > 2 ? innerThird - 1 : innerThird);
            graphicsContext->strokeArc(IntRect(innerX, innerY, innerWidth, innerHeight), angleStart, angleSpan);
            break;
        }
        case GROOVE:
        case RIDGE: {
            Color c2;
            if ((style == RIDGE && (s == BSTop || s == BSLeft)) ||
                    (style == GROOVE && (s == BSBottom || s == BSRight)))
                c2 = c.dark();
            else {
                c2 = c;
                c = c.dark();
            }

            graphicsContext->setStrokeStyle(SolidStroke);
            graphicsContext->setStrokeColor(c);
            graphicsContext->setStrokeThickness(thickness);
            graphicsContext->strokeArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), angleStart, angleSpan);

            float halfThickness = (thickness + 1.0f) / 4.0f;
            int shiftForInner = static_cast<int>(halfThickness * 1.5f);
            graphicsContext->setStrokeColor(c2);
            graphicsContext->setStrokeThickness(halfThickness > 2 ? halfThickness - 1 : halfThickness);
            graphicsContext->strokeArc(IntRect(x + shiftForInner, y + shiftForInner, (radius.width() - shiftForInner) * 2,
                                       (radius.height() - shiftForInner) * 2), angleStart, angleSpan);
            break;
        }
        case INSET:
            if (s == BSTop || s == BSLeft)
                c = c.dark();
        case OUTSET:
            if (style == OUTSET && (s == BSBottom || s == BSRight))
                c = c.dark();
        case SOLID:
            graphicsContext->setStrokeStyle(SolidStroke);
            graphicsContext->setStrokeColor(c);
            graphicsContext->setStrokeThickness(thickness);
            graphicsContext->strokeArc(IntRect(x, y, radius.width() * 2, radius.height() * 2), angleStart, angleSpan);
            break;
    }
}

void RenderObject::drawBorder(GraphicsContext* graphicsContext, int x1, int y1, int x2, int y2,
                              BorderSide s, Color c, const Color& textcolor, EBorderStyle style,
                              int adjbw1, int adjbw2)
{
    int width = (s == BSTop || s == BSBottom ? y2 - y1 : x2 - x1);

    if (style == DOUBLE && width < 3)
        style = SOLID;

    if (!c.isValid()) {
        if (style == INSET || style == OUTSET || style == RIDGE || style == GROOVE)
            c.setRGB(238, 238, 238);
        else
            c = textcolor;
    }

    switch (style) {
        case BNONE:
        case BHIDDEN:
            return;
        case DOTTED:
        case DASHED:
            graphicsContext->setStrokeColor(c);
            graphicsContext->setStrokeThickness(width);
            graphicsContext->setStrokeStyle(style == DASHED ? DashedStroke : DottedStroke);

            if (width > 0)
                switch (s) {
                    case BSBottom:
                    case BSTop:
                        graphicsContext->drawLine(IntPoint(x1, (y1 + y2) / 2), IntPoint(x2, (y1 + y2) / 2));
                        break;
                    case BSRight:
                    case BSLeft:
                        graphicsContext->drawLine(IntPoint((x1 + x2) / 2, y1), IntPoint((x1 + x2) / 2, y2));
                        break;
                }
            break;
        case DOUBLE: {
            int third = (width + 1) / 3;

            if (adjbw1 == 0 && adjbw2 == 0) {
                graphicsContext->setStrokeStyle(NoStroke);
                graphicsContext->setFillColor(c);
                switch (s) {
                    case BSTop:
                    case BSBottom:
                        graphicsContext->drawRect(IntRect(x1, y1, x2 - x1, third));
                        graphicsContext->drawRect(IntRect(x1, y2 - third, x2 - x1, third));
                        break;
                    case BSLeft:
                        graphicsContext->drawRect(IntRect(x1, y1 + 1, third, y2 - y1 - 1));
                        graphicsContext->drawRect(IntRect(x2 - third, y1 + 1, third, y2 - y1 - 1));
                        break;
                    case BSRight:
                        graphicsContext->drawRect(IntRect(x1, y1 + 1, third, y2 - y1 - 1));
                        graphicsContext->drawRect(IntRect(x2 - third, y1 + 1, third, y2 - y1 - 1));
                        break;
                }
            } else {
                int adjbw1bigthird = ((adjbw1 > 0) ? adjbw1 + 1 : adjbw1 - 1) / 3;
                int adjbw2bigthird = ((adjbw2 > 0) ? adjbw2 + 1 : adjbw2 - 1) / 3;

                switch (s) {
                    case BSTop:
                        drawBorder(graphicsContext, x1 + max((-adjbw1 * 2 + 1) / 3, 0),
                                   y1, x2 - max((-adjbw2 * 2 + 1) / 3, 0), y1 + third,
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        drawBorder(graphicsContext, x1 + max((adjbw1 * 2 + 1) / 3, 0),
                                   y2 - third, x2 - max((adjbw2 * 2 + 1) / 3, 0), y2,
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        break;
                    case BSLeft:
                        drawBorder(graphicsContext, x1, y1 + max((-adjbw1 * 2 + 1) / 3, 0),
                                   x1 + third, y2 - max((-adjbw2 * 2 + 1) / 3, 0),
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        drawBorder(graphicsContext, x2 - third, y1 + max((adjbw1 * 2 + 1) / 3, 0),
                                   x2, y2 - max((adjbw2 * 2 + 1) / 3, 0),
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        break;
                    case BSBottom:
                        drawBorder(graphicsContext, x1 + max((adjbw1 * 2 + 1) / 3, 0),
                                   y1, x2 - max((adjbw2 * 2 + 1) / 3, 0), y1 + third,
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        drawBorder(graphicsContext, x1 + max((-adjbw1 * 2 + 1) / 3, 0),
                                   y2 - third, x2 - max((-adjbw2 * 2 + 1) / 3, 0), y2,
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        break;
                    case BSRight:
                        drawBorder(graphicsContext, x1, y1 + max((adjbw1 * 2 + 1) / 3, 0),
                                   x1 + third, y2 - max(( adjbw2 * 2 + 1) / 3, 0),
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        drawBorder(graphicsContext, x2 - third, y1 + max((-adjbw1 * 2 + 1) / 3, 0),
                                   x2, y2 - max((-adjbw2 * 2 + 1) / 3, 0),
                                   s, c, textcolor, SOLID, adjbw1bigthird, adjbw2bigthird);
                        break;
                    default:
                        break;
                }
            }
            break;
        }
        case RIDGE:
        case GROOVE:
        {
            EBorderStyle s1;
            EBorderStyle s2;
            if (style == GROOVE) {
                s1 = INSET;
                s2 = OUTSET;
            } else {
                s1 = OUTSET;
                s2 = INSET;
            }

            int adjbw1bighalf = ((adjbw1 > 0) ? adjbw1 + 1 : adjbw1 - 1) / 2;
            int adjbw2bighalf = ((adjbw2 > 0) ? adjbw2 + 1 : adjbw2 - 1) / 2;

            switch (s) {
                case BSTop:
                    drawBorder(graphicsContext, x1 + max(-adjbw1, 0) / 2, y1, x2 - max(-adjbw2, 0) / 2, (y1 + y2 + 1) / 2,
                               s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
                    drawBorder(graphicsContext, x1 + max(adjbw1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - max(adjbw2 + 1, 0) / 2, y2,
                               s, c, textcolor, s2, adjbw1 / 2, adjbw2 / 2);
                    break;
                case BSLeft:
                    drawBorder(graphicsContext, x1, y1 + max(-adjbw1, 0) / 2, (x1 + x2 + 1) / 2, y2 - max(-adjbw2, 0) / 2,
                               s, c, textcolor, s1, adjbw1bighalf, adjbw2bighalf);
                    drawBorder(graphicsContext, (x1 + x2 + 1) / 2, y1 + max(adjbw1 + 1, 0) / 2, x2, y2 - max(adjbw2 + 1, 0) / 2,
                               s, c, textcolor, s2, adjbw1 / 2, adjbw2 / 2);
                    break;
                case BSBottom:
                    drawBorder(graphicsContext, x1 + max(adjbw1, 0) / 2, y1, x2 - max(adjbw2, 0) / 2, (y1 + y2 + 1) / 2,
                               s, c, textcolor, s2, adjbw1bighalf, adjbw2bighalf);
                    drawBorder(graphicsContext, x1 + max(-adjbw1 + 1, 0) / 2, (y1 + y2 + 1) / 2, x2 - max(-adjbw2 + 1, 0) / 2, y2,
                               s, c, textcolor, s1, adjbw1/2, adjbw2/2);
                    break;
                case BSRight:
                    drawBorder(graphicsContext, x1, y1 + max(adjbw1, 0) / 2, (x1 + x2 + 1) / 2, y2 - max(adjbw2, 0) / 2,
                               s, c, textcolor, s2, adjbw1bighalf, adjbw2bighalf);
                    drawBorder(graphicsContext, (x1 + x2 + 1) / 2, y1 + max(-adjbw1 + 1, 0) / 2, x2, y2 - max(-adjbw2 + 1, 0) / 2,
                               s, c, textcolor, s1, adjbw1/2, adjbw2/2);
                    break;
            }
            break;
        }
        case INSET:
            if (s == BSTop || s == BSLeft)
                c = c.dark();
            // fall through
        case OUTSET:
            if (style == OUTSET && (s == BSBottom || s == BSRight))
                c = c.dark();
            // fall through
        case SOLID: {
            graphicsContext->setStrokeStyle(NoStroke);
            graphicsContext->setFillColor(c);
            ASSERT(x2 >= x1);
            ASSERT(y2 >= y1);
            if (!adjbw1 && !adjbw2) {
                graphicsContext->drawRect(IntRect(x1, y1, x2 - x1, y2 - y1));
                return;
            }
            FloatPoint quad[4];
            switch (s) {
                case BSTop:
                    quad[0] = FloatPoint(x1 + max(-adjbw1, 0), y1);
                    quad[1] = FloatPoint(x1 + max(adjbw1, 0), y2);
                    quad[2] = FloatPoint(x2 - max(adjbw2, 0), y2);
                    quad[3] = FloatPoint(x2 - max(-adjbw2, 0), y1);
                    break;
                case BSBottom:
                    quad[0] = FloatPoint(x1 + max(adjbw1, 0), y1);
                    quad[1] = FloatPoint(x1 + max(-adjbw1, 0), y2);
                    quad[2] = FloatPoint(x2 - max(-adjbw2, 0), y2);
                    quad[3] = FloatPoint(x2 - max(adjbw2, 0), y1);
                    break;
                case BSLeft:
                    quad[0] = FloatPoint(x1, y1 + max(-adjbw1, 0));
                    quad[1] = FloatPoint(x1, y2 - max(-adjbw2, 0));
                    quad[2] = FloatPoint(x2, y2 - max(adjbw2, 0));
                    quad[3] = FloatPoint(x2, y1 + max(adjbw1, 0));
                    break;
                case BSRight:
                    quad[0] = FloatPoint(x1, y1 + max(adjbw1, 0));
                    quad[1] = FloatPoint(x1, y2 - max(adjbw2, 0));
                    quad[2] = FloatPoint(x2, y2 - max(-adjbw2, 0));
                    quad[3] = FloatPoint(x2, y1 + max(-adjbw1, 0));
                    break;
            }
            graphicsContext->drawConvexPolygon(4, quad);
            break;
        }
    }
}

bool RenderObject::paintNinePieceImage(GraphicsContext* graphicsContext, int tx, int ty, int w, int h, const RenderStyle* style,
                                       const NinePieceImage& ninePieceImage, CompositeOperator op)
{
    StyleImage* styleImage = ninePieceImage.image();
    if (!styleImage || !styleImage->canRender(style->effectiveZoom()))
        return false;

    if (!styleImage->isLoaded())
        return true; // Never paint a nine-piece image incrementally, but don't paint the fallback borders either.

    // If we have a border radius, the image gets clipped to the rounded rect.
    bool clipped = false;
    if (style->hasBorderRadius()) {
        IntRect clipRect(tx, ty, w, h);
        graphicsContext->save();
        graphicsContext->addRoundedRectClip(clipRect, style->borderTopLeftRadius(), style->borderTopRightRadius(),
                                            style->borderBottomLeftRadius(), style->borderBottomRightRadius());
        clipped = true;
    }

    // FIXME: border-image is broken with full page zooming when tiling has to happen, since the tiling function
    // doesn't have any understanding of the zoom that is in effect on the tile.
    styleImage->setImageContainerSize(IntSize(w, h));
    IntSize imageSize = styleImage->imageSize(this, 1.0f);
    int imageWidth = imageSize.width();
    int imageHeight = imageSize.height();

    int topSlice = min(imageHeight, ninePieceImage.m_slices.top().calcValue(imageHeight));
    int bottomSlice = min(imageHeight, ninePieceImage.m_slices.bottom().calcValue(imageHeight));
    int leftSlice = min(imageWidth, ninePieceImage.m_slices.left().calcValue(imageWidth));
    int rightSlice = min(imageWidth, ninePieceImage.m_slices.right().calcValue(imageWidth));

    ENinePieceImageRule hRule = ninePieceImage.horizontalRule();
    ENinePieceImageRule vRule = ninePieceImage.verticalRule();

    bool fitToBorder = style->borderImage() == ninePieceImage;
    
    int leftWidth = fitToBorder ? style->borderLeftWidth() : leftSlice;
    int topWidth = fitToBorder ? style->borderTopWidth() : topSlice;
    int rightWidth = fitToBorder ? style->borderRightWidth() : rightSlice;
    int bottomWidth = fitToBorder ? style->borderBottomWidth() : bottomSlice;

    bool drawLeft = leftSlice > 0 && leftWidth > 0;
    bool drawTop = topSlice > 0 && topWidth > 0;
    bool drawRight = rightSlice > 0 && rightWidth > 0;
    bool drawBottom = bottomSlice > 0 && bottomWidth > 0;
    bool drawMiddle = (imageWidth - leftSlice - rightSlice) > 0 && (w - leftWidth - rightWidth) > 0 &&
                      (imageHeight - topSlice - bottomSlice) > 0 && (h - topWidth - bottomWidth) > 0;

    Image* image = styleImage->image(this, imageSize);

    if (drawLeft) {
        // Paint the top and bottom left corners.

        // The top left corner rect is (tx, ty, leftWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (0, 0, leftSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, IntRect(tx, ty, leftWidth, topWidth),
                                       IntRect(0, 0, leftSlice, topSlice), op);

        // The bottom left corner rect is (tx, ty + h - bottomWidth, leftWidth, bottomWidth)
        // The rect to use from within the image is (0, imageHeight - bottomSlice, leftSlice, botomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, IntRect(tx, ty + h - bottomWidth, leftWidth, bottomWidth),
                                       IntRect(0, imageHeight - bottomSlice, leftSlice, bottomSlice), op);

        // Paint the left edge.
        // Have to scale and tile into the border rect.
        graphicsContext->drawTiledImage(image, IntRect(tx, ty + topWidth, leftWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(0, topSlice, leftSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    if (drawRight) {
        // Paint the top and bottom right corners
        // The top right corner rect is (tx + w - rightWidth, ty, rightWidth, topWidth)
        // The rect to use from within the image is obtained from our slice, and is (imageWidth - rightSlice, 0, rightSlice, topSlice)
        if (drawTop)
            graphicsContext->drawImage(image, IntRect(tx + w - rightWidth, ty, rightWidth, topWidth),
                                       IntRect(imageWidth - rightSlice, 0, rightSlice, topSlice), op);

        // The bottom right corner rect is (tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth)
        // The rect to use from within the image is (imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice)
        if (drawBottom)
            graphicsContext->drawImage(image, IntRect(tx + w - rightWidth, ty + h - bottomWidth, rightWidth, bottomWidth),
                                       IntRect(imageWidth - rightSlice, imageHeight - bottomSlice, rightSlice, bottomSlice), op);

        // Paint the right edge.
        graphicsContext->drawTiledImage(image, IntRect(tx + w - rightWidth, ty + topWidth, rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(imageWidth - rightSlice, topSlice, rightSlice, imageHeight - topSlice - bottomSlice),
                                        Image::StretchTile, (Image::TileRule)vRule, op);
    }

    // Paint the top edge.
    if (drawTop)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty, w - leftWidth - rightWidth, topWidth),
                                        IntRect(leftSlice, 0, imageWidth - rightSlice - leftSlice, topSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the bottom edge.
    if (drawBottom)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty + h - bottomWidth,
                                        w - leftWidth - rightWidth, bottomWidth),
                                        IntRect(leftSlice, imageHeight - bottomSlice, imageWidth - rightSlice - leftSlice, bottomSlice),
                                        (Image::TileRule)hRule, Image::StretchTile, op);

    // Paint the middle.
    if (drawMiddle)
        graphicsContext->drawTiledImage(image, IntRect(tx + leftWidth, ty + topWidth, w - leftWidth - rightWidth,
                                        h - topWidth - bottomWidth),
                                        IntRect(leftSlice, topSlice, imageWidth - rightSlice - leftSlice, imageHeight - topSlice - bottomSlice),
                                        (Image::TileRule)hRule, (Image::TileRule)vRule, op);

    // Clear the clip for the border radius.
    if (clipped)
        graphicsContext->restore();

    return true;
}

void RenderObject::paintBorder(GraphicsContext* graphicsContext, int tx, int ty, int w, int h,
                               const RenderStyle* style, bool begin, bool end)
{
    if (paintNinePieceImage(graphicsContext, tx, ty, w, h, style, style->borderImage()))
        return;

    const Color& tc = style->borderTopColor();
    const Color& bc = style->borderBottomColor();
    const Color& lc = style->borderLeftColor();
    const Color& rc = style->borderRightColor();

    bool tt = style->borderTopIsTransparent();
    bool bt = style->borderBottomIsTransparent();
    bool rt = style->borderRightIsTransparent();
    bool lt = style->borderLeftIsTransparent();

    EBorderStyle ts = style->borderTopStyle();
    EBorderStyle bs = style->borderBottomStyle();
    EBorderStyle ls = style->borderLeftStyle();
    EBorderStyle rs = style->borderRightStyle();

    bool renderTop = ts > BHIDDEN && !tt;
    bool renderLeft = ls > BHIDDEN && begin && !lt;
    bool renderRight = rs > BHIDDEN && end && !rt;
    bool renderBottom = bs > BHIDDEN && !bt;

    // Need sufficient width and height to contain border radius curves.  Sanity check our border radii
    // and our width/height values to make sure the curves can all fit. If not, then we won't paint
    // any border radii.
    bool renderRadii = false;
    IntSize topLeft = style->borderTopLeftRadius();
    IntSize topRight = style->borderTopRightRadius();
    IntSize bottomLeft = style->borderBottomLeftRadius();
    IntSize bottomRight = style->borderBottomRightRadius();

    if (style->hasBorderRadius() &&
        static_cast<unsigned>(w) >= static_cast<unsigned>(topLeft.width()) + static_cast<unsigned>(topRight.width()) &&
        static_cast<unsigned>(w) >= static_cast<unsigned>(bottomLeft.width()) + static_cast<unsigned>(bottomRight.width()) &&
        static_cast<unsigned>(h) >= static_cast<unsigned>(topLeft.height()) + static_cast<unsigned>(bottomLeft.height()) &&
        static_cast<unsigned>(h) >= static_cast<unsigned>(topRight.height()) + static_cast<unsigned>(bottomRight.height()))
        renderRadii = true;

    // Clip to the rounded rectangle.
    if (renderRadii) {
        graphicsContext->save();
        graphicsContext->addRoundedRectClip(IntRect(tx, ty, w, h), topLeft, topRight, bottomLeft, bottomRight);
    }

    int firstAngleStart, secondAngleStart, firstAngleSpan, secondAngleSpan;
    float thickness;
    bool upperLeftBorderStylesMatch = renderLeft && (ts == ls) && (tc == lc);
    bool upperRightBorderStylesMatch = renderRight && (ts == rs) && (tc == rc) && (ts != OUTSET) && (ts != RIDGE) && (ts != INSET) && (ts != GROOVE);
    bool lowerLeftBorderStylesMatch = renderLeft && (bs == ls) && (bc == lc) && (bs != OUTSET) && (bs != RIDGE) && (bs != INSET) && (bs != GROOVE);
    bool lowerRightBorderStylesMatch = renderRight && (bs == rs) && (bc == rc);

    if (renderTop) {
        bool ignore_left = (renderRadii && topLeft.width() > 0) ||
            (tc == lc && tt == lt && ts >= OUTSET &&
             (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && topRight.width() > 0) ||
            (tc == rc && tt == rt && ts >= OUTSET &&
             (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += topLeft.width();
            x2 -= topRight.width();
        }

        drawBorder(graphicsContext, x, ty, x2, ty + style->borderTopWidth(), BSTop, tc, style->color(), ts,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            int leftY = ty;

            // We make the arc double thick and let the clip rect take care of clipping the extra off.
            // We're doing this because it doesn't seem possible to match the curve of the clip exactly
            // with the arc-drawing function.
            thickness = style->borderTopWidth() * 2;

            if (topLeft.width()) {
                int leftX = tx;
                // The inner clip clips inside the arc. This is especially important for 1px borders.
                bool applyLeftInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (ts != DOUBLE || style->borderTopWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderTopWidth());
                }

                firstAngleStart = 90;
                firstAngleSpan = upperLeftBorderStylesMatch ? 90 : 45;

                // Draw upper left arc
                drawBorderArc(graphicsContext, leftX, leftY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSTop, tc, style->color(), ts, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (topRight.width()) {
                int rightX = tx + w - topRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (ts != DOUBLE || style->borderTopWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, leftY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderTopWidth());
                }

                if (upperRightBorderStylesMatch) {
                    secondAngleStart = 0;
                    secondAngleSpan = 90;
                } else {
                    secondAngleStart = 45;
                    secondAngleSpan = 45;
                }

                // Draw upper right arc
                drawBorderArc(graphicsContext, rightX, leftY, thickness, topRight, secondAngleStart, secondAngleSpan,
                              BSTop, tc, style->color(), ts, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderBottom) {
        bool ignore_left = (renderRadii && bottomLeft.width() > 0) ||
            (bc == lc && bt == lt && bs >= OUTSET &&
             (ls == DOTTED || ls == DASHED || ls == SOLID || ls == OUTSET));

        bool ignore_right = (renderRadii && bottomRight.width() > 0) ||
            (bc == rc && bt == rt && bs >= OUTSET &&
             (rs == DOTTED || rs == DASHED || rs == SOLID || rs == INSET));

        int x = tx;
        int x2 = tx + w;
        if (renderRadii) {
            x += bottomLeft.width();
            x2 -= bottomRight.width();
        }

        drawBorder(graphicsContext, x, ty + h - style->borderBottomWidth(), x2, ty + h, BSBottom, bc, style->color(), bs,
                   ignore_left ? 0 : style->borderLeftWidth(), ignore_right ? 0 : style->borderRightWidth());

        if (renderRadii) {
            thickness = style->borderBottomWidth() * 2;

            if (bottomLeft.width()) {
                int leftX = tx;
                int leftY = ty + h - bottomLeft.height() * 2;
                bool applyLeftInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (bs != DOUBLE || style->borderBottomWidth() > 6);
                if (applyLeftInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(leftX, leftY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderBottomWidth());
                }

                if (lowerLeftBorderStylesMatch) {
                    firstAngleStart = 180;
                    firstAngleSpan = 90;
                } else {
                    firstAngleStart = 225;
                    firstAngleSpan = 45;
                }

                // Draw lower left arc
                drawBorderArc(graphicsContext, leftX, leftY, thickness, bottomLeft, firstAngleStart, firstAngleSpan,
                              BSBottom, bc, style->color(), bs, true);
                if (applyLeftInnerClip)
                    graphicsContext->restore();
            }

            if (bottomRight.width()) {
                int rightY = ty + h - bottomRight.height() * 2;
                int rightX = tx + w - bottomRight.width() * 2;
                bool applyRightInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (bs != DOUBLE || style->borderBottomWidth() > 6);
                if (applyRightInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(rightX, rightY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderBottomWidth());
                }

                secondAngleStart = 270;
                secondAngleSpan = lowerRightBorderStylesMatch ? 90 : 45;

                // Draw lower right arc
                drawBorderArc(graphicsContext, rightX, rightY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSBottom, bc, style->color(), bs, false);
                if (applyRightInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderLeft) {
        bool ignore_top = (renderRadii && topLeft.height() > 0) ||
            (tc == lc && tt == lt && ls >= OUTSET &&
             (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomLeft.height() > 0) ||
            (bc == lc && bt == lt && ls >= OUTSET &&
             (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topLeft.height();
            y2 -= bottomLeft.height();
        }

        drawBorder(graphicsContext, tx, y, tx + style->borderLeftWidth(), y2, BSLeft, lc, style->color(), ls,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperLeftBorderStylesMatch || !lowerLeftBorderStylesMatch)) {
            int topX = tx;
            thickness = style->borderLeftWidth() * 2;

            if (!upperLeftBorderStylesMatch && topLeft.width()) {
                int topY = ty;
                bool applyTopInnerClip = (style->borderLeftWidth() < topLeft.width())
                    && (style->borderTopWidth() < topLeft.height())
                    && (ls != DOUBLE || style->borderLeftWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topLeft.width() * 2, topLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                firstAngleStart = 135;
                firstAngleSpan = 45;

                // Draw top left arc
                drawBorderArc(graphicsContext, topX, topY, thickness, topLeft, firstAngleStart, firstAngleSpan,
                              BSLeft, lc, style->color(), ls, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerLeftBorderStylesMatch && bottomLeft.width()) {
                int bottomY = ty + h - bottomLeft.height() * 2;
                bool applyBottomInnerClip = (style->borderLeftWidth() < bottomLeft.width())
                    && (style->borderBottomWidth() < bottomLeft.height())
                    && (ls != DOUBLE || style->borderLeftWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, bottomY, bottomLeft.width() * 2, bottomLeft.height() * 2),
                                                             style->borderLeftWidth());
                }

                secondAngleStart = 180;
                secondAngleSpan = 45;

                // Draw bottom left arc
                drawBorderArc(graphicsContext, topX, bottomY, thickness, bottomLeft, secondAngleStart, secondAngleSpan,
                              BSLeft, lc, style->color(), ls, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRight) {
        bool ignore_top = (renderRadii && topRight.height() > 0) ||
            ((tc == rc) && (tt == rt) &&
            (rs >= DOTTED || rs == INSET) &&
            (ts == DOTTED || ts == DASHED || ts == SOLID || ts == OUTSET));

        bool ignore_bottom = (renderRadii && bottomRight.height() > 0) ||
            ((bc == rc) && (bt == rt) &&
            (rs >= DOTTED || rs == INSET) &&
            (bs == DOTTED || bs == DASHED || bs == SOLID || bs == INSET));

        int y = ty;
        int y2 = ty + h;
        if (renderRadii) {
            y += topRight.height();
            y2 -= bottomRight.height();
        }

        drawBorder(graphicsContext, tx + w - style->borderRightWidth(), y, tx + w, y2, BSRight, rc, style->color(), rs,
                   ignore_top ? 0 : style->borderTopWidth(), ignore_bottom ? 0 : style->borderBottomWidth());

        if (renderRadii && (!upperRightBorderStylesMatch || !lowerRightBorderStylesMatch)) {
            thickness = style->borderRightWidth() * 2;

            if (!upperRightBorderStylesMatch && topRight.width()) {
                int topX = tx + w - topRight.width() * 2;
                int topY = ty;
                bool applyTopInnerClip = (style->borderRightWidth() < topRight.width())
                    && (style->borderTopWidth() < topRight.height())
                    && (rs != DOUBLE || style->borderRightWidth() > 6);
                if (applyTopInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(topX, topY, topRight.width() * 2, topRight.height() * 2),
                                                             style->borderRightWidth());
                }

                firstAngleStart = 0;
                firstAngleSpan = 45;

                // Draw top right arc
                drawBorderArc(graphicsContext, topX, topY, thickness, topRight, firstAngleStart, firstAngleSpan,
                              BSRight, rc, style->color(), rs, true);
                if (applyTopInnerClip)
                    graphicsContext->restore();
            }

            if (!lowerRightBorderStylesMatch && bottomRight.width()) {
                int bottomX = tx + w - bottomRight.width() * 2;
                int bottomY = ty + h - bottomRight.height() * 2;
                bool applyBottomInnerClip = (style->borderRightWidth() < bottomRight.width())
                    && (style->borderBottomWidth() < bottomRight.height())
                    && (rs != DOUBLE || style->borderRightWidth() > 6);
                if (applyBottomInnerClip) {
                    graphicsContext->save();
                    graphicsContext->addInnerRoundedRectClip(IntRect(bottomX, bottomY, bottomRight.width() * 2, bottomRight.height() * 2),
                                                             style->borderRightWidth());
                }

                secondAngleStart = 315;
                secondAngleSpan = 45;

                // Draw bottom right arc
                drawBorderArc(graphicsContext, bottomX, bottomY, thickness, bottomRight, secondAngleStart, secondAngleSpan,
                              BSRight, rc, style->color(), rs, false);
                if (applyBottomInnerClip)
                    graphicsContext->restore();
            }
        }
    }

    if (renderRadii)
        graphicsContext->restore();
}

void RenderObject::paintBoxShadow(GraphicsContext* context, int tx, int ty, int w, int h, const RenderStyle* s, bool begin, bool end)
{
    // FIXME: Deal with border-image.  Would be great to use border-image as a mask.

    IntRect rect(tx, ty, w, h);
    bool hasBorderRadius = s->hasBorderRadius();
    bool hasOpaqueBackground = s->backgroundColor().isValid() && s->backgroundColor().alpha() == 255;
    for (ShadowData* shadow = s->boxShadow(); shadow; shadow = shadow->next) {
        context->save();

        IntSize shadowOffset(shadow->x, shadow->y);
        int shadowBlur = shadow->blur;
        IntRect fillRect(rect);

        if (hasBorderRadius) {
            IntRect shadowRect(rect);
            shadowRect.inflate(shadowBlur);
            shadowRect.move(shadowOffset);
            context->clip(shadowRect);

            // Move the fill just outside the clip, adding 1 pixel separation so that the fill does not
            // bleed in (due to antialiasing) if the context is transformed.
            IntSize extraOffset(w + max(0, shadowOffset.width()) + shadowBlur + 1, 0);
            shadowOffset -= extraOffset;
            fillRect.move(extraOffset);
        }

        context->setShadow(shadowOffset, shadowBlur, shadow->color);
        if (hasBorderRadius) {
            IntSize topLeft = begin ? s->borderTopLeftRadius() : IntSize();
            IntSize topRight = end ? s->borderTopRightRadius() : IntSize();
            IntSize bottomLeft = begin ? s->borderBottomLeftRadius() : IntSize();
            IntSize bottomRight = end ? s->borderBottomRightRadius() : IntSize();
            if (!hasOpaqueBackground)
                context->clipOutRoundedRect(rect, topLeft, topRight, bottomLeft, bottomRight);
            context->fillRoundedRect(fillRect, topLeft, topRight, bottomLeft, bottomRight, Color::black);
        } else {
            if (!hasOpaqueBackground)
                context->clipOut(rect);
            context->fillRect(fillRect, Color::black);
        }
        context->restore();
    }
}

void RenderObject::addLineBoxRects(Vector<IntRect>&, unsigned startOffset, unsigned endOffset, bool useSelectionHeight)
{
}

void RenderObject::absoluteRects(Vector<IntRect>& rects, int tx, int ty, bool topLevel)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (topLevel && continuation()) {
        rects.append(IntRect(tx, ty - collapsedMarginTop(),
                             width(), height() + collapsedMarginTop() + collapsedMarginBottom()));
        continuation()->absoluteRects(rects,
                                      tx - xPos() + continuation()->containingBlock()->xPos(),
                                      ty - yPos() + continuation()->containingBlock()->yPos(), topLevel);
    } else
        rects.append(IntRect(tx, ty, width(), height() + borderTopExtra() + borderBottomExtra()));
}

IntRect RenderObject::absoluteBoundingBoxRect(bool useTransforms)
{
    if (useTransforms) {
        Vector<FloatQuad> quads;
        absoluteQuads(quads);

        size_t n = quads.size();
        if (!n)
            return IntRect();
    
        IntRect result = quads[0].enclosingBoundingBox();
        for (size_t i = 1; i < n; ++i)
            result.unite(quads[i].enclosingBoundingBox());
        return result;
    }

    FloatPoint absPos = localToAbsolute();
    Vector<IntRect> rects;
    absoluteRects(rects, absPos.x(), absPos.y());

    size_t n = rects.size();
    if (!n)
        return IntRect();

    IntRect result = rects[0];
    for (size_t i = 1; i < n; ++i)
        result.unite(rects[i]);
    return result;
}

void RenderObject::collectAbsoluteLineBoxQuads(Vector<FloatQuad>& quads, unsigned startOffset, unsigned endOffset, bool useSelectionHeight)
{
}

void RenderObject::absoluteQuads(Vector<FloatQuad>& quads, bool topLevel)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (topLevel && continuation()) {
        FloatRect localRect(0, -collapsedMarginTop(),
                            width(), height() + collapsedMarginTop() + collapsedMarginBottom());
        quads.append(localToAbsoluteQuad(localRect));
        continuation()->absoluteQuads(quads, topLevel);
    } else
        quads.append(localToAbsoluteQuad(FloatRect(0, 0, width(), height() + borderTopExtra() + borderBottomExtra())));
}

void RenderObject::addAbsoluteRectForLayer(IntRect& result)
{
    if (hasLayer())
        result.unite(absoluteBoundingBoxRect());
    for (RenderObject* current = firstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
}

IntRect RenderObject::paintingRootRect(IntRect& topLevelRect)
{
    IntRect result = absoluteBoundingBoxRect();
    topLevelRect = result;
    for (RenderObject* current = firstChild(); current; current = current->nextSibling())
        current->addAbsoluteRectForLayer(result);
    return result;
}

void RenderObject::addPDFURLRect(GraphicsContext* context, const IntRect& rect)
{
    if (rect.isEmpty())
        return;
    Node* node = element();
    if (!node || !node->isLink() || !node->isElementNode())
        return;
    const AtomicString& href = static_cast<Element*>(node)->getAttribute(hrefAttr);
    if (href.isNull())
        return;
    context->setURLForRect(node->document()->completeURL(href), rect);
}


void RenderObject::addFocusRingRects(GraphicsContext* graphicsContext, int tx, int ty)
{
    // For blocks inside inlines, we go ahead and include margins so that we run right up to the
    // inline boxes above and below us (thus getting merged with them to form a single irregular
    // shape).
    if (continuation()) {
        graphicsContext->addFocusRingRect(IntRect(tx, ty - collapsedMarginTop(), width(), height() + collapsedMarginTop() + collapsedMarginBottom()));
        continuation()->addFocusRingRects(graphicsContext,
                                          tx - xPos() + continuation()->containingBlock()->xPos(),
                                          ty - yPos() + continuation()->containingBlock()->yPos());
    } else
        graphicsContext->addFocusRingRect(IntRect(tx, ty, width(), height()));
}

void RenderObject::paintOutline(GraphicsContext* graphicsContext, int tx, int ty, int w, int h, const RenderStyle* style)
{
    if (!hasOutline())
        return;

    int ow = style->outlineWidth();

    EBorderStyle os = style->outlineStyle();

    Color oc = style->outlineColor();
    if (!oc.isValid())
        oc = style->color();

    int offset = style->outlineOffset();

    if (style->outlineStyleIsAuto() || hasOutlineAnnotation()) {
        if (!theme()->supportsFocusRing(style)) {
            // Only paint the focus ring by hand if the theme isn't able to draw the focus ring.
            graphicsContext->initFocusRing(ow, offset);
            if (style->outlineStyleIsAuto())
                addFocusRingRects(graphicsContext, tx, ty);
            else
                addPDFURLRect(graphicsContext, graphicsContext->focusRingBoundingRect());
            graphicsContext->drawFocusRing(oc);
            graphicsContext->clearFocusRing();
        }
    }

    if (style->outlineStyleIsAuto() || style->outlineStyle() == BNONE)
        return;

    tx -= offset;
    ty -= offset;
    w += 2 * offset;
    h += 2 * offset;

    if (h < 0 || w < 0)
        return;

    drawBorder(graphicsContext, tx - ow, ty - ow, tx, ty + h + ow,
               BSLeft, Color(oc), style->color(), os, ow, ow);

    drawBorder(graphicsContext, tx - ow, ty - ow, tx + w + ow, ty,
               BSTop, Color(oc), style->color(), os, ow, ow);

    drawBorder(graphicsContext, tx + w, ty - ow, tx + w + ow, ty + h + ow,
               BSRight, Color(oc), style->color(), os, ow, ow);

    drawBorder(graphicsContext, tx - ow, ty + h, tx + w + ow, ty + h + ow,
               BSBottom, Color(oc), style->color(), os, ow, ow);
}

void RenderObject::paint(PaintInfo& /*paintInfo*/, int /*tx*/, int /*ty*/)
{
}

void RenderObject::repaint(bool immediate)
{
    // Can't use view(), since we might be unrooted.
    RenderObject* o = this;
    while (o->parent())
        o = o->parent();
    if (!o->isRenderView())
        return;
    RenderView* view = static_cast<RenderView*>(o);
    if (view->printing())
        return; // Don't repaint if we're printing.
    view->repaintViewRectangle(absoluteClippedOverflowRect(), immediate);
}

void RenderObject::repaintRectangle(const IntRect& r, bool immediate)
{
    // Can't use view(), since we might be unrooted.
    RenderObject* o = this;
    while (o->parent())
        o = o->parent();
    if (!o->isRenderView())
        return;
    RenderView* view = static_cast<RenderView*>(o);
    if (view->printing())
        return; // Don't repaint if we're printing.
    IntRect absRect(r);
    absRect.move(view->layoutDelta());
    computeAbsoluteRepaintRect(absRect);
    view->repaintViewRectangle(absRect, immediate);
}

bool RenderObject::repaintAfterLayoutIfNeeded(const IntRect& oldBounds, const IntRect& oldOutlineBox)
{
    RenderView* v = view();
    if (v->printing())
        return false; // Don't repaint if we're printing.

    IntRect newBounds = absoluteClippedOverflowRect();
    IntRect newOutlineBox;

    bool fullRepaint = selfNeedsLayout();
    // Presumably a background or a border exists if border-fit:lines was specified.
    if (!fullRepaint && style()->borderFit() == BorderFitLines)
        fullRepaint = true;
    if (!fullRepaint) {
        newOutlineBox = absoluteOutlineBounds();
        if (newOutlineBox.location() != oldOutlineBox.location() || (mustRepaintBackgroundOrBorder() && (newBounds != oldBounds || newOutlineBox != oldOutlineBox)))
            fullRepaint = true;
    }
    if (fullRepaint) {
        v->repaintViewRectangle(oldBounds);
        if (newBounds != oldBounds)
            v->repaintViewRectangle(newBounds);
        return true;
    }

    if (newBounds == oldBounds && newOutlineBox == oldOutlineBox)
        return false;

    int deltaLeft = newBounds.x() - oldBounds.x();
    if (deltaLeft > 0)
        v->repaintViewRectangle(IntRect(oldBounds.x(), oldBounds.y(), deltaLeft, oldBounds.height()));
    else if (deltaLeft < 0)
        v->repaintViewRectangle(IntRect(newBounds.x(), newBounds.y(), -deltaLeft, newBounds.height()));

    int deltaRight = newBounds.right() - oldBounds.right();
    if (deltaRight > 0)
        v->repaintViewRectangle(IntRect(oldBounds.right(), newBounds.y(), deltaRight, newBounds.height()));
    else if (deltaRight < 0)
        v->repaintViewRectangle(IntRect(newBounds.right(), oldBounds.y(), -deltaRight, oldBounds.height()));

    int deltaTop = newBounds.y() - oldBounds.y();
    if (deltaTop > 0)
        v->repaintViewRectangle(IntRect(oldBounds.x(), oldBounds.y(), oldBounds.width(), deltaTop));
    else if (deltaTop < 0)
        v->repaintViewRectangle(IntRect(newBounds.x(), newBounds.y(), newBounds.width(), -deltaTop));

    int deltaBottom = newBounds.bottom() - oldBounds.bottom();
    if (deltaBottom > 0)
        v->repaintViewRectangle(IntRect(newBounds.x(), oldBounds.bottom(), newBounds.width(), deltaBottom));
    else if (deltaBottom < 0)
        v->repaintViewRectangle(IntRect(oldBounds.x(), newBounds.bottom(), oldBounds.width(), -deltaBottom));

    if (newOutlineBox == oldOutlineBox)
        return false;

    // We didn't move, but we did change size.  Invalidate the delta, which will consist of possibly
    // two rectangles (but typically only one).
    RenderStyle* outlineStyle = !isInline() && continuation() ? continuation()->style() : style();
    int ow = outlineStyle->outlineSize();
    ShadowData* boxShadow = style()->boxShadow();
    int width = abs(newOutlineBox.width() - oldOutlineBox.width());
    if (width) {
        int shadowRight = 0;
        for (ShadowData* shadow = boxShadow; shadow; shadow = shadow->next)
            shadowRight = max(shadow->x + shadow->blur, shadowRight);

        int borderWidth = max(-outlineStyle->outlineOffset(), max(borderRight(), max(style()->borderTopRightRadius().width(), style()->borderBottomRightRadius().width()))) + max(ow, shadowRight);
        IntRect rightRect(newOutlineBox.x() + min(newOutlineBox.width(), oldOutlineBox.width()) - borderWidth,
            newOutlineBox.y(),
            width + borderWidth,
            max(newOutlineBox.height(), oldOutlineBox.height()));
        int right = min(newBounds.right(), oldBounds.right());
        if (rightRect.x() < right) {
            rightRect.setWidth(min(rightRect.width(), right - rightRect.x()));
            v->repaintViewRectangle(rightRect);
        }
    }
    int height = abs(newOutlineBox.height() - oldOutlineBox.height());
    if (height) {
        int shadowBottom = 0;
        for (ShadowData* shadow = boxShadow; shadow; shadow = shadow->next)
            shadowBottom = max(shadow->y + shadow->blur, shadowBottom);

        int borderHeight = max(-outlineStyle->outlineOffset(), max(borderBottom(), max(style()->borderBottomLeftRadius().height(), style()->borderBottomRightRadius().height()))) + max(ow, shadowBottom);
        IntRect bottomRect(newOutlineBox.x(),
            min(newOutlineBox.bottom(), oldOutlineBox.bottom()) - borderHeight,
            max(newOutlineBox.width(), oldOutlineBox.width()),
            height + borderHeight);
        int bottom = min(newBounds.bottom(), oldBounds.bottom());
        if (bottomRect.y() < bottom) {
            bottomRect.setHeight(min(bottomRect.height(), bottom - bottomRect.y()));
            v->repaintViewRectangle(bottomRect);
        }
    }
    return false;
}

void RenderObject::repaintDuringLayoutIfMoved(const IntRect& rect)
{
}

void RenderObject::repaintOverhangingFloats(bool paintAllDescendants)
{
}

bool RenderObject::checkForRepaintDuringLayout() const
{
    // FIXME: <https://bugs.webkit.org/show_bug.cgi?id=20885> It is probably safe to also require
    // m_everHadLayout. Currently, only RenderBlock::layoutBlock() adds this condition. See also
    // <https://bugs.webkit.org/show_bug.cgi?id=15129>.
    return !document()->view()->needsFullRepaint() && !hasLayer();
}

IntRect RenderObject::getAbsoluteRepaintRectWithOutline(int ow)
{
    IntRect r(absoluteClippedOverflowRect());
    r.inflate(ow);

    if (continuation() && !isInline())
        r.inflateY(collapsedMarginTop());

    if (isInlineFlow()) {
        for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling()) {
            if (!curr->isText())
                r.unite(curr->getAbsoluteRepaintRectWithOutline(ow));
        }
    }

    return r;
}

IntRect RenderObject::absoluteClippedOverflowRect()
{
    if (parent())
        return parent()->absoluteClippedOverflowRect();
    return IntRect();
}

void RenderObject::computeAbsoluteRepaintRect(IntRect& rect, bool fixed)
{
    if (RenderObject* o = parent()) {
        if (o->isBlockFlow()) {
            RenderBlock* cb = static_cast<RenderBlock*>(o);
            if (cb->hasColumns())
                cb->adjustRectForColumns(rect);
        }

        if (o->hasOverflowClip()) {
            // o->height() is inaccurate if we're in the middle of a layout of |o|, so use the
            // layer's size instead.  Even if the layer's size is wrong, the layer itself will repaint
            // anyway if its size does change.
            IntRect boxRect(0, 0, o->layer()->width(), o->layer()->height());
            int x = rect.x();
            int y = rect.y();
            o->layer()->subtractScrolledContentOffset(x, y); // For overflow:auto/scroll/hidden.
            IntRect repaintRect(x, y, rect.width(), rect.height());
            rect = intersection(repaintRect, boxRect);
            if (rect.isEmpty())
                return;
        }

        o->computeAbsoluteRepaintRect(rect, fixed);
    }
}

void RenderObject::dirtyLinesFromChangedChild(RenderObject* child)
{
}

#ifndef NDEBUG

void RenderObject::showTreeForThis() const
{
    if (element())
        element()->showTreeForThis();
}

#endif // NDEBUG

Color RenderObject::selectionBackgroundColor() const
{
    Color color;
    if (style()->userSelect() != SELECT_NONE) {
        RenderStyle* pseudoStyle = getCachedPseudoStyle(RenderStyle::SELECTION);
        if (pseudoStyle && pseudoStyle->backgroundColor().isValid())
            color = pseudoStyle->backgroundColor().blendWithWhite();
        else
            color = document()->frame()->selection()->isFocusedAndActive() ?
                    theme()->activeSelectionBackgroundColor() :
                    theme()->inactiveSelectionBackgroundColor();
    }

    return color;
}

Color RenderObject::selectionForegroundColor() const
{
    Color color;
    if (style()->userSelect() == SELECT_NONE)
        return color;

    if (RenderStyle* pseudoStyle = getCachedPseudoStyle(RenderStyle::SELECTION)) {
        color = pseudoStyle->textFillColor();
        if (!color.isValid())
            color = pseudoStyle->color();
    } else
        color = document()->frame()->selection()->isFocusedAndActive() ?
                theme()->platformActiveSelectionForegroundColor() :
                theme()->platformInactiveSelectionForegroundColor();

    return color;
}

Node* RenderObject::draggableNode(bool dhtmlOK, bool uaOK, int x, int y, bool& dhtmlWillDrag) const
{
    if (!dhtmlOK && !uaOK)
        return 0;

    for (const RenderObject* curr = this; curr; curr = curr->parent()) {
        Node* elt = curr->element();
        if (elt && elt->nodeType() == Node::TEXT_NODE) {
            // Since there's no way for the author to address the -webkit-user-drag style for a text node,
            // we use our own judgement.
            if (uaOK && view()->frameView()->frame()->eventHandler()->shouldDragAutoNode(curr->node(), IntPoint(x, y))) {
                dhtmlWillDrag = false;
                return curr->node();
            }
            if (elt->canStartSelection())
                // In this case we have a click in the unselected portion of text.  If this text is
                // selectable, we want to start the selection process instead of looking for a parent
                // to try to drag.
                return 0;
        } else {
            EUserDrag dragMode = curr->style()->userDrag();
            if (dhtmlOK && dragMode == DRAG_ELEMENT) {
                dhtmlWillDrag = true;
                return curr->node();
            }
            if (uaOK && dragMode == DRAG_AUTO
                    && view()->frameView()->frame()->eventHandler()->shouldDragAutoNode(curr->node(), IntPoint(x, y))) {
                dhtmlWillDrag = false;
                return curr->node();
            }
        }
    }
    return 0;
}

void RenderObject::selectionStartEnd(int& spos, int& epos) const
{
    view()->selectionStartEnd(spos, epos);
}

RenderBlock* RenderObject::createAnonymousBlock()
{
    RefPtr<RenderStyle> newStyle = RenderStyle::create();
    newStyle->inheritFrom(m_style.get());
    newStyle->setDisplay(BLOCK);

    RenderBlock* newBox = new (renderArena()) RenderBlock(document() /* anonymous box */);
    newBox->setStyle(newStyle.release());
    return newBox;
}

void RenderObject::handleDynamicFloatPositionChange()
{
    // We have gone from not affecting the inline status of the parent flow to suddenly
    // having an impact.  See if there is a mismatch between the parent flow's
    // childrenInline() state and our state.
    setInline(style()->isDisplayInlineType());
    if (isInline() != parent()->childrenInline()) {
        if (!isInline()) {
            if (parent()->isRenderInline()) {
                // We have to split the parent flow.
                RenderInline* parentInline = static_cast<RenderInline*>(parent());
                RenderBlock* newBox = parentInline->createAnonymousBlock();

                RenderFlow* oldContinuation = parent()->continuation();
                parentInline->setContinuation(newBox);

                RenderObject* beforeChild = nextSibling();
                parent()->removeChildNode(this);
                parentInline->splitFlow(beforeChild, newBox, this, oldContinuation);
            } else if (parent()->isRenderBlock()) {
                RenderBlock* o = static_cast<RenderBlock*>(parent());
                o->makeChildrenNonInline();
                if (o->isAnonymousBlock() && o->parent())
                    o->parent()->removeLeftoverAnonymousBlock(o);
                // o may be dead here
            }
        } else {
            // An anonymous block must be made to wrap this inline.
            RenderBlock* box = createAnonymousBlock();
            parent()->insertChildNode(box, this);
            box->appendChildNode(parent()->removeChildNode(this));
        }
    }
}

void RenderObject::setAnimatableStyle(PassRefPtr<RenderStyle> style)
{
    if (!isText() && style)
        setStyle(animation()->updateAnimations(this, style.get()));
    else
        setStyle(style);
}

void RenderObject::setStyle(PassRefPtr<RenderStyle> style)
{
    if (m_style == style)
        return;

    RenderStyle::Diff diff = RenderStyle::Equal;
    if (m_style)
        diff = m_style->diff(style.get());

    // If we have no layer(), just treat a RepaintLayer hint as a normal Repaint.
    if (diff == RenderStyle::RepaintLayer && !hasLayer())
        diff = RenderStyle::Repaint;

    styleWillChange(diff, style.get());
    
    RefPtr<RenderStyle> oldStyle = m_style.release();
    m_style = style;

    updateFillImages(oldStyle ? oldStyle->backgroundLayers() : 0, m_style ? m_style->backgroundLayers() : 0);
    updateFillImages(oldStyle ? oldStyle->maskLayers() : 0, m_style ? m_style->maskLayers() : 0);

    updateImage(oldStyle ? oldStyle->borderImage().image() : 0, m_style ? m_style->borderImage().image() : 0);
    updateImage(oldStyle ? oldStyle->maskBoxImage().image() : 0, m_style ? m_style->maskBoxImage().image() : 0);

    styleDidChange(diff, oldStyle.get());
}

void RenderObject::setStyleInternal(PassRefPtr<RenderStyle> style)
{
    m_style = style;
}

void RenderObject::styleWillChange(RenderStyle::Diff diff, const RenderStyle* newStyle)
{
    if (m_style) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        if (newStyle) {
#if ENABLE(DASHBOARD_SUPPORT)
            if (m_style->visibility() != newStyle->visibility() ||
                    m_style->zIndex() != newStyle->zIndex() ||
                    m_style->hasAutoZIndex() != newStyle->hasAutoZIndex())
                document()->setDashboardRegionsDirty(true);
#endif

            if ((m_style->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                    m_style->zIndex() != newStyle->zIndex() ||
                    m_style->visibility() != newStyle->visibility()) && hasLayer()) {
                layer()->dirtyStackingContextZOrderLists();
                if (m_style->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                        m_style->visibility() != newStyle->visibility())
                    layer()->dirtyZOrderLists();
            }
            // keep layer hierarchy visibility bits up to date if visibility changes
            if (m_style->visibility() != newStyle->visibility()) {
                if (RenderLayer* l = enclosingLayer()) {
                    if (newStyle->visibility() == VISIBLE)
                        l->setHasVisibleContent(true);
                    else if (l->hasVisibleContent() && (this == l->renderer() || l->renderer()->style()->visibility() != VISIBLE)) {
                        l->dirtyVisibleContentStatus();
                        if (diff > RenderStyle::RepaintLayer)
                            repaint();
                    }
                }
            }
        }

        // The background of the root element or the body element could propagate up to
        // the canvas.  Just dirty the entire canvas when our style changes substantially.
        if (diff >= RenderStyle::Repaint && element() &&
                (element()->hasTagName(htmlTag) || element()->hasTagName(bodyTag)))
            view()->repaint();
        else if (m_parent && !isText()) {
            // Do a repaint with the old style first, e.g., for example if we go from
            // having an outline to not having an outline.
            if (diff == RenderStyle::RepaintLayer) {
                layer()->repaintIncludingDescendants();
                if (!(m_style->clip() == newStyle->clip()))
                    layer()->clearClipRectsIncludingDescendants();
            } else if (diff == RenderStyle::Repaint || newStyle->outlineSize() < m_style->outlineSize())
                repaint();
        }

        if (diff == RenderStyle::Layout) {
            // When a layout hint happens, we go ahead and do a repaint of the layer, since the layer could
            // end up being destroyed.
            if (hasLayer()) {
                if (m_style->position() != newStyle->position() ||
                    m_style->zIndex() != newStyle->zIndex() ||
                    m_style->hasAutoZIndex() != newStyle->hasAutoZIndex() ||
                    !(m_style->clip() == newStyle->clip()) ||
                    m_style->hasClip() != newStyle->hasClip() ||
                    m_style->opacity() != newStyle->opacity() ||
                    m_style->transform() != newStyle->transform())
                layer()->repaintIncludingDescendants();
            } else if (m_style->transform() != newStyle->transform()) {
                // If we don't have a layer yet, but we are going to get one because of a transform change, then
                // we need to repaint the old position of the object
                repaint();
            }
        }
        
        // When a layout hint happens and an object's position style changes, we have to do a layout
        // to dirty the render tree using the old position value now.
        if (diff == RenderStyle::Layout && m_parent && m_style->position() != newStyle->position()) {
            markContainingBlocksForLayout();
            if (m_style->position() == StaticPosition)
                repaint();
            if (isFloating() && !isPositioned() && (newStyle->position() == AbsolutePosition || newStyle->position() == FixedPosition))
                removeFromObjectLists();
            if (isRenderBlock()) {
                if (newStyle->position() == StaticPosition)
                    // Clear our positioned objects list. Our absolutely positioned descendants will be
                    // inserted into our containing block's positioned objects list during layout.
                    removePositionedObjects(0);
                else if (m_style->position() == StaticPosition) {
                    // Remove our absolutely positioned descendants from their current containing block.
                    // They will be inserted into our positioned objects list during layout.
                    RenderObject* cb = parent();
                    while (cb && (cb->style()->position() == StaticPosition || (cb->isInline() && !cb->isReplaced())) && !cb->isRenderView()) {
                        if (cb->style()->position() == RelativePosition && cb->isInline() && !cb->isReplaced()) {
                            cb = cb->containingBlock();
                            break;
                        }
                        cb = cb->parent();
                    }
                    cb->removePositionedObjects(static_cast<RenderBlock*>(this));
                }
            }
        }

        if (isFloating() && (m_style->floating() != newStyle->floating()))
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            removeFromObjectLists();
        else if (isPositioned() && (newStyle->position() != AbsolutePosition && newStyle->position() != FixedPosition))
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            removeFromObjectLists();

        s_affectsParentBlock = isFloatingOrPositioned() &&
            (!newStyle->isFloating() && newStyle->position() != AbsolutePosition && newStyle->position() != FixedPosition)
            && parent() && (parent()->isBlockFlow() || parent()->isInlineFlow());

        // reset style flags
        if (diff == RenderStyle::Layout || diff == RenderStyle::LayoutPositionedMovementOnly) {
            m_floating = false;
            m_positioned = false;
            m_relPositioned = false;
        }
        m_paintBackground = false;
        m_hasOverflowClip = false;
        m_hasTransform = false;
        m_hasReflection = false;
    } else
        s_affectsParentBlock = false;

    if (view()->frameView()) {
        // FIXME: A better solution would be to only invalidate the fixed regions when scrolling.  It's overkill to
        // prevent the entire view from blitting on a scroll.
        bool newStyleSlowScroll = newStyle && (newStyle->position() == FixedPosition || newStyle->hasFixedBackgroundImage());
        bool oldStyleSlowScroll = m_style && (m_style->position() == FixedPosition || m_style->hasFixedBackgroundImage());
        if (oldStyleSlowScroll != newStyleSlowScroll) {
            if (oldStyleSlowScroll)
                view()->frameView()->removeSlowRepaintObject();
            if (newStyleSlowScroll)
                view()->frameView()->addSlowRepaintObject();
        }
    }
}

void RenderObject::styleDidChange(RenderStyle::Diff diff, const RenderStyle* oldStyle)
{
    setHasBoxDecorations(m_style->hasBorder() || m_style->hasBackground() || m_style->hasAppearance() || m_style->boxShadow());

    if (s_affectsParentBlock)
        handleDynamicFloatPositionChange();

    // No need to ever schedule repaints from a style change of a text run, since
    // we already did this for the parent of the text run.
    // We do have to schedule layouts, though, since a style change can force us to
    // need to relayout.
    if (diff == RenderStyle::Layout && m_parent)
        setNeedsLayoutAndPrefWidthsRecalc();
    else if (diff == RenderStyle::LayoutPositionedMovementOnly && m_parent && !isText())
        setNeedsPositionedMovementLayout();
    else if (m_parent && !isText() && (diff == RenderStyle::RepaintLayer || diff == RenderStyle::Repaint))
        // Do a repaint with the new style now, e.g., for example if we go from
        // not having an outline to having an outline.
        repaint();
}

void RenderObject::updateFillImages(const FillLayer* oldLayers, const FillLayer* newLayers)
{
    // FIXME: This will be slow when a large number of images is used.  Fix by using a dict.
    for (const FillLayer* currOld = oldLayers; currOld; currOld = currOld->next()) {
        if (currOld->image() && (!newLayers || !newLayers->containsImage(currOld->image())))
            currOld->image()->removeClient(this);
    }
    for (const FillLayer* currNew = newLayers; currNew; currNew = currNew->next()) {
        if (currNew->image() && (!oldLayers || !oldLayers->containsImage(currNew->image())))
            currNew->image()->addClient(this);
    }
}

void RenderObject::updateImage(StyleImage* oldImage, StyleImage* newImage)
{
    if (oldImage != newImage) {
        if (oldImage)
            oldImage->removeClient(this);
        if (newImage)
            newImage->addClient(this);
    }
}

IntRect RenderObject::viewRect() const
{
    return view()->viewRect();
}

FloatPoint RenderObject::localToAbsolute(FloatPoint localPoint, bool fixed, bool useTransforms) const
{
    RenderObject* o = parent();
    if (o) {
        localPoint.move(0.0f, static_cast<float>(o->borderTopExtra()));
        if (o->hasOverflowClip())
            localPoint -= o->layer()->scrolledContentOffset();
        return o->localToAbsolute(localPoint, fixed, useTransforms);
    }

    return FloatPoint();
}

FloatPoint RenderObject::absoluteToLocal(FloatPoint containerPoint, bool fixed, bool useTransforms) const
{
    RenderObject* o = parent();
    if (o) {
        FloatPoint localPoint = o->absoluteToLocal(containerPoint, fixed, useTransforms);
        localPoint.move(0.0f, -static_cast<float>(o->borderTopExtra()));
        if (o->hasOverflowClip())
            localPoint += o->layer()->scrolledContentOffset();
        return localPoint;
    }
    return FloatPoint();
}

FloatQuad RenderObject::localToAbsoluteQuad(const FloatQuad& localQuad, bool fixed) const
{
    RenderObject* o = parent();
    if (o) {
        FloatQuad quad = localQuad;
        quad.move(0.0f, static_cast<float>(o->borderTopExtra()));
        if (o->hasOverflowClip())
            quad -= o->layer()->scrolledContentOffset();
        return o->localToAbsoluteQuad(quad, fixed);
    }

    return FloatQuad();
}

IntSize RenderObject::offsetFromContainer(RenderObject* o) const
{
    ASSERT(o == container());

    IntSize offset;
    offset.expand(0, o->borderTopExtra());

    if (o->hasOverflowClip())
        offset -= o->layer()->scrolledContentOffset();

    return offset;
}

IntRect RenderObject::localCaretRect(InlineBox* inlineBox, int caretOffset, int* extraWidthToEndOfLine)
{
   if (extraWidthToEndOfLine)
       *extraWidthToEndOfLine = 0;

    return IntRect();
}

int RenderObject::paddingTop() const
{
    int w = 0;
    Length padding = m_style->paddingTop();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderObject::paddingBottom() const
{
    int w = 0;
    Length padding = style()->paddingBottom();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderObject::paddingLeft() const
{
    int w = 0;
    Length padding = style()->paddingLeft();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

int RenderObject::paddingRight() const
{
    int w = 0;
    Length padding = style()->paddingRight();
    if (padding.isPercent())
        w = containingBlock()->availableWidth();
    return padding.calcMinValue(w);
}

RenderView* RenderObject::view() const
{
    return static_cast<RenderView*>(document()->renderer());
}

bool RenderObject::hasOutlineAnnotation() const
{
    return element() && element()->isLink() && document()->printing();
}

RenderObject* RenderObject::container() const
{
    // This method is extremely similar to containingBlock(), but with a few notable
    // exceptions.
    // (1) It can be used on orphaned subtrees, i.e., it can be called safely even when
    // the object is not part of the primary document subtree yet.
    // (2) For normal flow elements, it just returns the parent.
    // (3) For absolute positioned elements, it will return a relative positioned inline.
    // containingBlock() simply skips relpositioned inlines and lets an enclosing block handle
    // the layout of the positioned object.  This does mean that calcAbsoluteHorizontal and
    // calcAbsoluteVertical have to use container().
    RenderObject* o = parent();

    if (isText())
        return o;

    EPosition pos = m_style->position();
    if (pos == FixedPosition) {
        // container() can be called on an object that is not in the
        // tree yet.  We don't call view() since it will assert if it
        // can't get back to the canvas.  Instead we just walk as high up
        // as we can.  If we're in the tree, we'll get the root.  If we
        // aren't we'll get the root of our little subtree (most likely
        // we'll just return 0).
        while (o && o->parent() && !(o->hasTransform() && o->isRenderBlock()))
            o = o->parent();
    } else if (pos == AbsolutePosition) {
        // Same goes here.  We technically just want our containing block, but
        // we may not have one if we're part of an uninstalled subtree.  We'll
        // climb as high as we can though.
        while (o && o->style()->position() == StaticPosition && !o->isRenderView() && !(o->hasTransform() && o->isRenderBlock()))
            o = o->parent();
    }

    return o;
}

// This code has been written to anticipate the addition of CSS3-::outside and ::inside generated
// content (and perhaps XBL).  That's why it uses the render tree and not the DOM tree.
RenderObject* RenderObject::hoverAncestor() const
{
    return (!isInline() && continuation()) ? continuation() : parent();
}

bool RenderObject::isSelectionBorder() const
{
    SelectionState st = selectionState();
    return st == SelectionStart || st == SelectionEnd || st == SelectionBoth;
}

void RenderObject::removeFromObjectLists()
{
    if (documentBeingDestroyed())
        return;

    if (isFloating()) {
        RenderBlock* outermostBlock = containingBlock();
        for (RenderBlock* p = outermostBlock; p && !p->isRenderView(); p = p->containingBlock()) {
            if (p->containsFloat(this))
                outermostBlock = p;
        }

        if (outermostBlock)
            outermostBlock->markAllDescendantsWithFloatsForLayout(this);
    }

    if (isPositioned()) {
        RenderObject* p;
        for (p = parent(); p; p = p->parent()) {
            if (p->isRenderBlock())
                static_cast<RenderBlock*>(p)->removePositionedObject(this);
        }
    }
}

bool RenderObject::documentBeingDestroyed() const
{
    return !document()->renderer();
}

void RenderObject::destroy()
{
    // If this renderer is being autoscrolled, stop the autoscroll timer
    if (document()->frame() && document()->frame()->eventHandler()->autoscrollRenderer() == this)
        document()->frame()->eventHandler()->stopAutoscrollTimer(true);

    if (m_hasCounterNodeMap)
        RenderCounter::destroyCounterNodes(this);

    if (AXObjectCache::accessibilityEnabled())
        document()->axObjectCache()->remove(this);

    animation()->cancelAnimations(this);

    // By default no ref-counting. RenderWidget::destroy() doesn't call
    // this function because it needs to do ref-counting. If anything
    // in this function changes, be sure to fix RenderWidget::destroy() as well.

    remove();

    RenderArena* arena = renderArena();

    if (hasLayer())
        layer()->destroy(arena);

    arenaDelete(arena, this);
}

void RenderObject::arenaDelete(RenderArena* arena, void* base)
{
    if (m_style) {
        for (const FillLayer* bgLayer = m_style->backgroundLayers(); bgLayer; bgLayer = bgLayer->next()) {
            if (StyleImage* backgroundImage = bgLayer->image())
                backgroundImage->removeClient(this);
        }

        for (const FillLayer* maskLayer = m_style->maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
            if (StyleImage* maskImage = maskLayer->image())
                maskImage->removeClient(this);
        }

        if (StyleImage* borderImage = m_style->borderImage().image())
            borderImage->removeClient(this);

        if (StyleImage* maskBoxImage = m_style->maskBoxImage().image())
            maskBoxImage->removeClient(this);
    }

#ifndef NDEBUG
    void* savedBase = baseOfRenderObjectBeingDeleted;
    baseOfRenderObjectBeingDeleted = base;
#endif
    delete this;
#ifndef NDEBUG
    baseOfRenderObjectBeingDeleted = savedBase;
#endif

    // Recover the size left there for us by operator delete and free the memory.
    arena->free(*(size_t*)base, base);
}

VisiblePosition RenderObject::positionForCoordinates(int x, int y)
{
    return VisiblePosition(element(), caretMinOffset(), DOWNSTREAM);
}

void RenderObject::updateDragState(bool dragOn)
{
    bool valueChanged = (dragOn != m_isDragging);
    m_isDragging = dragOn;
    if (valueChanged && style()->affectedByDragRules())
        element()->setChanged();
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->updateDragState(dragOn);
    if (continuation())
        continuation()->updateDragState(dragOn);
}

bool RenderObject::hitTest(const HitTestRequest& request, HitTestResult& result, const IntPoint& point, int tx, int ty, HitTestFilter hitTestFilter)
{
    bool inside = false;
    if (hitTestFilter != HitTestSelf) {
        // First test the foreground layer (lines and inlines).
        inside = nodeAtPoint(request, result, point.x(), point.y(), tx, ty, HitTestForeground);

        // Test floats next.
        if (!inside)
            inside = nodeAtPoint(request, result, point.x(), point.y(), tx, ty, HitTestFloat);

        // Finally test to see if the mouse is in the background (within a child block's background).
        if (!inside)
            inside = nodeAtPoint(request, result, point.x(), point.y(), tx, ty, HitTestChildBlockBackgrounds);
    }

    // See if the mouse is inside us but not any of our descendants
    if (hitTestFilter != HitTestDescendants && !inside)
        inside = nodeAtPoint(request, result, point.x(), point.y(), tx, ty, HitTestBlockBackground);

    return inside;
}

void RenderObject::updateHitTestResult(HitTestResult& result, const IntPoint& point)
{
    if (result.innerNode())
        return;

    Node* node = element();
    IntPoint localPoint(point);
    if (isRenderView())
        node = document()->documentElement();
    else if (!isInline() && continuation())
        // We are in the margins of block elements that are part of a continuation.  In
        // this case we're actually still inside the enclosing inline element that was
        // split.  Go ahead and set our inner node accordingly.
        node = continuation()->element();

    if (node) {
        if (node->renderer() && node->renderer()->continuation() && node->renderer() != this) {
            // We're in the continuation of a split inline.  Adjust our local point to be in the coordinate space
            // of the principal renderer's containing block.  This will end up being the innerNonSharedNode.
            RenderObject* firstBlock = node->renderer()->containingBlock();
            
            // Get our containing block.
            RenderObject* block = this;
            if (isInline())
                block = containingBlock();
        
            localPoint.move(block->xPos() - firstBlock->xPos(), block->yPos() - firstBlock->yPos());
        }

        result.setInnerNode(node);
        if (!result.innerNonSharedNode())
            result.setInnerNonSharedNode(node);
        result.setLocalPoint(localPoint);
    }
}

bool RenderObject::nodeAtPoint(const HitTestRequest&, HitTestResult&, int /*x*/, int /*y*/, int /*tx*/, int /*ty*/, HitTestAction)
{
    return false;
}

int RenderObject::verticalPositionHint(bool firstLine) const
{
    if (firstLine) // We're only really a first-line style if the document actually uses first-line rules.
        firstLine = document()->usesFirstLineRules();
    int vpos = m_verticalPosition;
    if (m_verticalPosition == PositionUndefined || firstLine) {
        vpos = getVerticalPosition(firstLine);
        if (!firstLine)
            m_verticalPosition = vpos;
    }

    return vpos;
}

int RenderObject::getVerticalPosition(bool firstLine) const
{
    if (!isInline())
        return 0;

    // This method determines the vertical position for inline elements.
    int vpos = 0;
    EVerticalAlign va = style()->verticalAlign();
    if (va == TOP)
        vpos = PositionTop;
    else if (va == BOTTOM)
        vpos = PositionBottom;
    else {
        bool checkParent = parent()->isInline() && !parent()->isInlineBlockOrInlineTable() && parent()->style()->verticalAlign() != TOP && parent()->style()->verticalAlign() != BOTTOM;
        vpos = checkParent ? parent()->verticalPositionHint(firstLine) : 0;
        // don't allow elements nested inside text-top to have a different valignment.
        if (va == BASELINE)
            return vpos;

        const Font& f = parent()->style(firstLine)->font();
        int fontsize = f.pixelSize();

        if (va == SUB)
            vpos += fontsize / 5 + 1;
        else if (va == SUPER)
            vpos -= fontsize / 3 + 1;
        else if (va == TEXT_TOP)
            vpos += baselinePosition(firstLine) - f.ascent();
        else if (va == MIDDLE)
            vpos += -static_cast<int>(f.xHeight() / 2) - lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == TEXT_BOTTOM) {
            vpos += f.descent();
            if (!isReplaced())
                vpos -= style(firstLine)->font().descent();
        } else if (va == BASELINE_MIDDLE)
            vpos += -lineHeight(firstLine) / 2 + baselinePosition(firstLine);
        else if (va == LENGTH)
            vpos -= style()->verticalAlignLength().calcValue(lineHeight(firstLine));
    }

    return vpos;
}

int RenderObject::lineHeight(bool firstLine, bool /*isRootLineBox*/) const
{
    RenderStyle* s = style(firstLine);

    Length lh = s->lineHeight();

    // its "unset", choose nice default
    if (lh.isNegative())
        return s->font().lineSpacing();

    if (lh.isPercent())
        return lh.calcMinValue(s->fontSize());

    // its fixed
    return lh.value();
}

int RenderObject::baselinePosition(bool firstLine, bool isRootLineBox) const
{
    const Font& f = style(firstLine)->font();
    return f.ascent() + (lineHeight(firstLine, isRootLineBox) - f.height()) / 2;
}

void RenderObject::scheduleRelayout()
{
    if (isRenderView()) {
        FrameView* view = static_cast<RenderView*>(this)->frameView();
        if (view)
            view->scheduleRelayout();
    } else if (parent()) {
        FrameView* v = view() ? view()->frameView() : 0;
        if (v)
            v->scheduleRelayoutOfSubtree(this);
    }
}

void RenderObject::removeLeftoverAnonymousBlock(RenderBlock*)
{
}

InlineBox* RenderObject::createInlineBox(bool, bool isRootLineBox, bool)
{
    ASSERT(!isRootLineBox);
    return new (renderArena()) InlineBox(this);
}

void RenderObject::dirtyLineBoxes(bool, bool)
{
}

InlineBox* RenderObject::inlineBoxWrapper() const
{
    return 0;
}

void RenderObject::setInlineBoxWrapper(InlineBox*)
{
}

void RenderObject::deleteLineBoxWrapper()
{
}

RenderStyle* RenderObject::firstLineStyle() const
{
    if (!document()->usesFirstLineRules())
        return m_style.get();

    RenderStyle* s = m_style.get();
    const RenderObject* obj = isText() ? parent() : this;
    if (obj->isBlockFlow()) {
        RenderBlock* firstLineBlock = obj->firstLineBlock();
        if (firstLineBlock)
            s = firstLineBlock->getCachedPseudoStyle(RenderStyle::FIRST_LINE, style());
    } else if (!obj->isAnonymous() && obj->isInlineFlow()) {
        RenderStyle* parentStyle = obj->parent()->firstLineStyle();
        if (parentStyle != obj->parent()->style()) {
            // A first-line style is in effect. We need to cache a first-line style
            // for ourselves.
            style()->setHasPseudoStyle(RenderStyle::FIRST_LINE_INHERITED);
            s = obj->getCachedPseudoStyle(RenderStyle::FIRST_LINE_INHERITED, parentStyle);
        }
    }
    return s;
}

RenderStyle* RenderObject::getCachedPseudoStyle(RenderStyle::PseudoId pseudo, RenderStyle* parentStyle) const
{
    if (pseudo < RenderStyle::FIRST_INTERNAL_PSEUDOID && !style()->hasPseudoStyle(pseudo))
        return 0;

    RenderStyle* cachedStyle = style()->getCachedPseudoStyle(pseudo);
    if (cachedStyle)
        return cachedStyle;
    
    RefPtr<RenderStyle> result = getUncachedPseudoStyle(pseudo, parentStyle);
    if (result)
        return style()->addCachedPseudoStyle(result.release());
    return 0;
}

PassRefPtr<RenderStyle> RenderObject::getUncachedPseudoStyle(RenderStyle::PseudoId pseudo, RenderStyle* parentStyle) const
{
    if (pseudo < RenderStyle::FIRST_INTERNAL_PSEUDOID && !style()->hasPseudoStyle(pseudo))
        return 0;
    
    if (!parentStyle)
        parentStyle = style();

    Node* node = element();
    while (node && !node->isElementNode())
        node = node->parentNode();
    if (!node)
        return 0;

    RefPtr<RenderStyle> result;
    if (pseudo == RenderStyle::FIRST_LINE_INHERITED) {
        result = document()->styleSelector()->styleForElement(static_cast<Element*>(node), parentStyle, false);
        result->setStyleType(RenderStyle::FIRST_LINE_INHERITED);
    } else
        result = document()->styleSelector()->pseudoStyleForElement(pseudo, static_cast<Element*>(node), parentStyle);
    return result.release();
}

static Color decorationColor(RenderStyle* style)
{
    Color result;
    if (style->textStrokeWidth() > 0) {
        // Prefer stroke color if possible but not if it's fully transparent.
        result = style->textStrokeColor();
        if (!result.isValid())
            result = style->color();
        if (result.alpha())
            return result;
    }
    
    result = style->textFillColor();
    if (!result.isValid())
        result = style->color();
    return result;
}

void RenderObject::getTextDecorationColors(int decorations, Color& underline, Color& overline,
                                           Color& linethrough, bool quirksMode)
{
    RenderObject* curr = this;
    do {
        int currDecs = curr->style()->textDecoration();
        if (currDecs) {
            if (currDecs & UNDERLINE) {
                decorations &= ~UNDERLINE;
                underline = decorationColor(curr->style());
            }
            if (currDecs & OVERLINE) {
                decorations &= ~OVERLINE;
                overline = decorationColor(curr->style());
            }
            if (currDecs & LINE_THROUGH) {
                decorations &= ~LINE_THROUGH;
                linethrough = decorationColor(curr->style());
            }
        }
        curr = curr->parent();
        if (curr && curr->isRenderBlock() && curr->continuation())
            curr = curr->continuation();
    } while (curr && decorations && (!quirksMode || !curr->element() ||
                                     (!curr->element()->hasTagName(aTag) && !curr->element()->hasTagName(fontTag))));

    // If we bailed out, use the element we bailed out at (typically a <font> or <a> element).
    if (decorations && curr) {
        if (decorations & UNDERLINE)
            underline = decorationColor(curr->style());
        if (decorations & OVERLINE)
            overline = decorationColor(curr->style());
        if (decorations & LINE_THROUGH)
            linethrough = decorationColor(curr->style());
    }
}

void RenderObject::updateWidgetPosition()
{
}

#if ENABLE(DASHBOARD_SUPPORT)
void RenderObject::addDashboardRegions(Vector<DashboardRegionValue>& regions)
{
    // Convert the style regions to absolute coordinates.
    if (style()->visibility() != VISIBLE)
        return;

    const Vector<StyleDashboardRegion>& styleRegions = style()->dashboardRegions();
    unsigned i, count = styleRegions.size();
    for (i = 0; i < count; i++) {
        StyleDashboardRegion styleRegion = styleRegions[i];

        int w = width();
        int h = height();

        DashboardRegionValue region;
        region.label = styleRegion.label;
        region.bounds = IntRect(styleRegion.offset.left().value(),
                                styleRegion.offset.top().value(),
                                w - styleRegion.offset.left().value() - styleRegion.offset.right().value(),
                                h - styleRegion.offset.top().value() - styleRegion.offset.bottom().value());
        region.type = styleRegion.type;

        region.clip = region.bounds;
        computeAbsoluteRepaintRect(region.clip);
        if (region.clip.height() < 0) {
            region.clip.setHeight(0);
            region.clip.setWidth(0);
        }

        FloatPoint absPos = localToAbsolute();
        region.bounds.setX(absPos.x() + styleRegion.offset.left().value());
        region.bounds.setY(absPos.y() + styleRegion.offset.top().value());

        if (document()->frame()) {
            float pageScaleFactor = document()->frame()->page()->chrome()->scaleFactor();
            if (pageScaleFactor != 1.0f) {
                region.bounds.scale(pageScaleFactor);
                region.clip.scale(pageScaleFactor);
            }
        }

        regions.append(region);
    }
}

void RenderObject::collectDashboardRegions(Vector<DashboardRegionValue>& regions)
{
    // RenderTexts don't have their own style, they just use their parent's style,
    // so we don't want to include them.
    if (isText())
        return;

    addDashboardRegions(regions);
    for (RenderObject* curr = firstChild(); curr; curr = curr->nextSibling())
        curr->collectDashboardRegions(regions);
}
#endif

bool RenderObject::avoidsFloats() const
{
    return isReplaced() || hasOverflowClip() || isHR();
}

bool RenderObject::shrinkToAvoidFloats() const
{
    // FIXME: Technically we should be able to shrink replaced elements on a line, but this is difficult to accomplish, since this
    // involves doing a relayout during findNextLineBreak and somehow overriding the containingBlockWidth method to return the
    // current remaining width on a line.
    if (isInline() && !isHTMLMarquee() || !avoidsFloats())
        return false;

    // All auto-width objects that avoid floats should always use lineWidth.
    return style()->width().isAuto();
}

UChar RenderObject::backslashAsCurrencySymbol() const
{
    if (Node *node = element()) {
        if (TextResourceDecoder* decoder = node->document()->decoder())
            return decoder->encoding().backslashAsCurrencySymbol();
    }
    return '\\';
}

bool RenderObject::willRenderImage(CachedImage*)
{
    // Without visibility we won't render (and therefore don't care about animation).
    if (style()->visibility() != VISIBLE)
        return false;

    // If we're not in a window (i.e., we're dormant from being put in the b/f cache or in a background tab)
    // then we don't want to render either.
    return !document()->inPageCache() && !document()->view()->isOffscreen();
}

int RenderObject::maximalOutlineSize(PaintPhase p) const
{
    if (p != PaintPhaseOutline && p != PaintPhaseSelfOutline && p != PaintPhaseChildOutlines)
        return 0;
    return static_cast<RenderView*>(document()->renderer())->maximalOutlineSize();
}

int RenderObject::caretMinOffset() const
{
    return 0;
}

int RenderObject::caretMaxOffset() const
{
    if (isReplaced())
        return element() ? max(1U, element()->childNodeCount()) : 1;
    if (isHR())
        return 1;
    return 0;
}

unsigned RenderObject::caretMaxRenderedOffset() const
{
    return 0;
}

int RenderObject::previousOffset(int current) const
{
    return current - 1;
}

int RenderObject::nextOffset(int current) const
{
    return current + 1;
}

int RenderObject::maxTopMargin(bool positive) const
{
    return positive ? max(0, marginTop()) : -min(0, marginTop());
}

int RenderObject::maxBottomMargin(bool positive) const
{
    return positive ? max(0, marginBottom()) : -min(0, marginBottom());
}

IntRect RenderObject::contentBox() const
{
    return IntRect(borderLeft() + paddingLeft(), borderTop() + paddingTop(),
        contentWidth(), contentHeight());
}

IntRect RenderObject::absoluteContentBox() const
{
    IntRect rect = contentBox();
    FloatPoint absPos = localToAbsoluteForContent(FloatPoint());
    rect.move(absPos.x(), absPos.y());
    return rect;
}

FloatQuad RenderObject::absoluteContentQuad() const
{
    IntRect rect = contentBox();
    return localToAbsoluteQuad(FloatRect(rect));
}

void RenderObject::adjustRectForOutlineAndShadow(IntRect& rect) const
{
    int outlineSize = !isInline() && continuation() ? continuation()->style()->outlineSize() : style()->outlineSize();
    if (ShadowData* boxShadow = style()->boxShadow()) {
        int shadowLeft = 0;
        int shadowRight = 0;
        int shadowTop = 0;
        int shadowBottom = 0;

        do {
            shadowLeft = min(boxShadow->x - boxShadow->blur - outlineSize, shadowLeft);
            shadowRight = max(boxShadow->x + boxShadow->blur + outlineSize, shadowRight);
            shadowTop = min(boxShadow->y - boxShadow->blur - outlineSize, shadowTop);
            shadowBottom = max(boxShadow->y + boxShadow->blur + outlineSize, shadowBottom);

            boxShadow = boxShadow->next;
        } while (boxShadow);

        rect.move(shadowLeft, shadowTop);
        rect.setWidth(rect.width() - shadowLeft + shadowRight);
        rect.setHeight(rect.height() - shadowTop + shadowBottom);
    } else
        rect.inflate(outlineSize);
}

IntRect RenderObject::absoluteOutlineBounds() const
{
    IntRect box = borderBox();
    adjustRectForOutlineAndShadow(box);

    FloatQuad absOutlineQuad = localToAbsoluteQuad(FloatRect(box));
    box = absOutlineQuad.enclosingBoundingBox();
    box.move(view()->layoutDelta());

    return box;
}

bool RenderObject::isScrollable() const
{
    RenderLayer* l = enclosingLayer();
    return l && (l->verticalScrollbar() || l->horizontalScrollbar());
}

AnimationController* RenderObject::animation() const
{
    return document()->frame()->animation();
}

void RenderObject::imageChanged(CachedImage* image, const IntRect* rect)
{
    imageChanged(static_cast<WrappedImagePtr>(image), rect);
}

IntRect RenderObject::reflectionBox() const
{
    IntRect result;
    if (!m_style->boxReflect())
        return result;
    IntRect box = borderBox();
    result = box;
    switch (m_style->boxReflect()->direction()) {
        case ReflectionBelow:
            result.move(0, box.height() + reflectionOffset());
            break;
        case ReflectionAbove:
            result.move(0, -box.height() - reflectionOffset());
            break;
        case ReflectionLeft:
            result.move(-box.width() - reflectionOffset(), 0);
            break;
        case ReflectionRight:
            result.move(box.width() + reflectionOffset(), 0);
            break;
    }
    return result;
}

int RenderObject::reflectionOffset() const
{
    if (!m_style->boxReflect())
        return 0;
    if (m_style->boxReflect()->direction() == ReflectionLeft || m_style->boxReflect()->direction() == ReflectionRight)
        return m_style->boxReflect()->offset().calcValue(borderBox().width());
    return m_style->boxReflect()->offset().calcValue(borderBox().height());
}

IntRect RenderObject::reflectedRect(const IntRect& r) const
{
    if (!m_style->boxReflect())
        return IntRect();

    IntRect box = borderBox();
    IntRect result = r;
    switch (m_style->boxReflect()->direction()) {
        case ReflectionBelow:
            result.setY(box.bottom() + reflectionOffset() + (box.bottom() - r.bottom()));
            break;
        case ReflectionAbove:
            result.setY(box.y() - reflectionOffset() - box.height() + (box.bottom() - r.bottom()));
            break;
        case ReflectionLeft:
            result.setX(box.x() - reflectionOffset() - box.width() + (box.right() - r.right()));
            break;
        case ReflectionRight:
            result.setX(box.right() + reflectionOffset() + (box.right() - r.right()));
            break;
    }
    return result;
}

#if ENABLE(SVG)

FloatRect RenderObject::relativeBBox(bool) const
{
    return FloatRect();
}

AffineTransform RenderObject::localTransform() const
{
    return AffineTransform(1, 0, 0, 1, xPos(), yPos());
}

AffineTransform RenderObject::absoluteTransform() const
{
    if (parent())
        return localTransform() * parent()->absoluteTransform();
    return localTransform();
}

#endif // ENABLE(SVG)

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::RenderObject* ro)
{
    if (ro)
        ro->showTreeForThis();
}

#endif
