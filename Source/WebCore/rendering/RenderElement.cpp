/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2005 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2005, 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2005-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2010-2014 Google Inc. All rights reserved.
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
#include "BorderPainter.h"
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
#include "InlineIteratorLineBox.h"
#include "InlineIteratorTextBox.h"
#include "LayoutElementBox.h"
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
#include "RenderSVGViewportContainer.h"
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
#include "SVGLengthContext.h"
#include "SVGRenderSupport.h"
#include "SVGSVGElement.h"
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

Layout::ElementBox* RenderElement::layoutBox()
{
    return downcast<Layout::ElementBox>(RenderObject::layoutBox());
}

const Layout::ElementBox* RenderElement::layoutBox() const
{
    return downcast<Layout::ElementBox>(RenderObject::layoutBox());
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

const RenderStyle& RenderElement::firstLineStyle() const
{
    // FIXME: It would be better to just set anonymous block first-line styles correctly.
    if (isAnonymousBlock()) {
        if (!previousInFlowSibling()) {
            if (auto* firstLineStyle = parent()->style().getCachedPseudoStyle(PseudoId::FirstLine))
                return *firstLineStyle;
        }
        return style();
    }

    if (auto* firstLineStyle = style().getCachedPseudoStyle(PseudoId::FirstLine))
        return *firstLineStyle;

    return style();
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


inline bool RenderElement::shouldRepaintForStyleDifference(StyleDifference diff) const
{
    auto hasImmediateNonWhitespaceTextChild = [&] {
        for (auto& child : childrenOfType<RenderText>(*this)) {
            if (!child.isAllCollapsibleWhitespace())
                return true;
        }
        return false;
    };
    return diff == StyleDifference::Repaint || (diff == StyleDifference::RepaintIfText && hasImmediateNonWhitespaceTextChild());
}

void RenderElement::updateFillImages(const FillLayer* oldLayers, const FillLayer* newLayers)
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
        for (auto* layer = newLayers; layer; layer = layer->next()) {
            if (layer->image() && !layer->image()->hasClient(*this))
                return false;
        }
        return true;
    };

    // If images have the same characteristics and this element is already registered as a
    // client to the new images, there is nothing to do.
    if (fillImagesAreIdentical(oldLayers, newLayers) && isRegisteredWithNewFillImages())
        return;

    // Add before removing, to avoid removing all clients of an image that is in both sets.
    for (auto* layer = newLayers; layer; layer = layer->next()) {
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
            bool willHaveLayer = newStyle.affectsTransform() || newStyle.hasOpacity() || newStyle.hasFilter() || newStyle.hasBackdropFilter();
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

    // The following only applies to the legacy SVG engine -- LBSE always creates layers
    // independant of the position in the render tree, see comment in layerCreationAllowedForSubtree().

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

static RenderLayer* findNextLayer(const RenderElement& currRenderer, const RenderLayer& parentLayer, const RenderObject* siblingToTraverseFrom, bool checkParent = true)
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

static RenderLayer* layerNextSiblingRespectingTopLayer(const RenderElement& renderer, const RenderLayer& parentLayer)
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

static void addLayers(const RenderElement& insertedRenderer, RenderElement& currentRenderer, RenderLayer& parentLayer)
{
    if (currentRenderer.hasLayer()) {
        auto* layerToUse = &parentLayer;
        if (isInTopLayerOrBackdrop(currentRenderer.style(), currentRenderer.element())) {
            // The special handling of a toplayer/backdrop content may result in trying to insert the associated
            // layer twice as we connect subtrees.
            if (auto* parentLayer = downcast<RenderLayerModelObject>(currentRenderer).layer()->parent()) {
                ASSERT_UNUSED(parentLayer, parentLayer == currentRenderer.view().layer());
                return;
            }
            layerToUse = insertedRenderer.view().layer();
        }
        auto* beforeChild = layerNextSiblingRespectingTopLayer(insertedRenderer, *layerToUse);
        layerToUse->addChild(*downcast<RenderLayerModelObject>(currentRenderer).layer(), beforeChild);
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(currentRenderer))
        addLayers(insertedRenderer, child, parentLayer);
}

void RenderElement::removeLayers()
{
    RenderLayer* parentLayer = layerParent();
    if (!parentLayer)
        return;

    if (hasLayer()) {
        parentLayer->removeChild(*downcast<RenderLayerModelObject>(*this).layer());
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.removeLayers();
}

void RenderElement::moveLayers(RenderLayer& newParent)
{
    if (hasLayer()) {
        if (isInTopLayerOrBackdrop(style(), element()))
            return;
        RenderLayer* layer = downcast<RenderLayerModelObject>(*this).layer();
        auto* layerParent = layer->parent();
        if (layerParent)
            layerParent->removeChild(*layer);
        newParent.addChild(*layer);
        return;
    }

    for (auto& child : childrenOfType<RenderElement>(*this))
        child.moveLayers(newParent);
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
#if ENABLE(LAYER_BASED_SVG_ENGINE)
    // In LBSE layers are always created regardless of there position in the render tree.
    // Consider the SVG document fragment: "<defs><mask><rect transform="scale(2)".../>"
    // To paint the <rect> into the mask image, the rect needs to be transformed -
    // which is handled via RenderLayer in LBSE, unlike as in the legacy engine where no
    // layers are involved for any SVG painting features. In the legacy engine we could
    // simply omit the layer creation for any children of a <defs> element (or in general
    // any "hidden container"). For LBSE layers are needed for painting, even if a
    // RenderSVGHiddenContainer is in the render tree ancestor chain -- however they are
    // never painted directly, only indirectly through the "RenderSVGResourceContainer
    // elements (such as RenderSVGResourceClipper, RenderSVGResourceMasker, etc.)
    if (document().settings().layerBasedSVGEngineEnabled())
        return true;
#endif

    RenderElement* parentRenderer = parent();
    while (parentRenderer) {
        if (parentRenderer->isLegacySVGHiddenContainer())
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
            if (m_style.effectivePointerEvents() != newStyle.effectivePointerEvents())
                return true;
#if ENABLE(TOUCH_ACTION_REGIONS)
            if (m_style.effectiveTouchActions() != newStyle.effectiveTouchActions())
                return true;
#endif
            if (m_style.eventListenerRegionTypes() != newStyle.eventListenerRegionTypes())
                return true;
#if ENABLE(EDITABLE_REGION)
            bool wasEditable = m_style.effectiveUserModify() != UserModify::ReadOnly;
            bool isEditable = newStyle.effectiveUserModify() != UserModify::ReadOnly;
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
    if (newStyle.hasAnyFixedBackground() && !settings().fixedBackgroundsPaintRelativeToDocument()) {
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
    auto registerImages = [this](auto* style, auto* oldStyle) {
        if (!style && !oldStyle)
            return;
        updateFillImages(oldStyle ? &oldStyle->backgroundLayers() : nullptr, style ? &style->backgroundLayers() : nullptr);
        updateFillImages(oldStyle ? &oldStyle->maskLayers() : nullptr, style ? &style->maskLayers() : nullptr);
        updateImage(oldStyle ? oldStyle->borderImage().image() : nullptr, style ? style->borderImage().image() : nullptr);
        updateImage(oldStyle ? oldStyle->maskBoxImage().image() : nullptr, style ? style->maskBoxImage().image() : nullptr);
        updateShapeImage(oldStyle ? oldStyle->shapeOutside() : nullptr, style ? style->shapeOutside() : nullptr);
    };

    registerImages(&style(), oldStyle);

    // Are there other pseudo-elements that need the resources to be registered?
    registerImages(style().getCachedPseudoStyle(PseudoId::FirstLine), oldStyle ? oldStyle->getCachedPseudoStyle(PseudoId::FirstLine) : nullptr);

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
        if (auto* parentLayer = layerParent())
            addLayers(*this, *this, *parentLayer);
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
    if (firstChild() || hasLayer())
        removeLayers();

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
    if (m_style.hasAnyFixedBackground() && !settings().fixedBackgroundsPaintRelativeToDocument())
        view().frameView().removeSlowRepaintObject(*this);

    unregisterForVisibleInViewportCallback();

    if (hasCounterNodeMap())
        RenderCounter::destroyCounterNodes(*this);

    RenderObject::willBeDestroyed();

    clearSubtreeLayoutRootIfNeeded();

    auto unregisterImage = [this](auto* image) {
        if (image)
            image->removeClient(*this);
    };

    auto unregisterImages = [&](auto& style) {
        for (auto* backgroundLayer = &style.backgroundLayers(); backgroundLayer; backgroundLayer = backgroundLayer->next())
            unregisterImage(backgroundLayer->image());
        for (auto* maskLayer = &style.maskLayers(); maskLayer; maskLayer = maskLayer->next())
            unregisterImage(maskLayer->image());
        unregisterImage(style.borderImage().image());
        unregisterImage(style.maskBoxImage().image());
        if (auto shapeValue = style.shapeOutside())
            unregisterImage(shapeValue->image());
    };

    if (hasInitializedStyle()) {
        unregisterImages(m_style);

        if (style().hasOutline())
            view().decrementRendersWithOutline();

        if (auto* firstLineStyle = style().getCachedPseudoStyle(PseudoId::FirstLine))
            unregisterImages(*firstLineStyle);
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
    if (paintInfo.phase == PaintPhase::Selection || paintInfo.phase == PaintPhase::EventRegion || paintInfo.phase == PaintPhase::TextClip)
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

    if (!fullRepaint && oldBounds != newBounds && style().hasBorderRadius()) {
        auto oldRadius = style().getRoundedBorderFor(oldBounds).radii();
        auto newRadius = style().getRoundedBorderFor(newBounds).radii();

        fullRepaint = oldRadius != newRadius;
    }

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

    if (newBounds.isEmpty() && !oldBounds.isEmpty())
        repaintUsingContainer(repaintContainer, oldBounds);
    else if (!newBounds.isEmpty() && oldBounds.isEmpty())
        repaintUsingContainer(repaintContainer, newBounds);
    else {
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
    }

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
        LayoutUnit decorationsLeftWidth = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderWidth + minInsetRightShadowExtent) + std::max(outlineWidth, -shadowLeft);
        LayoutUnit decorationsRightWidth = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderWidth + minInsetRightShadowExtent) + std::max(outlineWidth, shadowRight);
        LayoutRect rightRect(newOutlineBox.x() + std::min(newOutlineBox.width(), oldOutlineBox.width()) - decorationsLeftWidth,
            newOutlineBox.y(),
            width + decorationsLeftWidth + decorationsRightWidth,
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
        LayoutUnit decorationsTopHeight = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderHeight + minInsetBottomShadowExtent) + std::max(outlineWidth, -shadowTop);
        LayoutUnit decorationsBottomHeight = std::max(LayoutUnit(-outlineStyle.outlineOffset()), borderHeight + minInsetBottomShadowExtent) + std::max(outlineWidth, shadowBottom);
        LayoutRect bottomRect(newOutlineBox.x(),
            std::min(newOutlineBox.maxY(), oldOutlineBox.maxY()) - decorationsTopHeight,
            std::max(newOutlineBox.width(), oldOutlineBox.width()),
            height + decorationsTopHeight + decorationsBottomHeight);
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

bool RenderElement::allowsAnimation() const
{
    return page().imageAnimationEnabled();
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
    if (!page().imageAnimationEnabled() || !isVisibleInDocumentRect(visibleRect))
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

    auto resolvedStyle = styleResolver.styleForPseudoElement(*element(), pseudoElementRequest, { parentStyle });
    if (!resolvedStyle)
        return nullptr;

    Style::loadPendingResources(*resolvedStyle->style, document(), element());

    return WTFMove(resolvedStyle->style);
}

Color RenderElement::selectionColor(CSSPropertyID colorProperty) const
{
    // If the element is unselectable, or we are only painting the selection,
    // don't override the foreground color with the selection foreground color.
    if (style().effectiveUserSelect() == UserSelect::None
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
    if (style().effectiveUserSelect() == UserSelect::None)
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
                    point.move(textRenderer.linesBoundingBox().x(), run->lineBox()->contentLogicalTop());
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

MarginRect RenderElement::absoluteAnchorRectWithScrollMargin(bool* insideFixed) const
{
    LayoutRect anchorRect = absoluteAnchorRect(insideFixed);
    const LengthBox& scrollMargin = style().scrollMargin();
    if (scrollMargin.isZero())
        return { anchorRect, anchorRect };

    // The scroll snap specification says that the scroll-margin should be applied in the
    // coordinate system of the scroll container and applied to the rectangular bounding
    // box of the transformed border box of the target element.
    // See https://www.w3.org/TR/css-scroll-snap-1/#scroll-margin.
    const LayoutBoxExtent margin(
        valueForLength(scrollMargin.top(), anchorRect.height()),
        valueForLength(scrollMargin.right(), anchorRect.width()),
        valueForLength(scrollMargin.bottom(), anchorRect.height()),
        valueForLength(scrollMargin.left(), anchorRect.width()));
    auto marginRect = anchorRect;
    marginRect.expand(margin);
    return { marginRect, anchorRect };
}

static bool usePlatformFocusRingColorForOutlineStyleAuto()
{
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    return true;
#else
    return false;
#endif
}

static bool useShrinkWrappedFocusRingForOutlineStyleAuto()
{
#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    return true;
#else
    return false;
#endif
}

static void drawFocusRing(GraphicsContext& context, const Path& path, const RenderStyle& style, const Color& color)
{
    context.drawFocusRing(path, style.outlineWidth(), color);
}

static void drawFocusRing(GraphicsContext& context, Vector<FloatRect> rects, const RenderStyle& style, const Color& color)
{
#if PLATFORM(MAC)
    context.drawFocusRing(rects, 0, style.outlineWidth(), color);
#else
    context.drawFocusRing(rects, style.outlineOffset(), style.outlineWidth(), color);
#endif
}


void RenderElement::paintFocusRing(const PaintInfo& paintInfo, const RenderStyle& style, const Vector<LayoutRect>& focusRingRects) const
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
    if (useShrinkWrappedFocusRingForOutlineStyleAuto() && style.hasBorderRadius()) {
        Path path = PathUtilities::pathWithShrinkWrappedRectsForOutline(pixelSnappedFocusRingRects, style.border(), outlineOffset, style.direction(), style.writingMode(),
            document().deviceScaleFactor());
        if (path.isEmpty()) {
            for (auto rect : pixelSnappedFocusRingRects)
                path.addRect(rect);
        }
        drawFocusRing(paintInfo.context(), path, style, focusRingColor);
    } else
        drawFocusRing(paintInfo.context(), pixelSnappedFocusRingRects, style, focusRingColor);
}

void RenderElement::paintOutline(PaintInfo& paintInfo, const LayoutRect& paintRect)
{
    if (paintInfo.context().paintingDisabled())
        return;

    if (!hasOutline())
        return;

    BorderPainter { *this, paintInfo }.paintOutline(paintRect);
}

void RenderElement::issueRepaintForOutlineAuto(float outlineSize)
{
    LayoutRect repaintRect;
    Vector<LayoutRect> focusRingRects;
    addFocusRingRects(focusRingRects, LayoutPoint(), containerForRepaint().renderer);
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
    return (imageElement && !imageElement->allowsOrientationOverride()) ? ImageOrientation(ImageOrientation::Orientation::FromImage) : style().imageOrientation();
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
    auto* localFrame = dynamicDowncast<LocalFrame>(view().frameView().frame());
    auto* document = localFrame ? localFrame->document() : nullptr;
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
    auto* localFrame = dynamicDowncast<LocalFrame>(view().frameView().frame());
    auto* document = localFrame ? localFrame->document() : nullptr;
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

bool RenderElement::createsNewFormattingContext() const
{
    // Writing-mode changes establish an independent block formatting context
    // if the box is a block-container.
    // https://drafts.csswg.org/css-writing-modes/#block-flow
    if (isWritingModeRoot() && isBlockContainer())
        return true;
    return isInlineBlockOrInlineTable() || isFlexItemIncludingDeprecated()
        || isTableCell() || isTableCaption() || isFieldset() || isDocumentElementRenderer() || isRenderFragmentedFlow() || isSVGForeignObject()
        || style().specifiesColumns() || style().columnSpan() == ColumnSpan::All || style().display() == DisplayType::FlowRoot || establishesIndependentFormattingContext();
}

bool RenderElement::establishesIndependentFormattingContext() const
{
    return isFloatingOrOutOfFlowPositioned() || hasPotentiallyScrollableOverflow() || style().containsLayout() || paintContainmentApplies();
}

FloatRect RenderElement::referenceBoxRect(CSSBoxType boxType) const
{
    // CSS box model code is implemented in RenderBox::referenceBoxRect().

    // For the legacy SVG engine, RenderElement is the only class that's
    // present in the ancestor chain of all SVG renderers. In LBSE the
    // common class is RenderLayerModelObject. Once the legacy SVG engine
    // is removed this function should be moved to RenderLayerModelObject.
    // As this method is used by both SVG engines, we need to place it
    // here in RenderElement, as temporary solution.
    if (element() && !is<SVGElement>(element()))
        return { };

    auto alignReferenceBox = [&](FloatRect referenceBox) {
        // The CSS borderBoxRect() is defined to start at an origin of (0, 0).
        // A possible shift of a CSS box (e.g. due to non-static position + top/left properties)
        // does not effect the borderBoxRect() location. The location information
        // is propagated upon paint time, e.g. via 'paintOffset' when calling RenderObject::paint(),
        // or by altering the RenderLayer TransformationMatrix to include the 'offsetFromAncestor'
        // right in the transformation matrix, when CSS transformations are present (see RenderLayer
        // paintLayerByApplyingTransform() for details).
        //
        // To mimic the expectation for SVG, 'fill-box' must behave the same: if we'd include
        // the 'referenceBox' location in the returned rect, we'd apply the (x, y) location
        // information for the SVG renderer twice. We would shift the 'transform-origin' by (x, y)
        // and at the same time alter the CTM in RenderLayer::paintLayerByApplyingTransform() by
        // including a translation to the enclosing transformed ancestor ('offsetFromAncestor').
        // Avoid that, and move by -nominalSVGLayoutLocation().
#if ENABLE(LAYER_BASED_SVG_ENGINE)
        if (isSVGLayerAwareRenderer() && !isSVGRoot() && document().settings().layerBasedSVGEngineEnabled())
            referenceBox.moveBy(-downcast<RenderLayerModelObject>(*this).nominalSVGLayoutLocation());
#endif
        return referenceBox;
    };

    auto determineSVGViewport = [&]() {
        const auto* viewportElement = downcast<SVGElement>(element());

#if ENABLE(LAYER_BASED_SVG_ENGINE)
        // RenderSVGViewportContainer is the only possible anonymous renderer in the SVG tree.
        if (!viewportElement && document().settings().layerBasedSVGEngineEnabled()) {
            ASSERT(is<RenderSVGViewportContainer>(this));
            ASSERT(isAnonymous());
            viewportElement = &downcast<RenderSVGViewportContainer>(*this).svgSVGElement();
        }
#endif

        // FIXME: [LBSE] Upstream: Cache the immutable SVGLengthContext per SVGElement, to avoid the repeated RenderSVGRoot size queries in determineViewport().
        ASSERT(viewportElement);
        auto viewportSize = SVGLengthContext(viewportElement).viewportSize().value_or(FloatSize { });
        return FloatRect { { }, viewportSize };
    };

    switch (boxType) {
    case CSSBoxType::BoxMissing:
    case CSSBoxType::ContentBox:
    case CSSBoxType::PaddingBox:
    case CSSBoxType::FillBox:
        return alignReferenceBox(objectBoundingBox());
    case CSSBoxType::BorderBox:
    case CSSBoxType::MarginBox:
    case CSSBoxType::StrokeBox:
        return alignReferenceBox(strokeBoundingBox());
    case CSSBoxType::ViewBox:
        return alignReferenceBox(determineSVGViewport());
    }

    ASSERT_NOT_REACHED();
    return { };
}

}
