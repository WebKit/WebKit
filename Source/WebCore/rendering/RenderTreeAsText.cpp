/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RenderTreeAsText.h"

#include "ClipRect.h"
#include "ColorSerialization.h"
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "ElementInlines.h"
#include "FrameSelection.h"
#include "GraphicsLayer.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "InlineIteratorBoxInlines.h"
#include "InlineIteratorTextBox.h"
#include "LegacyRenderSVGContainer.h"
#include "LegacyRenderSVGImage.h"
#include "LegacyRenderSVGResourceContainer.h"
#include "LegacyRenderSVGRoot.h"
#include "LegacyRenderSVGShape.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NodeInlines.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "RemoteFrame.h"
#include "RemoteFrameView.h"
#include "RenderBlockFlow.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderCounter.h"
#include "RenderElementInlines.h"
#include "RenderFileUploadControl.h"
#include "RenderFragmentContainer.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayerBacking.h"
#include "RenderLayerInlines.h"
#include "RenderLayerScrollableArea.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderObjectInlines.h"
#include "RenderQuote.h"
#include "RenderSVGContainer.h"
#include "RenderSVGGradientStop.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGRoot.h"
#include "RenderSVGShapeInlines.h"
#include "RenderSVGText.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGRenderTreeAsText.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "StylePropertiesInlines.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/HexNumber.h>
#include <wtf/Vector.h>
#include <wtf/text/TextStream.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)
#include "ScrollbarThemeMac.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static void writeLayers(TextStream&, const RenderLayer& rootLayer, RenderLayer&, const LayoutRect& paintDirtyRect, OptionSet<RenderAsTextFlag>);

static void printBorderStyle(TextStream& ts, const BorderStyle borderStyle)
{
    switch (borderStyle) {
    case BorderStyle::None:
        ts << "none"_s;
        break;
    case BorderStyle::Hidden:
        ts << "hidden"_s;
        break;
    case BorderStyle::Inset:
        ts << "inset"_s;
        break;
    case BorderStyle::Groove:
        ts << "groove"_s;
        break;
    case BorderStyle::Ridge:
        ts << "ridge"_s;
        break;
    case BorderStyle::Outset:
        ts << "outset"_s;
        break;
    case BorderStyle::Dotted:
        ts << "dotted"_s;
        break;
    case BorderStyle::Dashed:
        ts << "dashed"_s;
        break;
    case BorderStyle::Solid:
        ts << "solid"_s;
        break;
    case BorderStyle::Double:
        ts << "double"_s;
        break;
    }

    ts << ' ';
}

static String getTagName(Node* n)
{
    if (n->isDocumentNode())
        return ""_s;
    if (n->nodeType() == Node::COMMENT_NODE)
        return "COMMENT"_s;
    return n->nodeName();
}

static bool isEmptyOrUnstyledAppleStyleSpan(const Node* node)
{
    auto* element = dynamicDowncast<HTMLSpanElement>(node);
    if (!element)
        return false;

    if (element->getAttribute(classAttr) != "Apple-style-span"_s)
        return false;

    if (!node->hasChildNodes())
        return true;

    const StyleProperties* inlineStyleDecl = element->inlineStyle();
    return (!inlineStyleDecl || inlineStyleDecl->isEmpty());
}

String quoteAndEscapeNonPrintables(StringView s)
{
    StringBuilder result;
    result.append('"');
    for (unsigned i = 0; i != s.length(); ++i) {
        char16_t c = s[i];
        if (c == '\\') {
            result.append("\\\\"_s);
        } else if (c == '"') {
            result.append("\\\""_s);
        } else if (c == '\n' || c == noBreakSpace)
            result.append(' ');
        else {
            if (c >= 0x20 && c < 0x7F)
                result.append(c);
            else
                result.append("\\x{"_s, hex(c), '}');
        }
    }
    result.append('"');
    return result.toString();
}

inline bool shouldEnableSubpixelPrecisionForTextDump(const Document& document)
{
    // If LBSE is activated and the document contains outermost <svg> elements, generate the text
    // representation with subpixel precision. It would be awkward to only see the SVG part of a
    // compound document with subpixel precision in the render tree dumps, and not the surrounding content.
    return document.settings().layerBasedSVGEngineEnabled() && document.mayHaveRenderedSVGRootElements();
}

