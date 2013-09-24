/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005, 2006, 2007, 2008, 2009, 2013 Apple Inc. All rights reserved.
 * Copyright (C) 2010, 2012 Google Inc. All rights reserved.
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
 */

#include "config.h"
#include "RenderElement.h"

#include "AXObjectCache.h"
#include "ContentData.h"
#include "CursorList.h"
#include "EventHandler.h"
#include "Frame.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "RenderCounter.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderFlexibleBox.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderLayer.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderMultiColumnBlock.h"
#include "RenderRegion.h"
#include "RenderRuby.h"
#include "RenderRubyText.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderView.h"
#include "SVGRenderSupport.h"

#if USE(ACCELERATED_COMPOSITING)
#include "RenderLayerCompositor.h"
#endif

namespace WebCore {

bool RenderElement::s_affectsParentBlock = false;
bool RenderElement::s_noLongerAffectsParentBlock = false;

RenderElement::RenderElement(Element* element)
    : RenderObject(element)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
{
}

RenderElement::~RenderElement()
{
}

RenderElement* RenderElement::createFor(Element& element, RenderStyle& style)
{
    Document& document = element.document();
    RenderArena& arena = *document.renderArena();

    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    const ContentData* contentData = style.contentData();
    if (contentData && !contentData->next() && contentData->isImage() && !element.isPseudoElement()) {
        RenderImage* image = new (arena) RenderImage(&element);
        // RenderImageResourceStyleImage requires a style being present on the image but we don't want to
        // trigger a style change now as the node is not fully attached. Moving this code to style change
        // doesn't make sense as it should be run once at renderer creation.
        image->setStyleInternal(&style);
        if (const StyleImage* styleImage = static_cast<const ImageContentData*>(contentData)->image()) {
            image->setImageResource(RenderImageResourceStyleImage::create(const_cast<StyleImage*>(styleImage)));
            image->setIsGeneratedContent();
        } else
            image->setImageResource(RenderImageResource::create());
        image->setStyleInternal(0);
        return image;
    }

    if (element.hasTagName(HTMLNames::rubyTag)) {
        if (style.display() == INLINE)
            return new (arena) RenderRubyAsInline(element);
        if (style.display() == BLOCK)
            return new (arena) RenderRubyAsBlock(element);
    }
    // treat <rt> as ruby text ONLY if it still has its default treatment of block
    if (element.hasTagName(HTMLNames::rtTag) && style.display() == BLOCK)
        return new (arena) RenderRubyText(element);
    if (document.cssRegionsEnabled() && style.isDisplayRegionType() && !style.regionThread().isEmpty())
        return new (arena) RenderRegion(&element, 0);
    switch (style.display()) {
    case NONE:
        return 0;
    case INLINE:
        return new (arena) RenderInline(&element);
    case BLOCK:
    case INLINE_BLOCK:
    case RUN_IN:
    case COMPACT:
        if ((!style.hasAutoColumnCount() || !style.hasAutoColumnWidth()) && document.regionBasedColumnsEnabled())
            return new (arena) RenderMultiColumnBlock(element);
        return new (arena) RenderBlockFlow(&element);
    case LIST_ITEM:
        return new (arena) RenderListItem(element);
    case TABLE:
    case INLINE_TABLE:
        return new (arena) RenderTable(&element);
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        return new (arena) RenderTableSection(&element);
    case TABLE_ROW:
        return new (arena) RenderTableRow(&element);
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        return new (arena) RenderTableCol(element);
    case TABLE_CELL:
        return new (arena) RenderTableCell(&element);
    case TABLE_CAPTION:
        return new (arena) RenderTableCaption(element);
    case BOX:
    case INLINE_BOX:
        return new (arena) RenderDeprecatedFlexibleBox(element);
    case FLEX:
    case INLINE_FLEX:
        return new (arena) RenderFlexibleBox(&element);
    case GRID:
    case INLINE_GRID:
        return new (arena) RenderGrid(element);
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

void RenderElement::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    bool needsTable = false;

    if (newChild->isRenderTableCol()) {
        RenderTableCol* newTableColumn = toRenderTableCol(newChild);
        bool isColumnInColumnGroup = newTableColumn->isTableColumn() && isRenderTableCol();
        needsTable = !isTable() && !isColumnInColumnGroup;
    } else if (newChild->isTableCaption())
        needsTable = !isTable();
    else if (newChild->isTableSection())
        needsTable = !isTable();
    else if (newChild->isTableRow())
        needsTable = !isTableSection();
    else if (newChild->isTableCell())
        needsTable = !isTableRow();

    if (needsTable) {
        RenderTable* table;
        RenderObject* afterChild = beforeChild ? beforeChild->previousSibling() : m_lastChild;
        if (afterChild && afterChild->isAnonymous() && afterChild->isTable() && !afterChild->isBeforeContent())
            table = toRenderTable(afterChild);
        else {
            table = RenderTable::createAnonymousWithParentRenderer(this);
            addChild(table, beforeChild);
        }
        table->addChild(newChild);
    } else
        insertChildInternal(newChild, beforeChild, NotifyChildren);

    if (newChild->isText() && newChild->style()->textTransform() == CAPITALIZE)
        toRenderText(newChild)->transformText();

    // SVG creates renderers for <g display="none">, as SVG requires children of hidden
    // <g>s to have renderers - at least that's how our implementation works. Consider:
    // <g display="none"><foreignObject><body style="position: relative">FOO...
    // - requiresLayer() would return true for the <body>, creating a new RenderLayer
    // - when the document is painted, both layers are painted. The <body> layer doesn't
    //   know that it's inside a "hidden SVG subtree", and thus paints, even if it shouldn't.
    // To avoid the problem alltogether, detect early if we're inside a hidden SVG subtree
    // and stop creating layers at all for these cases - they're not used anyways.
    if (newChild->hasLayer() && !layerCreationAllowedForSubtree())
        toRenderLayerModelObject(newChild)->layer()->removeOnlyThisLayer();

#if ENABLE(SVG)
    SVGRenderSupport::childAdded(this, newChild);
#endif
}

void RenderElement::removeChild(RenderObject* oldChild)
{
    removeChildInternal(oldChild, NotifyChildren);
}

void RenderElement::destroyLeftoverChildren()
{
    while (m_firstChild) {
        if (m_firstChild->isListMarker() || (m_firstChild->style()->styleType() == FIRST_LETTER && !m_firstChild->isText()))
            m_firstChild->removeFromParent(); // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else if (m_firstChild->isRunIn() && m_firstChild->node()) {
            m_firstChild->node()->setRenderer(0);
            m_firstChild->node()->setNeedsStyleRecalc();
            m_firstChild->destroy();
        } else {
            // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (m_firstChild->node())
                m_firstChild->node()->setRenderer(0);
            m_firstChild->destroy();
        }
    }
}

void RenderElement::insertChildInternal(RenderObject* newChild, RenderObject* beforeChild, NotifyChildrenType notifyChildren)
{
    ASSERT(canHaveChildren() || canHaveGeneratedChildren());
    ASSERT(!newChild->parent());
    ASSERT(!isRenderBlockFlow() || (!newChild->isTableSection() && !newChild->isTableRow() && !newChild->isTableCell()));

    while (beforeChild && beforeChild->parent() && beforeChild->parent() != this)
        beforeChild = beforeChild->parent();

    // This should never happen, but if it does prevent render tree corruption
    // where child->parent() ends up being owner but child->nextSibling()->parent()
    // is not owner.
    if (beforeChild && beforeChild->parent() != this) {
        ASSERT_NOT_REACHED();
        return;
    }

    newChild->setParent(this);

    if (m_firstChild == beforeChild)
        m_firstChild = newChild;

    if (beforeChild) {
        RenderObject* previousSibling = beforeChild->previousSibling();
        if (previousSibling)
            previousSibling->setNextSibling(newChild);
        newChild->setPreviousSibling(previousSibling);
        newChild->setNextSibling(beforeChild);
        beforeChild->setPreviousSibling(newChild);
    } else {
        if (lastChild())
            lastChild()->setNextSibling(newChild);
        newChild->setPreviousSibling(lastChild());
        m_lastChild = newChild;
    }

    if (!documentBeingDestroyed()) {
        if (notifyChildren == NotifyChildren)
            newChild->insertedIntoTree();
        RenderCounter::rendererSubtreeAttached(newChild);
    }

    newChild->setNeedsLayoutAndPrefWidthsRecalc();
    setPreferredLogicalWidthsDirty(true);
    if (!normalChildNeedsLayout())
        setChildNeedsLayout(true); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache* cache = document().axObjectCache())
        cache->childrenChanged(this);
}

