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
#include "Document.h"
#include "ElementInlines.h"
#include "FrameSelection.h"
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
#include "PrintContext.h"
#include "PseudoElement.h"
#include "RemoteFrame.h"
#include "RemoteFrameView.h"
#include "RenderBlockFlow.h"
#include "RenderBoxModelObjectInlines.h"
#include "RenderCounter.h"
#include "RenderDetailsMarker.h"
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
        ts << "none";
        break;
    case BorderStyle::Hidden:
        ts << "hidden";
        break;
    case BorderStyle::Inset:
        ts << "inset";
        break;
    case BorderStyle::Groove:
        ts << "groove";
        break;
    case BorderStyle::Ridge:
        ts << "ridge";
        break;
    case BorderStyle::Outset:
        ts << "outset";
        break;
    case BorderStyle::Dotted:
        ts << "dotted";
        break;
    case BorderStyle::Dashed:
        ts << "dashed";
        break;
    case BorderStyle::Solid:
        ts << "solid";
        break;
    case BorderStyle::Double:
        ts << "double";
        break;
    }

    ts << " ";
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
        UChar c = s[i];
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
        ts << " " << &o;

    if (o.style().usedZIndex()) // FIXME: This should use !hasAutoUsedZIndex().
        ts << " zI: " << o.style().usedZIndex();

    if (o.node()) {
        String tagName = getTagName(o.node());
        // FIXME: Temporary hack to make tests pass by simulating the old generated content output.
        if (o.isPseudoElement() || (o.parent() && o.parent()->isPseudoElement()))
            tagName = emptyAtom();
        if (!tagName.isEmpty()) {
            ts << " {" << tagName << "}";
            // flag empty or unstyled AppleStyleSpan because we never
            // want to leave them in the DOM
            if (isEmptyOrUnstyledAppleStyleSpan(o.node()))
                ts << " *empty or unstyled AppleStyleSpan*";
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
        ts << " " << r;
    else
        ts << " " << enclosingIntRect(r);

    if (auto* svgModelObject = dynamicDowncast<RenderSVGModelObject>(o)) {
        writeSVGPaintingFeatures(ts, *svgModelObject, behavior);

        if (auto* svgShape = dynamicDowncast<RenderSVGShape>(*svgModelObject))
            writeSVGGraphicsElement(ts, svgShape->graphicsElement());

        writeDebugInfo(ts, o, behavior);
        return;
    }

    if (!is<RenderText>(o)) {
        if (auto* control = dynamicDowncast<RenderFileUploadControl>(o))
            ts << " " << quoteAndEscapeNonPrintables(control->fileTextValue());

        if (o.parent()) {
            Color color = o.style().visitedDependentColor(CSSPropertyColor);
            if (!equalIgnoringSemanticColor(o.parent()->style().visitedDependentColor(CSSPropertyColor), color))
                ts << " [color=" << serializationForRenderTreeAsText(color) << "]";

            // Do not dump invalid or transparent backgrounds, since that is the default.
            Color backgroundColor = o.style().visitedDependentColor(CSSPropertyBackgroundColor);
            if (!equalIgnoringSemanticColor(o.parent()->style().visitedDependentColor(CSSPropertyBackgroundColor), backgroundColor)
                && backgroundColor != Color::transparentBlack)
                ts << " [bgcolor=" << serializationForRenderTreeAsText(backgroundColor) << "]";
            
            Color textFillColor = o.style().visitedDependentColor(CSSPropertyWebkitTextFillColor);
            if (!equalIgnoringSemanticColor(o.parent()->style().visitedDependentColor(CSSPropertyWebkitTextFillColor), textFillColor)
                && textFillColor != color && textFillColor != Color::transparentBlack)
                ts << " [textFillColor=" << serializationForRenderTreeAsText(textFillColor) << "]";

            Color textStrokeColor = o.style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor);
            if (!equalIgnoringSemanticColor(o.parent()->style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor), textStrokeColor)
                && textStrokeColor != color && textStrokeColor != Color::transparentBlack)
                ts << " [textStrokeColor=" << serializationForRenderTreeAsText(textStrokeColor) << "]";

            if (o.parent()->style().textStrokeWidth() != o.style().textStrokeWidth() && o.style().textStrokeWidth() > 0)
                ts << " [textStrokeWidth=" << o.style().textStrokeWidth() << "]";
        }

        auto* box = dynamicDowncast<RenderBoxModelObject>(o);
        if (!box || is<RenderLineBreak>(*box))
            return;

        LayoutUnit borderTop = box->borderTop();
        LayoutUnit borderRight = box->borderRight();
        LayoutUnit borderBottom = box->borderBottom();
        LayoutUnit borderLeft = box->borderLeft();
        bool overridden = o.style().borderImage().overridesBorderWidths();
        if (box->isFieldset()) {
            const auto& block = downcast<RenderBlock>(*box);
            if (o.style().blockFlowDirection() == BlockFlowDirection::TopToBottom)
                borderTop -= block.intrinsicBorderForFieldset();
            else if (o.style().blockFlowDirection() == BlockFlowDirection::BottomToTop)
                borderBottom -= block.intrinsicBorderForFieldset();
            else if (o.style().blockFlowDirection() == BlockFlowDirection::LeftToRight)
                borderLeft -= block.intrinsicBorderForFieldset();
            else if (o.style().blockFlowDirection() == BlockFlowDirection::RightToLeft)
                borderRight -= block.intrinsicBorderForFieldset();
            
        }
        if (borderTop || borderRight || borderBottom || borderLeft) {
            ts << " [border:";

            auto printBorder = [&ts, &o] (const LayoutUnit& width, const BorderStyle& style, const StyleColor& color) {
                if (!width)
                    ts << " none";
                else {
                    ts << " (" << width << "px ";
                    printBorderStyle(ts, style);
                    auto resolvedColor = o.style().colorResolvingCurrentColor(color);
                    ts << serializationForRenderTreeAsText(resolvedColor) << ")";
                }

            };

            BorderValue prevBorder = o.style().borderTop();
            printBorder(borderTop, o.style().borderTopStyle(), o.style().borderTopColor());

            if (o.style().borderRight() != prevBorder || (overridden && borderRight != borderTop)) {
                prevBorder = o.style().borderRight();
                printBorder(borderRight, o.style().borderRightStyle(), o.style().borderRightColor());
            }

            if (o.style().borderBottom() != prevBorder || (overridden && borderBottom != borderRight)) {
                prevBorder = o.style().borderBottom();
                printBorder(borderBottom, o.style().borderBottomStyle(), o.style().borderBottomColor());
            }

            if (o.style().borderLeft() != prevBorder || (overridden && borderLeft != borderBottom)) {
                prevBorder = o.style().borderLeft();
                printBorder(borderLeft, o.style().borderLeftStyle(), o.style().borderLeftColor());
            }
            ts << "]";
        }

#if ENABLE(MATHML)
        // We want to show any layout padding, both CSS padding and intrinsic padding, so we can't just check o.style().hasPadding().
        if (o.isRenderMathMLBlock() && (box->paddingTop() || box->paddingRight() || box->paddingBottom() || box->paddingLeft())) {
            ts << " [";
            LayoutUnit cssTop = box->computedCSSPaddingTop();
            LayoutUnit cssRight = box->computedCSSPaddingRight();
            LayoutUnit cssBottom = box->computedCSSPaddingBottom();
            LayoutUnit cssLeft = box->computedCSSPaddingLeft();
            if (box->paddingTop() != cssTop || box->paddingRight() != cssRight || box->paddingBottom() != cssBottom || box->paddingLeft() != cssLeft) {
                ts << "intrinsic ";
                if (cssTop || cssRight || cssBottom || cssLeft)
                    ts << "+ CSS ";
            }
            ts << "padding: " << roundToInt(box->paddingTop()) << " " << roundToInt(box->paddingRight()) << " " << roundToInt(box->paddingBottom()) << " " << roundToInt(box->paddingLeft()) << "]";
        }
#endif
    }

    if (auto* cell = dynamicDowncast<RenderTableCell>(o))
        ts << " [r=" << cell->rowIndex() << " c=" << cell->col() << " rs=" << cell->rowSpan() << " cs=" << cell->colSpan() << "]";

    if (auto* detailsMarker = dynamicDowncast<RenderDetailsMarker>(o)) {
        ts << ": ";
        switch (detailsMarker->orientation()) {
        case RenderDetailsMarker::Left:
            ts << "left";
            break;
        case RenderDetailsMarker::Right:
            ts << "right";
            break;
        case RenderDetailsMarker::Up:
            ts << "up";
            break;
        case RenderDetailsMarker::Down:
            ts << "down";
            break;
        }
    }

    if (auto* listMarker = dynamicDowncast<RenderListMarker>(o)) {
        String text = listMarker->textWithoutSuffix().toString();
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
            ts << ": " << text;
        }
    }
    
    writeDebugInfo(ts, o, behavior);
}

