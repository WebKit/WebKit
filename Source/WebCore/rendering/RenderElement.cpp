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
#include "AnimationController.h"
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
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
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
#include "StyleResolver.h"
#include <wtf/StackStats.h>

namespace WebCore {

bool RenderElement::s_affectsParentBlock = false;
bool RenderElement::s_noLongerAffectsParentBlock = false;

RenderElement::RenderElement(Element& element, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderObject(element)
    , m_baseTypeFlags(baseTypeFlags)
    , m_ancestorLineBoxDirty(false)
    , m_hasInitializedStyle(false)
    , m_renderInlineAlwaysCreatesLineBoxes(false)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
    , m_style(std::move(style))
{
}

RenderElement::RenderElement(Document& document, PassRef<RenderStyle> style, unsigned baseTypeFlags)
    : RenderObject(document)
    , m_baseTypeFlags(baseTypeFlags)
    , m_ancestorLineBoxDirty(false)
    , m_hasInitializedStyle(false)
    , m_renderInlineAlwaysCreatesLineBoxes(false)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
    , m_style(std::move(style))
{
}

RenderElement::~RenderElement()
{
    if (hasInitializedStyle()) {
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

#if ENABLE(CSS_SHAPES)
        if (auto shapeValue = m_style->shapeInside()) {
            if (auto shapeImage = shapeValue->image())
                shapeImage->removeClient(this);
        }
        if (auto shapeValue = m_style->shapeOutside()) {
            if (auto shapeImage = shapeValue->image())
                shapeImage->removeClient(this);
        }
#endif
    }
}

RenderPtr<RenderElement> RenderElement::createFor(Element& element, PassRef<RenderStyle> style)
{
    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    const ContentData* contentData = style.get().contentData();
    if (contentData && !contentData->next() && contentData->isImage() && !element.isPseudoElement()) {
        auto styleImage = const_cast<StyleImage*>(static_cast<const ImageContentData*>(contentData)->image());
        auto image = createRenderer<RenderImage>(element, std::move(style), styleImage);
        if (styleImage)
            image->setIsGeneratedContent();
        return std::move(image);
    }

    if (element.hasTagName(HTMLNames::rubyTag)) {
        if (style.get().display() == INLINE)
            return createRenderer<RenderRubyAsInline>(element, std::move(style));
        if (style.get().display() == BLOCK)
            return createRenderer<RenderRubyAsBlock>(element, std::move(style));
    }
    // treat <rt> as ruby text ONLY if it still has its default treatment of block
    if (element.hasTagName(HTMLNames::rtTag) && style.get().display() == BLOCK)
        return createRenderer<RenderRubyText>(element, std::move(style));
    switch (style.get().display()) {
    case NONE:
        style.dropRef();
        return nullptr;
    case INLINE:
        return createRenderer<RenderInline>(element, std::move(style));
    case BLOCK:
    case INLINE_BLOCK:
    case RUN_IN:
    case COMPACT:
        return createRenderer<RenderBlockFlow>(element, std::move(style));
    case LIST_ITEM:
        return createRenderer<RenderListItem>(element, std::move(style));
    case TABLE:
    case INLINE_TABLE:
        return createRenderer<RenderTable>(element, std::move(style));
    case TABLE_ROW_GROUP:
    case TABLE_HEADER_GROUP:
    case TABLE_FOOTER_GROUP:
        return createRenderer<RenderTableSection>(element, std::move(style));
    case TABLE_ROW:
        return createRenderer<RenderTableRow>(element, std::move(style));
    case TABLE_COLUMN_GROUP:
    case TABLE_COLUMN:
        return createRenderer<RenderTableCol>(element, std::move(style));
    case TABLE_CELL:
        return createRenderer<RenderTableCell>(element, std::move(style));
    case TABLE_CAPTION:
        return createRenderer<RenderTableCaption>(element, std::move(style));
    case BOX:
    case INLINE_BOX:
        return createRenderer<RenderDeprecatedFlexibleBox>(element, std::move(style));
    case FLEX:
    case INLINE_FLEX:
        return createRenderer<RenderFlexibleBox>(element, std::move(style));
    case GRID:
    case INLINE_GRID:
        return createRenderer<RenderGrid>(element, std::move(style));
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

enum StyleCacheState {
    Cached,
    Uncached
};

static PassRefPtr<RenderStyle> firstLineStyleForCachedUncachedType(StyleCacheState type, const RenderElement& renderer, RenderStyle* style)
{
    RenderElement& rendererForFirstLineStyle = renderer.isBeforeOrAfterContent() ? *renderer.parent() : const_cast<RenderElement&>(renderer);

    if (rendererForFirstLineStyle.isRenderBlockFlow() || rendererForFirstLineStyle.isRenderButton()) {
        if (RenderBlock* firstLineBlock = rendererForFirstLineStyle.firstLineBlock()) {
            if (type == Cached)
                return firstLineBlock->getCachedPseudoStyle(FIRST_LINE, style);
            return firstLineBlock->getUncachedPseudoStyle(PseudoStyleRequest(FIRST_LINE), style, firstLineBlock == &renderer ? style : nullptr);
        }
    } else if (!rendererForFirstLineStyle.isAnonymous() && rendererForFirstLineStyle.isRenderInline()) {
        RenderStyle& parentStyle = rendererForFirstLineStyle.parent()->firstLineStyle();
        if (&parentStyle != &rendererForFirstLineStyle.parent()->style()) {
            if (type == Cached) {
                // A first-line style is in effect. Cache a first-line style for ourselves.
                rendererForFirstLineStyle.style().setHasPseudoStyle(FIRST_LINE_INHERITED);
                return rendererForFirstLineStyle.getCachedPseudoStyle(FIRST_LINE_INHERITED, &parentStyle);
            }
            return rendererForFirstLineStyle.getUncachedPseudoStyle(PseudoStyleRequest(FIRST_LINE_INHERITED), &parentStyle, style);
        }
    }
    return nullptr;
}

PassRefPtr<RenderStyle> RenderElement::uncachedFirstLineStyle(RenderStyle* style) const
{
    if (!document().styleSheetCollection().usesFirstLineRules())
        return nullptr;

    return firstLineStyleForCachedUncachedType(Uncached, *this, style);
}

RenderStyle* RenderElement::cachedFirstLineStyle() const
{
    ASSERT(document().styleSheetCollection().usesFirstLineRules());

    RenderStyle& style = this->style();
    if (RefPtr<RenderStyle> firstLineStyle = firstLineStyleForCachedUncachedType(Cached, *this, &style))
        return firstLineStyle.get();

    return &style;
}

StyleDifference RenderElement::adjustStyleDifference(StyleDifference diff, unsigned contextSensitiveProperties) const
{
    // If transform changed, and we are not composited, need to do a layout.
    if (contextSensitiveProperties & ContextSensitivePropertyTransform) {
        // Text nodes share style with their parents but transforms don't apply to them,
        // hence the !isText() check.
        // FIXME: when transforms are taken into account for overflow, we will need to do a layout.
        if (!hasLayer() || !toRenderLayerModelObject(this)->layer()->isComposited()) {
            // We need to set at least SimplifiedLayout, but if PositionedMovementOnly is already set
            // then we actually need SimplifiedLayoutAndPositionedMovement.
            if (!hasLayer())
                diff = StyleDifferenceLayout; // FIXME: Do this for now since SimplifiedLayout cannot handle updating floating objects lists.
            else if (diff < StyleDifferenceLayoutPositionedMovementOnly)
                diff = StyleDifferenceSimplifiedLayout;
            else if (diff < StyleDifferenceSimplifiedLayout)
                diff = StyleDifferenceSimplifiedLayoutAndPositionedMovement;
        } else if (diff < StyleDifferenceRecompositeLayer)
            diff = StyleDifferenceRecompositeLayer;
    }

    // If opacity changed, and we are not composited, need to repaint (also
    // ignoring text nodes)
    if (contextSensitiveProperties & ContextSensitivePropertyOpacity) {
        if (!hasLayer() || !toRenderLayerModelObject(this)->layer()->isComposited())
            diff = StyleDifferenceRepaintLayer;
        else if (diff < StyleDifferenceRecompositeLayer)
            diff = StyleDifferenceRecompositeLayer;
    }
    
#if ENABLE(CSS_FILTERS)
    if ((contextSensitiveProperties & ContextSensitivePropertyFilter) && hasLayer()) {
        RenderLayer* layer = toRenderLayerModelObject(this)->layer();
        if (!layer->isComposited() || layer->paintsWithFilters())
            diff = StyleDifferenceRepaintLayer;
        else if (diff < StyleDifferenceRecompositeLayer)
            diff = StyleDifferenceRecompositeLayer;
    }
#endif
    
    // The answer to requiresLayer() for plugins, iframes, and canvas can change without the actual
    // style changing, since it depends on whether we decide to composite these elements. When the
    // layer status of one of these elements changes, we need to force a layout.
    if (diff == StyleDifferenceEqual && isRenderLayerModelObject()) {
        if (hasLayer() != toRenderLayerModelObject(this)->requiresLayer())
            diff = StyleDifferenceLayout;
    }

    // If we have no layer(), just treat a RepaintLayer hint as a normal Repaint.
    if (diff == StyleDifferenceRepaintLayer && !hasLayer())
        diff = StyleDifferenceRepaint;

    return diff;
}

inline bool RenderElement::hasImmediateNonWhitespaceTextChildOrBorderOrOutline() const
{
    for (auto& child : childrenOfType<RenderObject>(*this)) {
        if (child.isText() && !toRenderText(child).isAllCollapsibleWhitespace())
            return true;
        if (child.style().hasOutline() || child.style().hasBorder())
            return true;
    }
    return false;
}

inline bool RenderElement::shouldRepaintForStyleDifference(StyleDifference diff) const
{
    return diff == StyleDifferenceRepaint || (diff == StyleDifferenceRepaintIfTextOrBorderOrOutline && hasImmediateNonWhitespaceTextChildOrBorderOrOutline());
}

void RenderElement::updateFillImages(const FillLayer* oldLayers, const FillLayer* newLayers)
{
    // Optimize the common case
    if (oldLayers && !oldLayers->next() && newLayers && !newLayers->next() && (oldLayers->image() == newLayers->image()))
        return;
    
    // Go through the new layers and addClients first, to avoid removing all clients of an image.
    for (const FillLayer* currNew = newLayers; currNew; currNew = currNew->next()) {
        if (currNew->image())
            currNew->image()->addClient(this);
    }

    for (const FillLayer* currOld = oldLayers; currOld; currOld = currOld->next()) {
        if (currOld->image())
            currOld->image()->removeClient(this);
    }
}

void RenderElement::updateImage(StyleImage* oldImage, StyleImage* newImage)
{
    if (oldImage == newImage)
        return;
    if (oldImage)
        oldImage->removeClient(this);
    if (newImage)
        newImage->addClient(this);
}

#if ENABLE(CSS_SHAPES)
void RenderElement::updateShapeImage(const ShapeValue* oldShapeValue, const ShapeValue* newShapeValue)
{
    if (oldShapeValue || newShapeValue)
        updateImage(oldShapeValue ? oldShapeValue->image() : nullptr, newShapeValue ? newShapeValue->image() : nullptr);
}
#endif

void RenderElement::initializeStyle()
{
    styleWillChange(StyleDifferenceEqual, style());

    m_hasInitializedStyle = true;

    updateFillImages(nullptr, m_style->backgroundLayers());
    updateFillImages(nullptr, m_style->maskLayers());

    updateImage(nullptr, m_style->borderImage().image());
    updateImage(nullptr, m_style->maskBoxImage().image());

#if ENABLE(CSS_SHAPES)
    updateShapeImage(nullptr, m_style->shapeInside());
    updateShapeImage(nullptr, m_style->shapeOutside());
#endif

    // We need to ensure that view->maximalOutlineSize() is valid for any repaints that happen
    // during styleDidChange (it's used by clippedOverflowRectForRepaint()).
    if (m_style->outlineWidth() > 0 && m_style->outlineSize() > maximalOutlineSize(PaintPhaseOutline))
        view().setMaximalOutlineSize(m_style->outlineSize());

    styleDidChange(StyleDifferenceEqual, nullptr);

    // We shouldn't have any text children that would need styleDidChange at this point.
    ASSERT(!childrenOfType<RenderText>(*this).first());

    // It would be nice to assert that !parent() here, but some RenderLayer subrenderers
    // have their parent set before getting a call to initializeStyle() :|
}

void RenderElement::setStyle(PassRef<RenderStyle> style)
{
    // FIXME: Should change RenderView so it can use initializeStyle too.
    // If we do that, we can assert m_hasInitializedStyle unconditionally,
    // and remove the check of m_hasInitializedStyle below too.
    ASSERT(m_hasInitializedStyle || isRenderView());

    if (&m_style.get() == &style.get()) {
        // FIXME: Can we change things so we never hit this code path?
        // We need to run through adjustStyleDifference() for iframes, plugins, and canvas so
        // style sharing is disabled for them. That should ensure that we never hit this code path.
        ASSERT(!isRenderIFrame());
        ASSERT(!isEmbeddedObject());
        ASSERT(!isCanvas());
        style.dropRef();
        return;
    }

    StyleDifference diff = StyleDifferenceEqual;
    unsigned contextSensitiveProperties = ContextSensitivePropertyNone;
    if (m_hasInitializedStyle)
        diff = m_style->diff(&style.get(), contextSensitiveProperties);

    diff = adjustStyleDifference(diff, contextSensitiveProperties);

    styleWillChange(diff, style.get());

    Ref<RenderStyle> oldStyle(m_style.replace(std::move(style)));

    updateFillImages(oldStyle.get().backgroundLayers(), m_style->backgroundLayers());
    updateFillImages(oldStyle.get().maskLayers(), m_style->maskLayers());

    updateImage(oldStyle.get().borderImage().image(), m_style->borderImage().image());
    updateImage(oldStyle.get().maskBoxImage().image(), m_style->maskBoxImage().image());

#if ENABLE(CSS_SHAPES)
    updateShapeImage(oldStyle.get().shapeInside(), m_style->shapeInside());
    updateShapeImage(oldStyle.get().shapeOutside(), m_style->shapeOutside());
#endif

    // We need to ensure that view->maximalOutlineSize() is valid for any repaints that happen
    // during styleDidChange (it's used by clippedOverflowRectForRepaint()).
    if (m_style->outlineWidth() > 0 && m_style->outlineSize() > maximalOutlineSize(PaintPhaseOutline))
        view().setMaximalOutlineSize(m_style->outlineSize());

    bool doesNotNeedLayout = !parent();

    styleDidChange(diff, &oldStyle.get());

    // Text renderers use their parent style. Notify them about the change.
    for (auto& child : childrenOfType<RenderText>(*this))
        child.styleDidChange(diff, &oldStyle.get());

    // FIXME: |this| might be destroyed here. This can currently happen for a RenderTextFragment when
    // its first-letter block gets an update in RenderTextFragment::styleDidChange. For RenderTextFragment(s),
    // we will safely bail out with the doesNotNeedLayout flag. We might want to broaden this condition
    // in the future as we move renderer changes out of layout and into style changes.
    if (doesNotNeedLayout)
        return;

    // Now that the layer (if any) has been updated, we need to adjust the diff again,
    // check whether we should layout now, and decide if we need to repaint.
    StyleDifference updatedDiff = adjustStyleDifference(diff, contextSensitiveProperties);
    
    if (diff <= StyleDifferenceLayoutPositionedMovementOnly) {
        if (updatedDiff == StyleDifferenceLayout)
            setNeedsLayoutAndPrefWidthsRecalc();
        else if (updatedDiff == StyleDifferenceLayoutPositionedMovementOnly)
            setNeedsPositionedMovementLayout(&oldStyle.get());
        else if (updatedDiff == StyleDifferenceSimplifiedLayoutAndPositionedMovement) {
            setNeedsPositionedMovementLayout(&oldStyle.get());
            setNeedsSimplifiedNormalFlowLayout();
        } else if (updatedDiff == StyleDifferenceSimplifiedLayout)
            setNeedsSimplifiedNormalFlowLayout();
    }

    if (updatedDiff == StyleDifferenceRepaintLayer || shouldRepaintForStyleDifference(updatedDiff)) {
        // Do a repaint with the new style now, e.g., for example if we go from
        // not having an outline to having an outline.
        repaint();
    }
}

void RenderElement::setAnimatableStyle(PassRef<RenderStyle> style)
{
    setStyle(animation().updateAnimations(*this, std::move(style)));
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

    if (newChild->isText())
        toRenderText(newChild)->styleDidChange(StyleDifferenceEqual, nullptr);

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
    SVGRenderSupport::childAdded(*this, *newChild);
#endif
}

void RenderElement::removeChild(RenderObject& oldChild)
{
    removeChildInternal(oldChild, NotifyChildren);
}

void RenderElement::destroyLeftoverChildren()
{
    while (m_firstChild) {
        if (m_firstChild->isListMarker() || (m_firstChild->style().styleType() == FIRST_LETTER && !m_firstChild->isText()))
            m_firstChild->removeFromParent(); // List markers are owned by their enclosing list and so don't get destroyed by this container. Similarly, first letters are destroyed by their remaining text fragment.
        else if (m_firstChild->isRunIn() && m_firstChild->node()) {
            m_firstChild->node()->setRenderer(nullptr);
            m_firstChild->node()->setNeedsStyleRecalc();
            m_firstChild->destroy();
        } else {
            // Destroy any anonymous children remaining in the render tree, as well as implicit (shadow) DOM elements like those used in the engine-based text fields.
            if (m_firstChild->node())
                m_firstChild->node()->setRenderer(nullptr);
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
        setChildNeedsLayout(); // We may supply the static position for an absolute positioned child.

    if (AXObjectCache* cache = document().axObjectCache())
        cache->childrenChanged(this, newChild);
}

void RenderElement::removeChildInternal(RenderObject& oldChild, NotifyChildrenType notifyChildren)
{
    ASSERT(canHaveChildren() || canHaveGeneratedChildren());
    ASSERT(oldChild.parent() == this);

    if (oldChild.isFloatingOrOutOfFlowPositioned())
        toRenderBox(oldChild).removeFloatingOrPositionedChildFromBlockLists();

    // So that we'll get the appropriate dirty bit set (either that a normal flow child got yanked or
    // that a positioned child got yanked). We also repaint, so that the area exposed when the child
    // disappears gets repainted properly.
    if (!documentBeingDestroyed() && notifyChildren == NotifyChildren && oldChild.everHadLayout()) {
        oldChild.setNeedsLayoutAndPrefWidthsRecalc();
        // We only repaint |oldChild| if we have a RenderLayer as its visual overflow may not be tracked by its parent.
        if (oldChild.isBody())
            view().repaintRootContents();
        else
            oldChild.repaint();
    }

    // If we have a line box wrapper, delete it.
    if (oldChild.isBox())
        toRenderBox(oldChild).deleteLineBoxWrapper();
    else if (oldChild.isLineBreak())
        toRenderLineBreak(oldChild).deleteInlineBoxWrapper();

    // If oldChild is the start or end of the selection, then clear the selection to
    // avoid problems of invalid pointers.
    // FIXME: The FrameSelection should be responsible for this when it
    // is notified of DOM mutations.
    if (!documentBeingDestroyed() && oldChild.isSelectionBorder())
        view().clearSelection();

    if (!documentBeingDestroyed() && notifyChildren == NotifyChildren)
        oldChild.willBeRemovedFromTree();

    // WARNING: There should be no code running between willBeRemovedFromTree and the actual removal below.
    // This is needed to avoid race conditions where willBeRemovedFromTree would dirty the tree's structure
    // and the code running here would force an untimely rebuilding, leaving |oldChild| dangling.

    if (oldChild.previousSibling())
        oldChild.previousSibling()->setNextSibling(oldChild.nextSibling());
    if (oldChild.nextSibling())
        oldChild.nextSibling()->setPreviousSibling(oldChild.previousSibling());

    if (m_firstChild == &oldChild)
        m_firstChild = oldChild.nextSibling();
    if (m_lastChild == &oldChild)
        m_lastChild = oldChild.previousSibling();

    oldChild.setPreviousSibling(nullptr);
    oldChild.setNextSibling(nullptr);
    oldChild.setParent(nullptr);

    // rendererRemovedFromTree walks the whole subtree. We can improve performance
    // by skipping this step when destroying the entire tree.
    if (!documentBeingDestroyed())
        RenderCounter::rendererRemovedFromTree(oldChild);

    if (AXObjectCache* cache = document().existingAXObjectCache())
        cache->childrenChanged(this);
}

static void addLayers(RenderElement& renderer, RenderLayer* parentLayer, RenderElement*& newObject, RenderLayer*& beforeChild)
{
    if (renderer.hasLayer()) {
        if (!beforeChild && newObject) {
            // We need to figure out the layer that follows newObject. We only do
            // this the first time we find a child layer, and then we update the
            // pointer values for newObject and beforeChild used by everyone else.
            beforeChild = newObject->parent()->findNextLayer(parentLayer, newObject);
            newObject = nullptr;
        }
        parentLayer->addChild(toRenderLayerModelObject(renderer).layer(), beforeChild);
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(renderer))
        addLayers(child, parentLayer, newObject, beforeChild);
}

void RenderElement::addLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    RenderElement* renderer = this;
    RenderLayer* beforeChild = nullptr;
    WebCore::addLayers(*this, parentLayer, renderer, beforeChild);
}

void RenderElement::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(toRenderLayerModelObject(this)->layer());
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.removeLayers(parentLayer);
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

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.moveLayers(oldParent, newParent);
}

RenderLayer* RenderElement::findNextLayer(RenderLayer* parentLayer, RenderObject* startPoint, bool checkParent)
{
    // Error check the parent layer passed in. If it's null, we can't find anything.
    if (!parentLayer)
        return nullptr;

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
    for (auto& elementChild : childrenOfType<RenderElement>(*this)) {
        if (!elementChild.isAnonymous() || elementChild.style().styleType() != NOPSEUDO)
            continue;

        if (propagationType == PropagateToBlockChildrenOnly && !elementChild.isRenderBlock())
            continue;

#if ENABLE(FULLSCREEN_API)
        if (elementChild.isRenderFullScreen() || elementChild.isRenderFullScreenPlaceholder())
            continue;
#endif

        // RenderFlowThreads are updated through the RenderView::styleDidChange function.
        if (elementChild.isRenderFlowThread())
            continue;

        auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(&style(), elementChild.style().display());
        if (style().specifiesColumns()) {
            if (elementChild.style().specifiesColumns())
                newStyle.get().inheritColumnPropertiesFrom(&style());
            if (elementChild.style().columnSpan())
                newStyle.get().setColumnSpan(ColumnSpanAll);
        }

        // Preserve the position style of anonymous block continuations as they can have relative or sticky position when
        // they contain block descendants of relative or sticky positioned inlines.
        if (elementChild.isInFlowPositioned() && toRenderBlock(elementChild).isAnonymousBlockContinuation())
            newStyle.get().setPosition(elementChild.style().position());

        elementChild.setStyle(std::move(newStyle));
    }
}

// On low-powered/mobile devices, preventing blitting on a scroll can cause noticeable delays
// when scrolling a page with a fixed background image. As an optimization, assuming there are
// no fixed positoned elements on the page, we can acclerate scrolling (via blitting) if we
// ignore the CSS property "background-attachment: fixed".
static bool shouldRepaintFixedBackgroundsOnScroll()
{
#if ENABLE(FAST_MOBILE_SCROLLING)
    return false;
#else
    return true;
#endif
}

static inline bool rendererHasBackground(const RenderElement* renderer)
{
    return renderer && renderer->hasBackground();
}

void RenderElement::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    RenderStyle* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        bool visibilityChanged = m_style->visibility() != newStyle.visibility()
            || m_style->zIndex() != newStyle.zIndex()
            || m_style->hasAutoZIndex() != newStyle.hasAutoZIndex();
#if ENABLE(DASHBOARD_SUPPORT)
        if (visibilityChanged)
            document().setAnnotatedRegionsDirty(true);
#endif
#if PLATFORM(IOS) && ENABLE(TOUCH_EVENTS)
        if (visibilityChanged)
            document().dirtyTouchEventRects();
#endif
        if (visibilityChanged) {
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->childrenChanged(parent(), this);
        }

        // Keep layer hierarchy visibility bits up to date if visibility changes.
        if (m_style->visibility() != newStyle.visibility()) {
            if (RenderLayer* layer = enclosingLayer()) {
                if (newStyle.visibility() == VISIBLE)
                    layer->setHasVisibleContent();
                else if (layer->hasVisibleContent() && (this == &layer->renderer() || layer->renderer().style().visibility() != VISIBLE)) {
                    layer->dirtyVisibleContentStatus();
                    if (diff > StyleDifferenceRepaintLayer)
                        repaint();
                }
            }
        }

        if (m_parent && (newStyle.outlineSize() < m_style->outlineSize() || shouldRepaintForStyleDifference(diff)))
            repaint();
        if (isFloating() && (m_style->floating() != newStyle.floating()))
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();
        else if (isOutOfFlowPositioned() && (m_style->position() != newStyle.position()))
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            toRenderBox(this)->removeFloatingOrPositionedChildFromBlockLists();

        s_affectsParentBlock = isFloatingOrOutOfFlowPositioned()
            && (!newStyle.isFloating() && !newStyle.hasOutOfFlowPosition())
            && parent() && (parent()->isRenderBlockFlow() || parent()->isRenderInline());

        s_noLongerAffectsParentBlock = ((!isFloating() && newStyle.isFloating()) || (!isOutOfFlowPositioned() && newStyle.hasOutOfFlowPosition()))
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

    bool repaintFixedBackgroundsOnScroll = shouldRepaintFixedBackgroundsOnScroll();

    bool newStyleSlowScroll = repaintFixedBackgroundsOnScroll && newStyle.hasFixedBackgroundImage();
    bool oldStyleSlowScroll = oldStyle && repaintFixedBackgroundsOnScroll && m_style->hasFixedBackgroundImage();
    bool drawsRootBackground = isRoot() || (isBody() && !rendererHasBackground(document().documentElement()->renderer()));
    if (drawsRootBackground && repaintFixedBackgroundsOnScroll) {
        if (view().compositor().supportsFixedRootBackgroundCompositing()) {
            if (newStyleSlowScroll && newStyle.hasEntirelyFixedBackground())
                newStyleSlowScroll = false;

            if (oldStyleSlowScroll && m_style->hasEntirelyFixedBackground())
                oldStyleSlowScroll = false;
        }
    }

    if (oldStyleSlowScroll != newStyleSlowScroll) {
        if (oldStyleSlowScroll)
            view().frameView().removeSlowRepaintObject(this);

        if (newStyleSlowScroll)
            view().frameView().addSlowRepaintObject(this);
    }

    if (isRoot() || isBody()) {
        bool needsExtendedBackground = view().frameView().needsExtendedBackgroundRectForPainting();
        if (view().frameView().hasExtendedBackgroundRectForPainting() != needsExtendedBackground)
            view().frameView().setHasExtendedBackgroundRectForPainting(needsExtendedBackground);
    }
}

#if !PLATFORM(IOS)
static bool areNonIdenticalCursorListsEqual(const RenderStyle* a, const RenderStyle* b)
{
    ASSERT(a->cursors() != b->cursors());
    return a->cursors() && b->cursors() && *a->cursors() == *b->cursors();
}

static inline bool areCursorsEqual(const RenderStyle* a, const RenderStyle* b)
{
    return a->cursor() == b->cursor() && (a->cursors() == b->cursors() || areNonIdenticalCursorListsEqual(a, b));
}
#endif

void RenderElement::styleDidChange(StyleDifference diff, const RenderStyle* oldStyle)
{
    if (s_affectsParentBlock)
        handleDynamicFloatPositionChange();

    if (s_noLongerAffectsParentBlock)
        removeAnonymousWrappersForInlinesIfNecessary();
#if ENABLE(SVG)
    SVGRenderSupport::styleChanged(*this);
#endif

    if (!m_parent)
        return;
    
    if (diff == StyleDifferenceLayout || diff == StyleDifferenceSimplifiedLayout) {
        RenderCounter::rendererStyleChanged(this, oldStyle, &m_style.get());

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

#if !PLATFORM(IOS)
    if (oldStyle && !areCursorsEqual(oldStyle, &style()))
        frame().eventHandler().scheduleCursorUpdate();
#endif
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
    if (parent()->style().visibility() != VISIBLE && style().visibility() == VISIBLE && !hasLayer()) {
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
    if (parent()->style().visibility() != VISIBLE && style().visibility() == VISIBLE && !hasLayer()) {
        if ((layer = parent()->enclosingLayer()))
            layer->dirtyVisibleContentStatus();
    }
    // Keep our layer hierarchy updated.
    if (firstChild() || hasLayer()) {
        if (!layer)
            layer = parent()->enclosingLayer();
        removeLayers(layer);
    }

    bool repaintFixedBackgroundsOnScroll = shouldRepaintFixedBackgroundsOnScroll();
    if (repaintFixedBackgroundsOnScroll && m_style->hasFixedBackgroundImage())
        view().frameView().removeSlowRepaintObject(this);

    if (isOutOfFlowPositioned() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(this);

    RenderObject::willBeRemovedFromTree();
}

void RenderElement::willBeDestroyed()
{
    animation().cancelAnimations(this);

    destroyLeftoverChildren();

    RenderObject::willBeDestroyed();
}

void RenderElement::setNeedsPositionedMovementLayout(const RenderStyle* oldStyle)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (needsPositionedMovementLayout())
        return;
    setNeedsPositionedMovementLayoutBit(true);
    markContainingBlocksForLayout();
    if (hasLayer()) {
        if (oldStyle && style().diffRequiresRepaint(oldStyle))
            setLayerNeedsFullRepaint();
        else
            setLayerNeedsFullRepaintForPositionedMovementLayout();
    }
}

void RenderElement::clearChildNeedsLayout()
{
    setNormalChildNeedsLayoutBit(false);
    setPosChildNeedsLayoutBit(false);
    setNeedsSimplifiedNormalFlowLayoutBit(false);
    setNormalChildNeedsLayoutBit(false);
    setNeedsPositionedMovementLayoutBit(false);
}

void RenderElement::setNeedsSimplifiedNormalFlowLayout()
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (needsSimplifiedNormalFlowLayout())
        return;
    setNeedsSimplifiedNormalFlowLayoutBit(true);
    markContainingBlocksForLayout();
    if (hasLayer())
        setLayerNeedsFullRepaint();
}

RenderElement& RenderElement::rendererForRootBackground()
{
    ASSERT(isRoot());
    if (!hasBackground() && element() && element()->hasTagName(HTMLNames::htmlTag)) {
        // Locate the <body> element using the DOM. This is easier than trying
        // to crawl around a render tree with potential :before/:after content and
        // anonymous blocks created by inline <body> tags etc. We can locate the <body>
        // render object very easily via the DOM.
        if (auto body = document().body()) {
            if (body->hasLocalName(HTMLNames::bodyTag)) {
                if (auto renderer = body->renderer())
                    return *renderer;
            }
        }
    }
    return *this;
}

RenderElement* RenderElement::hoverAncestor() const
{
    // When searching for the hover ancestor and encountering a named flow thread,
    // the search will continue with the DOM ancestor of the top-most element
    // in the named flow thread.
    // See https://bugs.webkit.org/show_bug.cgi?id=111749
    RenderElement* hoverAncestor = parent();

    // Skip anonymous blocks directly flowed into flow threads as it would
    // prevent us from continuing the search on the DOM tree when reaching the named flow thread.
    if (hoverAncestor && hoverAncestor->isAnonymousBlock() && hoverAncestor->parent() && hoverAncestor->parent()->isRenderNamedFlowThread())
        hoverAncestor = hoverAncestor->parent();

    if (hoverAncestor && hoverAncestor->isRenderNamedFlowThread()) {
        hoverAncestor = nullptr;
        if (Element* element = this->element()) {
            if (auto parent = element->parentNode())
                hoverAncestor = parent->renderer();
        }
    }

    return hoverAncestor;
}

void RenderElement::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    RenderObject* child = firstChild();
    while (child) {
        if (child->needsLayout())
            toRenderElement(child)->layout();
        ASSERT(!child->needsLayout());
        child = child->nextSibling();
    }
    clearNeedsLayout();
}

static bool mustRepaintFillLayers(const RenderElement& renderer, const FillLayer* layer)
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer->next())
        return true;

    // Make sure we have a valid image.
    StyleImage* image = layer->image();
    if (!image || !image->canRender(&renderer, renderer.style().effectiveZoom()))
        return false;

    if (!layer->xPosition().isZero() || !layer->yPosition().isZero())
        return true;

    EFillSizeType sizeType = layer->sizeType();

    if (sizeType == Contain || sizeType == Cover)
        return true;

    if (sizeType == SizeLength) {
        LengthSize size = layer->sizeLength();
        if (size.width().isPercent() || size.height().isPercent())
            return true;
        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for 'contain'.
        if ((size.width().isAuto() || size.height().isAuto()) && image->isGeneratedImage())
            return true;
    } else if (image->usesImageContainerSize())
        return true;

    return false;
}

static bool mustRepaintBackgroundOrBorder(const RenderElement& renderer)
{
    if (renderer.hasMask() && mustRepaintFillLayers(renderer, renderer.style().maskLayers()))
        return true;

    // If we don't have a background/border/mask, then nothing to do.
    if (!renderer.hasBoxDecorations())
        return false;

    if (mustRepaintFillLayers(renderer, renderer.style().backgroundLayers()))
        return true;

    // Our fill layers are ok. Let's check border.
    if (renderer.style().hasBorder() && renderer.borderImageIsLoadedAndCanBeRendered())
        return true;

    return false;
}

bool RenderElement::repaintAfterLayoutIfNeeded(const RenderLayerModelObject* repaintContainer, const LayoutRect& oldBounds, const LayoutRect& oldOutlineBox, const LayoutRect* newBoundsPtr, const LayoutRect* newOutlineBoxRectPtr)
{
    if (view().printing())
        return false; // Don't repaint if we're printing.

    // This ASSERT fails due to animations. See https://bugs.webkit.org/show_bug.cgi?id=37048
    // ASSERT(!newBoundsPtr || *newBoundsPtr == clippedOverflowRectForRepaint(repaintContainer));
    LayoutRect newBounds = newBoundsPtr ? *newBoundsPtr : clippedOverflowRectForRepaint(repaintContainer);
    LayoutRect newOutlineBox;

    bool fullRepaint = selfNeedsLayout();
    // Presumably a background or a border exists if border-fit:lines was specified.
    if (!fullRepaint && style().borderFit() == BorderFitLines)
        fullRepaint = true;
    if (!fullRepaint) {
        // This ASSERT fails due to animations. See https://bugs.webkit.org/show_bug.cgi?id=37048
        // ASSERT(!newOutlineBoxRectPtr || *newOutlineBoxRectPtr == outlineBoundsForRepaint(repaintContainer));
        newOutlineBox = newOutlineBoxRectPtr ? *newOutlineBoxRectPtr : outlineBoundsForRepaint(repaintContainer);
        if (newOutlineBox.location() != oldOutlineBox.location() || (mustRepaintBackgroundOrBorder(*this) && (newBounds != oldBounds || newOutlineBox != oldOutlineBox)))
            fullRepaint = true;
    }

    if (!repaintContainer)
        repaintContainer = &view();

    if (fullRepaint) {
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldBounds));
        if (newBounds != oldBounds)
            repaintUsingContainer(repaintContainer, pixelSnappedIntRect(newBounds));
        return true;
    }

    if (newBounds == oldBounds && newOutlineBox == oldOutlineBox)
        return false;

    LayoutUnit deltaLeft = newBounds.x() - oldBounds.x();
    if (deltaLeft > 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldBounds.x(), oldBounds.y(), deltaLeft, oldBounds.height()));
    else if (deltaLeft < 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(newBounds.x(), newBounds.y(), -deltaLeft, newBounds.height()));

    LayoutUnit deltaRight = newBounds.maxX() - oldBounds.maxX();
    if (deltaRight > 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldBounds.maxX(), newBounds.y(), deltaRight, newBounds.height()));
    else if (deltaRight < 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(newBounds.maxX(), oldBounds.y(), -deltaRight, oldBounds.height()));

    LayoutUnit deltaTop = newBounds.y() - oldBounds.y();
    if (deltaTop > 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldBounds.x(), oldBounds.y(), oldBounds.width(), deltaTop));
    else if (deltaTop < 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(newBounds.x(), newBounds.y(), newBounds.width(), -deltaTop));

    LayoutUnit deltaBottom = newBounds.maxY() - oldBounds.maxY();
    if (deltaBottom > 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(newBounds.x(), oldBounds.maxY(), newBounds.width(), deltaBottom));
    else if (deltaBottom < 0)
        repaintUsingContainer(repaintContainer, pixelSnappedIntRect(oldBounds.x(), newBounds.maxY(), oldBounds.width(), -deltaBottom));

    if (newOutlineBox == oldOutlineBox)
        return false;

    // We didn't move, but we did change size. Invalidate the delta, which will consist of possibly
    // two rectangles (but typically only one).
    const RenderStyle& outlineStyle = outlineStyleForRepaint();
    LayoutUnit outlineWidth = outlineStyle.outlineSize();
    LayoutBoxExtent insetShadowExtent = style().getBoxShadowInsetExtent();
    LayoutUnit width = absoluteValue(newOutlineBox.width() - oldOutlineBox.width());
    if (width) {
        LayoutUnit shadowLeft;
        LayoutUnit shadowRight;
        style().getBoxShadowHorizontalExtent(shadowLeft, shadowRight);
        int borderRight = isBox() ? toRenderBox(this)->borderRight() : LayoutUnit::fromPixel(0);
        LayoutUnit boxWidth = isBox() ? toRenderBox(this)->width() : LayoutUnit();
        LayoutUnit minInsetRightShadowExtent = std::min<LayoutUnit>(-insetShadowExtent.right(), std::min<LayoutUnit>(newBounds.width(), oldBounds.width()));
        LayoutUnit borderWidth = std::max<LayoutUnit>(borderRight, std::max<LayoutUnit>(valueForLength(style().borderTopRightRadius().width(), boxWidth, &view()), valueForLength(style().borderBottomRightRadius().width(), boxWidth)));
        LayoutUnit decorationsWidth = std::max<LayoutUnit>(-outlineStyle.outlineOffset(), borderWidth + minInsetRightShadowExtent) + std::max<LayoutUnit>(outlineWidth, shadowRight);
        LayoutRect rightRect(newOutlineBox.x() + std::min(newOutlineBox.width(), oldOutlineBox.width()) - decorationsWidth,
            newOutlineBox.y(),
            width + decorationsWidth,
            std::max(newOutlineBox.height(), oldOutlineBox.height()));
        LayoutUnit right = std::min<LayoutUnit>(newBounds.maxX(), oldBounds.maxX());
        if (rightRect.x() < right) {
            rightRect.setWidth(std::min(rightRect.width(), right - rightRect.x()));
            repaintUsingContainer(repaintContainer, pixelSnappedIntRect(rightRect));
        }
    }
    LayoutUnit height = absoluteValue(newOutlineBox.height() - oldOutlineBox.height());
    if (height) {
        LayoutUnit shadowTop;
        LayoutUnit shadowBottom;
        style().getBoxShadowVerticalExtent(shadowTop, shadowBottom);
        int borderBottom = isBox() ? toRenderBox(this)->borderBottom() : LayoutUnit::fromPixel(0);
        LayoutUnit boxHeight = isBox() ? toRenderBox(this)->height() : LayoutUnit();
        LayoutUnit minInsetBottomShadowExtent = std::min<LayoutUnit>(-insetShadowExtent.bottom(), std::min<LayoutUnit>(newBounds.height(), oldBounds.height()));
        LayoutUnit borderHeight = std::max<LayoutUnit>(borderBottom, std::max<LayoutUnit>(valueForLength(style().borderBottomLeftRadius().height(), boxHeight), valueForLength(style().borderBottomRightRadius().height(), boxHeight, &view())));
        LayoutUnit decorationsHeight = std::max<LayoutUnit>(-outlineStyle.outlineOffset(), borderHeight + minInsetBottomShadowExtent) + std::max<LayoutUnit>(outlineWidth, shadowBottom);
        LayoutRect bottomRect(newOutlineBox.x(),
            std::min(newOutlineBox.maxY(), oldOutlineBox.maxY()) - decorationsHeight,
            std::max(newOutlineBox.width(), oldOutlineBox.width()),
            height + decorationsHeight);
        LayoutUnit bottom = std::min(newBounds.maxY(), oldBounds.maxY());
        if (bottomRect.y() < bottom) {
            bottomRect.setHeight(std::min(bottomRect.height(), bottom - bottomRect.y()));
            repaintUsingContainer(repaintContainer, pixelSnappedIntRect(bottomRect));
        }
    }
    return false;
}

bool RenderElement::borderImageIsLoadedAndCanBeRendered() const
{
    ASSERT(style().hasBorder());

    StyleImage* borderImage = style().borderImage().image();
    return borderImage && borderImage->canRender(this, style().effectiveZoom()) && borderImage->isLoaded();
}

}