void RenderElement::removeChildInternal(RenderObject* oldChild, NotifyChildrenType notifyChildren)
{
    ASSERT(canHaveChildren() || canHaveGeneratedChildren());
    ASSERT(oldChild->parent() == this);

    if (oldChild->isFloatingOrOutOfFlowPositioned())
        toRenderBox(oldChild)->removeFloatingOrPositionedChildFromBlockLists();

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked). We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed() && notifyChildren == NotifyChildren && oldChild->everHadLayout()) {
        oldChild->setNeedsLayoutAndPrefWidthsRecalc();
        // We only repaint |oldChild| if we have a RenderLayer as its visual overflow may not be tracked by its parent.
        if (oldChild->isBody())
            view().repaintRootContents();
        else
            oldChild->repaint();
    }

    // If we have a line box wrapper, delete it.
    if (oldChild->isBox())
        toRenderBox(oldChild)->deleteLineBoxWrapper();
    else if (oldChild->isLineBreak())
        toRenderLineBreak(oldChild)->deleteInlineBoxWrapper();

    // If oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    // FIXME: The FrameSelection should be responsible for this when it
    // is notified of DOM mutations.
    if (!documentBeingDestroyed() && oldChild->isSelectionBorder())
        view().clearSelection();

    if (!documentBeingDestroyed() && notifyChildren == NotifyChildren)
        oldChild->willBeRemovedFromTree();

    // WARNING: There should be no code running between willBeRemovedFromTree and the actual removal below.
    // This is needed to avoid race conditions where willBeRemovedFromTree would dirty the tree's structure
    // and the code running here would force an untimely rebuilding, leaving |oldChild| dangling.

    if (oldChild->previousSibling())
        oldChild->previousSibling()->setNextSibling(oldChild->nextSibling());
    if (oldChild->nextSibling())
        oldChild->nextSibling()->setPreviousSibling(oldChild->previousSibling());

    if (m_firstChild == oldChild)
        m_firstChild = oldChild->nextSibling();
    if (m_lastChild == oldChild)
        m_lastChild = oldChild->previousSibling();

    oldChild->setPreviousSibling(0);
    oldChild->setNextSibling(0);
    oldChild->setParent(0);

    // rendererRemovedFromTree walks the whole subtree. We can improve performance
    // by skipping this step when destroying the entire tree.
    if (!documentBeingDestroyed())
        RenderCounter::rendererRemovedFromTree(oldChild);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this);
}

