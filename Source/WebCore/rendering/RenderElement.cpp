/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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
#include "CachedResourceLoader.h"
#include "ContentData.h"
#include "CursorList.h"
#include "DocumentInlines.h"
#include "ElementChildIterator.h"
#include "EventHandler.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "HTMLAnchorElement.h"
#include "HTMLBodyElement.h"
#include "HTMLHtmlElement.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "InlineIteratorLine.h"
#include "InlineIteratorTextBox.h"
#include "LengthFunctions.h"
#include "Logging.h"
#include "Page.h"
#include "PathUtilities.h"
#include "ReferencedSVGResources.h"
#include "RenderBlock.h"
#include "RenderChildIterator.h"
#include "RenderCounter.h"
#include "RenderDeprecatedFlexibleBox.h"
#include "RenderDescendantIterator.h"
#include "RenderFlexibleBox.h"
#include "RenderFragmentedFlow.h"
#include "RenderGrid.h"
#include "RenderImage.h"
#include "RenderImageResourceStyleImage.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerCompositor.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#if ASSERT_ENABLED
#include "RenderListMarker.h"
#endif
#include "RenderFragmentContainer.h"
#include "RenderStyle.h"
#include "RenderTableCaption.h"
#include "RenderTableCell.h"
#include "RenderTableCol.h"
#include "RenderTableRow.h"
#include "RenderText.h"
#include "RenderTheme.h"
#include "RenderTreeBuilder.h"
#include "RenderView.h"
#include "SVGElementTypeHelpers.h"
#include "SVGImage.h"
#include "SVGRenderSupport.h"
#include "Settings.h"
#include "ShadowRoot.h"
#include "StylePendingResources.h"
#include "StyleResolver.h"
#include "Styleable.h"
#include "TextAutoSizing.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>
#include <wtf/StackStats.h>

#if ENABLE(CONTENT_CHANGE_OBSERVER)
#include "ContentChangeObserver.h"
#endif

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(RenderElement);

struct SameSizeAsRenderElement : public RenderObject {
    unsigned bitfields : 25;
    void* firstChild;
    void* lastChild;
    RenderStyle style;
};

static_assert(sizeof(RenderElement) == sizeof(SameSizeAsRenderElement), "RenderElement should stay small");