void RenderTreeAsText::writeRenderObject(TextStream& ts, const RenderObject& o, OptionSet<RenderAsTextFlag> behavior)
{
    ts << o.renderName().characters();

    if (behavior.contains(RenderAsTextFlag::ShowAddresses))
        ts << ' ' << &o;

    if (auto value = o.style().usedZIndex().tryValue(); value && value->value) // FIXME: This should log even when value->value is zero.
        ts << " zI: "_s << value->value;

    if (o.node()) {
        String tagName = getTagName(o.node());
        // FIXME: Temporary hack to make tests pass by simulating the old generated content output.
        if (o.isPseudoElement() || (o.parent() && o.parent()->isPseudoElement()))
            tagName = emptyAtom();
        if (!tagName.isEmpty()) {
            ts << " {"_s << tagName << '}';
            // flag empty or unstyled AppleStyleSpan because we never
            // want to leave them in the DOM
            if (isEmptyOrUnstyledAppleStyleSpan(o.node()))
                ts << " *empty or unstyled AppleStyleSpan*"_s;
        }
    }
    
    bool enableSubpixelPrecisionForTextDump = shouldEnableSubpixelPrecisionForTextDump(o.document());
    LayoutRect r;
    if (auto* text = dynamicDowncast<RenderText>(o))
        r = text->linesBoundingBox();
    else if (auto* br = dynamicDowncast<RenderLineBreak>(o); br && br->isBR())
        r = br->linesBoundingBox();
    else if (auto* inlineFlow = dynamicDowncast<RenderInline>(o))
        r = inlineFlow->linesBoundingBox();
    else if (auto* cell = dynamicDowncast<RenderTableCell>(o)) {
        // FIXME: Deliberately dump the "inner" box of table cells, since that is what current results reflect.  We'd like
        // to clean up the results to dump both the outer box and the intrinsic padding so that both bits of information are
        // captured by the results.
        r = LayoutRect(cell->x(), cell->y() + cell->intrinsicPaddingBefore(), cell->width(), cell->height() - cell->intrinsicPaddingBefore() - cell->intrinsicPaddingAfter());
    } else if (auto* box = dynamicDowncast<RenderBox>(o))
        r = box->frameRect();
    else if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(o)) {
        r = svgModelObject->frameRectEquivalent();
        ASSERT(r.location() == svgModelObject->currentSVGLayoutLocation());
    }
    // FIXME: Convert layout test results to report sub-pixel values, in the meantime using enclosingIntRect
    // for consistency with old results.
    if (enableSubpixelPrecisionForTextDump)
        ts << ' ' << r;
    else
        ts << ' ' << enclosingIntRect(r);

    if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(o)) {
        writeSVGPaintingFeatures(ts, *svgModelObject, behavior);

        if (auto* svgShape = dynamicDowncast<RenderSVGShape>(*svgModelObject))
            writeSVGGraphicsElement(ts, svgShape->graphicsElement());

        writeDebugInfo(ts, o, behavior);
        return;
    }

    if (CheckedPtr renderElement = dynamicDowncast<RenderElement>(o)) {
        if (auto* control = dynamicDowncast<RenderFileUploadControl>(*renderElement))
            ts << ' ' << quoteAndEscapeNonPrintables(control->fileTextValue());

        if (renderElement->parent()) {
            Color color = renderElement->style().visitedDependentColor(CSSPropertyColor);
            if (!equalIgnoringSemanticColor(renderElement->parent()->style().visitedDependentColor(CSSPropertyColor), color))
                ts << " [color="_s << serializationForRenderTreeAsText(color) << ']';

            // Do not dump invalid or transparent backgrounds, since that is the default.
            Color backgroundColor = renderElement->style().visitedDependentColor(CSSPropertyBackgroundColor);
            if (!equalIgnoringSemanticColor(renderElement->parent()->style().visitedDependentColor(CSSPropertyBackgroundColor), backgroundColor)
                && backgroundColor != Color::transparentBlack)
                ts << " [bgcolor="_s << serializationForRenderTreeAsText(backgroundColor) << ']';
            
            Color textFillColor = renderElement->style().visitedDependentColor(CSSPropertyWebkitTextFillColor);
            if (!equalIgnoringSemanticColor(renderElement->parent()->style().visitedDependentColor(CSSPropertyWebkitTextFillColor), textFillColor)
                && textFillColor != color && textFillColor != Color::transparentBlack)
                ts << " [textFillColor="_s << serializationForRenderTreeAsText(textFillColor) << ']';

            Color textStrokeColor = renderElement->style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor);
            if (!equalIgnoringSemanticColor(renderElement->parent()->style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor), textStrokeColor)
                && textStrokeColor != color && textStrokeColor != Color::transparentBlack)
                ts << " [textStrokeColor="_s << serializationForRenderTreeAsText(textStrokeColor) << ']';

            if (renderElement->parent()->style().textStrokeWidth() != renderElement->style().textStrokeWidth() && renderElement->style().textStrokeWidth().isPositive())
                ts << " [textStrokeWidth="_s << Style::evaluate<float>(renderElement->style().textStrokeWidth(), renderElement->style().usedZoomForLength()) << ']';
        }

        auto* box = dynamicDowncast<RenderBoxModelObject>(o);
        if (!box || is<RenderLineBreak>(*box))
            return;

        LayoutUnit borderTop = box->borderTop();
        LayoutUnit borderRight = box->borderRight();
        LayoutUnit borderBottom = box->borderBottom();
        LayoutUnit borderLeft = box->borderLeft();
        bool overridden = renderElement->style().borderImage().overridesBorderWidths();
        if (box->isFieldset()) {
            const auto& block = downcast<RenderBlock>(*box);
            switch (renderElement->writingMode().blockDirection()) {
            case FlowDirection::TopToBottom:
                borderTop -= block.intrinsicBorderForFieldset();
                break;
            case FlowDirection::BottomToTop:
                borderBottom -= block.intrinsicBorderForFieldset();
                break;
            case FlowDirection::LeftToRight:
                borderLeft -= block.intrinsicBorderForFieldset();
                break;
            case FlowDirection::RightToLeft:
                borderRight -= block.intrinsicBorderForFieldset();
            }
        }
        if (borderTop || borderRight || borderBottom || borderLeft) {
            ts << " [border:"_s;

            auto printBorder = [&] (const LayoutUnit& width, const BorderStyle& style, const Style::Color& color) {
                if (!width)
                    ts << " none"_s;
                else {
                    ts << " ("_s << width << "px "_s;
                    printBorderStyle(ts, style);
                    auto resolvedColor = renderElement->style().colorResolvingCurrentColor(color);
                    ts << serializationForRenderTreeAsText(resolvedColor) << ')';
                }

            };

            BorderValue prevBorder = renderElement->style().borderTop();
            printBorder(borderTop, renderElement->style().borderTopStyle(), renderElement->style().borderTopColor());

            if (renderElement->style().borderRight() != prevBorder || (overridden && borderRight != borderTop)) {
                prevBorder = renderElement->style().borderRight();
                printBorder(borderRight, renderElement->style().borderRightStyle(), renderElement->style().borderRightColor());
            }

            if (renderElement->style().borderBottom() != prevBorder || (overridden && borderBottom != borderRight)) {
                prevBorder = renderElement->style().borderBottom();
                printBorder(borderBottom, renderElement->style().borderBottomStyle(), renderElement->style().borderBottomColor());
            }

            if (renderElement->style().borderLeft() != prevBorder || (overridden && borderLeft != borderBottom)) {
                prevBorder = renderElement->style().borderLeft();
                printBorder(borderLeft, renderElement->style().borderLeftStyle(), renderElement->style().borderLeftColor());
            }
            ts << ']';
        }