static void addLayers(RenderElement* obj, RenderLayer* parentLayer, RenderElement*& newObject, RenderLayer*& beforeChild)
{
    if (obj->hasLayer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject. We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = nullptr;
        }
        parentLayer->addChild(toRenderLayerModelObject(obj)->layer(), beforeChild);
        return;
    }

    for (RenderObject* current = obj->firstChild(); current; current = current->nextSibling()) {
        if (current->isRenderElement())
            addLayers(toRenderElement(current), parentLayer, newObject, beforeChild);
    }
}

void RenderElement::addLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    RenderElement* renderer = this;
    RenderLayer* beforeChild = nullptr;
    WebCore::addLayers(this, parentLayer, renderer, beforeChild);
}

void RenderElement::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(toRenderLayerModelObject(this)->layer());
        return;
    }

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isRenderElement())
            toRenderElement(child)->removeLayers(parentLayer);
    }
}

void RenderElement::moveLayers(RenderLayer* oldParent, RenderLayer* newParent)
{
    if (!newParent)
        return;

    if (hasLayer()) {
        RenderLayer* layer = toRenderLayerModelObject(this)->layer();
        ASSERT(oldParent == layer->parent());
        if (oldParent)
            oldParent->removeChild(layer);
        newParent->addChild(layer);
        return;
    }

    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (child->isRenderElement())
            toRenderElement(child)->moveLayers(oldParent, newParent);
    }
}

