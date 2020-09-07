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
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "InlineTextBox.h"
#include "LineLayoutTraversal.h"
#include "Logging.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "RenderBlockFlow.h"
#include "RenderCounter.h"
#include "RenderDetailsMarker.h"
#include "RenderFileUploadControl.h"
#include "RenderFragmentContainer.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderSVGContainer.h"
#include "RenderSVGGradientStop.h"
#include "RenderSVGImage.h"
#include "RenderSVGInlineText.h"
#include "RenderSVGPath.h"
#include "RenderSVGResourceContainer.h"
#include "RenderSVGRoot.h"
#include "RenderSVGText.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "RenderWidget.h"
#include "SVGRenderTreeAsText.h"
#include "ShadowRoot.h"
#include "StyleProperties.h"
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
        return "";
    if (n->nodeType() == Node::COMMENT_NODE)
        return "COMMENT";
    return n->nodeName();
}

static bool isEmptyOrUnstyledAppleStyleSpan(const Node* node)
{
    if (!is<HTMLSpanElement>(node))
        return false;

    const HTMLElement& element = downcast<HTMLSpanElement>(*node);
    if (element.getAttribute(classAttr) != "Apple-style-span")
        return false;

    if (!node->hasChildNodes())
        return true;

    const StyleProperties* inlineStyleDecl = element.inlineStyle();
    return (!inlineStyleDecl || inlineStyleDecl->isEmpty());
}

String quoteAndEscapeNonPrintables(StringView s)
{
    StringBuilder result;
    result.append('"');
    for (unsigned i = 0; i != s.length(); ++i) {
        UChar c = s[i];
        if (c == '\\') {
            result.appendLiteral("\\\\");
        } else if (c == '"') {
            result.appendLiteral("\\\"");
        } else if (c == '\n' || c == noBreakSpace)
            result.append(' ');
        else {
            if (c >= 0x20 && c < 0x7F)
                result.append(c);
            else {
                result.appendLiteral("\\x{");
                result.append(hex(c));
                result.append('}');
            }
        }
    }
    result.append('"');
    return result.toString();
}