void writeDebugInfo(TextStream& ts, const RenderObject& object, OptionSet<RenderAsTextFlag> behavior)
{
    if (behavior.contains(RenderAsTextFlag::ShowIDAndClass)) {
        if (auto* element = dynamicDowncast<Element>(object.node())) {
            if (element->hasID())
                ts << " id=\"" << element->getIdAttribute() << "\"";

            if (element->hasClass()) {
                ts << " class=\"";
                for (size_t i = 0; i < element->classNames().size(); ++i) {
                    if (i > 0)
                        ts << " ";
                    ts << element->classNames()[i];
                }
                ts << "\"";
            }
        }
    }

    if (behavior.contains(RenderAsTextFlag::ShowLayoutState)) {
        bool needsLayout = object.selfNeedsLayout() || object.needsPositionedMovementLayout() || object.posChildNeedsLayout() || object.normalChildNeedsLayout();
        if (needsLayout)
            ts << " (needs layout:";
        
        bool havePrevious = false;
        if (object.selfNeedsLayout()) {
            ts << " self";
            havePrevious = true;
        }

        if (object.needsPositionedMovementLayout()) {
            if (havePrevious)
                ts << ",";
            havePrevious = true;
            ts << " positioned movement";
        }

        if (object.normalChildNeedsLayout()) {
            if (havePrevious)
                ts << ",";
            havePrevious = true;
            ts << " child";
        }

        if (object.posChildNeedsLayout()) {
            if (havePrevious)
                ts << ",";
            ts << " positioned child";
        }

        if (needsLayout)
            ts << ")";
    }

    if (behavior.contains(RenderAsTextFlag::ShowOverflow)) {
        if (auto* box = dynamicDowncast<RenderBox>(object)) {
            if (box->hasRenderOverflow()) {
                LayoutRect layoutOverflow = box->layoutOverflowRect();
                ts << " (layout overflow " << layoutOverflow.x().toInt() << "," << layoutOverflow.y().toInt() << " " << layoutOverflow.width().toInt() << "x" << layoutOverflow.height().toInt() << ")";

                if (box->hasVisualOverflow()) {
                    LayoutRect visualOverflow = box->visualOverflowRect();
                    ts << " (visual overflow " << visualOverflow.x().toInt() << "," << visualOverflow.y().toInt() << " " << visualOverflow.width().toInt() << "x" << visualOverflow.height().toInt() << ")";
                }
            }
        }

        if (auto* renderSVGModelObject = dynamicDowncast<RenderSVGModelObject>(object)) {
            if (renderSVGModelObject->hasVisualOverflow()) {
                auto visualOverflow = renderSVGModelObject->visualOverflowRectEquivalent();
                ts << " (visual overflow " << visualOverflow.x() << "," << visualOverflow.y() << " " << visualOverflow.width() << "x" << visualOverflow.height() << ")";
            }
        }
    }
}

