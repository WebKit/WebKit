/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2026 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Andrew Wellington (proton@wiretapped.net)
 * Copyright (C) 2010 Daniel Bates (dbates@intudata.com)
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
#include "RenderListMarker.h"

#include "BaselineAlignment.h"
#include "CSSCounterStyleDescriptors.h"
#include "CSSCounterStyleRegistry.h"
#include "CSSFontSelector.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "FontCascade.h"
#include "FontCascadeInlines.h"
#include "FontCascadeDescription.h"
#include "GraphicsContext.h"
#include "LocalFrameView.h"
#include "LocalFrameViewLayoutContext.h"
#include "InlineIteratorBoxInlines.h"
#include "PaintInfoInlines.h"
#include "PseudoElement.h"
#include "RenderBlockFlow.h"
#include "RenderBlockInlines.h"
#include "RenderBoxInlines.h"
#include "RenderInline.h"
#include "RenderLayer.h"
#include "RenderListItem.h"
#include "RenderMenuList.h"
#include "RenderMultiColumnFlow.h"
#include "RenderMultiColumnSpannerPlaceholder.h"
#include "RenderObjectInlines.h"
#include "RenderTable.h"
#include "RenderText.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleComputedStyle+SettersInlines.h"
#include "StyleContent.h"
#include "StyleListStyleType.h"
#include "StyleScope.h"
#include "TextUtil.h"
#include <wtf/HashSet.h>
#include <wtf/StackStats.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(RenderListMarker);

RenderListMarker::RenderListMarker(RenderListItem& listItem, Style::ComputedStyle&& style)
    : RenderBox(Type::ListMarker, listItem.document(), WTF::move(style))
    , m_listItem(listItem)
{
    setInline(true);
    setBlockLevelReplacedOrAtomicInline(true); // pretend to be replaced
    ASSERT(isRenderListMarker());
}

// Do not add any code in below destructor. Add it to willBeDestroyed() instead.
RenderListMarker::~RenderListMarker() = default;

void RenderListMarker::willBeDestroyed()
{
    if (m_image)
        protect(m_image)->removeClient(*this);
    RenderBox::willBeDestroyed();
}

static Style::Difference NODELETE adjustedStyleDifference(Style::Difference diff, const Style::ComputedStyle& oldStyle, const Style::ComputedStyle& newStyle)
{
    if (diff >= Style::DifferenceResult::Layout)
        return diff;
    // FIXME: Preferably we do this at Style::ComputedStyle::changeRequiresLayout but checking against pseudo(::marker) is not sufficient.
    auto needsLayout =
           oldStyle.listStylePosition() != newStyle.listStylePosition()
        || oldStyle.listStyleType() != newStyle.listStyleType()
        || oldStyle.display().isInlineType() != newStyle.display().isInlineType();
    return needsLayout ? Style::DifferenceResult::Layout : diff;
}

void RenderListMarker::styleWillChange(Style::Difference diff, const Style::ComputedStyle& newStyle)
{
    RenderBox::styleWillChange(adjustedStyleDifference(diff, style(), newStyle), newStyle);
}

void RenderListMarker::styleDidChange(Style::Difference diff, const Style::ComputedStyle* oldStyle)
{
    if (oldStyle)
        diff = adjustedStyleDifference(diff, *oldStyle, style());
    RenderBox::styleDidChange(diff, oldStyle);

    propagateStyleToAnonymousChildren(StylePropagationType::AllChildren);

    // The anonymous inline box around us is our parent, so nothing propagates our style into it.
    if (CheckedPtr wrapper = inlineWrapper())
        wrapper->setStyle(styleForInlineWrapper());

    if (RefPtr newImage = style().listStyleImage().tryStyleImage(); m_image != newImage) {
        if (m_image)
            protect(m_image)->removeClient(*this);
        m_image = WTF::move(newImage);
        if (m_image)
            protect(m_image)->addClient(*this);
    }
}

bool RenderListMarker::isImage() const
{
    // `content` supersedes list-style-image (css-lists-3 §3.3), so a marker with generated content
    // is never treated as an image marker (affects inline margins, baseline, and layout attributes).
    return m_image && !protect(m_image)->errorOccurred() && !hasContentProperty();
}

bool RenderListMarker::hasContentProperty() const
{
    return document().settings().cssMarkerContentEnabled() && style().content().isData();
}