inline RenderElement::RenderElement(ContainerNode& elementOrDocument, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderObject(elementOrDocument)
    , m_baseTypeFlags(baseTypeFlags)
    , m_ancestorLineBoxDirty(false)
    , m_hasInitializedStyle(false)
    , m_renderBoxNeedsLazyRepaint(false)
    , m_hasPausedImageAnimations(false)
    , m_hasCounterNodeMap(false)
    , m_hasContinuationChainNode(false)
    , m_isContinuation(false)
    , m_isFirstLetter(false)
    , m_hasValidCachedFirstLineStyle(false)
    , m_renderBlockHasMarginBeforeQuirk(false)
    , m_renderBlockHasMarginAfterQuirk(false)
    , m_renderBlockShouldForceRelayoutChildren(false)
    , m_renderBlockFlowHasMarkupTruncation(false)
    , m_renderBlockFlowLineLayoutPath(RenderBlockFlow::UndeterminedPath)
    , m_isRegisteredForVisibleInViewportCallback(false)
    , m_visibleInViewportState(static_cast<unsigned>(VisibleInViewportState::Unknown))
    , m_didContributeToVisuallyNonEmptyPixelCount(false)
    , m_firstChild(nullptr)
    , m_lastChild(nullptr)
    , m_style(WTFMove(style))
{
}

RenderElement::RenderElement(Element& element, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(static_cast<ContainerNode&>(element), WTFMove(style), baseTypeFlags)
{
}

RenderElement::RenderElement(Document& document, RenderStyle&& style, BaseTypeFlags baseTypeFlags)
    : RenderElement(static_cast<ContainerNode&>(document), WTFMove(style), baseTypeFlags)
{
}

RenderElement::~RenderElement()
{
    // Do not add any code here. Add it to willBeDestroyed() instead.
    ASSERT(!m_firstChild);
}

bool RenderElement::isContentDataSupported(const ContentData& contentData)
{
    // Minimal support for content properties replacing an entire element.
    // Works only if we have exactly one piece of content and it's a URL.
    // Otherwise acts as if we didn't support this feature.
    return is<ImageContentData>(contentData) && !contentData.next();
}

RenderPtr<RenderElement> RenderElement::createFor(Element& element, RenderStyle&& style, OptionSet<ConstructBlockLevelRendererFor> rendererTypeOverride)
{

    const ContentData* contentData = style.contentData();
    if (!rendererTypeOverride && contentData && isContentDataSupported(*contentData) && !element.isPseudoElement()) {
        Style::loadPendingResources(style, element.document(), &element);
        auto& styleImage = downcast<ImageContentData>(*contentData).image();
        auto image = createRenderer<RenderImage>(element, WTFMove(style), const_cast<StyleImage*>(&styleImage));
        image->setIsGeneratedContent();
        return image;
    }

    switch (style.display()) {
    case DisplayType::None:
    case DisplayType::Contents:
        return nullptr;
    case DisplayType::Inline:
        if (rendererTypeOverride.contains(ConstructBlockLevelRendererFor::Inline))
            return createRenderer<RenderBlockFlow>(element, WTFMove(style));
        return createRenderer<RenderInline>(element, WTFMove(style));
    case DisplayType::Block:
    case DisplayType::FlowRoot:
    case DisplayType::InlineBlock:
        return createRenderer<RenderBlockFlow>(element, WTFMove(style));
    case DisplayType::ListItem:
        if (rendererTypeOverride.contains(ConstructBlockLevelRendererFor::ListItem))
            return createRenderer<RenderBlockFlow>(element, WTFMove(style));
        return createRenderer<RenderListItem>(element, WTFMove(style));
    case DisplayType::Flex:
    case DisplayType::InlineFlex:
        return createRenderer<RenderFlexibleBox>(element, WTFMove(style));
    case DisplayType::Grid:
    case DisplayType::InlineGrid:
        return createRenderer<RenderGrid>(element, WTFMove(style));
    case DisplayType::Box:
    case DisplayType::InlineBox:
        return createRenderer<RenderDeprecatedFlexibleBox>(element, WTFMove(style));
    default: {
        if (style.isDisplayTableOrTablePart() && rendererTypeOverride.contains(ConstructBlockLevelRendererFor::TableOrTablePart))
            return createRenderer<RenderBlockFlow>(element, WTFMove(style));

        switch (style.display()) {
        case DisplayType::Table:
        case DisplayType::InlineTable:
            return createRenderer<RenderTable>(element, WTFMove(style));
        case DisplayType::TableCell:
            return createRenderer<RenderTableCell>(element, WTFMove(style));
        case DisplayType::TableCaption:
            return createRenderer<RenderTableCaption>(element, WTFMove(style));
        case DisplayType::TableRowGroup:
        case DisplayType::TableHeaderGroup:
        case DisplayType::TableFooterGroup:
            return createRenderer<RenderTableSection>(element, WTFMove(style));
        case DisplayType::TableRow:
            return createRenderer<RenderTableRow>(element, WTFMove(style));
        case DisplayType::TableColumnGroup:
        case DisplayType::TableColumn:
            return createRenderer<RenderTableCol>(element, WTFMove(style));
        default:
            break;
        }
        break;
    }
    }
    ASSERT_NOT_REACHED();
    return nullptr;
}

std::unique_ptr<RenderStyle> RenderElement::computeFirstLineStyle() const
{
    ASSERT(view().usesFirstLineRules());

    RenderElement& rendererForFirstLineStyle = isBeforeOrAfterContent() ? *parent() : const_cast<RenderElement&>(*this);

    if (rendererForFirstLineStyle.isRenderBlockFlow() || rendererForFirstLineStyle.isRenderButton()) {
        RenderBlock* firstLineBlock = rendererForFirstLineStyle.firstLineBlock();
        if (!firstLineBlock)
            return nullptr;
        auto* firstLineStyle = firstLineBlock->getCachedPseudoStyle(PseudoId::FirstLine, &style());
        if (!firstLineStyle)
            return nullptr;
        return RenderStyle::clonePtr(*firstLineStyle);
    }

    if (!rendererForFirstLineStyle.isRenderInline())
        return nullptr;

    auto& parentStyle = rendererForFirstLineStyle.parent()->firstLineStyle();
    if (&parentStyle == &rendererForFirstLineStyle.parent()->style())
        return nullptr;

    if (rendererForFirstLineStyle.isAnonymous()) {
        auto* textRendererWithDisplayContentsParent = RenderText::findByDisplayContentsInlineWrapperCandidate(rendererForFirstLineStyle);
        if (!textRendererWithDisplayContentsParent)
            return nullptr;
        auto* composedTreeParentElement = textRendererWithDisplayContentsParent->textNode()->parentElementInComposedTree();
        if (!composedTreeParentElement)
            return nullptr;

        auto style = composedTreeParentElement->styleResolver().styleForElement(*composedTreeParentElement, { &parentStyle }).renderStyle;
        ASSERT(style->display() == DisplayType::Contents);

        // We act as if there was an unstyled <span> around the text node. Only styling happens via inheritance.
        auto firstLineStyle = RenderStyle::createPtr();
        firstLineStyle->inheritFrom(*style);
        return firstLineStyle;
    }

    return rendererForFirstLineStyle.element()->styleResolver().styleForElement(*element(), { &parentStyle }).renderStyle;
}

const RenderStyle& RenderElement::firstLineStyle() const
{
    if (!view().usesFirstLineRules())
        return style();

    if (!m_hasValidCachedFirstLineStyle) {
        auto firstLineStyle = computeFirstLineStyle();
        if (firstLineStyle || hasRareData())
            const_cast<RenderElement&>(*this).ensureRareData().cachedFirstLineStyle = WTFMove(firstLineStyle);
        m_hasValidCachedFirstLineStyle = true;
    }

    return (hasRareData() && rareData().cachedFirstLineStyle) ? *rareData().cachedFirstLineStyle : style();
}

StyleDifference RenderElement::adjustStyleDifference(StyleDifference diff, OptionSet<StyleDifferenceContextSensitiveProperty> contextSensitiveProperties) const
{
    // If transform changed, and we are not composited, need to do a layout.
    if (contextSensitiveProperties & StyleDifferenceContextSensitiveProperty::Transform) {
        // FIXME: when transforms are taken into account for overflow, we will need to do a layout.
        if (!hasLayer() || !downcast<RenderLayerModelObject>(*this).layer()->isComposited()) {
            if (!hasLayer())
                diff = std::max(diff, StyleDifference::Layout);
            else {
                // We need to set at least SimplifiedLayout, but if PositionedMovementOnly is already set
                // then we actually need SimplifiedLayoutAndPositionedMovement.
                diff = std::max(diff, (diff == StyleDifference::LayoutPositionedMovementOnly) ? StyleDifference::SimplifiedLayoutAndPositionedMovement : StyleDifference::SimplifiedLayout);
            }
        
        } else
            diff = std::max(diff, StyleDifference::RecompositeLayer);
    }

    if (contextSensitiveProperties & StyleDifferenceContextSensitiveProperty::Opacity) {
        if (!hasLayer() || !downcast<RenderLayerModelObject>(*this).layer()->isComposited())
            diff = std::max(diff, StyleDifference::RepaintLayer);
        else
            diff = std::max(diff, StyleDifference::RecompositeLayer);
    }

    if (contextSensitiveProperties & StyleDifferenceContextSensitiveProperty::ClipPath) {
        if (hasLayer() && downcast<RenderLayerModelObject>(*this).layer()->willCompositeClipPath())
            diff = std::max(diff, StyleDifference::RecompositeLayer);
        else
            diff = std::max(diff, StyleDifference::Repaint);
    }
    
    if (contextSensitiveProperties & StyleDifferenceContextSensitiveProperty::WillChange) {
        if (style().willChange() && style().willChange()->canTriggerCompositing())
            diff = std::max(diff, StyleDifference::RecompositeLayer);
    }
    
    if ((contextSensitiveProperties & StyleDifferenceContextSensitiveProperty::Filter) && hasLayer()) {
        auto& layer = *downcast<RenderLayerModelObject>(*this).layer();
        if (!layer.isComposited() || layer.paintsWithFilters())
            diff = std::max(diff, StyleDifference::RepaintLayer);
        else
            diff = std::max(diff, StyleDifference::RecompositeLayer);
    }
    
    // The answer to requiresLayer() for plugins, iframes, and canvas can change without the actual
    // style changing, since it depends on whether we decide to composite these elements. When the
    // layer status of one of these elements changes, we need to force a layout.
    if (diff < StyleDifference::Layout && isRenderLayerModelObject()) {
        if (hasLayer() != downcast<RenderLayerModelObject>(*this).requiresLayer())
            diff = StyleDifference::Layout;
    }

    // If we have no layer(), just treat a RepaintLayer hint as a normal Repaint.
    if (diff == StyleDifference::RepaintLayer && !hasLayer())
        diff = StyleDifference::Repaint;

    return diff;
}

inline bool RenderElement::hasImmediateNonWhitespaceTextChildOrBorderOrOutline() const
{
    for (auto& child : childrenOfType<RenderObject>(*this)) {
        if (is<RenderText>(child) && !downcast<RenderText>(child).isAllCollapsibleWhitespace())
            return true;
        if (child.style().hasOutline() || child.style().hasBorder())
            return true;
    }
    return false;
}

inline bool RenderElement::shouldRepaintForStyleDifference(StyleDifference diff) const
{
    return diff == StyleDifference::Repaint || (diff == StyleDifference::RepaintIfTextOrBorderOrOutline && hasImmediateNonWhitespaceTextChildOrBorderOrOutline());
}

void RenderElement::updateFillImages(const FillLayer* oldLayers, const FillLayer& newLayers)
{
    auto fillImagesAreIdentical = [](const FillLayer* layer1, const FillLayer* layer2) -> bool {
        if (layer1 == layer2)
            return true;

        for (; layer1 && layer2; layer1 = layer1->next(), layer2 = layer2->next()) {
            if (!arePointingToEqualData(layer1->image(), layer2->image()))
                return false;
            if (layer1->image() && layer1->image()->usesDataProtocol())
                return false;
            if (auto styleImage = layer1->image()) {
                if (styleImage->errorOccurred() || !styleImage->hasImage() || styleImage->usesDataProtocol())
                    return false;
            }
        }

        return !layer1 && !layer2;
    };

    auto isRegisteredWithNewFillImages = [&]() -> bool {
        for (auto* layer = &newLayers; layer; layer = layer->next()) {
            if (layer->image() && !layer->image()->hasClient(*this))
                return false;
        }
        return true;
    };

    // If images have the same characteristics and this element is already registered as a
    // client to the new images, there is nothing to do.
    if (fillImagesAreIdentical(oldLayers, &newLayers) && isRegisteredWithNewFillImages())
        return;

    // Add before removing, to avoid removing all clients of an image that is in both sets.
    for (auto* layer = &newLayers; layer; layer = layer->next()) {
        if (layer->image())
            layer->image()->addClient(*this);
    }
    for (auto* layer = oldLayers; layer; layer = layer->next()) {
        if (layer->image())
            layer->image()->removeClient(*this);
    }
}

void RenderElement::updateImage(StyleImage* oldImage, StyleImage* newImage)
{
    if (oldImage == newImage)
        return;
    if (oldImage)
        oldImage->removeClient(*this);
    if (newImage)
        newImage->addClient(*this);
}

void RenderElement::updateShapeImage(const ShapeValue* oldShapeValue, const ShapeValue* newShapeValue)
{
    if (oldShapeValue || newShapeValue)
        updateImage(oldShapeValue ? oldShapeValue->image() : nullptr, newShapeValue ? newShapeValue->image() : nullptr);
}

bool RenderElement::repaintBeforeStyleChange(StyleDifference diff, const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (oldStyle.visibility() == Visibility::Hidden) {
        // Repaint on hidden renderer is a no-op.
        return false;
    }
    enum class RequiredRepaint { None, RendererOnly, RendererAndDescendantsRenderersWithLayers };
    auto shouldRepaintBeforeStyleChange = [&]() -> RequiredRepaint {
        if (!parent()) {
            // Can't resolve absolute coordinates.
            return RequiredRepaint::None;
        }

        if (is<RenderLayerModelObject>(this) && hasLayer()) {
            if (diff == StyleDifference::RepaintLayer)
                return RequiredRepaint::RendererAndDescendantsRenderersWithLayers;

            if (diff == StyleDifference::Layout || diff == StyleDifference::SimplifiedLayout) {
                // Certain style changes require layer repaint, since the layer could end up being destroyed.
                auto layerMayGetDestroyed = oldStyle.position() != newStyle.position()
                    || oldStyle.usedZIndex() != newStyle.usedZIndex()
                    || oldStyle.hasAutoUsedZIndex() != newStyle.hasAutoUsedZIndex()
                    || oldStyle.clip() != newStyle.clip()
                    || oldStyle.hasClip() != newStyle.hasClip()
                    || oldStyle.hasOpacity() != newStyle.hasOpacity()
                    || oldStyle.hasTransform() != newStyle.hasTransform()
                    || oldStyle.hasFilter() != newStyle.hasFilter();
                if (layerMayGetDestroyed)
                    return RequiredRepaint::RendererAndDescendantsRenderersWithLayers;
            }
        }

        if (shouldRepaintForStyleDifference(diff))
            return RequiredRepaint::RendererOnly;

        if (newStyle.outlineSize() < oldStyle.outlineSize())
            return RequiredRepaint::RendererOnly;

        if (is<RenderLayerModelObject>(*this)) {
            // If we don't have a layer yet, but we are going to get one because of transform or opacity, then we need to repaint the old position of the object.
            bool hasLayer = downcast<RenderLayerModelObject>(*this).hasLayer();
            bool willHaveLayer = newStyle.hasTransform() || newStyle.hasOpacity() || newStyle.hasFilter() || newStyle.hasBackdropFilter();
            if (!hasLayer && willHaveLayer)
                return RequiredRepaint::RendererOnly;
        }

        if (is<RenderBox>(*this)) {
            if (diff == StyleDifference::Layout && oldStyle.position() != newStyle.position() && oldStyle.position() == PositionType::Static)
                return RequiredRepaint::RendererOnly;
        }

        if (diff > StyleDifference::RepaintLayer && oldStyle.visibility() != newStyle.visibility()) {
            if (auto* enclosingLayer = this->enclosingLayer()) {
                auto rendererWillBeHidden = newStyle.visibility() != Visibility::Visible;
                if (rendererWillBeHidden && enclosingLayer->hasVisibleContent() && (this == &enclosingLayer->renderer() || enclosingLayer->renderer().style().visibility() != Visibility::Visible))
                    return RequiredRepaint::RendererOnly;
            }
        }

        return RequiredRepaint::None;
    }();

    if (shouldRepaintBeforeStyleChange == RequiredRepaint::RendererAndDescendantsRenderersWithLayers) {
        ASSERT(hasLayer());
        downcast<RenderLayerModelObject>(*this).layer()->repaintIncludingDescendants();
        return true;
    }

    if (shouldRepaintBeforeStyleChange == RequiredRepaint::RendererOnly) {
        repaint();
        return true;
    }

    return false;
}

void RenderElement::initializeStyle()
{
    Style::loadPendingResources(m_style, document(), element());

    styleWillChange(StyleDifference::NewStyle, style());
    m_hasInitializedStyle = true;
    styleDidChange(StyleDifference::NewStyle, nullptr);

    // We shouldn't have any text children that would need styleDidChange at this point.
    ASSERT(!childrenOfType<RenderText>(*this).first());

    // It would be nice to assert that !parent() here, but some RenderLayer subrenderers
    // have their parent set before getting a call to initializeStyle() :|
}

void RenderElement::setStyle(RenderStyle&& style, StyleDifference minimalStyleDifference)
{
    // FIXME: Should change RenderView so it can use initializeStyle too.
    // If we do that, we can assert m_hasInitializedStyle unconditionally,
    // and remove the check of m_hasInitializedStyle below too.
    ASSERT(m_hasInitializedStyle || isRenderView());

    StyleDifference diff = StyleDifference::Equal;
    OptionSet<StyleDifferenceContextSensitiveProperty> contextSensitiveProperties;
    if (m_hasInitializedStyle)
        diff = m_style.diff(style, contextSensitiveProperties);

    diff = std::max(diff, minimalStyleDifference);

    diff = adjustStyleDifference(diff, contextSensitiveProperties);

    Style::loadPendingResources(style, document(), element());

    auto didRepaint = repaintBeforeStyleChange(diff, m_style, style);
    styleWillChange(diff, style);
    auto oldStyle = m_style.replace(WTFMove(style));
    bool detachedFromParent = !parent();

    adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(oldStyle, m_style);

    styleDidChange(diff, &oldStyle);

    // Text renderers use their parent style. Notify them about the change.
    for (auto& child : childrenOfType<RenderText>(*this))
        child.styleDidChange(diff, &oldStyle);

    // FIXME: |this| might be destroyed here. This can currently happen for a RenderTextFragment when
    // its first-letter block gets an update in RenderTextFragment::styleDidChange. For RenderTextFragment(s),
    // we will safely bail out with the detachedFromParent flag. We might want to broaden this condition
    // in the future as we move renderer changes out of layout and into style changes.
    if (detachedFromParent)
        return;

    // Now that the layer (if any) has been updated, we need to adjust the diff again,
    // check whether we should layout now, and decide if we need to repaint.
    StyleDifference updatedDiff = adjustStyleDifference(diff, contextSensitiveProperties);
    
    if (diff <= StyleDifference::LayoutPositionedMovementOnly) {
        if (updatedDiff == StyleDifference::Layout)
            setNeedsLayoutAndPrefWidthsRecalc();
        else if (updatedDiff == StyleDifference::LayoutPositionedMovementOnly)
            setNeedsPositionedMovementLayout(&oldStyle);
        else if (updatedDiff == StyleDifference::SimplifiedLayoutAndPositionedMovement) {
            setNeedsPositionedMovementLayout(&oldStyle);
            setNeedsSimplifiedNormalFlowLayout();
        } else if (updatedDiff == StyleDifference::SimplifiedLayout)
            setNeedsSimplifiedNormalFlowLayout();
    }

    if (!didRepaint && (updatedDiff == StyleDifference::RepaintLayer || shouldRepaintForStyleDifference(updatedDiff))) {
        // Do a repaint with the new style now, e.g., for example if we go from
        // not having an outline to having an outline.
        repaint();
    }
}

void RenderElement::didAttachChild(RenderObject& child, RenderObject*)
{
    if (is<RenderText>(child))
        downcast<RenderText>(child).styleDidChange(StyleDifference::Equal, nullptr);
    // SVG creates renderers for <g display="none">, as SVG requires children of hidden
    // <g>s to have renderers - at least that's how our implementation works. Consider:
    // <g display="none"><foreignObject><body style="position: relative">FOO...
    // - requiresLayer() would return true for the <body>, creating a new RenderLayer
    // - when the document is painted, both layers are painted. The <body> layer doesn't
    //   know that it's inside a "hidden SVG subtree", and thus paints, even if it shouldn't.
    // To avoid the problem alltogether, detect early if we're inside a hidden SVG subtree
    // and stop creating layers at all for these cases - they're not used anyways.
    if (child.hasLayer() && !layerCreationAllowedForSubtree())
        downcast<RenderLayerModelObject>(child).layer()->removeOnlyThisLayer(RenderLayer::LayerChangeTiming::RenderTreeConstruction);
}

RenderObject* RenderElement::attachRendererInternal(RenderPtr<RenderObject> child, RenderObject* beforeChild)
{
    child->setParent(this);

    if (m_firstChild == beforeChild)
        m_firstChild = child.get();

    if (beforeChild) {
        auto* previousSibling = beforeChild->previousSibling();
        if (previousSibling)
            previousSibling->setNextSibling(child.get());
        child->setPreviousSibling(previousSibling);
        child->setNextSibling(beforeChild);
        beforeChild->setPreviousSibling(child.get());
        return child.release();
    }
    if (m_lastChild)
        m_lastChild->setNextSibling(child.get());
    child->setPreviousSibling(m_lastChild);
    m_lastChild = child.get();
    return child.release();
}

RenderPtr<RenderObject> RenderElement::detachRendererInternal(RenderObject& renderer)
{
    auto* parent = renderer.parent();
    ASSERT(parent);
    auto* nextSibling = renderer.nextSibling();

    if (renderer.previousSibling())
        renderer.previousSibling()->setNextSibling(nextSibling);
    if (nextSibling)
        nextSibling->setPreviousSibling(renderer.previousSibling());

    if (parent->firstChild() == &renderer)
        parent->m_firstChild = nextSibling;
    if (parent->lastChild() == &renderer)
        parent->m_lastChild = renderer.previousSibling();

    renderer.setPreviousSibling(nullptr);
    renderer.setNextSibling(nullptr);
    renderer.setParent(nullptr);
    return RenderPtr<RenderObject>(&renderer);
}

static RenderLayer* findNextLayer(const RenderElement& currRenderer, RenderLayer& parentLayer, const RenderObject* siblingToTraverseFrom, bool checkParent = true)
{
    // Step 1: If our layer is a child of the desired parent, then return our layer.
    auto* ourLayer = currRenderer.hasLayer() ? downcast<RenderLayerModelObject>(currRenderer).layer() : nullptr;
    if (ourLayer && ourLayer->parent() == &parentLayer)
        return ourLayer;

    // Step 2: If we don't have a layer, or our layer is the desired parent, then descend
    // into our siblings trying to find the next layer whose parent is the desired parent.
    if (!ourLayer || ourLayer == &parentLayer) {
        for (auto* child = siblingToTraverseFrom ? siblingToTraverseFrom->nextSibling() : currRenderer.firstChild(); child; child = child->nextSibling()) {
            if (!is<RenderElement>(*child))
                continue;
            if (auto* nextLayer = findNextLayer(downcast<RenderElement>(*child), parentLayer, nullptr, false))
                return nextLayer;
        }
    }

    // Step 3: If our layer is the desired parent layer, then we're finished. We didn't
    // find anything.
    if (ourLayer == &parentLayer)
        return nullptr;

    // Step 4: If |checkParent| is set, climb up to our parent and check its siblings that
    // follow us to see if we can locate a layer.
    if (checkParent && currRenderer.parent())
        return findNextLayer(*currRenderer.parent(), parentLayer, &currRenderer, true);

    return nullptr;
}

static RenderLayer* layerNextSiblingRespectingTopLayer(const RenderElement& renderer, RenderLayer& parentLayer)
{
    ASSERT_IMPLIES(isInTopLayerOrBackdrop(renderer.style(), renderer.element()), renderer.hasLayer());

    if (is<RenderLayerModelObject>(renderer) && isInTopLayerOrBackdrop(renderer.style(), renderer.element())) {
        auto& layerModelObject = downcast<RenderLayerModelObject>(renderer);
        ASSERT(layerModelObject.hasLayer());
        auto topLayerLayers = RenderLayer::topLayerRenderLayers(renderer.view());
        auto layerIndex = topLayerLayers.find(layerModelObject.layer());
        if (layerIndex != notFound && layerIndex < topLayerLayers.size() - 1)
            return topLayerLayers[layerIndex + 1];

        return nullptr;
    }

    return findNextLayer(*renderer.parent(), parentLayer, &renderer);
}

static void addLayers(const RenderElement& addedRenderer, RenderElement& currentRenderer, RenderLayer& parentLayer, std::optional<RenderLayer*>& beforeChild)
{
    if (currentRenderer.hasLayer()) {
        if (!beforeChild.has_value())
            beforeChild = layerNextSiblingRespectingTopLayer(addedRenderer, parentLayer);

        parentLayer.addChild(*downcast<RenderLayerModelObject>(currentRenderer).layer(), beforeChild.value());
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(currentRenderer))
        addLayers(addedRenderer, child, parentLayer, beforeChild);
}

void RenderElement::addLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    std::optional<RenderLayer*> beforeChild;
    WebCore::addLayers(*this, *this, *parentLayer, beforeChild);
}

void RenderElement::removeLayers(RenderLayer* parentLayer)
{
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(*downcast<RenderLayerModelObject>(*this).layer());
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.removeLayers(parentLayer);
}

void RenderElement::moveLayers(RenderLayer* oldParent, RenderLayer& newParent)
{
    if (hasLayer()) {
        RenderLayer* layer = downcast<RenderLayerModelObject>(*this).layer();
        ASSERT(oldParent == layer->parent());
        if (oldParent)
            oldParent->removeChild(*layer);
        newParent.addChild(*layer);
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.moveLayers(oldParent, newParent);
}

RenderLayer* RenderElement::layerParent() const
{
    ASSERT_IMPLIES(isInTopLayerOrBackdrop(style(), element()), hasLayer());

    if (hasLayer() && isInTopLayerOrBackdrop(style(), element()))
        return view().layer();

    return parent()->enclosingLayer();
}

// This answers the question "if this renderer had a layer, what would its next sibling layer be".
RenderLayer* RenderElement::layerNextSibling(RenderLayer& parentLayer) const
{
    return WebCore::layerNextSiblingRespectingTopLayer(*this, parentLayer);
}

bool RenderElement::layerCreationAllowedForSubtree() const
{
    RenderElement* parentRenderer = parent();
    while (parentRenderer) {
        if (parentRenderer->isSVGHiddenContainer())
            return false;
        parentRenderer = parentRenderer->parent();
    }
    
    return true;
}

void RenderElement::propagateStyleToAnonymousChildren(StylePropagationType propagationType)
{
    // FIXME: We could save this call when the change only affected non-inherited properties.
    for (auto& elementChild : childrenOfType<RenderElement>(*this)) {
        if (!elementChild.isAnonymous() || elementChild.style().styleType() != PseudoId::None)
            continue;

        if (propagationType == PropagateToBlockChildrenOnly && !is<RenderBlock>(elementChild))
            continue;

#if ENABLE(FULLSCREEN_API)
        if (elementChild.isRenderFullScreen() || elementChild.isRenderFullScreenPlaceholder())
            continue;
#endif

        // RenderFragmentedFlows are updated through the RenderView::styleDidChange function.
        if (is<RenderFragmentedFlow>(elementChild))
            continue;

        auto newStyle = RenderStyle::createAnonymousStyleWithDisplay(style(), elementChild.style().display());
        if (style().specifiesColumns()) {
            if (elementChild.style().specifiesColumns())
                newStyle.inheritColumnPropertiesFrom(style());
            if (elementChild.style().columnSpan() == ColumnSpan::All)
                newStyle.setColumnSpan(ColumnSpan::All);
        }

        // Preserve the position style of anonymous block continuations as they can have relative or sticky position when
        // they contain block descendants of relative or sticky positioned inlines.
        if (elementChild.isInFlowPositioned() && elementChild.isContinuation())
            newStyle.setPosition(elementChild.style().position());

        updateAnonymousChildStyle(newStyle);
        
        elementChild.setStyle(WTFMove(newStyle));
    }
}

static inline bool rendererHasBackground(const RenderElement* renderer)
{
    return renderer && renderer->hasBackground();
}

void RenderElement::invalidateCachedFirstLineStyle()
{
    if (!m_hasValidCachedFirstLineStyle)
        return;
    m_hasValidCachedFirstLineStyle = false;
    // Invalidate the subtree as descendant's first line style may depend on ancestor's.
    for (auto& descendant : descendantsOfType<RenderElement>(*this))
        descendant.m_hasValidCachedFirstLineStyle = false;
}

void RenderElement::styleWillChange(StyleDifference diff, const RenderStyle& newStyle)
{
    ASSERT(settings().shouldAllowUserInstalledFonts() || newStyle.fontDescription().shouldAllowUserInstalledFonts() == AllowUserInstalledFonts::No);

    auto* oldStyle = hasInitializedStyle() ? &style() : nullptr;
    if (oldStyle) {
        // If our z-index changes value or our visibility changes,
        // we need to dirty our stacking context's z-order list.
        bool visibilityChanged = m_style.visibility() != newStyle.visibility()
            || m_style.usedZIndex() != newStyle.usedZIndex()
            || m_style.hasAutoUsedZIndex() != newStyle.hasAutoUsedZIndex();

        if (visibilityChanged)
            document().invalidateRenderingDependentRegions();

        if (visibilityChanged) {
            if (AXObjectCache* cache = document().existingAXObjectCache())
                cache->childrenChanged(parent(), this);
        }

        // Keep layer hierarchy visibility bits up to date if visibility changes.
        if (m_style.visibility() != newStyle.visibility()) {
            if (RenderLayer* layer = enclosingLayer()) {
                if (newStyle.visibility() == Visibility::Visible)
                    layer->setHasVisibleContent();
                else if (layer->hasVisibleContent() && (this == &layer->renderer() || layer->renderer().style().visibility() != Visibility::Visible))
                    layer->dirtyVisibleContentStatus();
            }
        }

        auto needsInvalidateEventRegion = [&] {
            if (m_style.effectiveInert() != newStyle.effectiveInert())
                return true;
            if (m_style.pointerEvents() != newStyle.pointerEvents())
                return true;
#if ENABLE(TOUCH_ACTION_REGIONS)
            if (m_style.effectiveTouchActions() != newStyle.effectiveTouchActions())
                return true;
#endif
            if (m_style.eventListenerRegionTypes() != newStyle.eventListenerRegionTypes())
                return true;
#if ENABLE(EDITABLE_REGION)
            bool wasEditable = m_style.userModify() != UserModify::ReadOnly;
            bool isEditable = newStyle.userModify() != UserModify::ReadOnly;
            if (wasEditable != isEditable)
                return page().shouldBuildEditableRegion();
#endif
            return false;
        };

        if (needsInvalidateEventRegion()) {
            // Usually the event region gets updated as a result of paint invalidation. Here we need to request an update explicitly.
            if (auto* layer = enclosingLayer())
                layer->invalidateEventRegion(RenderLayer::EventRegionInvalidationReason::Style);
        }

        if (isFloating() && m_style.floating() != newStyle.floating()) {
            // For changes in float styles, we need to conceivably remove ourselves
            // from the floating objects list.
            downcast<RenderBox>(*this).removeFloatingOrPositionedChildFromBlockLists();
        } else if (isOutOfFlowPositioned() && m_style.position() != newStyle.position()) {
            // For changes in positioning styles, we need to conceivably remove ourselves
            // from the positioned objects list.
            downcast<RenderBox>(*this).removeFloatingOrPositionedChildFromBlockLists();
        }

        // reset style flags
        if (diff == StyleDifference::Layout || diff == StyleDifference::LayoutPositionedMovementOnly) {
            setFloating(false);
            clearPositionedState();
        }

        if (newStyle.hasPseudoStyle(PseudoId::FirstLine) || oldStyle->hasPseudoStyle(PseudoId::FirstLine))
            invalidateCachedFirstLineStyle();

        setHorizontalWritingMode(true);
        setHasVisibleBoxDecorations(false);
        setHasNonVisibleOverflow(false);
        setHasTransformRelatedProperty(false);
        setHasReflection(false);
    }

    bool hadOutline = oldStyle && oldStyle->hasOutline();
    bool hasOutline = newStyle.hasOutline();
    if (hadOutline != hasOutline) {
        if (hasOutline)
            view().incrementRendersWithOutline();
        else
            view().decrementRendersWithOutline();
    }

    bool newStyleSlowScroll = false;
    if (newStyle.hasFixedBackgroundImage() && !settings().fixedBackgroundsPaintRelativeToDocument()) {
        newStyleSlowScroll = true;
        bool drawsRootBackground = isDocumentElementRenderer() || (isBody() && !rendererHasBackground(document().documentElement()->renderer()));
        if (drawsRootBackground && newStyle.hasEntirelyFixedBackground() && view().compositor().supportsFixedRootBackgroundCompositing())
            newStyleSlowScroll = false;
    }

    if (view().frameView().hasSlowRepaintObject(*this)) {
        if (!newStyleSlowScroll)
            view().frameView().removeSlowRepaintObject(*this);
    } else if (newStyleSlowScroll)
        view().frameView().addSlowRepaintObject(*this);

    if (isDocumentElementRenderer() || isBody())
        view().frameView().updateExtendBackgroundIfNecessary();
}

#if !PLATFORM(IOS_FAMILY)
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
    updateFillImages(oldStyle ? &oldStyle->backgroundLayers() : nullptr, m_style.backgroundLayers());
    updateFillImages(oldStyle ? &oldStyle->maskLayers() : nullptr, m_style.maskLayers());
    updateImage(oldStyle ? oldStyle->borderImage().image() : nullptr, m_style.borderImage().image());
    updateImage(oldStyle ? oldStyle->maskBoxImage().image() : nullptr, m_style.maskBoxImage().image());
    updateShapeImage(oldStyle ? oldStyle->shapeOutside() : nullptr, m_style.shapeOutside());

    SVGRenderSupport::styleChanged(*this, oldStyle);

    if (diff >= StyleDifference::Repaint)
        updateReferencedSVGResources();

    if (!m_parent)
        return;
    
    if (diff == StyleDifference::Layout || diff == StyleDifference::SimplifiedLayout) {
        RenderCounter::rendererStyleChanged(*this, oldStyle, m_style);

        // If the object already needs layout, then setNeedsLayout won't do
        // any work. But if the containing block has changed, then we may need
        // to mark the new containing blocks for layout. The change that can
        // directly affect the containing block of this object is a change to
        // the position style.
        if (needsLayout() && oldStyle && oldStyle->position() != m_style.position())
            markContainingBlocksForLayout();

        if (diff == StyleDifference::Layout)
            setNeedsLayoutAndPrefWidthsRecalc();
        else
            setNeedsSimplifiedNormalFlowLayout();
    } else if (diff == StyleDifference::SimplifiedLayoutAndPositionedMovement) {
        setNeedsPositionedMovementLayout(oldStyle);
        setNeedsSimplifiedNormalFlowLayout();
    } else if (diff == StyleDifference::LayoutPositionedMovementOnly)
        setNeedsPositionedMovementLayout(oldStyle);

    // Don't check for repaint here; we need to wait until the layer has been
    // updated by subclasses before we know if we have to repaint (in setStyle()).

#if !PLATFORM(IOS_FAMILY)
    if (oldStyle && !areCursorsEqual(oldStyle, &style()))
        frame().eventHandler().scheduleCursorUpdate();
#endif

    bool hadOutlineAuto = oldStyle && oldStyle->outlineStyleIsAuto() == OutlineIsAuto::On;
    bool hasOutlineAuto = outlineStyleForRepaint().outlineStyleIsAuto() == OutlineIsAuto::On;
    if (hasOutlineAuto != hadOutlineAuto) {
        updateOutlineAutoAncestor(hasOutlineAuto);
        issueRepaintForOutlineAuto(hasOutlineAuto ? outlineStyleForRepaint().outlineSize() : oldStyle->outlineSize());
    }
}

void RenderElement::insertedIntoTree(IsInternalMove isInternalMove)
{
    // Keep our layer hierarchy updated. Optimize for the common case where we don't have any children
    // and don't have a layer attached to ourselves.
    RenderLayer* parentLayer = nullptr;
    if (firstChild() || hasLayer()) {
        auto* parentLayer = layerParent();
        addLayers(parentLayer);
    }

    // If |this| is visible but this object was not, tell the layer it has some visible content
    // that needs to be drawn and layer visibility optimization can't be used
    if (parent()->style().visibility() != Visibility::Visible && style().visibility() == Visibility::Visible && !hasLayer()) {
        if (!parentLayer)
            parentLayer = layerParent();
        if (parentLayer)
            parentLayer->dirtyVisibleContentStatus();
    }

    RenderObject::insertedIntoTree(isInternalMove);
}

void RenderElement::willBeRemovedFromTree(IsInternalMove isInternalMove)
{
    // If we remove a visible child from an invisible parent, we don't know the layer visibility any more.
    if (parent()->style().visibility() != Visibility::Visible && style().visibility() == Visibility::Visible && !hasLayer()) {
        // FIXME: should get parent layer. Necessary?
        if (auto* enclosingLayer = parent()->enclosingLayer())
            enclosingLayer->dirtyVisibleContentStatus();
    }
    // Keep our layer hierarchy updated.
    if (firstChild() || hasLayer()) {
        auto* parentLayer = layerParent();
        removeLayers(parentLayer);
    }

    if (isOutOfFlowPositioned() && parent()->childrenInline())
        parent()->dirtyLinesFromChangedChild(*this);

    RenderObject::willBeRemovedFromTree(isInternalMove);
}

inline void RenderElement::clearSubtreeLayoutRootIfNeeded() const
{
    if (renderTreeBeingDestroyed())
        return;

    if (view().frameView().layoutContext().subtreeLayoutRoot() != this)
        return;

    // Normally when a renderer is detached from the tree, the appropriate dirty bits get set
    // which ensures that this renderer is no longer the layout root.
    ASSERT_NOT_REACHED();
    
    // This indicates a failure to layout the child, which is why
    // the layout root is still set to |this|. Make sure to clear it
    // since we are getting destroyed.
    view().frameView().layoutContext().clearSubtreeLayoutRoot();
}

void RenderElement::willBeDestroyed()
{
#if ENABLE(CONTENT_CHANGE_OBSERVER)
    if (!renderTreeBeingDestroyed() && element())
        document().contentChangeObserver().rendererWillBeDestroyed(*element());
#endif
    if (m_style.hasFixedBackgroundImage() && !settings().fixedBackgroundsPaintRelativeToDocument())
        view().frameView().removeSlowRepaintObject(*this);

    unregisterForVisibleInViewportCallback();

    if (hasCounterNodeMap())
        RenderCounter::destroyCounterNodes(*this);

    if (style().hasOutline())
        view().decrementRendersWithOutline();

    RenderObject::willBeDestroyed();

    clearSubtreeLayoutRootIfNeeded();

    if (hasInitializedStyle()) {
        for (auto* bgLayer = &m_style.backgroundLayers(); bgLayer; bgLayer = bgLayer->next()) {
            if (auto* backgroundImage = bgLayer->image())
                backgroundImage->removeClient(*this);
        }
        for (auto* maskLayer = &m_style.maskLayers(); maskLayer; maskLayer = maskLayer->next()) {
            if (auto* maskImage = maskLayer->image())
                maskImage->removeClient(*this);
        }
        if (auto* borderImage = m_style.borderImage().image())
            borderImage->removeClient(*this);
        if (auto* maskBoxImage = m_style.maskBoxImage().image())
            maskBoxImage->removeClient(*this);
        if (auto shapeValue = m_style.shapeOutside()) {
            if (auto shapeImage = shapeValue->image())
                shapeImage->removeClient(*this);
        }
    }
    if (m_hasPausedImageAnimations)
        view().removeRendererWithPausedImageAnimations(*this);
}

void RenderElement::setNeedsPositionedMovementLayout(const RenderStyle* oldStyle)
{
    ASSERT(!isSetNeedsLayoutForbidden());
    if (needsPositionedMovementLayout())
        return;
    setNeedsPositionedMovementLayoutBit(true);
    markContainingBlocksForLayout();
    if (hasLayer()) {
        if (oldStyle && style().diffRequiresLayerRepaint(*oldStyle, downcast<RenderLayerModelObject>(*this).layer()->isComposited()))
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

static inline void paintPhase(RenderElement& element, PaintPhase phase, PaintInfo& paintInfo, const LayoutPoint& childPoint)
{
    paintInfo.phase = phase;
    element.paint(paintInfo, childPoint);
}

void RenderElement::paintAsInlineBlock(PaintInfo& paintInfo, const LayoutPoint& childPoint)
{
    // Paint all phases atomically, as though the element established its own stacking context.
    // (See Appendix E.2, section 6.4 on inline block/table/replaced elements in the CSS2.1 specification.)
    // This is also used by other elements (e.g. flex items and grid items).
    PaintPhase paintPhaseToUse = isExcludedAndPlacedInBorder() ? paintInfo.phase : PaintPhase::Foreground;
    if (paintInfo.phase == PaintPhase::Selection || paintInfo.phase == PaintPhase::EventRegion)
        paint(paintInfo, childPoint);
    else if (paintInfo.phase == paintPhaseToUse) {
        paintPhase(*this, PaintPhase::BlockBackground, paintInfo, childPoint);
        paintPhase(*this, PaintPhase::ChildBlockBackgrounds, paintInfo, childPoint);
        paintPhase(*this, PaintPhase::Float, paintInfo, childPoint);
        paintPhase(*this, PaintPhase::Foreground, paintInfo, childPoint);
        paintPhase(*this, PaintPhase::Outline, paintInfo, childPoint);

        // Reset |paintInfo| to the original phase.
        paintInfo.phase = paintPhaseToUse;
    }
}

void RenderElement::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());
    for (auto* child = firstChild(); child; child = child->nextSibling()) {
        if (child->needsLayout())
            downcast<RenderElement>(*child).layout();
        ASSERT(!child->needsLayout());
    }
    clearNeedsLayout();
}

static bool mustRepaintFillLayers(const RenderElement& renderer, const FillLayer& layer)
{
    // Nobody will use multiple layers without wanting fancy positioning.
    if (layer.next())
        return true;

    // Make sure we have a valid image.
    auto* image = layer.image();
    if (!image || !image->canRender(&renderer, renderer.style().effectiveZoom()))
        return false;

    if (!layer.xPosition().isZero() || !layer.yPosition().isZero())
        return true;

    auto sizeType = layer.sizeType();

    if (sizeType == FillSizeType::Contain || sizeType == FillSizeType::Cover)
        return true;

    if (sizeType == FillSizeType::Size) {
        auto size = layer.sizeLength();
        if (size.width.isPercentOrCalculated() || size.height.isPercentOrCalculated())
            return true;
        // If the image has neither an intrinsic width nor an intrinsic height, its size is determined as for 'contain'.
        if ((size.width.isAuto() || size.height.isAuto()) && image->isGeneratedImage())
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
    if (!renderer.hasVisibleBoxDecorations())
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
    if (!fullRepaint) {
        // This ASSERT fails due to animations. See https://bugs.webkit.org/show_bug.cgi?id=37048
        // ASSERT(!newOutlineBoxRectPtr || *newOutlineBoxRectPtr == outlineBoundsForRepaint(repaintContainer));
        newOutlineBox = newOutlineBoxRectPtr ? *newOutlineBoxRectPtr : outlineBoundsForRepaint(repaintContainer);
        fullRepaint = (newOutlineBox.location() != oldOutlineBox.location() || (mustRepaintBackgroundOrBorder(*this) && (newBounds != oldBounds || newOutlineBox != oldOutlineBox)));
    }

    if (!repaintContainer)
        repaintContainer = &view();

    if (fullRepaint) {
        repaintUsingContainer(repaintContainer, oldBounds);
        if (newBounds != oldBounds)
            repaintUsingContainer(repaintContainer, newBounds);
        return true;
    }

    if (newBounds == oldBounds && newOutlineBox == oldOutlineBox)
        return false;

    LayoutUnit deltaLeft = newBounds.x() - oldBounds.x();
    if (deltaLeft > 0)
        repaintUsingContainer(repaintContainer, LayoutRect(oldBounds.x(), oldBounds.y(), deltaLeft, oldBounds.height()));
    else if (deltaLeft < 0)
        repaintUsingContainer(repaintContainer, LayoutRect(newBounds.x(), newBounds.y(), -deltaLeft, newBounds.height()));

    LayoutUnit deltaRight = newBounds.maxX() - oldBounds.maxX();
    if (deltaRight > 0)
        repaintUsingContainer(repaintContainer, LayoutRect(oldBounds.maxX(), newBounds.y(), deltaRight, newBounds.height()));
    else if (deltaRight < 0)
        repaintUsingContainer(repaintContainer, LayoutRect(newBounds.maxX(), oldBounds.y(), -deltaRight, oldBounds.height()));

    LayoutUnit deltaTop = newBounds.y() - oldBounds.y();
    if (deltaTop > 0)
        repaintUsingContainer(repaintContainer, LayoutRect(oldBounds.x(), oldBounds.y(), oldBounds.width(), deltaTop));
    else if (deltaTop < 0)
        repaintUsingContainer(repaintContainer, LayoutRect(newBounds.x(), newBounds.y(), newBounds.width(), -deltaTop));

    LayoutUnit deltaBottom = newBounds.maxY() - oldBounds.maxY();
    if (deltaBottom > 0)
        repaintUsingContainer(repaintContainer, LayoutRect(newBounds.x(), oldBounds.maxY(), newBounds.width(), deltaBottom));
    else if (deltaBottom < 0)
        repaintUsingContainer(repaintContainer, LayoutRect(oldBounds.x(), newBounds.maxY(), oldBounds.width(), -deltaBottom));

    if (newOutlineBox == oldOutlineBox)
        return false;

    // We didn't move, but we did change size. Invalidate the delta, which will consist of possibly
    // two rectangles (but typically only one).
    const RenderStyle& outlineStyle = outlineStyleForRepaint();
    LayoutUnit outlineWidth { outlineStyle.outlineSize() };
    auto insetShadowExtent = style().boxShadowInsetExtent();
    LayoutUnit width = absoluteValue(newOutlineBox.width() - oldOutlineBox.width());
    if (width) {
        LayoutUnit shadowLeft;
        LayoutUnit shadowRight;
        style().getBoxShadowHorizontalExtent(shadowLeft, shadowRight);
        LayoutUnit borderRight = is<RenderBox>(*this) ? downcast<RenderBox>(*this).borderRight() : 0_lu;
        LayoutUnit boxWidth = is<RenderBox>(*this) ? downcast<RenderBox>(*this).width() : 0_lu;
        LayoutUnit minInsetRightShadowExtent = std::min<LayoutUnit>(-insetShadowExtent.right(), std::min(newBounds.width(), oldBounds.width()));
        LayoutUnit borderWidth = std::max(borderRight, std::max(valueForLength(style().borderTopRightRadius().width, boxWidth), valueForLength(style().borderBottomRightRadius().width, boxWidth)));
        LayoutUnit decorationsWidth = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderWidth + minInsetRightShadowExtent) + std::max(outlineWidth, shadowRight);
        LayoutRect rightRect(newOutlineBox.x() + std::min(newOutlineBox.width(), oldOutlineBox.width()) - decorationsWidth,
            newOutlineBox.y(),
            width + decorationsWidth,
            std::max(newOutlineBox.height(), oldOutlineBox.height()));
        LayoutUnit right = std::min(newBounds.maxX(), oldBounds.maxX());
        if (rightRect.x() < right) {
            rightRect.setWidth(std::min(rightRect.width(), right - rightRect.x()));
            repaintUsingContainer(repaintContainer, rightRect);
        }
    }
    LayoutUnit height = absoluteValue(newOutlineBox.height() - oldOutlineBox.height());
    if (height) {
        LayoutUnit shadowTop;
        LayoutUnit shadowBottom;
        style().getBoxShadowVerticalExtent(shadowTop, shadowBottom);
        LayoutUnit borderBottom = is<RenderBox>(*this) ? downcast<RenderBox>(*this).borderBottom() : 0_lu;
        LayoutUnit boxHeight = is<RenderBox>(*this) ? downcast<RenderBox>(*this).height() : 0_lu;
        LayoutUnit minInsetBottomShadowExtent = std::min<LayoutUnit>(-insetShadowExtent.bottom(), std::min(newBounds.height(), oldBounds.height()));
        LayoutUnit borderHeight = std::max(borderBottom, std::max(valueForLength(style().borderBottomLeftRadius().height, boxHeight),
            valueForLength(style().borderBottomRightRadius().height, boxHeight)));
        LayoutUnit decorationsHeight = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderHeight + minInsetBottomShadowExtent) + std::max(outlineWidth, shadowBottom);
        LayoutRect bottomRect(newOutlineBox.x(),
            std::min(newOutlineBox.maxY(), oldOutlineBox.maxY()) - decorationsHeight,
            std::max(newOutlineBox.width(), oldOutlineBox.width()),
            height + decorationsHeight);
        LayoutUnit bottom = std::min(newBounds.maxY(), oldBounds.maxY());
        if (bottomRect.y() < bottom) {
            bottomRect.setHeight(std::min(bottomRect.height(), bottom - bottomRect.y()));
            repaintUsingContainer(repaintContainer, bottomRect);
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

bool RenderElement::mayCauseRepaintInsideViewport(const IntRect* optionalViewportRect) const
{
    auto& frameView = view().frameView();
    if (frameView.isOffscreen())
        return false;

    if (!hasNonVisibleOverflow()) {
        // FIXME: Computing the overflow rect is expensive if any descendant has
        // its own self-painting layer. As a result, we prefer to abort early in
        // this case and assume it may cause us to repaint inside the viewport.
        if (!hasLayer() || downcast<RenderLayerModelObject>(*this).layer()->firstChild())
            return true;
    }

    // Compute viewport rect if it was not provided.
    const IntRect& visibleRect = optionalViewportRect ? *optionalViewportRect : frameView.windowToContents(frameView.windowClipRect());
    return visibleRect.intersects(enclosingIntRect(absoluteClippedOverflowRectForRepaint()));
}

bool RenderElement::isVisibleIgnoringGeometry() const
{
    if (document().activeDOMObjectsAreSuspended())
        return false;
    if (style().visibility() != Visibility::Visible)
        return false;
    if (view().frameView().isOffscreen())
        return false;

    return true;
}

bool RenderElement::isVisibleInDocumentRect(const IntRect& documentRect) const
{
    if (!isVisibleIgnoringGeometry())
        return false;

    // Use background rect if we are the root or if we are the body and the background is propagated to the root.
    // FIXME: This is overly conservative as the image may not be a background-image, in which case it will not
    // be propagated to the root. At this point, we unfortunately don't have access to the image anymore so we
    // can no longer check if it is a background image.
    bool backgroundIsPaintedByRoot = isDocumentElementRenderer();
    if (isBody()) {
        auto& rootRenderer = *parent(); // If <body> has a renderer then <html> does too.
        ASSERT(rootRenderer.isDocumentElementRenderer());
        ASSERT(is<HTMLHtmlElement>(rootRenderer.element()));
        // FIXME: Should share body background propagation code.
        backgroundIsPaintedByRoot = !rootRenderer.hasBackground();
    }

    LayoutRect backgroundPaintingRect = backgroundIsPaintedByRoot ? view().backgroundRect() : absoluteClippedOverflowRectForRepaint();
    if (!documentRect.intersects(enclosingIntRect(backgroundPaintingRect)))
        return false;

    return true;
}

bool RenderElement::isInsideEntirelyHiddenLayer() const
{
    return style().visibility() != Visibility::Visible && !enclosingLayer()->hasVisibleContent();
}

void RenderElement::registerForVisibleInViewportCallback()
{
    if (m_isRegisteredForVisibleInViewportCallback)
        return;
    m_isRegisteredForVisibleInViewportCallback = true;

    view().registerForVisibleInViewportCallback(*this);
}

void RenderElement::unregisterForVisibleInViewportCallback()
{
    if (!m_isRegisteredForVisibleInViewportCallback)
        return;
    m_isRegisteredForVisibleInViewportCallback = false;

    view().unregisterForVisibleInViewportCallback(*this);
}

void RenderElement::setVisibleInViewportState(VisibleInViewportState state)
{
    if (state == visibleInViewportState())
        return;
    m_visibleInViewportState = static_cast<unsigned>(state);
    visibleInViewportStateChanged();
}

void RenderElement::visibleInViewportStateChanged()
{
    ASSERT_NOT_REACHED();
}

bool RenderElement::isVisibleInViewport() const
{
    auto& frameView = view().frameView();
    auto visibleRect = frameView.windowToContents(frameView.windowClipRect());
    return isVisibleInDocumentRect(visibleRect);
}

VisibleInViewportState RenderElement::imageFrameAvailable(CachedImage& image, ImageAnimatingState animatingState, const IntRect* changeRect)
{
    bool isVisible = isVisibleInViewport();

    if (!isVisible && animatingState == ImageAnimatingState::Yes)
        view().addRendererWithPausedImageAnimations(*this, image);

    // Static images should repaint even if they are outside the viewport rectangle
    // because they should be inside the TileCoverageRect.
    if (isVisible || animatingState == ImageAnimatingState::No)
        imageChanged(&image, changeRect);

    if (element() && image.image()->isBitmapImage())
        element()->dispatchWebKitImageReadyEventForTesting();

    return isVisible ? VisibleInViewportState::Yes : VisibleInViewportState::No;
}

VisibleInViewportState RenderElement::imageVisibleInViewport(const Document& document) const
{
    if (&this->document() != &document)
        return VisibleInViewportState::No;

    return isVisibleInViewport() ? VisibleInViewportState::Yes : VisibleInViewportState::No;
}

void RenderElement::notifyFinished(CachedResource& resource, const NetworkLoadMetrics&)
{
    document().cachedResourceLoader().notifyFinished(resource);
}

void RenderElement::didRemoveCachedImageClient(CachedImage& cachedImage)
{
    if (hasPausedImageAnimations())
        view().removeRendererWithPausedImageAnimations(*this, cachedImage);
}

void RenderElement::scheduleRenderingUpdateForImage(CachedImage&)
{
    if (auto* page = document().page())
        page->scheduleRenderingUpdate(RenderingUpdateStep::Images);
}

bool RenderElement::repaintForPausedImageAnimationsIfNeeded(const IntRect& visibleRect, CachedImage& cachedImage)
{
    ASSERT(m_hasPausedImageAnimations);
    if (!isVisibleInDocumentRect(visibleRect))
        return false;

    repaint();

    if (auto* image = cachedImage.image()) {
        if (is<SVGImage>(image))
            downcast<SVGImage>(image)->scheduleStartAnimation();
        else
            image->startAnimation();
    }

    // For directly-composited animated GIFs it does not suffice to call repaint() to resume animation. We need to mark the image as changed.
    if (is<RenderBoxModelObject>(*this))
        downcast<RenderBoxModelObject>(*this).contentChanged(ImageChanged);

    return true;
}

const RenderStyle* RenderElement::getCachedPseudoStyle(PseudoId pseudo, const RenderStyle* parentStyle) const
{
    if (pseudo < PseudoId::FirstInternalPseudoId && !style().hasPseudoStyle(pseudo))
        return nullptr;

    RenderStyle* cachedStyle = style().getCachedPseudoStyle(pseudo);
    if (cachedStyle)
        return cachedStyle;

    std::unique_ptr<RenderStyle> result = getUncachedPseudoStyle({ pseudo }, parentStyle);
    if (result)
        return const_cast<RenderStyle&>(m_style).addCachedPseudoStyle(WTFMove(result));
    return nullptr;
}

std::unique_ptr<RenderStyle> RenderElement::getUncachedPseudoStyle(const Style::PseudoElementRequest& pseudoElementRequest, const RenderStyle* parentStyle, const RenderStyle* ownStyle) const
{
    if (pseudoElementRequest.pseudoId < PseudoId::FirstInternalPseudoId && !ownStyle && !style().hasPseudoStyle(pseudoElementRequest.pseudoId))
        return nullptr;

    if (!parentStyle) {
        ASSERT(!ownStyle);
        parentStyle = &style();
    }

    if (isAnonymous())
        return nullptr;

    auto& styleResolver = element()->styleResolver();

    std::unique_ptr<RenderStyle> style = styleResolver.pseudoStyleForElement(*element(), pseudoElementRequest, { parentStyle });

    if (style)
        Style::loadPendingResources(*style, document(), element());

    return style;
}

Color RenderElement::selectionColor(CSSPropertyID colorProperty) const
{
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if (style().userSelectIncludingInert() == UserSelect::None
        || (view().frameView().paintBehavior().containsAny({ PaintBehavior::SelectionOnly, PaintBehavior::SelectionAndBackgroundsOnly })))
        return Color();

    if (std::unique_ptr<RenderStyle> pseudoStyle = selectionPseudoStyle()) {
        Color color = pseudoStyle->visitedDependentColorWithColorFilter(colorProperty);
        if (!color.isValid())
            color = pseudoStyle->visitedDependentColorWithColorFilter(CSSPropertyColor);
        return color;
    }

    if (frame().selection().isFocusedAndActive())
        return theme().activeSelectionForegroundColor(styleColorOptions());
    return theme().inactiveSelectionForegroundColor(styleColorOptions());
}

std::unique_ptr<RenderStyle> RenderElement::selectionPseudoStyle() const
{
    if (isAnonymous())
        return nullptr;

    if (auto selectionStyle = getUncachedPseudoStyle({ PseudoId::Selection })) {
        // We intentionally return the pseudo selection style here if it exists before ascending to
        // the shadow host element. This allows us to apply selection pseudo styles in user agent
        // shadow roots, instead of always deferring to the shadow host's selection pseudo style.
        return selectionStyle;
    }

    if (RefPtr root = element()->containingShadowRoot()) {
        if (root->mode() == ShadowRootMode::UserAgent) {
            RefPtr currentElement = element()->shadowHost();
            // When an element has display: contents, this element doesn't have a renderer
            // and its children will render as children of the parent element.
            while (currentElement && currentElement->hasDisplayContents())
                currentElement = currentElement->parentElement();
            if (currentElement && currentElement->renderer())
                return currentElement->renderer()->getUncachedPseudoStyle({ PseudoId::Selection });
        }
    }

    return nullptr;
}

Color RenderElement::selectionForegroundColor() const
{
    return selectionColor(CSSPropertyWebkitTextFillColor);
}

Color RenderElement::selectionEmphasisMarkColor() const
{
    return selectionColor(CSSPropertyTextEmphasisColor);
}

Color RenderElement::selectionBackgroundColor() const
{
    if (style().userSelectIncludingInert() == UserSelect::None)
        return Color();

    if (frame().selection().shouldShowBlockCursor() && frame().selection().isCaret())
        return theme().transformSelectionBackgroundColor(style().visitedDependentColorWithColorFilter(CSSPropertyColor), styleColorOptions());

    std::unique_ptr<RenderStyle> pseudoStyle = selectionPseudoStyle();
    if (pseudoStyle && pseudoStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor).isValid())
        return theme().transformSelectionBackgroundColor(pseudoStyle->visitedDependentColorWithColorFilter(CSSPropertyBackgroundColor), styleColorOptions());

    if (frame().selection().isFocusedAndActive())
        return theme().activeSelectionBackgroundColor(styleColorOptions());
    return theme().inactiveSelectionBackgroundColor(styleColorOptions());
}

bool RenderElement::getLeadingCorner(FloatPoint& point, bool& insideFixed) const
{
    if (!isInline() || isReplacedOrInlineBlock()) {
        point = localToAbsolute(FloatPoint(), UseTransforms, &insideFixed);
        return true;
    }

    // find the next text/image child, to get a position
    const RenderObject* o = this;
    while (o) {
        const RenderObject* p = o;
        if (RenderObject* child = o->firstChildSlow())
            o = child;
        else if (o->nextSibling())
            o = o->nextSibling();
        else {
            RenderObject* next = 0;
            while (!next && o->parent()) {
                o = o->parent();
                next = o->nextSibling();
            }
            o = next;

            if (!o)
                break;
        }
        ASSERT(o);

        if (!o->isInline() || o->isReplacedOrInlineBlock()) {
            point = o->localToAbsolute(FloatPoint(), UseTransforms, &insideFixed);
            return true;
        }

        if (p->node() && p->node() == element() && is<RenderText>(*o) && !InlineIterator::firstTextBoxFor(downcast<RenderText>(*o))) {
            // do nothing - skip unrendered whitespace that is a child or next sibling of the anchor
        } else if (is<RenderText>(*o) || o->isReplacedOrInlineBlock()) {
            point = FloatPoint();
            if (is<RenderText>(*o)) {
                auto& textRenderer = downcast<RenderText>(*o);
                if (auto run = InlineIterator::firstTextBoxFor(textRenderer))
                    point.move(textRenderer.linesBoundingBox().x(), run->line()->top());
            } else if (is<RenderBox>(*o))
                point.moveBy(downcast<RenderBox>(*o).location());
            point = o->container()->localToAbsolute(point, UseTransforms, &insideFixed);
            return true;
        }
    }
    
    // If the target doesn't have any children or siblings that could be used to calculate the scroll position, we must be
    // at the end of the document. Scroll to the bottom. FIXME: who said anything about scrolling?
    if (!o && document().view()) {
        point = FloatPoint(0, document().view()->contentsHeight());
        return true;
    }
    return false;
}

bool RenderElement::getTrailingCorner(FloatPoint& point, bool& insideFixed) const
{
    if (!isInline() || isReplacedOrInlineBlock()) {
        point = localToAbsolute(LayoutPoint(downcast<RenderBox>(*this).size()), UseTransforms, &insideFixed);
        return true;
    }

    // find the last text/image child, to get a position
    const RenderObject* o = this;
    while (o) {
        if (RenderObject* child = o->lastChildSlow())
            o = child;
        else if (o->previousSibling())
            o = o->previousSibling();
        else {
            RenderObject* prev = 0;
            while (!prev) {
                o = o->parent();
                if (!o)
                    return false;
                prev = o->previousSibling();
            }
            o = prev;
        }
        ASSERT(o);
        if (is<RenderText>(*o) || o->isReplacedOrInlineBlock()) {
            point = FloatPoint();
            if (is<RenderText>(*o)) {
                LayoutRect linesBox = downcast<RenderText>(*o).linesBoundingBox();
                if (!linesBox.maxX() && !linesBox.maxY())
                    continue;
                point.moveBy(linesBox.maxXMaxYCorner());
            } else
                point.moveBy(downcast<RenderBox>(*o).frameRect().maxXMaxYCorner());
            point = o->container()->localToAbsolute(point, UseTransforms, &insideFixed);
            return true;
        }
    }
    return true;
}

LayoutRect RenderElement::absoluteAnchorRect(bool* insideFixed) const
{
    FloatPoint leading, trailing;
    bool leadingInFixed = false;
    bool trailingInFixed = false;
    getLeadingCorner(leading, leadingInFixed);
    getTrailingCorner(trailing, trailingInFixed);

    FloatPoint upperLeft = leading;
    FloatPoint lowerRight = trailing;

    // Vertical writing modes might mean the leading point is not in the top left
    if (!isInline() || isReplacedOrInlineBlock()) {
        upperLeft = FloatPoint(std::min(leading.x(), trailing.x()), std::min(leading.y(), trailing.y()));
        lowerRight = FloatPoint(std::max(leading.x(), trailing.x()), std::max(leading.y(), trailing.y()));
    } // Otherwise, it's not obvious what to do.

    if (insideFixed) {
        // For now, just look at the leading corner. Handling one inside fixed and one not would be tricky.
        *insideFixed = leadingInFixed;
    }

    return enclosingLayoutRect(FloatRect(upperLeft, lowerRight.expandedTo(upperLeft) - upperLeft));
}

LayoutRect RenderElement::absoluteAnchorRectWithScrollMargin(bool* insideFixed) const
{
    LayoutRect anchorRect = absoluteAnchorRect(insideFixed);
    const LengthBox& scrollMargin = style().scrollMargin();
    if (scrollMargin.isZero())
        return anchorRect;

    // The scroll snap specification says that the scroll-margin should be applied in the
    // coordinate system of the scroll container and applied to the rectangular bounding
    // box of the transformed border box of the target element.
    // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-margin.
    const LayoutBoxExtent margin(
        valueForLength(scrollMargin.top(), anchorRect.height()),
        valueForLength(scrollMargin.right(), anchorRect.width()),
        valueForLength(scrollMargin.bottom(), anchorRect.height()),
        valueForLength(scrollMargin.left(), anchorRect.width()));
    anchorRect.expand(margin);
    return anchorRect;
}

void RenderElement::drawLineForBoxSide(GraphicsContext& graphicsContext, const FloatRect& rect, BoxSide side, Color color, BorderStyle borderStyle, float adjacentWidth1, float adjacentWidth2, bool antialias) const
{
    auto drawBorderRect = [&graphicsContext] (const FloatRect& rect)
    {
        if (rect.isEmpty())
            return;
        graphicsContext.drawRect(rect);
    };

    auto drawLineFor = [this, &graphicsContext, color, antialias] (const FloatRect& rect, BoxSide side, BorderStyle borderStyle, const FloatSize& adjacent)
    {
        if (rect.isEmpty())
            return;
        drawLineForBoxSide(graphicsContext, rect, side, color, borderStyle, adjacent.width(), adjacent.height(), antialias);
    };

    float x1 = rect.x();
    float x2 = rect.maxX();
    float y1 = rect.y();
    float y2 = rect.maxY();
    float thickness;
    float length;
    if (side == BoxSide::Top || side == BoxSide::Bottom) {
        thickness = y2 - y1;
        length = x2 - x1;
    } else {
        thickness = x2 - x1;
        length = y2 - y1;
    }
    // FIXME: We really would like this check to be an ASSERT as we don't want to draw empty borders. However
    // nothing guarantees that the following recursive calls to drawLineForBoxSide will have non-null dimensions.
    if (!thickness || !length)
        return;

    float deviceScaleFactor = document().deviceScaleFactor();
    if (borderStyle == BorderStyle::Double && (thickness * deviceScaleFactor) < 3)
        borderStyle = BorderStyle::Solid;

    switch (borderStyle) {
    case BorderStyle::None:
    case BorderStyle::Hidden:
        return;
    case BorderStyle::Dotted:
    case BorderStyle::Dashed: {
        bool wasAntialiased = graphicsContext.shouldAntialias();
        StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
        graphicsContext.setShouldAntialias(antialias);
        graphicsContext.setStrokeColor(color);
        graphicsContext.setStrokeThickness(thickness);
        graphicsContext.setStrokeStyle(borderStyle == BorderStyle::Dashed ? DashedStroke : DottedStroke);
        graphicsContext.drawLine(roundPointToDevicePixels(LayoutPoint(x1, y1), deviceScaleFactor), roundPointToDevicePixels(LayoutPoint(x2, y2), deviceScaleFactor));
        graphicsContext.setShouldAntialias(wasAntialiased);
        graphicsContext.setStrokeStyle(oldStrokeStyle);
        break;
    }
    case BorderStyle::Double: {
        float thirdOfThickness = ceilToDevicePixel(thickness / 3, deviceScaleFactor);
        ASSERT(thirdOfThickness);

        if (!adjacentWidth1 && !adjacentWidth2) {
            StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
            graphicsContext.setStrokeStyle(NoStroke);
            graphicsContext.setFillColor(color);

            bool wasAntialiased = graphicsContext.shouldAntialias();
            graphicsContext.setShouldAntialias(antialias);

            switch (side) {
            case BoxSide::Top:
            case BoxSide::Bottom:
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, length, thirdOfThickness), deviceScaleFactor));
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y2 - thirdOfThickness, length, thirdOfThickness), deviceScaleFactor));
                break;
            case BoxSide::Left:
            case BoxSide::Right:
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, thirdOfThickness, length), deviceScaleFactor));
                drawBorderRect(snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1, thirdOfThickness, length), deviceScaleFactor));
                break;
            }

            graphicsContext.setShouldAntialias(wasAntialiased);
            graphicsContext.setStrokeStyle(oldStrokeStyle);
        } else {
            float adjacent1BigThird = ceilToDevicePixel(adjacentWidth1 / 3, deviceScaleFactor);
            float adjacent2BigThird = ceilToDevicePixel(adjacentWidth2 / 3, deviceScaleFactor);

            float offset1 = floorToDevicePixel(fabs(adjacentWidth1) * 2 / 3, deviceScaleFactor);
            float offset2 = floorToDevicePixel(fabs(adjacentWidth2) * 2 / 3, deviceScaleFactor);

            float mitreOffset1 = adjacentWidth1 < 0 ? offset1 : 0;
            float mitreOffset2 = adjacentWidth1 > 0 ? offset1 : 0;
            float mitreOffset3 = adjacentWidth2 < 0 ? offset2 : 0;
            float mitreOffset4 = adjacentWidth2 > 0 ? offset2 : 0;

            FloatRect paintBorderRect;
            switch (side) {
            case BoxSide::Top:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset1, y1, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset2, y2 - thirdOfThickness, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Left:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Bottom:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset2, y1, (x2 - mitreOffset4) - (x1 + mitreOffset2), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1 + mitreOffset1, y2 - thirdOfThickness, (x2 - mitreOffset3) - (x1 + mitreOffset1), thirdOfThickness), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            case BoxSide::Right:
                paintBorderRect = snapRectToDevicePixels(LayoutRect(x1, y1 + mitreOffset2, thirdOfThickness, (y2 - mitreOffset4) - (y1 + mitreOffset2)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));

                paintBorderRect = snapRectToDevicePixels(LayoutRect(x2 - thirdOfThickness, y1 + mitreOffset1, thirdOfThickness, (y2 - mitreOffset3) - (y1 + mitreOffset1)), deviceScaleFactor);
                drawLineFor(paintBorderRect, side, BorderStyle::Solid, FloatSize(adjacent1BigThird, adjacent2BigThird));
                break;
            default:
                break;
            }
        }
        break;
    }
    case BorderStyle::Ridge:
    case BorderStyle::Groove: {
        BorderStyle s1;
        BorderStyle s2;
        if (borderStyle == BorderStyle::Groove) {
            s1 = BorderStyle::Inset;
            s2 = BorderStyle::Outset;
        } else {
            s1 = BorderStyle::Outset;
            s2 = BorderStyle::Inset;
        }

        float adjacent1BigHalf = ceilToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
        float adjacent2BigHalf = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float adjacent1SmallHalf = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);
        float adjacent2SmallHalf = floorToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float offset1 = 0;
        float offset2 = 0;
        float offset3 = 0;
        float offset4 = 0;

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth1 < 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth1 > 0))
            offset1 = floorToDevicePixel(adjacentWidth1 / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth2 < 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth2 > 0))
            offset2 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth1 > 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth1 < 0))
            offset3 = floorToDevicePixel(fabs(adjacentWidth1) / 2, deviceScaleFactor);

        if (((side == BoxSide::Top || side == BoxSide::Left) && adjacentWidth2 > 0) || ((side == BoxSide::Bottom || side == BoxSide::Right) && adjacentWidth2 < 0))
            offset4 = ceilToDevicePixel(adjacentWidth2 / 2, deviceScaleFactor);

        float adjustedX = ceilToDevicePixel((x1 + x2) / 2, deviceScaleFactor);
        float adjustedY = ceilToDevicePixel((y1 + y2) / 2, deviceScaleFactor);
        // Quads can't use the default snapping rect functions.
        x1 = roundToDevicePixel(x1, deviceScaleFactor);
        x2 = roundToDevicePixel(x2, deviceScaleFactor);
        y1 = roundToDevicePixel(y1, deviceScaleFactor);
        y2 = roundToDevicePixel(y2, deviceScaleFactor);

        switch (side) {
        case BoxSide::Top:
            drawLineFor(FloatRect(FloatPoint(x1 + offset1, y1), FloatPoint(x2 - offset2, adjustedY)), side, s1, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(x1 + offset3, adjustedY), FloatPoint(x2 - offset4, y2)), side, s2, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Left:
            drawLineFor(FloatRect(FloatPoint(x1, y1 + offset1), FloatPoint(adjustedX, y2 - offset2)), side, s1, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(adjustedX, y1 + offset3), FloatPoint(x2, y2 - offset4)), side, s2, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Bottom:
            drawLineFor(FloatRect(FloatPoint(x1 + offset1, y1), FloatPoint(x2 - offset2, adjustedY)), side, s2, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(x1 + offset3, adjustedY), FloatPoint(x2 - offset4, y2)), side, s1, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        case BoxSide::Right:
            drawLineFor(FloatRect(FloatPoint(x1, y1 + offset1), FloatPoint(adjustedX, y2 - offset2)), side, s2, FloatSize(adjacent1BigHalf, adjacent2BigHalf));
            drawLineFor(FloatRect(FloatPoint(adjustedX, y1 + offset3), FloatPoint(x2, y2 - offset4)), side, s1, FloatSize(adjacent1SmallHalf, adjacent2SmallHalf));
            break;
        }
        break;
    }
    case BorderStyle::Inset:
    case BorderStyle::Outset:
        calculateBorderStyleColor(borderStyle, side, color);
        FALLTHROUGH;
    case BorderStyle::Solid: {
        StrokeStyle oldStrokeStyle = graphicsContext.strokeStyle();
        ASSERT(x2 >= x1);
        ASSERT(y2 >= y1);
        if (!adjacentWidth1 && !adjacentWidth2) {
            graphicsContext.setStrokeStyle(NoStroke);
            graphicsContext.setFillColor(color);
            bool wasAntialiased = graphicsContext.shouldAntialias();
            graphicsContext.setShouldAntialias(antialias);
            drawBorderRect(snapRectToDevicePixels(LayoutRect(x1, y1, x2 - x1, y2 - y1), deviceScaleFactor));
            graphicsContext.setShouldAntialias(wasAntialiased);
            graphicsContext.setStrokeStyle(oldStrokeStyle);
            return;
        }

        // FIXME: These roundings should be replaced by ASSERT(device pixel positioned) when all the callers have transitioned to device pixels.
        x1 = roundToDevicePixel(x1, deviceScaleFactor);
        y1 = roundToDevicePixel(y1, deviceScaleFactor);
        x2 = roundToDevicePixel(x2, deviceScaleFactor);
        y2 = roundToDevicePixel(y2, deviceScaleFactor);

        Vector<FloatPoint> quad;
        switch (side) {
        case BoxSide::Top:
            quad = {
                { x1 + std::max<float>(-adjacentWidth1, 0), y1 },
                { x1 + std::max<float>(adjacentWidth1, 0), y2 },
                { x2 - std::max<float>(adjacentWidth2, 0), y2 },
                { x2 - std::max<float>(-adjacentWidth2, 0), y1 }
            };
            break;
        case BoxSide::Bottom:
            quad = {
                { x1 + std::max<float>(adjacentWidth1, 0), y1 },
                { x1 + std::max<float>(-adjacentWidth1, 0), y2 },
                { x2 - std::max<float>(-adjacentWidth2, 0), y2 },
                { x2 - std::max<float>(adjacentWidth2, 0), y1 }
            };
            break;
        case BoxSide::Left:
            quad = {
                { x1, y1 + std::max<float>(-adjacentWidth1, 0) },
                { x1, y2 - std::max<float>(-adjacentWidth2, 0) },
                { x2, y2 - std::max<float>(adjacentWidth2, 0) },
                { x2, y1 + std::max<float>(adjacentWidth1, 0) }
            };
            break;
        case BoxSide::Right:
            quad = {
                { x1, y1 + std::max<float>(adjacentWidth1, 0) },
                { x1, y2 - std::max<float>(adjacentWidth2, 0) },
                { x2, y2 - std::max<float>(-adjacentWidth2, 0) },
                { x2, y1 + std::max<float>(-adjacentWidth1, 0) }
            };
            break;
        }

        graphicsContext.setStrokeStyle(NoStroke);
        graphicsContext.setFillColor(color);
        bool wasAntialiased = graphicsContext.shouldAntialias();
        graphicsContext.setShouldAntialias(antialias);
        graphicsContext.fillPath(Path::polygonPathFromPoints(quad));
        graphicsContext.setShouldAntialias(wasAntialiased);

        graphicsContext.setStrokeStyle(oldStrokeStyle);
        break;
    }
    }
}