void RenderTreeAsText::writeRenderObject(TextStream& ts, const RenderObject& o, OptionSet<RenderAsTextFlag> behavior)
{
    ts << o.renderName();

    if (behavior.contains(RenderAsTextFlag::ShowAddresses))
        ts << " " << static_cast<const void*>(&o);

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
    
    RenderBlock* cb = o.containingBlock();
    bool adjustForTableCells = cb ? cb->isTableCell() : false;

    LayoutRect r;
    if (is<RenderText>(o)) {
        // FIXME: Would be better to dump the bounding box x and y rather than the first run's x and y, but that would involve updating
        // many test results.
        const RenderText& text = downcast<RenderText>(o);
        r = IntRect(text.firstRunLocation(), text.linesBoundingBox().size());
        if (!LineLayoutTraversal::firstTextBoxFor(text))
            adjustForTableCells = false;
    } else if (o.isBR()) {
        const RenderLineBreak& br = downcast<RenderLineBreak>(o);
        IntRect linesBox = br.boundingBoxForRenderTreeDump();
        r = IntRect(linesBox.x(), linesBox.y(), linesBox.width(), linesBox.height());
        if (!br.inlineBoxWrapper())
            adjustForTableCells = false;
    } else if (is<RenderInline>(o)) {
        const RenderInline& inlineFlow = downcast<RenderInline>(o);
        // FIXME: Would be better not to just dump 0, 0 as the x and y here.
        r = IntRect(0, 0, inlineFlow.linesBoundingBox().width(), inlineFlow.linesBoundingBox().height());
        adjustForTableCells = false;
    } else if (is<RenderTableCell>(o)) {
        // FIXME: Deliberately dump the "inner" box of table cells, since that is what current results reflect.  We'd like
        // to clean up the results to dump both the outer box and the intrinsic padding so that both bits of information are
        // captured by the results.
        const RenderTableCell& cell = downcast<RenderTableCell>(o);
        r = LayoutRect(cell.x(), cell.y() + cell.intrinsicPaddingBefore(), cell.width(), cell.height() - cell.intrinsicPaddingBefore() - cell.intrinsicPaddingAfter());
    } else if (is<RenderBox>(o))
        r = downcast<RenderBox>(o).frameRect();

    // FIXME: Temporary in order to ensure compatibility with existing layout test results.
    if (adjustForTableCells)
        r.move(0_lu, -downcast<RenderTableCell>(*o.containingBlock()).intrinsicPaddingBefore());

    // FIXME: Convert layout test results to report sub-pixel values, in the meantime using enclosingIntRect
    // for consistency with old results.
    ts << " " << enclosingIntRect(r);

    if (!is<RenderText>(o)) {
        if (is<RenderFileUploadControl>(o))
            ts << " " << quoteAndEscapeNonPrintables(downcast<RenderFileUploadControl>(o).fileTextValue());

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

        if (!is<RenderBoxModelObject>(o) || is<RenderLineBreak>(o))
            return;

        const RenderBoxModelObject& box = downcast<RenderBoxModelObject>(o);
        LayoutUnit borderTop = box.borderTop();
        LayoutUnit borderRight = box.borderRight();
        LayoutUnit borderBottom = box.borderBottom();
        LayoutUnit borderLeft = box.borderLeft();
        if (box.isFieldset()) {
            const auto& block = downcast<RenderBlock>(box);
            if (o.style().writingMode() == WritingMode::TopToBottom)
                borderTop -= block.intrinsicBorderForFieldset();
            else if (o.style().writingMode() == WritingMode::BottomToTop)
                borderBottom -= block.intrinsicBorderForFieldset();
            else if (o.style().writingMode() == WritingMode::LeftToRight)
                borderLeft -= block.intrinsicBorderForFieldset();
            else if (o.style().writingMode() == WritingMode::RightToLeft)
                borderRight -= block.intrinsicBorderForFieldset();
            
        }
        if (borderTop || borderRight || borderBottom || borderLeft) {
            ts << " [border:";

            BorderValue prevBorder = o.style().borderTop();
            if (!borderTop)
                ts << " none";
            else {
                ts << " (" << borderTop << "px ";
                printBorderStyle(ts, o.style().borderTopStyle());
                auto color = o.style().borderTopColor();
                if (!color.isValid())
                    color = o.style().color();
                ts << serializationForRenderTreeAsText(color) << ")";
            }

            if (o.style().borderRight() != prevBorder) {
                prevBorder = o.style().borderRight();
                if (!borderRight)
                    ts << " none";
                else {
                    ts << " (" << borderRight << "px ";
                    printBorderStyle(ts, o.style().borderRightStyle());
                    auto color = o.style().borderRightColor();
                    if (!color.isValid())
                        color = o.style().color();
                    ts << serializationForRenderTreeAsText(color) << ")";
                }
            }

            if (o.style().borderBottom() != prevBorder) {
                prevBorder = box.style().borderBottom();
                if (!borderBottom)
                    ts << " none";
                else {
                    ts << " (" << borderBottom << "px ";
                    printBorderStyle(ts, o.style().borderBottomStyle());
                    auto color = o.style().borderBottomColor();
                    if (!color.isValid())
                        color = o.style().color();
                    ts << serializationForRenderTreeAsText(color) << ")";
                }
            }

            if (o.style().borderLeft() != prevBorder) {
                prevBorder = o.style().borderLeft();
                if (!borderLeft)
                    ts << " none";
                else {
                    ts << " (" << borderLeft << "px ";
                    printBorderStyle(ts, o.style().borderLeftStyle());
                    auto color = o.style().borderLeftColor();
                    if (!color.isValid())
                        color = o.style().color();
                    ts << serializationForRenderTreeAsText(color) << ")";
                }
            }

            ts << "]";
        }

#if ENABLE(MATHML)
        // We want to show any layout padding, both CSS padding and intrinsic padding, so we can't just check o.style().hasPadding().
        if (o.isRenderMathMLBlock() && (box.paddingTop() || box.paddingRight() || box.paddingBottom() || box.paddingLeft())) {
            ts << " [";
            LayoutUnit cssTop = box.computedCSSPaddingTop();
            LayoutUnit cssRight = box.computedCSSPaddingRight();
            LayoutUnit cssBottom = box.computedCSSPaddingBottom();
            LayoutUnit cssLeft = box.computedCSSPaddingLeft();
            if (box.paddingTop() != cssTop || box.paddingRight() != cssRight || box.paddingBottom() != cssBottom || box.paddingLeft() != cssLeft) {
                ts << "intrinsic ";
                if (cssTop || cssRight || cssBottom || cssLeft)
                    ts << "+ CSS ";
            }
            ts << "padding: " << roundToInt(box.paddingTop()) << " " << roundToInt(box.paddingRight()) << " " << roundToInt(box.paddingBottom()) << " " << roundToInt(box.paddingLeft()) << "]";
        }
#endif
    }

    if (is<RenderTableCell>(o)) {
        const RenderTableCell& c = downcast<RenderTableCell>(o);
        ts << " [r=" << c.rowIndex() << " c=" << c.col() << " rs=" << c.rowSpan() << " cs=" << c.colSpan() << "]";
    }

    if (is<RenderDetailsMarker>(o)) {
        ts << ": ";
        switch (downcast<RenderDetailsMarker>(o).orientation()) {
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

    if (is<RenderListMarker>(o)) {
        String text = downcast<RenderListMarker>(o).text();
        if (!text.isEmpty()) {
            if (text.length() != 1)
                text = quoteAndEscapeNonPrintables(text);
            else {
                switch (text[0]) {
                    case bullet:
                        text = "bullet";
                        break;
                    case blackSquare:
                        text = "black square";
                        break;
                    case whiteBullet:
                        text = "white bullet";
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
        if (Element* element = is<Element>(object.node()) ? downcast<Element>(object.node()) : nullptr) {
            if (element->hasID())
                ts << " id=\"" + element->getIdAttribute() + "\"";

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

    if (behavior.contains(RenderAsTextFlag::ShowOverflow) && is<RenderBox>(object)) {
        const auto& box = downcast<RenderBox>(object);
        if (box.hasRenderOverflow()) {
            LayoutRect layoutOverflow = box.layoutOverflowRect();
            ts << " (layout overflow " << layoutOverflow.x().toInt() << "," << layoutOverflow.y().toInt() << " " << layoutOverflow.width().toInt() << "x" << layoutOverflow.height().toInt() << ")";
            
            if (box.hasVisualOverflow()) {
                LayoutRect visualOverflow = box.visualOverflowRect();
                ts << " (visual overflow " << visualOverflow.x().toInt() << "," << visualOverflow.y().toInt() << " " << visualOverflow.width().toInt() << "x" << visualOverflow.height().toInt() << ")";
            }
        }
    }
}

static void writeTextBox(TextStream& ts, const RenderText& o, const LineLayoutTraversal::TextBox& textBox)
{
    auto rect = textBox.rect();
    int x = rect.x();
    int y = rect.y();
    // FIXME: Use non-logical width. webkit.org/b/206809.
    int logicalWidth = ceilf(rect.x() + (textBox.isHorizontal() ? rect.width() : rect.height())) - x;
    // FIXME: Table cell adjustment is temporary until results can be updated.
    if (is<RenderTableCell>(*o.containingBlock()))
        y -= floorToInt(downcast<RenderTableCell>(*o.containingBlock()).intrinsicPaddingBefore());
        
    ts << "text run at (" << x << "," << y << ") width " << logicalWidth;
    if (!textBox.isLeftToRightDirection() || textBox.dirOverride()) {
        ts << (!textBox.isLeftToRightDirection() ? " RTL" : " LTR");
        if (textBox.dirOverride())
            ts << " override";
    }
    ts << ": "
        << quoteAndEscapeNonPrintables(textBox.text());
    if (textBox.hasHyphen())
        ts << " + hyphen string " << quoteAndEscapeNonPrintables(o.style().hyphenString().string());
    ts << "\n";
}

void write(TextStream& ts, const RenderObject& o, OptionSet<RenderAsTextFlag> behavior)
{
    if (is<RenderSVGShape>(o)) {
        write(ts, downcast<RenderSVGShape>(o), behavior);
        return;
    }
    if (is<RenderSVGGradientStop>(o)) {
        writeSVGGradientStop(ts, downcast<RenderSVGGradientStop>(o), behavior);
        return;
    }
    if (is<RenderSVGResourceContainer>(o)) {
        writeSVGResourceContainer(ts, downcast<RenderSVGResourceContainer>(o), behavior);
        return;
    }
    if (is<RenderSVGContainer>(o)) {
        writeSVGContainer(ts, downcast<RenderSVGContainer>(o), behavior);
        return;
    }
    if (is<RenderSVGRoot>(o)) {
        write(ts, downcast<RenderSVGRoot>(o), behavior);
        return;
    }
    if (is<RenderSVGText>(o)) {
        writeSVGText(ts, downcast<RenderSVGText>(o), behavior);
        return;
    }
    if (is<RenderSVGInlineText>(o)) {
        writeSVGInlineText(ts, downcast<RenderSVGInlineText>(o), behavior);
        return;
    }
    if (is<RenderSVGImage>(o)) {
        writeSVGImage(ts, downcast<RenderSVGImage>(o), behavior);
        return;
    }

    ts << indent;

    RenderTreeAsText::writeRenderObject(ts, o, behavior);
    ts << "\n";

    TextStream::IndentScope indentScope(ts);

    if (is<RenderText>(o)) {
        auto& text = downcast<RenderText>(o);
        for (auto& textBox : LineLayoutTraversal::textBoxesFor(text)) {
            ts << indent;
            writeTextBox(ts, text, textBox);
        }
    } else {
        for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(o))) {
            if (child.hasLayer())
                continue;
            write(ts, child, behavior);
        }
    }

    if (is<RenderWidget>(o)) {
        Widget* widget = downcast<RenderWidget>(o).widget();
        if (is<FrameView>(widget)) {
            FrameView& view = downcast<FrameView>(*widget);
            if (RenderView* root = view.frame().contentRenderer()) {
                if (!(behavior.contains(RenderAsTextFlag::DontUpdateLayout)))
                    view.layoutContext().layout();
                if (RenderLayer* layer = root->layer())
                    writeLayers(ts, *layer, *layer, layer->rect(), behavior);
            }
        }
    }
}

enum LayerPaintPhase {
    LayerPaintPhaseAll = 0,
    LayerPaintPhaseBackground = -1,
    LayerPaintPhaseForeground = 1
};

static void writeLayer(TextStream& ts, const RenderLayer& layer, const LayoutRect& layerBounds, const LayoutRect& backgroundClipRect, const LayoutRect& clipRect,
    LayerPaintPhase paintPhase = LayerPaintPhaseAll, OptionSet<RenderAsTextFlag> behavior = { })
{
    IntRect adjustedLayoutBounds = snappedIntRect(layerBounds);
    IntRect adjustedBackgroundClipRect = snappedIntRect(backgroundClipRect);
    IntRect adjustedClipRect = snappedIntRect(clipRect);

    ts << indent << "layer ";
    
    if (behavior.contains(RenderAsTextFlag::ShowAddresses))
        ts << static_cast<const void*>(&layer) << " ";
      
    ts << adjustedLayoutBounds;

    if (!adjustedLayoutBounds.isEmpty()) {
        if (!adjustedBackgroundClipRect.contains(adjustedLayoutBounds))
            ts << " backgroundClip " << adjustedBackgroundClipRect;
        if (!adjustedClipRect.contains(adjustedLayoutBounds))
            ts << " clip " << adjustedClipRect;
    }

    if (layer.renderer().hasOverflowClip()) {
        if (layer.scrollOffset().x())
            ts << " scrollX " << layer.scrollOffset().x();
        if (layer.scrollOffset().y())
            ts << " scrollY " << layer.scrollOffset().y();
        if (layer.renderBox() && roundToInt(layer.renderBox()->clientWidth()) != layer.scrollWidth())
            ts << " scrollWidth " << layer.scrollWidth();
        if (layer.renderBox() && roundToInt(layer.renderBox()->clientHeight()) != layer.scrollHeight())
            ts << " scrollHeight " << layer.scrollHeight();
#if PLATFORM(MAC)
        ScrollbarTheme& scrollbarTheme = ScrollbarTheme::theme();
        if (!scrollbarTheme.isMockTheme() && layer.hasVerticalScrollbar()) {
            ScrollbarThemeMac& macTheme = *static_cast<ScrollbarThemeMac*>(&scrollbarTheme);
            if (macTheme.isLayoutDirectionRTL(*layer.verticalScrollbar()))
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
            ts << " (composited, bounds=" << layer.backing()->compositedBounds() << ", drawsContent=" << layer.backing()->graphicsLayer()->drawsContent()
                << ", paints into ancestor=" << layer.backing()->paintsIntoCompositedAncestor() << ")";
        } else if (layer.paintsIntoProvidedBacking())
            ts << " (shared backing of " << layer.backingProviderLayer() << ")";
    }

#if ENABLE(CSS_COMPOSITING)
    if (layer.isolatesBlending())
        ts << " isolatesBlending";
    if (layer.hasBlendMode())
        ts << " blendMode: " << compositeOperatorName(CompositeOperator::SourceOver, layer.blendMode());
#endif
    
    ts << "\n";
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
            layer.collectFragments(layerFragments, &rootLayer, paintDirtyRect, RenderLayer::PaginationInclusionMode::ExcludeCompositedPaginatedLayers, TemporaryClipRects, IgnoreOverlayScrollbarSize, RespectOverflowClip, offsetFromRoot);
            
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
            result.appendLiteral(" of ");
        if (parent) {
            if (body && n == body) {
                // We don't care what offset body may be in the document.
                result.appendLiteral("body");
                break;
            }
            if (n->isShadowRoot()) {
                result.append('{');
                result.append(getTagName(n));
                result.append('}');
            } else {
                result.appendLiteral("child ");
                result.appendNumber(n->computeNodeIndex());
                result.appendLiteral(" {");
                result.append(getTagName(n));
                result.append('}');
            }
        } else
            result.appendLiteral("document");
    }

    return result.toString();
}

static void writeSelection(TextStream& ts, const RenderBox& renderer)
{
    if (!renderer.isRenderView())
        return;

    Frame* frame = renderer.document().frame();
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

static String externalRepresentation(RenderBox& renderer, OptionSet<RenderAsTextFlag> behavior)
{
    TextStream ts(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect | TextStream::Formatting::LayoutUnitsAsIntegers);
    if (!renderer.hasLayer())
        return ts.release();

    LOG(Layout, "externalRepresentation: dumping layer tree");

    RenderLayer& layer = *renderer.layer();
    writeLayers(ts, layer, layer, layer.rect(), behavior);
    writeSelection(ts, renderer);
    return ts.release();
}

static void updateLayoutIgnoringPendingStylesheetsIncludingSubframes(Document& document)
{
    document.updateLayoutIgnorePendingStylesheets();
    auto* frame = document.frame();
    for (auto* subframe = frame; subframe; subframe = subframe->tree().traverseNext(frame)) {
        if (auto* document = subframe->document())
            document->updateLayoutIgnorePendingStylesheets();
    }
}

String externalRepresentation(Frame* frame, OptionSet<RenderAsTextFlag> behavior)
{
    ASSERT(frame);
    ASSERT(frame->document());

    if (!(behavior.contains(RenderAsTextFlag::DontUpdateLayout)))
        updateLayoutIgnoringPendingStylesheetsIncludingSubframes(*frame->document());

    auto* renderer = frame->contentRenderer();
    if (!renderer)
        return String();

    PrintContext printContext(frame);
    if (behavior.contains(RenderAsTextFlag::PrintingMode))
        printContext.begin(renderer->width());

    return externalRepresentation(*renderer, behavior);
}

String externalRepresentation(Element* element, OptionSet<RenderAsTextFlag> behavior)
{
    ASSERT(element);

    // This function doesn't support printing mode.
    ASSERT(!(behavior.contains(RenderAsTextFlag::PrintingMode)));

    if (!(behavior.contains(RenderAsTextFlag::DontUpdateLayout)))
        updateLayoutIgnoringPendingStylesheetsIncludingSubframes(element->document());

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
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect | TextStream::Formatting::LayoutUnitsAsIntegers);
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
    RefPtr<Element> elementRef(element);
    element->document().updateLayout();

    RenderElement* renderer = element->renderer();
    if (!is<RenderListItem>(renderer))
        return String();

    return downcast<RenderListItem>(*renderer).markerText();
}

} // namespace WebCore