static bool symbolsContainStrongDirectionalityText(const CSSRegisteredCounterStyle& counterStyle)
{
    auto isStrongDirectionalitySymbol = [](auto& symbol) {
        return Layout::TextUtil::containsStrongDirectionalityText(symbol.text);
    };
    return isStrongDirectionalitySymbol(counterStyle.prefix())
        || isStrongDirectionalitySymbol(counterStyle.suffix())
        || isStrongDirectionalitySymbol(counterStyle.negative().m_prefix)
        || isStrongDirectionalitySymbol(counterStyle.negative().m_suffix)
        || isStrongDirectionalitySymbol(counterStyle.pad().m_padSymbol)
        || std::ranges::any_of(counterStyle.symbols(), isStrongDirectionalitySymbol)
        || std::ranges::any_of(counterStyle.additiveSymbols(), [&](auto& additiveSymbol) {
            return isStrongDirectionalitySymbol(additiveSymbol.first);
        });
}

static bool counterStyleChainHasStrongDirectionalitySymbols(const CSSRegisteredCounterStyle& counterStyle)
{
    // A counter style draws its fallback's text whenever a value is out of range or its own system cannot
    // represent it (CSSRegisteredCounterStyle::text), so every style the chain can reach has to be
    // left-to-right before the marker can go without content renderers. Measuring and painting the text
    // directly follows memory order, so text this gets wrong is painted wrong.
    HashSet<const CSSRegisteredCounterStyle*> visitedCounterStyles;
    RefPtr currentCounterStyle = &counterStyle;
    while (true) {
        // A cycle draws decimal instead (see fallbackText), which never reorders.
        if (!visitedCounterStyles.add(currentCounterStyle.get()).isNewEntry)
            return false;

        if (symbolsContainStrongDirectionalityText(*currentCounterStyle))
            return true;

        // No fallback to follow is the same condition fallbackText draws decimal on, whether the chain
        // ends here or its reference was never resolved.
        RefPtr fallbackCounterStyle = currentCounterStyle->fallbackStyle();
        if (!fallbackCounterStyle)
            return false;

        currentCounterStyle = WTF::move(fallbackCounterStyle);
    }
}

bool RenderListMarker::textNeedsBidiResolution() const
{
    if (hasContentProperty() || isImage() || synthesizesGlyph() || style().listStyleType().isNone())
        return false;

    if (auto markerString = style().listStyleType().tryString())
        return !style().writingMode().isBidiLTR() || Layout::TextUtil::containsStrongDirectionalityText(*markerString);

    RefPtr counterStyle = this->counterStyle();
    if (!counterStyle)
        return false;

    if (!style().writingMode().isBidiLTR())
        return true;

    return counterStyleChainHasStrongDirectionalitySymbols(*counterStyle);
}

bool RenderListMarker::textHasStrongDirectionality() const
{
    if (hasContentProperty() || isImage() || synthesizesGlyph() || style().listStyleType().isNone())
        return false;

    if (auto markerString = style().listStyleType().tryString())
        return Layout::TextUtil::containsStrongDirectionalityText(*markerString);

    RefPtr counterStyle = this->counterStyle();
    return counterStyle && counterStyleChainHasStrongDirectionalitySymbols(*counterStyle);
}

bool RenderListMarker::needsContentContainer() const
{
    return hasContentProperty() || textNeedsBidiResolution() || synthesizesGlyph();
}

RenderBlockFlow* RenderListMarker::contentContainer() const
{
    // When the marker has generated content, its sole child is the anonymous
    // inline-block box holding that content (created by RenderTreeBuilder::List).
    return dynamicDowncast<RenderBlockFlow>(firstChild());
}

RenderInline* RenderListMarker::inlineWrapper() const
{
    // The wrapper is put around the marker (and nothing else) by RenderTreeBuilder::List, so the marker is its
    // first child and the marker's contents follow.
    auto* wrapper = dynamicDowncast<RenderInline>(parent());
    if (wrapper && wrapper->isAnonymous() && wrapper->firstChild() == this)
        return wrapper;
    return nullptr;
}

Style::ComputedStyle RenderListMarker::styleForInlineWrapper() const
{
    auto wrapperStyle = Style::ComputedStyle::createAnonymousStyleWithDisplay(style(), Style::DisplayType::InlineFlow);
    // The wrapper is an anonymous child of the list item, which would otherwise hand it a style inheriting from itself
    // (RenderElement::propagateStyleToAnonymousChildren) and lose what the ::marker sets, tabular figures among it.
    wrapperStyle.setPseudoElementIdentifier({ { PseudoElementType::Marker } });
    return wrapperStyle;
}