static bool usePlatformFocusRingColorForOutlineStyleAuto()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

static bool useShrinkWrappedFocusRingForOutlineStyleAuto()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

static bool drawFocusRing(GraphicsContext& context, Page& page, const Path& path, const RenderStyle& style, Color focusRingColor)
{
    bool needsRepaint = false;
#if PLATFORM(MAC)
    context.drawFocusRing(path, page.focusController().timeSinceFocusWasSet().seconds(), needsRepaint, focusRingColor);
    UNUSED_PARAM(style);
#else
    context.drawFocusRing(path, style.outlineWidth(), style.outlineOffset(), focusRingColor);
    UNUSED_PARAM(page);
#endif
    return needsRepaint;
}

static bool drawFocusRing(GraphicsContext& context, Page& page, Vector<FloatRect> rects, const RenderStyle& style, Color focusRingColor)
{
    bool needsRepaint = false;
#if PLATFORM(MAC)
    context.drawFocusRing(rects, page.focusController().timeSinceFocusWasSet().seconds(), needsRepaint, focusRingColor);
    UNUSED_PARAM(style);
#else
    context.drawFocusRing(rects, style.outlineWidth(), style.outlineOffset(), focusRingColor);
    UNUSED_PARAM(page);
#endif
    return needsRepaint;
}