RenderLayer* RenderElement::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint, bool checkParent)
{
    // Error check the parent layer passed in. If it's null, we can't find anything.
    if (!parentLayer)
        return 0;

    // Step 1: If our layer is a child of the desired parent, then return our layer.
    RenderLayer* ourLayer = hasLayer() ? toRenderLayerModelObject(this)->layer() : nullptr;
    if (ourLayer && ourLayer->parent() == parentLayer)
        return ourLayer;

    // Step 2: If we don't have a layer, or our layer is the desired parent, then descend
    // into our siblings trying to find the next layer whose parent is the desired parent.
    if (!ourLayer || ourLayer == parentLayer) {
        for (RenderObject* child = startPoint ? startPoint->nextSibling() : firstChild(); child; child = child->nextSibling()) {
            if (!child->isRenderElement())
                continue;
            RenderLayer* nextLayer = toRenderElement(child)->findNextLayer(parentLayer, nullptr, false);
            if (nextLayer)
                return nextLayer;
        }
    }

    // Step 3: If our layer is the desired parent layer, then we're finished. We didn't
    // find anything.
    if (parentLayer == ourLayer)
        return nullptr;

    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && parent())
        return parent()->findNextLayer(parentLayer, this, true);

    return nullptr;
}

bool RenderElement::layerCreationAllowedForSubtree() const
{
#if ENABLE(SVG)
    RenderElement* parentRenderer = parent();
    while (parentRenderer) {
        if (parentRenderer->isSVGHiddenContainer())
            return false;
        parentRenderer = parentRenderer->parent();
    }
#endif
    
    return true;
}

void RenderElement::propagateStyleToAnonymousChildren(StylePropagationType propagationType)
{
    // FIXME: We could save this call when the change only affected non-inherited properties.
    for (RenderObject* child = firstChild(); child; child = child->nextSibling()) {
        if (!child->isAnonymous() || child->style()->styleType() != NOPSEUDO)
            continue;

        if (propagationType == PropagateToBlockChildrenOnly && !child->isRenderBlock())
            continue;

#if ENABLE(FULLSCREEN_API)
        if (child->isRenderFullScreen() || child->isRenderFullScreenPlaceholder())
            continue;
#endif

        RefPtr<RenderStyle> newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), child->style()->display());
        if (style()->specifiesColumns()) {
            if (child->style()->specifiesColumns())
                newStyle->inheritColumnPropertiesFrom(style());
            if (child->style()->columnSpan())
                newStyle->setColumnSpan(ColumnSpanAll);
        }

        // Preserve the position style of anonymous block continuations as they can have relative or sticky position when
        // they contain block descendants of relative or sticky positioned inlines.
        if (child->isInFlowPositioned() && toRenderBlock(child)->isAnonymousBlockContinuation())
            newStyle->setPosition(child->style()->position());

        child->setStyle(newStyle.release());
    }
}

// On low-powered/mobile devices, preventing blitting on a scroll can cause noticeable delays
// when scrolling a page with a fixed background image. As an optimization, assuming there are
// no fixed positoned elements on the page, we can acclerate scrolling (via blitting) if we
// ignore the CSS property "background-attachment: fixed".
static bool shouldRepaintFixedBackgroundsOnScroll(FrameView* frameView)
{
#if !ENABLE(FAST_MOBILE_SCROLLING) || !PLATFORM(QT)
    UNUSED_PARAM(frameView);
#endif

    bool repaintFixedBackgroundsOnScroll = true;
#if ENABLE(FAST_MOBILE_SCROLLING)
#if PLATFORM(QT)
    if (frameView->delegatesScrolling())
        repaintFixedBackgroundsOnScroll = false;
#else
    repaintFixedBackgroundsOnScroll = false;
#endif
#endif
    return repaintFixedBackgroundsOnScroll;
}

static inline bool rendererHasBackground(const RenderObject* renderer)
{
    return renderer && renderer->hasBackground();
}