RenderElement* RenderListMarker::contentRenderersParent() const
{
    if (auto* wrapper = inlineWrapper())
        return wrapper;
    return contentContainer();
}

bool isInlineWrapperForListMarker(const RenderObject& renderer)
{
    CheckedPtr wrapper = dynamicDowncast<RenderInline>(renderer);
    if (!wrapper || !wrapper->isAnonymous())
        return false;
    CheckedPtr marker = dynamicDowncast<RenderListMarker>(wrapper->firstChild());
    return marker && marker->inlineWrapper();
}

LayoutRect RenderListMarker::localSelectionRect()
{
    return LayoutRect(LayoutPoint(), borderBoxSize());
}

struct TextRunWithUnderlyingString {
    TextRun textRun;
    String underlyingString;
    operator const TextRun&() const { return textRun; }
};

static FontCascade disclosureMarkerFontCascade(const Style::ComputedStyle& style, Document& document)
{
    auto fontDescription = FontCascadeDescription { style.fontDescription() };
    fontDescription.setFamilies({ { "system-ui"_s, FontFamilyKind::Generic } });
    auto fontCascade = FontCascade(WTF::move(fontDescription));
    fontCascade.update(&document.fontSelector());
    return fontCascade;
}

static auto textRunForContent(ListMarkerTextContent textContent, const Style::ComputedStyle& style) -> TextRunWithUnderlyingString
{
    ASSERT(!textContent.isEmpty());

    auto textForRun = textContent.textWithSuffix;
    auto textRun = RenderBlock::constructTextRun(textForRun, style);
    return { WTF::move(textRun), WTF::move(textForRun) };
}

void RenderListMarker::paintDisclosureMarker(GraphicsContext& context, const FloatRect& markerRect)
{
    auto systemUIFontCascade = disclosureMarkerFontCascade(style(), protect(document()));
    auto textOrigin = FloatPoint { markerRect.x(), markerRect.y() + systemUIFontCascade.metricsOfPrimaryFont().ascent() };
    textOrigin = roundPointToDevicePixels(LayoutPoint(textOrigin), protect(document())->deviceScaleFactor(), writingMode().isLogicalLeftInlineStart());
    context.drawText(systemUIFontCascade, textRunForContent(m_textContent, style()), textOrigin);
}