void RenderElement::paintFocusRing(PaintInfo& paintInfo, const RenderStyle& style, const Vector<LayoutRect>& focusRingRects)
{
    ASSERT(style.outlineStyleIsAuto() == OutlineIsAuto::On);
    float outlineOffset = style.outlineOffset();
    Vector<FloatRect> pixelSnappedFocusRingRects;
    float deviceScaleFactor = document().deviceScaleFactor();
    for (auto rect : focusRingRects) {
        rect.inflate(outlineOffset);
        pixelSnappedFocusRingRects.append(snapRectToDevicePixels(rect, deviceScaleFactor));
    }
    Color focusRingColor = usePlatformFocusRingColorForOutlineStyleAuto() ? RenderTheme::singleton().focusRingColor(styleColorOptions()) : style.visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);
    bool needsRepaint;
    if (useShrinkWrappedFocusRingForOutlineStyleAuto() && style.hasBorderRadius()) {
        Path path = PathUtilities::pathWithShrinkWrappedRectsForOutline(pixelSnappedFocusRingRects, style.border(), outlineOffset, style.direction(), style.writingMode(),
            document().deviceScaleFactor());
        if (path.isEmpty()) {
            for (auto rect : pixelSnappedFocusRingRects)
                path.addRect(rect);
        }
        needsRepaint = drawFocusRing(paintInfo.context(), page(), path, style, focusRingColor);
    } else
        needsRepaint = drawFocusRing(paintInfo.context(), page(), pixelSnappedFocusRingRects, style, focusRingColor);
    if (needsRepaint)
        page().focusController().setFocusedElementNeedsRepaint();
}