void RenderElement::styleWillChange(StyleDifference diff, const RenderStyle* newStyle)
{
    if (m_style) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        if (newStyle) {
            bool visibilityChanged = m_style->visibility() != newStyle->visibility() 
                || m_style->zIndex() != newStyle->zIndex() 
                || m_style->hasAutoZIndex() != newStyle->hasAutoZIndex();
#if ENABLE(DASHBOARD_SUPPORT) || ENABLE(DRAGGABLE_REGION)
            if (visibilityChanged)
                document().setAnnotatedRegionsDirty(true);
#endif
            if (visibilityChanged) {
                if (AXObjectCache* cache = document().existingAXObjectCache())
                    cache->childrenChanged(parent());
            }

            // Keep layer hierarchy visibility bits up to date if visibility changes.
            if (m_style->visibility() != newStyle->visibility()) {
                if (RenderLayer* layer = enclosingLayer()) {
                    if (newStyle->visibility() == VISIBLE)
                        layer->setHasVisibleContent();
                    else if (layer->hasVisibleContent() && (this == &layer->renderer() || layer->renderer().style()->visibility() != VISIBLE)) {
                        layer->dirtyVisibleContentStatus();
                        if (diff > StyleDifferenceRepaintLayer)
                            repaint();
                    }
                }
            }
        }

        if (m_parent && (newStyle->outlineSize() < m_style->outlineSize() || shouldRepaintForStyleDifference(diff)))
            repaint();
        if (isFloating() && (m_style->floating() != newStyle->floating()))
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();
        else if (isOutOfFlowPositioned() && (m_style->position() != newStyle->position()))
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();

        s_affectsParentBlock = isFloatingOrOutOfFlowPositioned()
            && (!newStyle->isFloating() && !newStyle->hasOutOfFlowPosition())
            && parent() && (parent()->isRenderBlockFlow() || parent()->isRenderInline());

        s_noLongerAffectsParentBlock = ((!isFloating() && newStyle->isFloating()) || (!isOutOfFlowPositioned() && newStyle->hasOutOfFlowPosition()))
            && parent() && parent()->isRenderBlock();

        // reset style flags
        if (diff == StyleDifferenceLayout || diff == StyleDifferenceLayoutPositionedMovementOnly) {
            setFloating(false);
            clearPositionedState();
        }
        setHorizontalWritingMode(true);
        setHasBoxDecorations(false);
        setHasOverflowClip(false);
        setHasTransform(false);
        setHasReflection(false);
    } else {
        s_affectsParentBlock = false;
        s_noLongerAffectsParentBlock = false;
    }

    bool repaintFixedBackgroundsOnScroll = shouldRepaintFixedBackgroundsOnScroll(&view().frameView());

    bool newStyleSlowScroll = newStyle && repaintFixedBackgroundsOnScroll && newStyle->hasFixedBackgroundImage();
    bool oldStyleSlowScroll = m_style && repaintFixedBackgroundsOnScroll && m_style->hasFixedBackgroundImage();

#if USE(ACCELERATED_COMPOSITING)
    bool drawsRootBackground = isRoot() || (isBody() && !rendererHasBackground(document().documentElement()->renderer()));
    if (drawsRootBackground && repaintFixedBackgroundsOnScroll) {
        if (view().compositor().supportsFixedRootBackgroundCompositing()) {
            if (newStyleSlowScroll && newStyle->hasEntirelyFixedBackground())
                newStyleSlowScroll = false;

            if (oldStyleSlowScroll && m_style->hasEntirelyFixedBackground())
                oldStyleSlowScroll = false;
        }
    }
#endif
    if (oldStyleSlowScroll != newStyleSlowScroll) {
        if (oldStyleSlowScroll)
            view().frameView().removeSlowRepaintObject(this);

        if (newStyleSlowScroll)
            view().frameView().addSlowRepaintObject(this);
    }
}

static bool areNonIdenticalCursorListsEqual(const RenderStyle* a, const RenderStyle* b)
{
    ASSERT(a->cursors() != b->cursors());
    return a->cursors() && b->cursors() && *a->cursors() == *b->cursors();
}