void RenderListMarker::paint(PaintInfo& paintInfo, const LayoutPoint& paintOffset)
{
    if (style().usedVisibility() != Visibility::Visible)
        return;

    LayoutPoint boxOrigin(paintOffset + location());
    LayoutRect overflowRect(visualOverflowRect());
    overflowRect.moveBy(boxOrigin);
    if (!paintInfo.rect.intersects(overflowRect))
        return;

    if (CheckedPtr container = contentContainer()) {
        if (paintInfo.phase == PaintPhase::Accessibility) {
            paintInfo.accessibilityRegionContext()->takeBounds(*this, LayoutRect(boxOrigin, borderBoxSize()));
            return;
        }

        if (paintInfo.phase == PaintPhase::Foreground && selectionState() != HighlightState::None) {
            LayoutRect selectionRect = localSelectionRect();
            selectionRect.moveBy(boxOrigin);
            paintInfo.context().fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
        }

        // Paint the generated-content subtree as an atomic inline: paintAsInlineBlock fans out all
        // sub-phases for Foreground and forwards Selection/EventRegion/TextClip to the child's paint().
        container->paintAsInlineBlock(paintInfo, flipForWritingModeForChild(*container, boxOrigin));
        return;
    }

    if (inlineWrapper())
        return;

    if (paintInfo.phase != PaintPhase::Foreground && paintInfo.phase != PaintPhase::Accessibility)
        return;

    LayoutRect box(boxOrigin, borderBoxSize());

    auto markerRect = relativeMarkerRect();
    markerRect.moveBy(boxOrigin);

    if (paintInfo.phase == PaintPhase::Accessibility) {
        paintInfo.accessibilityRegionContext()->takeBounds(*this, markerRect);
        return;
    }

    if (markerRect.isEmpty())
        return;

    GraphicsContext& context = paintInfo.context();

    if (isImage()) {
        if (RefPtr markerImage = protect(m_image)->image(this, markerRect.size(), context))
            context.drawImage(*markerImage, markerRect, { imageOrientation() });
        if (selectionState() != HighlightState::None) {
            LayoutRect selectionRect = localSelectionRect();
            selectionRect.moveBy(boxOrigin);
            context.fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
        }
        return;
    }

    if (selectionState() != HighlightState::None) {
        LayoutRect selectionRect = localSelectionRect();
        selectionRect.moveBy(boxOrigin);
        context.fillRect(snappedIntRect(selectionRect), m_listItem->selectionBackgroundColor());
    }

    auto color = style().visitedDependentTextFillColorApplyingColorFilter();
    context.setStrokeColor(color);
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setStrokeThickness(1.0f);
    context.setFillColor(color);

    if (m_textContent.isEmpty())
        return;

    GraphicsContextStateSaver stateSaver(context, false);
    if (!writingMode().isHorizontal()) {
        markerRect.moveBy(-boxOrigin);
        markerRect = markerRect.transposedRect();
        markerRect.moveBy(FloatPoint(box.x(), box.y() - logicalHeight()));
        stateSaver.save();
        context.translate(markerRect.x(), markerRect.maxY());
        context.rotate(static_cast<float>(deg2rad(90.)));
        context.translate(-markerRect.x(), -markerRect.maxY());
    }

    if (isDisclosureMarker()) {
        paintDisclosureMarker(context, markerRect);
        return;
    }

    if (writingMode().isHorizontal()) {
        // This is required because RenderListMarker hand-draws the text, instead of running inline
        // layout and paint on its (RenderText) subtree (LayoutUnit vs. float precision)
        // FIXME: The vertical path mispositions the marker line box separately (it ignores fallback-font
        // metrics), so this is limited to horizontal writing modes for now. See webkit.org/b/319618.
        if (auto markerInlineBox = InlineIterator::boxFor(*this))
            markerRect.setY(paintOffset.y() + markerInlineBox->visualRectIgnoringBlockDirection().y());
    }

    auto textOrigin = FloatPoint { markerRect.x(), markerRect.y() + style().fontCascade().metricsOfPrimaryFont().ascent() };
    textOrigin = roundPointToDevicePixels(LayoutPoint(textOrigin), protect(document())->deviceScaleFactor(), writingMode().isLogicalLeftInlineStart());
    context.drawText(style().fontCascade(), textRunForContent(m_textContent, style()), textOrigin);
}

RenderBox* RenderListMarker::parentBox(RenderBox& box)
{
    ASSERT(m_listItem);
    CheckedPtr multiColumnFlow = dynamicDowncast<RenderMultiColumnFlow>(m_listItem->enclosingFragmentedFlow());
    if (!multiColumnFlow)
        return box.parentBox();
    auto* placeholder = multiColumnFlow->findColumnSpannerPlaceholder(box);
    return placeholder ? placeholder->parentBox() : box.parentBox();
};

void RenderListMarker::layout()
{
    StackStats::LayoutCheckPoint layoutCheckPoint;
    ASSERT(needsLayout());

    LayoutUnit blockOffset;
    // The anonymous inline box around an inside marker is not a box, so the block its line belongs to is where the
    // walk up to the list item starts.
    CheckedPtr wrapper = inlineWrapper();
    for (auto* ancestor = wrapper ? wrapper->containingBlock() : parentBox(*this); ancestor && ancestor != m_listItem.get(); ancestor = parentBox(*ancestor))
        blockOffset += ancestor->logicalTop();

    m_lineLogicalOffsetForListItem = m_listItem->logicalLeftOffsetForLine(blockOffset);
    m_lineOffsetForListItem = writingMode().isLogicalLeftInlineStart() ? m_lineLogicalOffsetForListItem : m_listItem->logicalRightOffsetForLine(blockOffset);

    if (CheckedPtr container = contentContainer()) {
        updateInlineMarginsAndContent();
        layoutContentContainer(*container);
    } else if (isImage()) {
        updateInlineMarginsAndContent();
        RefPtr image = m_image;
        setBorderBoxWidth(image->imageSize(this, style().usedZoom()).width());
        setBorderBoxHeight(image->imageSize(this, style().usedZoom()).height());
        m_layoutBounds = { borderBoxHeight(), 0 };
    } else if (inlineWrapper()) {
        updateInlineMarginsAndContent();
        setLogicalWidth({ });
        setLogicalHeight(style().metricsOfPrimaryFont().intHeight());
        m_layoutBounds = layoutBoundForTextContent(m_textContent.textWithSuffix);
    } else {
        setLogicalWidth(minContentLogicalWidthContribution());
        setLogicalHeight(style().metricsOfPrimaryFont().intHeight());
        m_layoutBounds = layoutBoundForTextContent(m_textContent.textWithSuffix);
    }

    setMarginStart(0);
    setMarginEnd(0);

    if (auto fixedStartMargin = style().marginStart().tryFixed())
        setMarginStart(LayoutUnit(fixedStartMargin->resolveZoom(style().usedZoomForLength())));
    if (auto fixedEndMargin = style().marginEnd().tryFixed())
        setMarginEnd(LayoutUnit(fixedEndMargin->resolveZoom(style().usedZoomForLength())));

    clearNeedsLayout();
}