void RenderElement::paintOutline(PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    GraphicsContext& graphicsContext = paintInfo.context();
    if (graphicsContext.paintingDisabled())
        return;

    if (!hasOutline())
        return;

    auto& styleToUse = style();
    float outlineWidth = floorToDevicePixel(styleToUse.outlineWidth(), document().deviceScaleFactor());
    float outlineOffset = floorToDevicePixel(styleToUse.outlineOffset(), document().deviceScaleFactor());

    // Only paint the focus ring by hand if the theme isn't able to draw it.
    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On && !theme().supportsFocusRing(styleToUse)) {
        Vector<LayoutRect> focusRingRects;
        LayoutRect paintRectToUse { paintRect };
        if (is<RenderBox>(*this))
            paintRectToUse = theme().adjustedPaintRect(downcast<RenderBox>(*this), paintRectToUse);
        addFocusRingRects(focusRingRects, paintRectToUse.location(), paintInfo.paintContainer);
        paintFocusRing(paintInfo, styleToUse, focusRingRects);
    }

    if (hasOutlineAnnotation() && styleToUse.outlineStyleIsAuto() == OutlineIsAuto::Off && !theme().supportsFocusRing(styleToUse))
        addPDFURLRect(paintInfo, paintRect.location());

    if (styleToUse.outlineStyleIsAuto() == OutlineIsAuto::On || styleToUse.outlineStyle() == BorderStyle::None)
        return;

    FloatRect outer = paintRect;
    outer.inflate(outlineOffset + outlineWidth);
    FloatRect inner = outer;
    inner.inflate(-outlineWidth);

    // FIXME: This prevents outlines from painting inside the object. See bug 12042
    if (outer.isEmpty())
        return;

    BorderStyle outlineStyle = styleToUse.outlineStyle();
    Color outlineColor = styleToUse.visitedDependentColorWithColorFilter(CSSPropertyOutlineColor);

    bool useTransparencyLayer = !outlineColor.isOpaque();
    if (useTransparencyLayer) {
        if (outlineStyle == BorderStyle::Solid) {
            Path path;
            path.addRect(outer);
            path.addRect(inner);
            graphicsContext.setFillRule(WindRule::EvenOdd);
            graphicsContext.setFillColor(outlineColor);
            graphicsContext.fillPath(path);
            return;
        }
        graphicsContext.beginTransparencyLayer(outlineColor.alphaAsFloat());
        outlineColor = outlineColor.opaqueColor();
    }

    float leftOuter = outer.x();
    float leftInner = inner.x();
    float rightOuter = outer.maxX();
    float rightInner = std::min(inner.maxX(), rightOuter);
    float topOuter = outer.y();
    float topInner = inner.y();
    float bottomOuter = outer.maxY();
    float bottomInner = std::min(inner.maxY(), bottomOuter);

    drawLineForBoxSide(graphicsContext, FloatRect(FloatPoint(leftOuter, topOuter), FloatPoint(leftInner, bottomOuter)), BoxSide::Left, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, FloatRect(FloatPoint(leftOuter, topOuter), FloatPoint(rightOuter, topInner)), BoxSide::Top, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, FloatRect(FloatPoint(rightInner, topOuter), FloatPoint(rightOuter, bottomOuter)), BoxSide::Right, outlineColor, outlineStyle, outlineWidth, outlineWidth);
    drawLineForBoxSide(graphicsContext, FloatRect(FloatPoint(leftOuter, bottomInner), FloatPoint(rightOuter, bottomOuter)), BoxSide::Bottom, outlineColor, outlineStyle, outlineWidth, outlineWidth);

    if (useTransparencyLayer)
        graphicsContext.endTransparencyLayer();
}