#if ENABLE(MATHML)
        // We want to show any layout padding, both CSS padding and intrinsic padding, so we can't just check o.style().hasPadding().
        if (o.isRenderMathMLBlock() && (box->paddingTop() || box->paddingRight() || box->paddingBottom() || box->paddingLeft())) {
            ts << " ["_s;
            LayoutUnit cssTop = box->computedCSSPaddingTop();
            LayoutUnit cssRight = box->computedCSSPaddingRight();
            LayoutUnit cssBottom = box->computedCSSPaddingBottom();
            LayoutUnit cssLeft = box->computedCSSPaddingLeft();
            if (box->paddingTop() != cssTop || box->paddingRight() != cssRight || box->paddingBottom() != cssBottom || box->paddingLeft() != cssLeft) {
                ts << "intrinsic "_s;
                if (cssTop || cssRight || cssBottom || cssLeft)
                    ts << "+ CSS "_s;
            }
            ts << "padding: "_s << roundToInt(box->paddingTop()) << ' ' << roundToInt(box->paddingRight()) << ' ' << roundToInt(box->paddingBottom()) << ' ' << roundToInt(box->paddingLeft()) << ']';
        }
#endif
    }

    if (auto* cell = dynamicDowncast<RenderTableCell>(o))
        ts << " [r="_s << cell->rowIndex() << " c="_s << cell->col() << " rs="_s << cell->rowSpan() << " cs="_s << cell->colSpan() << ']';

    if (auto* listMarker = dynamicDowncast<RenderListMarker>(o)) {
        auto text = listMarker->textWithoutSuffix();
        if (!text.isEmpty()) {
            if (text.length() != 1)
                text = quoteAndEscapeNonPrintables(text);
            else {
                switch (text[0]) {
                    case bullet:
                        text = "bullet"_s;
                        break;
                    case blackSquare:
                        text = "black square"_s;
                        break;
                    case whiteBullet:
                        text = "white bullet"_s;
                        break;
                    default:
                        text = quoteAndEscapeNonPrintables(text);
                }
            }
            ts << ": "_s << text;
        }
    }
    
    writeDebugInfo(ts, o, behavior);
}