void RenderListMarker::layoutContentContainer(RenderBlockFlow& container)
{
    // The marker participates in its list item's line as a single atomic inline. Lay its
    // generated-content subtree out at its max-content (shrink-to-fit) width, like an
    // inline-block, then adopt the resulting size and baseline.
    auto contentLogicalWidth = container.maxContentLogicalWidthContribution();
    container.setOverridingBorderBoxLogicalWidth(contentLogicalWidth);
    container.setNeedsLayout(MarkingBehavior::MarkOnlyThis);
    container.layoutIfNeeded();
    container.clearOverridingBorderBoxLogicalWidth();

    setLogicalWidth(container.logicalWidth());

    // The inline formatting context aligns the marker box on the marker's primary-font baseline
    // (like a text marker; see InlineLineBoxBuilder), then we paint the content box at the marker
    // box origin. Offset the content box along the block axis so its own first-line baseline lands
    // on that font baseline — otherwise the content's line-box half-leading shifts it off the list
    // item's baseline. Logical setters keep this correct in vertical writing modes.
    auto markerAscent = LayoutUnit { style().metricsOfPrimaryFont().ascent(BaselineAlignment::dominantBaseline(writingMode())) };
    auto contentBaseline = container.firstLineBaseline().value_or(container.logicalHeight());
    container.setLogicalTop(markerAscent - contentBaseline);

    if (textNeedsBidiResolution() || synthesizesGlyph()) {
        setLogicalHeight(style().metricsOfPrimaryFont().intHeight());
        m_layoutBounds = layoutBoundForTextContent(m_textContent.textWithSuffix);
    } else {
        setLogicalHeight(container.logicalHeight());
        m_layoutBounds = { contentBaseline, container.logicalHeight() - contentBaseline };
    }

    // The content box can extend outside the marker's border box (its baseline offset may be
    // negative, or the content taller than the marker font), so record its overflow. The marker
    // overrides layout() and otherwise reports no overflow, which would clip/mis-repaint the content.
    clearOverflow();
    addLayoutOverflow({ container.location(), container.borderBoxSize() });
    auto contentVisualOverflow = container.visualOverflowRect();
    contentVisualOverflow.moveBy(container.location());
    addVisualOverflow(contentVisualOverflow);
}

void RenderListMarker::imageChanged(WrappedImagePtr o, const IntRect* rect)
{
    if (parent()) {
        RefPtr image = m_image;
        if (image && o == image->data()) {
            if (image->errorOccurred()) {
                // A failed image turns this into a text marker, and that text needs renderers the marker was not built with.
                RefPtr element = m_listItem ? m_listItem->element() : nullptr;
                if (RefPtr pseudoElement = dynamicDowncast<PseudoElement>(element.get()))
                    element = pseudoElement->hostElement();
                if (element)
                    element->invalidateStyleAndRenderersForSubtree();
            }
            if (borderBoxWidth() != image->imageSize(this, style().usedZoom()).width() || borderBoxHeight() != image->imageSize(this, style().usedZoom()).height() || image->errorOccurred())
                setNeedsLayoutAndInvalidateContentLogicalWidths();
            else
                repaint();
        }
    }
    RenderBox::imageChanged(o, rect);
}

void RenderListMarker::updateInlineMarginsAndContent()
{
    // FIXME: It's messy to use the preferredLogicalWidths dirty bit for this optimization, also unclear if this is premature optimization.
    if (hasInvalidContentLogicalWidths())
        updateContent();
    updateInlineMargins();
}