static inline bool areCursorsEqual(const RenderStyle* a, const RenderStyle* b)
{
    return a->cursor() == b->cursor() && (a->cursors() == b->cursors() || areNonIdenticalCursorListsEqual(a, b));
}

void RenderElement::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (s_affectsParentBlock)
        handleDynamicFloatPositionChange();

    if (s_noLongerAffectsParentBlock)
        removeAnonymousWrappersForInlinesIfNecessary();
#if ENABLE(SVG)
    SVGRenderSupport::styleChanged(this);
#endif

    if (!m_parent)
        return;
    
    if (diff == StyleDifferenceLayout || diff == StyleDifferenceSimplifiedLayout) {
        RenderCounter::rendererStyleChanged(this, oldStyle, m_style.get());

        // If the object already needs layout, then setNeedsLayout won't do
        // any work. But if the containing block has changed, then we may need
        // to mark the new containing blocks for layout. The change that can
        // directly affect the containing block of this object is a change to
        // the position style.
        if (needsLayout() && oldStyle->position() != m_style->position())
            markContainingBlocksForLayout();

        if (diff == StyleDifferenceLayout)
            setNeedsLayoutAndPrefWidthsRecalc();
        else
            setNeedsSimplifiedNormalFlowLayout();
    } else if (diff == StyleDifferenceSimplifiedLayoutAndPositionedMovement) {
        setNeedsPositionedMovementLayout(oldStyle);
        setNeedsSimplifiedNormalFlowLayout();
    } else if (diff == StyleDifferenceLayoutPositionedMovementOnly)
        setNeedsPositionedMovementLayout(oldStyle);

    // Don't check for repaint here; we need to wait until the layer has been
    // updated by subclasses before we know if we have to repaint (in setStyle()).

    if (oldStyle && !areCursorsEqual(oldStyle, style()))
        frame().eventHandler().scheduleCursorUpdate();
}

void RenderElement::insertedIntoTree()
{
    RenderObject::insertedIntoTree();

    // Keep our layer hierarchy updated. Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    RenderLayer* layer = nullptr;
    if (firstChild() || hasLayer()) {
        layer = parent()->enclosingLayer();
        addLayers(layer);
    }

    // If |this| is visible but this object was not, tell the layer it has some visible content
    // that needs to be drawn and layer visibility optimization can't be used
    if (parent()->style()->visibility() != VISIBLE && style()->visibility() == VISIBLE && !hasLayer()) {
        if (!layer)
            layer = parent()->enclosingLayer();
        if (layer)
            layer->setHasVisibleContent();
    }
}

void RenderElement::willBeRemovedFromTree()
{
    // If we remove a visible child from an invisible parent, we don't know the layer visibility any more.
    RenderLayer* layer = nullptr;
    if (parent()->style()->visibility() != VISIBLE && style()->visibility() == VISIBLE && !hasLayer()) {
        if ((layer = parent()->enclosingLayer()))
            layer->dirtyVisibleContentStatus();
    }
    // Keep our layer hierarchy updated.
    if (firstChild() || hasLayer()) {
        if (!layer)
            layer = parent()->enclosingLayer();
        removeLayers(layer);
    }

    bool repaintFixedBackgroundsOnScroll = shouldRepaintFixedBackgroundsOnScroll(&view().frameView());
    if (repaintFixedBackgroundsOnScroll && m_style && m_style->hasFixedBackgroundImage())
        view().frameView().removeSlowRepaintObject(this);

    if (isOutOfFlowPositioned() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(this);

    RenderObject::willBeRemovedFromTree();
}

void RenderElement::willBeDestroyed()
{
    destroyLeftoverChildren();

    RenderObject::willBeDestroyed();
}

RenderElement* RenderElement::rendererForRootBackground()
{
    ASSERT(isRoot());
    if (!hasBackground() && element() && element()->hasTagName(HTMLNames::htmlTag)) {
        // Locate the <body> element using the DOM. This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc. We can locate the <body>
        // render object very easily via the DOM.
        HTMLElement* body = document().body();
        RenderElement* bodyObject = (body && body->hasLocalName(HTMLNames::bodyTag)) ? body->renderer() : 0;
        if (bodyObject)
            return bodyObject;
    }

    return this;
}

}