void RenderElement::issueRepaintForOutlineAuto(float outlineSize)
{
    LayoutRect repaintRect;
    Vector<LayoutRect> focusRingRects;
    addFocusRingRects(focusRingRects, LayoutPoint(), containerForRepaint());
    for (auto rect : focusRingRects) {
        rect.inflate(outlineSize);
        repaintRect.unite(rect);
    }
    repaintRectangle(repaintRect);
}

void RenderElement::updateOutlineAutoAncestor(bool hasOutlineAuto)
{
    for (auto& child : childrenOfType<RenderObject>(*this)) {
        if (hasOutlineAuto == child.hasOutlineAutoAncestor())
            continue;
        child.setHasOutlineAutoAncestor(hasOutlineAuto);
        bool childHasOutlineAuto = child.outlineStyleForRepaint().outlineStyleIsAuto() == OutlineIsAuto::On;
        if (childHasOutlineAuto)
            continue;
        if (!is<RenderElement>(child))
            continue;
        downcast<RenderElement>(child).updateOutlineAutoAncestor(hasOutlineAuto);
    }
    if (is<RenderBoxModelObject>(*this)) {
        if (auto* continuation = downcast<RenderBoxModelObject>(*this).continuation())
            continuation->updateOutlineAutoAncestor(hasOutlineAuto);
    }
}