void RenderListMarker::updateContent()
{
    if (hasContentProperty()) {
        // css-lists-3 §3.3: `content` (not normal) supersedes list-style-image/type. The generated
        // content lives in the anonymous inline-block contentContainer(); the marker has no text/image.
        m_textContent = { };
        return;
    }

    if (isImage()) {
        // FIXME: This is a somewhat arbitrary width.
        LayoutUnit bulletWidth = style().metricsOfPrimaryFont().intAscent() / 2_lu;
        LayoutSize defaultBulletSize(bulletWidth, bulletWidth);
        LayoutSize imageSize = calculateImageIntrinsicDimensions(m_image.get(), defaultBulletSize, ScaleByUsedZoom::No);
        protect(m_image)->setContainerContextForRenderer(*this, imageSize, style().usedZoom());
        m_textContent = {
            .textWithSuffix = emptyString(),
            .textWithoutSuffixLength = 0,
        };
        return;
    }

    WTF::switchOn(style().listStyleType(),
        [&](const CSS::Keyword::None&) {
            m_textContent = {
                .textWithSuffix = " "_s,
                .textWithoutSuffixLength = 0,
            };
        },
        [&](const Style::String& identifier) {
            m_textContent = {
                .textWithSuffix = identifier.value,
                .textWithoutSuffixLength = identifier.value.length(),
            };
        },
        [&](const Style::CounterStyle&) {
            auto counter = counterStyle();
            ASSERT(counter);

            auto text = makeString(counter->prefix().text, counter->text(m_listItem->value(), writingMode()));
            m_textContent = {
                .textWithSuffix = makeString(text, counter->suffix().text),
                .textWithoutSuffixLength = text.length(),
            };
        }
    );

    // The marker text is only known here (counter values resolve at layout), while the renderers
    // holding it were created from style. Push the text down so the inline formatting context has something to lay out.
    if (textNeedsBidiResolution() || synthesizesGlyph())
        updateContentContainerText();
}

void RenderListMarker::updateContentContainerText()
{
    CheckedPtr contentParent = contentRenderersParent();
    if (!contentParent) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr<RenderText> textRenderer;
    for (CheckedPtr child = contentParent->firstChild(); child && !textRenderer; child = child->nextSibling())
        textRenderer = dynamicDowncast<RenderText>(child.get());
    if (!textRenderer) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (synthesizesGlyph()) {
        textRenderer->setText(m_textContent.textWithoutSuffix().toString());
        return;
    }

    textRenderer->setText(m_textContent.textWithSuffix);
}

void RenderListMarker::computeIntrinsicLogicalWidthContributions()
{
    ASSERT(hasInvalidContentLogicalWidths());
    updateContent();

    if (inlineWrapper()) {
        // The text renderer next to us is inline content of the list item's own formatting context and already
        // contributes the marker's width there, so the marker box must not contribute it a second time.
        m_minContentLogicalWidthContribution = { };
        m_maxContentLogicalWidthContribution = { };
        clearContentLogicalWidthsInvalidation();
        updateInlineMargins();
        return;
    }

    if (CheckedPtr container = contentContainer()) {
        // The marker is non-wrapping, so its min- and max-content widths are both the
        // content's max-content width.
        auto logicalWidth = container->maxContentLogicalWidthContribution();
        m_minContentLogicalWidthContribution = logicalWidth;
        m_maxContentLogicalWidthContribution = logicalWidth;
        clearContentLogicalWidthsInvalidation();
        updateInlineMargins();
        return;
    }

    if (isImage()) {
        LayoutSize imageSize = LayoutSize(protect(m_image)->imageSize(this, style().usedZoom()));
        m_maxContentLogicalWidthContribution = writingMode().isHorizontal() ? imageSize.width() : imageSize.height();
        m_minContentLogicalWidthContribution = m_maxContentLogicalWidthContribution;
        clearContentLogicalWidthsInvalidation();
        updateInlineMargins();
        return;
    }

    ASSERT(!hasContentProperty());

    std::optional<FontCascade> systemUIFontCascade;
    // Use system-ui font for disclosure triangles
    if (isDisclosureMarker())
        systemUIFontCascade = disclosureMarkerFontCascade(style(), protect(document()));

    auto& font = systemUIFontCascade ? *systemUIFontCascade : style().fontCascade();

    LayoutUnit logicalWidth;
    if (!m_textContent.isEmpty())
        logicalWidth = font.width(textRunForContent(m_textContent, style()));

    m_minContentLogicalWidthContribution = logicalWidth;
    m_maxContentLogicalWidthContribution = logicalWidth;

    clearContentLogicalWidthsInvalidation();

    updateInlineMargins();
}