void writeDebugInfo(TextStream& ts, const RenderObject& object, OptionSet<RenderAsTextFlag> behavior)
{
    if (behavior.contains(RenderAsTextFlag::ShowIDAndClass)) {
        if (auto* element = dynamicDowncast<Element>(object.node())) {
            if (element->hasID())
                ts << " id=\"" << element->getIdAttribute() << '"';

            if (element->hasClass()) {
                ts << " class=\""_s;
                for (size_t i = 0; i < element->classNames().size(); ++i) {
                    if (i > 0)
                        ts << ' ';
                    ts << element->classNames()[i];
                }
                ts << '"';
            }
        }
    }

    if (behavior.contains(RenderAsTextFlag::ShowLayoutState)) {
        bool needsLayout = object.selfNeedsLayout() || object.needsOutOfFlowMovementLayout() || object.outOfFlowChildNeedsLayout() || object.normalChildNeedsLayout();
        if (needsLayout)
            ts << " (needs layout:"_s;
        
        bool havePrevious = false;
        if (object.selfNeedsLayout()) {
            ts << " self"_s;
            havePrevious = true;
        }

        if (object.needsOutOfFlowMovementLayout()) {
            if (havePrevious)
                ts << ',';
            havePrevious = true;
            ts << " positioned movement"_s;
        }

        if (object.normalChildNeedsLayout()) {
            if (havePrevious)
                ts << ',';
            havePrevious = true;
            ts << " child"_s;
        }

        if (object.outOfFlowChildNeedsLayout()) {
            if (havePrevious)
                ts << ',';
            ts << " positioned child"_s;
        }

        if (needsLayout)
            ts << ')';
    }

    if (behavior.contains(RenderAsTextFlag::ShowOverflow)) {
        if (auto* box = dynamicDowncast<RenderBox>(object)) {
            if (box->hasRenderOverflow()) {
                LayoutRect layoutOverflow = box->layoutOverflowRect();
                ts << " (layout overflow "_s << layoutOverflow.x().toInt() << ',' << layoutOverflow.y().toInt() << ' ' << layoutOverflow.width().toInt() << 'x' << layoutOverflow.height().toInt() << ')';

                if (box->hasVisualOverflow()) {
                    LayoutRect visualOverflow = box->visualOverflowRect();
                    ts << " (visual overflow "_s << visualOverflow.x().toInt() << ',' << visualOverflow.y().toInt() << ' ' << visualOverflow.width().toInt() << 'x' << visualOverflow.height().toInt() << ')';
                }
            }
        }

        if (auto* renderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(object)) {
            if (renderSVGModelObject->hasVisualOverflow()) {
                auto visualOverflow = renderSVGModelObject->visualOverflowRectEquivalent();
                ts << " (visual overflow "_s << visualOverflow.x() << ',' << visualOverflow.y() << ' ' << visualOverflow.width() << 'x' << visualOverflow.height() << ')';
            }
        }
    }
}

static inline void writeTextRuns(TextStream& ts, auto& textRenderer)
{
    auto writeTextRun = [&] (auto& textRun) {
        ts << indent;

        auto rect = textRun.visualRectIgnoringBlockDirection();
        int x = rect.x();
        int y = rect.y();
        // FIXME: Use non-logical width. webkit.org/b/206809.
        int logicalWidth = ceilf(rect.x() + (textRun.isHorizontal() ? rect.width() : rect.height())) - x;
        // FIXME: Table cell adjustment is temporary until results can be updated.
        if (auto* tableCell = dynamicDowncast<RenderTableCell>(*textRenderer.containingBlock()))
            y -= floorToInt(tableCell->intrinsicPaddingBefore());

        ts << "text run at ("_s << x << ',' << y << ") width "_s << logicalWidth;
        if (!textRun.isLeftToRightDirection())
            ts << " RTL"_s;
        ts << ": "_s
            << quoteAndEscapeNonPrintables(textRun.originalText());
        if (textRun.hasHyphen())
            ts << " + hyphen string "_s << quoteAndEscapeNonPrintables(textRenderer.style().hyphenString().string());
        ts << '\n';
    };

    for (auto& run : InlineIterator::textBoxesFor(textRenderer))
        writeTextRun(run);
}