bool RenderElement::hasOutlineAnnotation() const
{
    return element() && element()->isLink() && (document().printing() || (view().frameView().paintBehavior() & PaintBehavior::AnnotateLinks));
}

bool RenderElement::hasSelfPaintingLayer() const
{
    if (!hasLayer())
        return false;
    auto& layerModelObject = downcast<RenderLayerModelObject>(*this);
    return layerModelObject.hasSelfPaintingLayer();
}

bool RenderElement::checkForRepaintDuringLayout() const
{
    if (document().view()->layoutContext().needsFullRepaint() || !everHadLayout() || hasSelfPaintingLayer())
        return false;
    return !settings().repaintOutsideLayoutEnabled();
}

ImageOrientation RenderElement::imageOrientation() const
{
    auto* imageElement = dynamicDowncast<HTMLImageElement>(element());
    return (imageElement && !imageElement->allowsOrientationOverride()) ? ImageOrientation(ImageOrientation::FromImage) : style().imageOrientation();
}

void RenderElement::adjustFragmentedFlowStateOnContainingBlockChangeIfNeeded(const RenderStyle& oldStyle, const RenderStyle& newStyle)
{
    if (fragmentedFlowState() == NotInsideFragmentedFlow)
        return;

    // Make sure we invalidate the containing block cache for flows when the contianing block context changes
    // so that styleDidChange can safely use RenderBlock::locateEnclosingFragmentedFlow()
    // FIXME: Share some code with RenderElement::canContain*.
    auto mayNotBeContainingBlockForDescendantsAnymore = oldStyle.position() != m_style.position()
        || oldStyle.hasTransformRelatedProperty() != m_style.hasTransformRelatedProperty()
        || oldStyle.willChange() != newStyle.willChange()
        || oldStyle.containsLayout() != newStyle.containsLayout()
        || oldStyle.containsSize() != newStyle.containsSize();
    if (!mayNotBeContainingBlockForDescendantsAnymore)
        return;

    // Invalidate the containing block caches.
    if (is<RenderBlock>(*this))
        downcast<RenderBlock>(*this).resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants();
    else {
        // Relatively positioned inline boxes can have absolutely positioned block descendants. We need to reset them as well.
        for (auto& descendant : descendantsOfType<RenderBlock>(*this))
            descendant.resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants();
    }
    
    // Adjust the flow tread state on the subtree.
    setFragmentedFlowState(RenderObject::computedFragmentedFlowState(*this));
    for (auto& descendant : descendantsOfType<RenderObject>(*this))
        descendant.setFragmentedFlowState(RenderObject::computedFragmentedFlowState(descendant));
}