void RenderListMarker::updateInlineMargins()
{
    constexpr int markerPadding = 7;
    const FontMetrics& fontMetrics = style().metricsOfPrimaryFont();

    auto marginsForInsideMarker = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
        // The text renderer next to us draws the marker and carries its suffix, so the room between marker and
        // content is that text's, not something the empty marker box has to make.
        if (inlineWrapper())
            return { };

        if (isImage())
            return { 0, markerPadding };

        if (synthesizesGlyph())
            return { -1, fontMetrics.intAscent() - minContentLogicalWidthContribution() + 1 };

        return { };
    };

    auto marginsForOutsideMarker = [&]() -> std::pair<LayoutUnit, LayoutUnit> {
        if (isImage())
            return { -minContentLogicalWidthContribution() - markerPadding, markerPadding };

        int offset = fontMetrics.intAscent() * 2 / 3;
        if (synthesizesGlyph())
            return { -offset - markerPadding - 1, offset + markerPadding + 1 - minContentLogicalWidthContribution() };

        if (m_textContent.isEmpty() && !contentContainer())
            return { };

        return { -minContentLogicalWidthContribution(), 0 };
    };

    auto [marginStart, marginEnd] = isInside() ? marginsForInsideMarker() : marginsForOutsideMarker();
    auto zoom = style().usedZoomForLength().value;

    // Which side of the list item these hang the marker off follows the list item's directionality
    // rather than the marker's own: with marker-side: match-self (its initial value) css-lists-3
    // positions the marker box using the directionality of the ::marker's originating element.
    // Write the edges of the parent's inline axis, which is where
    // BoxGeometryUpdater::horizontalLogicalMargin reads them back from.
    auto listItemWritingMode = m_listItem->writingMode();
    auto startEdge = Style::MarginEdge::Fixed { marginStart / zoom };
    auto endEdge = Style::MarginEdge::Fixed { marginEnd / zoom };
    if (listItemWritingMode.isHorizontal()) {
        auto startIsLeft = listItemWritingMode.isInlineLeftToRight();
        mutableStyle().setMarginLeft(startIsLeft ? startEdge : endEdge);
        mutableStyle().setMarginRight(startIsLeft ? endEdge : startEdge);
        return;
    }
    auto startIsTop = listItemWritingMode.isInlineTopToBottom();
    mutableStyle().setMarginTop(startIsTop ? startEdge : endEdge);
    mutableStyle().setMarginBottom(startIsTop ? endEdge : startEdge);
}

bool RenderListMarker::isInside() const
{
    return style().listStylePosition() == ListStylePosition::Inside;
}

bool RenderListMarker::isDisclosureMarker() const
{
    auto counter = counterStyle();
    if (!counter)
        return false;
    auto system = counter->system();
    return system == CSSCounterStyleDescriptors::System::DisclosureClosed
        || system == CSSCounterStyleDescriptors::System::DisclosureOpen;
}

RenderListItem* RenderListMarker::listItem() const
{
    return m_listItem.get();
}

void RenderListMarker::setExcludedPosition(ExcludedPosition excludedPosition)
{
    ASSERT(excludedPosition.firstFormattedLineRoot);

    m_excludedPosition = excludedPosition;
}

void RenderListMarker::invalidateExcludedMarkerContainer()
{
    ASSERT(m_excludedPosition);

    if (!m_excludedPosition || !m_excludedPosition->firstFormattedLineRoot) {
        m_excludedPosition = { };
        return;
    }

    if (m_excludedPosition->firstFormattedLineRoot->isDescendantOf(m_listItem.get()))
        return m_excludedPosition->firstFormattedLineRoot->setNeedsLayout();

    m_excludedPosition = { };
}

Node* RenderListMarker::nodeForHitTest() const
{
    return m_listItem ? m_listItem->element() : nullptr;
}