static inline void writeSVGRenderer(TextStream& ts, const RenderObject& renderer, OptionSet<RenderAsTextFlag> behavior)
{
    if (auto* svgShape = dynamicDowncast<LegacyRenderSVGShape>(renderer)) {
        write(ts, *svgShape, behavior);
        return;
    }
    if (auto* svgGradientStop = dynamicDowncast<RenderSVGGradientStop>(renderer)) {
        writeSVGGradientStop(ts, *svgGradientStop, behavior);
        return;
    }
    if (auto* svgResourceContainer = dynamicDowncast<LegacyRenderSVGResourceContainer>(renderer)) {
        writeSVGResourceContainer(ts, *svgResourceContainer, behavior);
        return;
    }
    if (auto* svgContainer = dynamicDowncast<LegacyRenderSVGContainer>(renderer)) {
        writeSVGContainer(ts, *svgContainer, behavior);
        return;
    }
    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(renderer)) {
        write(ts, *svgRoot, behavior);
        return;
    }
    if (auto* text = dynamicDowncast<RenderSVGText>(renderer)) {
        writeSVGText(ts, *text, behavior);
        return;
    }
    if (auto* inlineText = dynamicDowncast<RenderSVGInlineText>(renderer)) {
        writeSVGInlineText(ts, *inlineText, behavior);
        return;
    }
    if (auto* svgImage = dynamicDowncast<LegacyRenderSVGImage>(renderer)) {
        writeSVGImage(ts, *svgImage, behavior);
        return;
    }
}

void write(TextStream& ts, const RenderObject& renderer, OptionSet<RenderAsTextFlag> behavior)
{

    if (is<LegacyRenderSVGShape>(renderer) || is<RenderSVGGradientStop>(renderer) || is<LegacyRenderSVGResourceContainer>(renderer)
        || is<LegacyRenderSVGContainer>(renderer) || is<LegacyRenderSVGRoot>(renderer) || is<RenderSVGText>(renderer)
        || is<RenderSVGInlineText>(renderer) || is<LegacyRenderSVGImage>(renderer)) {
        writeSVGRenderer(ts, renderer, behavior);
        return;
    }

    ts << indent;
    RenderTreeAsText::writeRenderObject(ts, renderer, behavior);
    ts << '\n';

    TextStream::IndentScope indentScope(ts);
    if (auto* textRenderer = dynamicDowncast<RenderText>(renderer)) {
        writeTextRuns(ts, *textRenderer);
        return;
    }

    for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(renderer))) {
        if (child.hasLayer())
            continue;
        write(ts, child, behavior);
    }

    if (auto* renderWidget = dynamicDowncast<RenderWidget>(renderer); renderWidget && renderWidget->widget() && is<FrameView>(renderWidget->widget()))
        dynamicDowncast<FrameView>(renderWidget->widget())->writeRenderTreeAsText(ts, behavior);

    if (is<RenderSVGModelObject>(renderer) || is<RenderSVGRoot>(renderer))
        writeResources(ts, renderer, behavior);
}

enum LayerPaintPhase {
    LayerPaintPhaseAll = 0,
    LayerPaintPhaseBackground = -1,
    LayerPaintPhaseForeground = 1
};

template<typename DumpRectType>
inline void writeLayerUsingGeometryType(TextStream& ts, const RenderLayer& layer, const DumpRectType& layerBounds, const DumpRectType& backgroundClipRect, const DumpRectType& clipRect,
    LayerPaintPhase paintPhase, OptionSet<RenderAsTextFlag> behavior)
{
    ts << indent << "layer "_s;
    
    if (behavior.contains(RenderAsTextFlag::ShowAddresses)) {
        ts << &layer << ' ';
        if (auto* scrollableArea = layer.scrollableArea())
            ts << "scrollableArea "_s << scrollableArea << ' ';
    }

    ts << layerBounds;

    if (!layerBounds.isEmpty()) {
        if (!backgroundClipRect.contains(layerBounds))
            ts << " backgroundClip "_s << backgroundClipRect;
        if (!clipRect.contains(layerBounds))
            ts << " clip "_s << clipRect;
    }

    if (layer.renderer().hasNonVisibleOverflow()) {
        if (auto* scrollableArea = layer.scrollableArea()) {
            if (scrollableArea->scrollOffset().x())
                ts << " scrollX "_s << scrollableArea->scrollOffset().x();
            if (scrollableArea->scrollOffset().y())
                ts << " scrollY "_s << scrollableArea->scrollOffset().y();
            if (layer.renderBox() && roundToInt(layer.renderBox()->clientWidth()) != scrollableArea->scrollWidth())
                ts << " scrollWidth "_s << scrollableArea->scrollWidth();
            if (layer.renderBox() && roundToInt(layer.renderBox()->clientHeight()) != scrollableArea->scrollHeight())
                ts << " scrollHeight "_s << scrollableArea->scrollHeight();
        }
#if PLATFORM(MAC)
        ScrollbarTheme& scrollbarTheme = ScrollbarTheme::theme();
        if (!scrollbarTheme.isMockTheme() && layer.scrollableArea() && layer.scrollableArea()->hasVerticalScrollbar()) {
            ScrollbarThemeMac& macTheme = *downcast<ScrollbarThemeMac>(&scrollbarTheme);
            if (macTheme.isLayoutDirectionRTL(*layer.scrollableArea()->verticalScrollbar()))
                ts << " scrollbarHasRTLLayoutDirection"_s;
        }
#endif
    }

    if (paintPhase == LayerPaintPhaseBackground)
        ts << " layerType: background only"_s;
    else if (paintPhase == LayerPaintPhaseForeground)
        ts << " layerType: foreground only"_s;

    if (behavior.contains(RenderAsTextFlag::ShowCompositedLayers)) {
        if (layer.isComposited()) {
            ts << " (composited "_s << layer.compositor().reasonsForCompositing(layer)
                << ", bounds=" << layer.backing()->compositedBounds()
                << ", drawsContent=" << layer.backing()->graphicsLayer()->drawsContent()
                << ", paints into ancestor=" << layer.backing()->paintsIntoCompositedAncestor() << ")";
        } else if (layer.paintsIntoProvidedBacking())
            ts << " (shared backing of "_s << layer.backingProviderLayer() << ')';
    }

    if (layer.isolatesBlending())
        ts << " isolatesBlending"_s;
    if (layer.hasBlendMode())
        ts << " blendMode: "_s << compositeOperatorName(CompositeOperator::SourceOver, layer.blendMode());
    
    ts << '\n';
}