void RenderElement::removeFromRenderFragmentedFlow()
{
    ASSERT(fragmentedFlowState() != NotInsideFragmentedFlow);
    // Sometimes we remove the element from the flow, but it's not destroyed at that time.
    // It's only until later when we actually destroy it and remove all the children from it.
    // Currently, that happens for firstLetter elements and list markers.
    // Pass in the flow thread so that we don't have to look it up for all the children.
    removeFromRenderFragmentedFlowIncludingDescendants(true);
}

void RenderElement::removeFromRenderFragmentedFlowIncludingDescendants(bool shouldUpdateState)
{
    // Once we reach another flow thread we don't need to update the flow thread state
    // but we have to continue cleanup the flow thread info.
    if (isRenderFragmentedFlow())
        shouldUpdateState = false;

    for (auto& child : childrenOfType<RenderObject>(*this)) {
        if (is<RenderElement>(child)) {
            downcast<RenderElement>(child).removeFromRenderFragmentedFlowIncludingDescendants(shouldUpdateState);
            continue;
        }
        if (shouldUpdateState)
            child.setFragmentedFlowState(NotInsideFragmentedFlow);
    }

    // We have to ask for our containing flow thread as it may be above the removed sub-tree.
    RenderFragmentedFlow* enclosingFragmentedFlow = this->enclosingFragmentedFlow();
    while (enclosingFragmentedFlow) {
        enclosingFragmentedFlow->removeFlowChildInfo(*this);

        if (enclosingFragmentedFlow->fragmentedFlowState() == NotInsideFragmentedFlow)
            break;
        auto* parent = enclosingFragmentedFlow->parent();
        if (!parent)
            break;
        enclosingFragmentedFlow = parent->enclosingFragmentedFlow();
    }
    if (is<RenderBlock>(*this))
        downcast<RenderBlock>(*this).setCachedEnclosingFragmentedFlowNeedsUpdate();

    if (shouldUpdateState)
        setFragmentedFlowState(NotInsideFragmentedFlow);
}

void RenderElement::resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(RenderFragmentedFlow* fragmentedFlow)
{
    if (fragmentedFlow)
        fragmentedFlow->removeFlowChildInfo(*this);

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.resetEnclosingFragmentedFlowAndChildInfoIncludingDescendants(fragmentedFlow);
}

ReferencedSVGResources& RenderElement::ensureReferencedSVGResources()
{
    auto& rareData = ensureRareData();
    if (!rareData.referencedSVGResources)
        rareData.referencedSVGResources = makeUnique<ReferencedSVGResources>(*this);

    return *rareData.referencedSVGResources;
}

void RenderElement::clearReferencedSVGResources()
{
    if (!hasRareData())
        return;

    ensureRareData().referencedSVGResources = nullptr;
}

// This needs to run when the entire render tree has been constructed, so can't be called from styleDidChange.
void RenderElement::updateReferencedSVGResources()
{
    auto referencedElementIDs = ReferencedSVGResources::referencedSVGResourceIDs(style());
    if (!referencedElementIDs.isEmpty())
        ensureReferencedSVGResources().updateReferencedResources(document(), referencedElementIDs);
    else
        clearReferencedSVGResources();
}

#if ENABLE(TEXT_AUTOSIZING)
static RenderObject::BlockContentHeightType includeNonFixedHeight(const RenderObject& renderer)
{
    const RenderStyle& style = renderer.style();
    if (style.height().isFixed()) {
        if (is<RenderBlock>(renderer)) {
            // For fixed height styles, if the overflow size of the element spills out of the specified
            // height, assume we can apply text auto-sizing.
            if (downcast<RenderBlock>(renderer).effectiveOverflowY() == Overflow::Visible
                && style.height().value() < downcast<RenderBlock>(renderer).layoutOverflowRect().maxY())
                return RenderObject::OverflowHeight;
        }
        return RenderObject::FixedHeight;
    }
    return RenderObject::FlexibleHeight;
}

void RenderElement::adjustComputedFontSizesOnBlocks(float size, float visibleWidth)
{
    Document* document = view().frameView().frame().document();
    if (!document)
        return;

    Vector<int> depthStack;
    int currentDepth = 0;
    int newFixedDepth = 0;

    // We don't apply autosizing to nodes with fixed height normally.
    // But we apply it to nodes which are located deep enough
    // (nesting depth is greater than some const) inside of a parent block
    // which has fixed height but its content overflows intentionally.
    for (RenderObject* descendent = traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth); descendent; descendent = descendent->traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth)) {
        while (depthStack.size() > 0 && currentDepth <= depthStack[depthStack.size() - 1])
            depthStack.remove(depthStack.size() - 1);
        if (newFixedDepth)
            depthStack.append(newFixedDepth);

        int stackSize = depthStack.size();
        if (is<RenderBlockFlow>(*descendent) && !descendent->isListItem() && (!stackSize || currentDepth - depthStack[stackSize - 1] > TextAutoSizingFixedHeightDepth))
            downcast<RenderBlockFlow>(*descendent).adjustComputedFontSizes(size, visibleWidth);
        newFixedDepth = 0;
    }

    // Remove style from auto-sizing table that are no longer valid.
    document->textAutoSizing().updateRenderTree();
}

void RenderElement::resetTextAutosizing()
{
    Document* document = view().frameView().frame().document();
    if (!document)
        return;

    LOG(TextAutosizing, "RenderElement::resetTextAutosizing()");

    document->textAutoSizing().reset();

    Vector<int> depthStack;
    int currentDepth = 0;
    int newFixedDepth = 0;

    for (RenderObject* descendent = traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth); descendent; descendent = descendent->traverseNext(this, includeNonFixedHeight, currentDepth, newFixedDepth)) {
        while (depthStack.size() > 0 && currentDepth <= depthStack[depthStack.size() - 1])
            depthStack.remove(depthStack.size() - 1);
        if (newFixedDepth)
            depthStack.append(newFixedDepth);

        int stackSize = depthStack.size();
        if (is<RenderBlockFlow>(*descendent) && !descendent->isListItem() && (!stackSize || currentDepth - depthStack[stackSize - 1] > TextAutoSizingFixedHeightDepth))
            downcast<RenderBlockFlow>(*descendent).resetComputedFontSize();
        newFixedDepth = 0;
    }
}
#endif // ENABLE(TEXT_AUTOSIZING)

std::unique_ptr<RenderStyle> RenderElement::animatedStyle()
{
    std::unique_ptr<RenderStyle> result;

    if (auto styleable = Styleable::fromRenderer(*this))
        result = styleable->computeAnimatedStyle();

    if (!result)
        result = RenderStyle::clonePtr(style());

    return result;
}

WeakPtr<RenderBlockFlow> RenderElement::backdropRenderer() const
{
    return hasRareData() ? rareData().backdropRenderer : nullptr;
}

void RenderElement::setBackdropRenderer(RenderBlockFlow& renderer)
{
    ensureRareData().backdropRenderer = renderer;
}

Overflow RenderElement::effectiveOverflowX() const
{
    auto overflowX = style().overflowX();
    if (paintContainmentApplies() && overflowX == Overflow::Visible)
        return Overflow::Clip;
    return overflowX;
}

Overflow RenderElement::effectiveOverflowY() const
{
    auto overflowY = style().overflowY();
    if (paintContainmentApplies() && overflowY == Overflow::Visible)
        return Overflow::Clip;
    return overflowY;
}

}