void write(TextStream& ts, const RenderObject& o, OptionSet<RenderAsTextFlag> behavior)
{
    auto writeTextRun = [&] (auto& textRenderer, auto& textRun) {
        auto rect = textRun.visualRectIgnoringBlockDirection();
        int x = rect.x();
        int y = rect.y();
        // FIXME: Use non-logical width. webkit.org/b/206809.
        int logicalWidth = ceilf(rect.x() + (textRun.isHorizontal() ? rect.width() : rect.height())) - x;
        // FIXME: Table cell adjustment is temporary until results can be updated.
        if (auto* tableCell = dynamicDowncast<RenderTableCell>(*o.containingBlock()))
            y -= floorToInt(tableCell->intrinsicPaddingBefore());

        ts << "text run at (" << x << "," << y << ") width " << logicalWidth;
        if (!textRun.isLeftToRightDirection())
            ts << " RTL";
        ts << ": "
            << quoteAndEscapeNonPrintables(textRun.originalText());
        if (textRun.hasHyphen())
            ts << " + hyphen string " << quoteAndEscapeNonPrintables(textRenderer.style().hyphenString().string());
        ts << "\n";
    };


    if (auto* svgShape = dynamicDowncast<LegacyRenderSVGShape>(o)) {
        write(ts, *svgShape, behavior);
        return;
    }
    if (auto* svgGradientStop = dynamicDowncast<RenderSVGGradientStop>(o)) {
        writeSVGGradientStop(ts, *svgGradientStop, behavior);
        return;
    }
    if (auto* svgResourceContainer = dynamicDowncast<LegacyRenderSVGResourceContainer>(o)) {
        writeSVGResourceContainer(ts, *svgResourceContainer, behavior);
        return;
    }
    if (auto* svgContainer = dynamicDowncast<LegacyRenderSVGContainer>(o)) {
        writeSVGContainer(ts, *svgContainer, behavior);
        return;
    }
    if (auto* svgRoot = dynamicDowncast<LegacyRenderSVGRoot>(o)) {
        write(ts, *svgRoot, behavior);
        return;
    }
    if (auto* text = dynamicDowncast<RenderSVGText>(o)) {
        writeSVGText(ts, *text, behavior);
        return;
    }
    if (auto* inlineText = dynamicDowncast<RenderSVGInlineText>(o)) {
        writeSVGInlineText(ts, *inlineText, behavior);
        return;
    }
    if (auto* svgImage = dynamicDowncast<LegacyRenderSVGImage>(o)) {
        writeSVGImage(ts, *svgImage, behavior);
        return;
    }

    ts << indent;

    RenderTreeAsText::writeRenderObject(ts, o, behavior);
    ts << "\n";

    TextStream::IndentScope indentScope(ts);

    if (auto* text = dynamicDowncast<RenderText>(o)) {
        for (auto& run : InlineIterator::textBoxesFor(*text)) {
            ts << indent;
            writeTextRun(*text, run);
        }
    } else {
        for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(o))) {
            if (child.hasLayer())
                continue;
            write(ts, child, behavior);
        }
    }

    if (auto* renderWidget = dynamicDowncast<RenderWidget>(o)) {
        if (auto* widget = renderWidget->widget()) {
            if (auto* frameView = dynamicDowncast<FrameView>(widget))
                frameView->writeRenderTreeAsText(ts, behavior);
        }
    }

    if (is<RenderSVGModelObject>(o) || is<RenderSVGRoot>(o))
        writeResources(ts, o, behavior);
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
    ts << indent << "layer ";
    
    if (behavior.contains(RenderAsTextFlag::ShowAddresses)) {
        ts << &layer << " ";
        if (auto* scrollableArea = layer.scrollableArea())
            ts << "scrollableArea " << scrollableArea << " ";
    }

    ts << layerBounds;

    if (!layerBounds.isEmpty()) {
        if (!backgroundClipRect.contains(layerBounds))
            ts << " backgroundClip " << backgroundClipRect;
        if (!clipRect.contains(layerBounds))
            ts << " clip " << clipRect;
    }

    if (layer.renderer().hasNonVisibleOverflow()) {
        if (auto* scrollableArea = layer.scrollableArea()) {
            if (scrollableArea->scrollOffset().x())
                ts << " scrollX " << scrollableArea->scrollOffset().x();
            if (scrollableArea->scrollOffset().y())
                ts << " scrollY " << scrollableArea->scrollOffset().y();
            if (layer.renderBox() && roundToInt(layer.renderBox()->clientWidth()) != scrollableArea->scrollWidth())
                ts << " scrollWidth " << scrollableArea->scrollWidth();
            if (layer.renderBox() && roundToInt(layer.renderBox()->clientHeight()) != scrollableArea->scrollHeight())
                ts << " scrollHeight " << scrollableArea->scrollHeight();
        }
#if PLATFORM(MAC)
        ScrollbarTheme& scrollbarTheme = ScrollbarTheme::theme();
        if (!scrollbarTheme.isMockTheme() && layer.scrollableArea() && layer.scrollableArea()->hasVerticalScrollbar()) {
            ScrollbarThemeMac& macTheme = *static_cast<ScrollbarThemeMac*>(&scrollbarTheme);
            if (macTheme.isLayoutDirectionRTL(*layer.scrollableArea()->verticalScrollbar()))
                ts << " scrollbarHasRTLLayoutDirection";
        }
#endif
    }

    if (paintPhase == LayerPaintPhaseBackground)
        ts << " layerType: background only";
    else if (paintPhase == LayerPaintPhaseForeground)
        ts << " layerType: foreground only";

    if (behavior.contains(RenderAsTextFlag::ShowCompositedLayers)) {
        if (layer.isComposited()) {
            ts << " (composited " << layer.compositor().reasonsForCompositing(layer)
                << ", bounds=" << layer.backing()->compositedBounds()
                << ", drawsContent=" << layer.backing()->graphicsLayer()->drawsContent()
                << ", paints into ancestor=" << layer.backing()->paintsIntoCompositedAncestor() << ")";
        } else if (layer.paintsIntoProvidedBacking())
            ts << " (shared backing of " << layer.backingProviderLayer() << ")";
    }

    if (layer.isolatesBlending())
        ts << " isolatesBlending";
    if (layer.hasBlendMode())
        ts << " blendMode: " << compositeOperatorName(CompositeOperator::SourceOver, layer.blendMode());
    
    ts << "\n";
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
    }
    
    // Calculate the clip rects we should use.
    LayoutRect layerBounds;
    ClipRect damageRect;
    ClipRect clipRectToApply;
    LayoutSize offsetFromRoot = layer.offsetFromAncestor(&rootLayer);
    layer.calculateRects(RenderLayer::ClipRectsContext(&rootLayer, TemporaryClipRects), paintDirtyRect, layerBounds, damageRect, clipRectToApply, offsetFromRoot);

    // Ensure our lists are up-to-date.
    layer.updateLayerListsIfNeeded();
    layer.updateDescendantDependentFlags();

    bool shouldPaint = (behavior.contains(RenderAsTextFlag::ShowAllLayers)) ? true : layer.intersectsDamageRect(layerBounds, damageRect.rect(), &rootLayer, layer.offsetFromAncestor(&rootLayer));
    auto negativeZOrderLayers = layer.negativeZOrderLayers();
    bool paintsBackgroundSeparately = negativeZOrderLayers.size() > 0;
    if (shouldPaint && paintsBackgroundSeparately) {
        writeLayer(ts, layer, layerBounds, damageRect.rect(), clipRectToApply.rect(), LayerPaintPhaseBackground, behavior);
        writeLayerRenderers(ts, layer, LayerPaintPhaseBackground, behavior);
    }
        
    if (negativeZOrderLayers.size()) {
        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting)) {
            ts << indent << " negative z-order list (" << negativeZOrderLayers.size() << ")\n";
            ts.increaseIndent();
        }
        
        for (auto* currLayer : negativeZOrderLayers)
            writeLayers(ts, rootLayer, *currLayer, paintDirtyRect, behavior);

        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting))
            ts.decreaseIndent();
    }

    if (shouldPaint) {
        writeLayer(ts, layer, layerBounds, damageRect.rect(), clipRectToApply.rect(), paintsBackgroundSeparately ? LayerPaintPhaseForeground : LayerPaintPhaseAll, behavior);
        
        if (behavior.contains(RenderAsTextFlag::ShowLayerFragments)) {
            LayerFragments layerFragments;
            layer.collectFragments(layerFragments, &rootLayer, paintDirtyRect, RenderLayer::PaginationInclusionMode::ExcludeCompositedPaginatedLayers, TemporaryClipRects, { RenderLayer::ClipRectsOption::RespectOverflowClip }, offsetFromRoot);
            
            if (layerFragments.size() > 1) {
                TextStream::IndentScope indentScope(ts, 2);
                for (unsigned i = 0; i < layerFragments.size(); ++i) {
                    const auto& fragment = layerFragments[i];
                    ts << indent << " fragment " << i << ": bounds in layer " << fragment.layerBounds << " fragment bounds " << fragment.boundingBox << "\n";
                }
            }
        }
        
        writeLayerRenderers(ts, layer, paintsBackgroundSeparately ? LayerPaintPhaseForeground : LayerPaintPhaseAll, behavior);
    }
    
    auto normalFlowLayers = layer.normalFlowLayers();
    if (normalFlowLayers.size()) {
        if (behavior.contains(RenderAsTextFlag::ShowLayerNesting)) {
            ts << indent << " normal flow list (" << normalFlowLayers.size() << ")\n";
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
                ts << indent << " positive z-order list (" << layerCount << ")\n";
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
        ts << "caret: position " << selection.start().deprecatedEditingOffset() << " of " << nodePosition(selection.start().deprecatedNode());
        if (selection.affinity() == Affinity::Upstream)
            ts << " (upstream affinity)";
        ts << "\n";
    } else if (selection.isRange())
        ts << "selection start: position " << selection.start().deprecatedEditingOffset() << " of " << nodePosition(selection.start().deprecatedNode()) << "\n"
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

    PrintContext printContext(frame);
    if (behavior.contains(RenderAsTextFlag::PrintingMode))
        printContext.begin(renderer->width());

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
    return renderer->markerTextWithoutSuffix().toString();
}

} // namespace WebCore