static void writeLayer(TextStream& ts, const RenderLayer& layer, const LayoutRect& layerBounds, const LayoutRect& backgroundClipRect, const LayoutRect& clipRect,
    LayerPaintPhase paintPhase = LayerPaintPhaseAll, OptionSet<RenderAsTextFlag> behavior = { })
{
    if (shouldEnableSubpixelPrecisionForTextDump(layer.renderer().document())) {
        writeLayerUsingGeometryType<LayoutRect>(ts, layer, layerBounds, backgroundClipRect, clipRect, paintPhase, behavior);
        return;
    }

    writeLayerUsingGeometryType<IntRect>(ts, layer, snappedIntRect(layerBounds), snappedIntRect(backgroundClipRect), snappedIntRect(clipRect), paintPhase, behavior);
}

static void writeLayerRenderers(TextStream& ts, const RenderLayer& layer, LayerPaintPhase paintPhase, OptionSet<RenderAsTextFlag> behavior)
{
    if (paintPhase != LayerPaintPhaseBackground) {
        TextStream::IndentScope indentScope(ts);
        write(ts, layer.renderer(), behavior);
    }
}

static LayoutSize maxLayoutOverflow(const RenderBox* box)
{
    LayoutRect overflowRect = box->layoutOverflowRect();
    return LayoutSize(overflowRect.maxX(), overflowRect.maxY());
}