FloatRect RenderListMarker::relativeMarkerRect()
{
    if (isImage())
        return { 0.f, 0.f, protect(m_image)->imageSize(this, style().usedZoom()).width(), protect(m_image)->imageSize(this, style().usedZoom()).height() };

    if (m_textContent.isEmpty())
        return { };

    FloatRect relativeRect;
    if (isDisclosureMarker()) {
        // Use system-ui font for disclosure triangles
        auto systemUIFontCascade = disclosureMarkerFontCascade(style(), protect(document()));
        auto& fontMetrics = style().metricsOfPrimaryFont();
        auto& systemUIFontMetrics = systemUIFontCascade.metricsOfPrimaryFont();
        auto width = systemUIFontCascade.width(textRunForContent(m_textContent, style()));
        auto height = systemUIFontMetrics.height();
        // Center vertically within the original font metrics
        auto yOffset = (fontMetrics.height() - height) / 2.0f;
        relativeRect = { 0.f, yOffset, width, height };
    } else {
        auto& font = style().fontCascade();
        relativeRect = { 0.f, 0.f, font.width(textRunForContent(m_textContent, style())), font.metricsOfPrimaryFont().height() };
    }

    if (!writingMode().isHorizontal()) {
        relativeRect = relativeRect.transposedRect();
        relativeRect.setX(borderBoxWidth() - relativeRect.x() - relativeRect.width());
    }

    return relativeRect;
}

LayoutRect RenderListMarker::selectionRectForRepaint(const RenderLayerModelObject*, bool)
{
    ASSERT(!needsLayout());
    return { };
}

RefPtr<CSSRegisteredCounterStyle> RenderListMarker::counterStyle() const
{
    auto counterStyle = style().listStyleType().tryCounterStyle();
    if (!counterStyle)
        return nullptr;
    return document().counterStyleRegistry().resolvedCounterStyle(*counterStyle);
}

bool RenderListMarker::synthesizesGlyph() const
{
    // `content` supersedes list-style-type, so a content marker never draws in place of a glyph, and a list-style-image marker draws the image instead.
    if (hasContentProperty() || isImage())
        return false;
    auto& listType = style().listStyleType();
    return listType.isCircle() || listType.isDisc() || listType.isSquare();
}

std::pair<float, float> RenderListMarker::layoutBoundForTextContent(String text) const
{
    // FIXME: This should be part of InlineBoxBuilder (webkit.org/b/294342)
    // This is essentially what we do in LineBoxBuilder::enclosingAscentDescentWithFallbackFonts.
    auto ascentAndDescent = [&] (auto& fontMetrics) {
        auto ascent = fontMetrics.ascent();
        auto descent = fontMetrics.descent();
        auto halfLeading = (fontMetrics.lineSpacing() - (ascent + descent)) / 2.f;
        return std::pair<float, float> { ascent + halfLeading, descent + halfLeading };
    };
    auto& style = this->style();
    auto& metricsOfPrimaryFont = style.metricsOfPrimaryFont();
    auto primaryFontHeight = metricsOfPrimaryFont.height();

    if (style.lineHeight().isNormal()) {
        auto maxAscentAndDescent = ascentAndDescent(metricsOfPrimaryFont);

        for (Ref fallbackFont : Layout::TextUtil::fallbackFontsForText(text, style, Layout::TextUtil::IncludeHyphen::No)) {
            auto& fontMetrics = fallbackFont->fontMetrics();
            if (primaryFontHeight >= fontMetrics.height()) {
                // FIXME: Figure out why certain symbols (e.g. disclosure-open) would initiate fallback fonts with just slightly different (subpixel) metrics.
                // This is mainly about preserving legacy behavior.
                continue;
            }
            auto ascentDescent = ascentAndDescent(fontMetrics);
            maxAscentAndDescent.first = std::max(maxAscentAndDescent.first, ascentDescent.first);
            maxAscentAndDescent.second = std::max(maxAscentAndDescent.second, ascentDescent.second);
        }
        return { maxAscentAndDescent.first, maxAscentAndDescent.second };
    }

    auto primaryFontAscentAndDescent = ascentAndDescent(metricsOfPrimaryFont);
    auto halfLeading = (style.computedLineHeight() - (primaryFontAscentAndDescent.first + primaryFontAscentAndDescent.second)) / 2.f;
    return { primaryFontAscentAndDescent.first + halfLeading, primaryFontAscentAndDescent.second + halfLeading };
}

} // namespace WebCore