static void writeLayers(TextStream& ts, const RenderLayer& rootLayer, RenderLayer& layer, const LayoutRect& paintRect, OptionSet<RenderAsTextFlag> behavior)
{
    // FIXME: Apply overflow to the root layer to not break every test. Complete hack. Sigh.
    LayoutRect paintDirtyRect(paintRect);
    if (&rootLayer == &layer) {
        paintDirtyRect.setWidth(std::max<LayoutUnit>(paintDirtyRect.width(), rootLayer.renderBox()->layoutOverflowRect().maxX()));
        paintDirtyRect.setHeight(std::max<LayoutUnit>(paintDirtyRect.height(), rootLayer.renderBox()->layoutOverflowRect().maxY()));
        layer.setSize(layer.size().expandedTo(snappedIntSize(maxLayoutOverflow(layer.renderBox()), LayoutPoint(0, 0))));
        layer.setNeedsPositionUpdate();
    }
    
    // Calculate the clip rects we should use.
    LayoutSize offsetFromRoot = layer.offsetFromAncestor(&rootLayer);
    RenderLayer::ClipRectsContext clipRectsContext(&rootLayer, PaintingClipRects, RenderLayer::clipRectTemporaryOptions);
    auto rects = layer.calculateRects(clipRectsContext, offsetFromRoot, paintDirtyRect);

    // Ensure our lists are up-to-date.
    layer.updateLayerListsIfNeeded();
    layer.updateDescendantDependentFlags();

    bool shouldPaint = (behavior.contains(RenderAsTextFlag::ShowAllLayers)) ? true : layer.intersectsDamageRect(rects.layerBounds(), rects.dirtyBackgroundRect().rect(), &rootLayer, layer.offsetFromAncestor(&rootLayer));
    auto negativeZOrderLayers = layer.negativeZOrderLayers();
    bool paintsBackgroundSeparately = negativeZOrderLayers.size() > 0;
    if (shouldPaint && paintsBackgroundSeparately) {
        writeLayer(ts, layer, rects.layerBounds(), rects.dirtyBackgroundRect().rect(), rects.dirtyForegroundRect().rect(), LayerPaintPhaseBackground, behavior);
        writeLayerRenderers(ts, layer, LayerPaintPhaseBackground, behavior);
    }
        
    if (negativeZOrderLayers.size()) {
        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting)) {
            ts << indent << " negative z-order list ("_s << negativeZOrderLayers.size() << ")\n"_s;
            ts.increaseIndent();
        }
        
        for (auto* currLayer : negativeZOrderLayers)
            writeLayers(ts, rootLayer, *currLayer, paintDirtyRect, behavior);

        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting))
            ts.decreaseIndent();
    }

    if (shouldPaint) {
        writeLayer(ts, layer, rects.layerBounds(), rects.dirtyBackgroundRect().rect(), rects.dirtyForegroundRect().rect(), paintsBackgroundSeparately ? LayerPaintPhaseForeground : LayerPaintPhaseAll, behavior);
        
        if (behavior.contains(RenderAsTextFlag::ShowLayerFragments)) {
            LayerFragments layerFragments;
            layer.collectFragments(layerFragments, &rootLayer, paintDirtyRect, RenderLayer::PaginationInclusionMode::ExcludeCompositedPaginatedLayers, PaintingClipRects, RenderLayer::clipRectTemporaryOptions, offsetFromRoot);

            if (layerFragments.size() > 1) {
                TextStream::IndentScope indentScope(ts, 2);
                for (unsigned i = 0; i < layerFragments.size(); ++i) {
                    const auto& fragment = layerFragments[i];
                    ts << indent << " fragment "_s << i << ": bounds in layer "_s << fragment.layerBounds() << " fragment bounds "_s << fragment.boundingBox() << '\n';
                }
            }
        }
        
        writeLayerRenderers(ts, layer, paintsBackgroundSeparately ? LayerPaintPhaseForeground : LayerPaintPhaseAll, behavior);
    }
    
    auto normalFlowLayers = layer.normalFlowLayers();
    if (normalFlowLayers.size()) {
        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting)) {
            ts << indent << " normal flow list ("_s << normalFlowLayers.size() << ")\n"_s;
            ts.increaseIndent();
        }
        
        for (auto* currLayer : normalFlowLayers)
            writeLayers(ts, rootLayer, *currLayer, paintDirtyRect, behavior);

        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting))
            ts.decreaseIndent();
    }

    auto positiveZOrderLayers = layer.positiveZOrderLayers();
    if (positiveZOrderLayers.size()) {
        size_t layerCount = positiveZOrderLayers.size();

        if (layerCount) {
            if (behavior.contains(RenderAsTextFlag::ShowLayerNesting)) {
                ts << indent << " positive z-order list ("_s << layerCount << ")\n"_s;
                ts.increaseIndent();
            }

            for (auto* currLayer : positiveZOrderLayers)
                writeLayers(ts, rootLayer, *currLayer, paintDirtyRect, behavior);

            if (behavior.contains(RenderAsTextFlag::ShowLayerNesting))
                ts.decreaseIndent();
        }
    }
}

static String nodePosition(Node* node)
{
    StringBuilder result;

    auto* body = node->document().bodyOrFrameset();
    Node* parent;
    for (Node* n = node; n; n = parent) {
        parent = n->parentOrShadowHostNode();
        if (n != node)
            result.append(" of "_s);
        if (parent) {
            if (body && n == body) {
                // We don't care what offset body may be in the document.
                result.append("body"_s);
                break;
            }
            if (n->isShadowRoot())
                result.append('{', getTagName(n), '}');
            else
                result.append("child "_s, n->computeNodeIndex(), " {"_s, getTagName(n), '}');
        } else
            result.append("document"_s);
    }

    return result.toString();
}

static void writeSelection(TextStream& ts, const RenderBox& renderer)
{
    if (!renderer.isRenderView())
        return;

    auto* frame = renderer.document().frame();
    if (!frame)
        return;

    VisibleSelection selection = frame->selection().selection();
    if (selection.isCaret()) {
        ts << "caret: position "_s << selection.start().deprecatedEditingOffset() << " of "_s << nodePosition(selection.start().deprecatedNode());
        if (selection.affinity() == Affinity::Upstream)
            ts << " (upstream affinity)"_s;
        ts << '\n';
    } else if (selection.isRange())
        ts << "selection start: position "_s << selection.start().deprecatedEditingOffset() << " of "_s << nodePosition(selection.start().deprecatedNode()) << '\n'
           << "selection end:   position " << selection.end().deprecatedEditingOffset() << " of " << nodePosition(selection.end().deprecatedNode()) << "\n";
}

static TextStream createTextStream(const Document& document)
{
    auto formattingFlags = [&document]() -> OptionSet<TextStream::Formatting> {
        if (shouldEnableSubpixelPrecisionForTextDump(document))
            return { TextStream::Formatting::SVGStyleRect };
        return { TextStream::Formatting::SVGStyleRect, TextStream::Formatting::LayoutUnitsAsIntegers };
    };

    return { TextStream::LineMode::MultipleLine, formattingFlags() };
}

TextStream createTextStream(const RenderView& view)
{
    return createTextStream(view.document());
}

static String externalRepresentation(RenderBox& renderer, OptionSet<RenderAsTextFlag> behavior)
{
    auto ts = createTextStream(renderer.document());
    if (!renderer.hasLayer())
        return ts.release();

    LOG(Layout, "externalRepresentation: dumping layer tree");

    ScriptDisallowedScope scriptDisallowedScope;
    RenderLayer& layer = *renderer.layer();
    writeLayers(ts, layer, layer, layer.rect(), behavior);
    writeSelection(ts, renderer);
    return ts.release();
}

String externalRepresentation(LocalFrame* frame, OptionSet<RenderAsTextFlag> behavior)
{
    ASSERT(frame);
    ASSERT(frame->document());

    if (!(behavior.contains(RenderAsTextFlag::DontUpdateLayout)) && frame->view())
        frame->view()->updateLayoutAndStyleIfNeededRecursive({ LayoutOptions::IgnorePendingStylesheets, LayoutOptions::UpdateCompositingLayers });

    auto* renderer = frame->contentRenderer();
    if (!renderer)
        return String();

    Ref printContext = PrintContext::create(frame);
    if (behavior.contains(RenderAsTextFlag::PrintingMode))
        printContext->begin(renderer->width());

    return externalRepresentation(*renderer, behavior);
}

void externalRepresentationForLocalFrame(TextStream &ts, LocalFrame& frame, OptionSet<RenderAsTextFlag> behavior)
{
    ASSERT(frame.document());

    if (RenderView* root = frame.contentRenderer()) {
        if (RenderLayer* layer = root->layer())
            writeLayers(ts, *layer, *layer, layer->rect(), behavior);
    }
}

String externalRepresentation(Element* element, OptionSet<RenderAsTextFlag> behavior)
{
    ASSERT(element);

    // This function doesn't support printing mode.
    ASSERT(!(behavior.contains(RenderAsTextFlag::PrintingMode)));

    if (!(behavior.contains(RenderAsTextFlag::DontUpdateLayout)) && element->document().view())
        element->document().view()->updateLayoutAndStyleIfNeededRecursive({ LayoutOptions::IgnorePendingStylesheets, LayoutOptions::UpdateCompositingLayers });

    auto* renderer = element->renderer();
    if (!is<RenderBox>(renderer))
        return String();

    return externalRepresentation(downcast<RenderBox>(*renderer), behavior | RenderAsTextFlag::ShowAllLayers);
}

static void writeCounterValuesFromChildren(TextStream& stream, const RenderElement* parent, bool& isFirstCounter)
{
    if (!parent)
        return;
    for (auto& counter : childrenOfType<RenderCounter>(*parent)) {
        if (!isFirstCounter)
            stream << " ";
        isFirstCounter = false;
        String str(counter.text());
        stream << str;
    }
}

String counterValueForElement(Element* element)
{
    // Make sure the element is not freed during the layout.
    RefPtr<Element> elementRef(element);
    element->document().updateLayout();
    auto stream = createTextStream(element->document());
    bool isFirstCounter = true;
    // The counter renderers should be children of :before or :after pseudo-elements.
    if (PseudoElement* before = element->beforePseudoElement())
        writeCounterValuesFromChildren(stream, before->renderer(), isFirstCounter);
    if (PseudoElement* after = element->afterPseudoElement())
        writeCounterValuesFromChildren(stream, after->renderer(), isFirstCounter);
    return stream.release();
}

String markerTextForListItem(Element* element)
{
    // Make sure the element is not freed during the layout.
    RefPtr protectedElement { element };
    element->document().updateLayout();

    auto* renderer = dynamicDowncast<RenderListItem>(element->renderer());
    if (!renderer)
        return String();
    return renderer->markerTextWithoutSuffix();
}

} // namespace WebCore
