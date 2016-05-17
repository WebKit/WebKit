/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
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
#include "Document.h"
#include "FlowThreadController.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "HTMLSpanElement.h"
#include "InlineTextBox.h"
#include "PrintContext.h"
#include "PseudoElement.h"
#include "RenderBlockFlow.h"
#include "RenderCounter.h"
#include "RenderDetailsMarker.h"
#include "RenderFileUploadControl.h"
#include "RenderInline.h"
#include "RenderIterator.h"
#include "RenderLayer.h"
#include "RenderLayerBacking.h"
#include "RenderLineBreak.h"
#include "RenderListItem.h"
#include "RenderListMarker.h"
#include "RenderNamedFlowFragment.h"
#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
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
#include "SimpleLineLayoutResolver.h"
#include "StyleProperties.h"
#include "TextStream.h"
#include <wtf/HexNumber.h>
#include <wtf/Vector.h>
#include <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)
#include "ScrollbarThemeMac.h"
#endif

namespace WebCore {

using namespace HTMLNames;

static void writeLayers(TextStream&, const RenderLayer* rootLayer, RenderLayer*, const LayoutRect& paintDirtyRect, int indent = 0, RenderAsTextBehavior = RenderAsTextBehaviorNormal);

static void printBorderStyle(TextStream& ts, const EBorderStyle borderStyle)
{
    switch (borderStyle) {
        case BNONE:
            ts << "none";
            break;
        case BHIDDEN:
            ts << "hidden";
            break;
        case INSET:
            ts << "inset";
            break;
        case GROOVE:
            ts << "groove";
            break;
        case RIDGE:
            ts << "ridge";
            break;
        case OUTSET:
            ts << "outset";
            break;
        case DOTTED:
            ts << "dotted";
            break;
        case DASHED:
            ts << "dashed";
            break;
        case SOLID:
            ts << "solid";
            break;
        case DOUBLE:
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
            result.append('\\');
            result.append('\\');
        } else if (c == '"') {
            result.append('\\');
            result.append('"');
        } else if (c == '\n' || c == noBreakSpace)
            result.append(' ');
        else {
            if (c >= 0x20 && c < 0x7F)
                result.append(c);
            else {
                result.append('\\');
                result.append('x');
                result.append('{');
                appendUnsignedAsHex(c, result); 
                result.append('}');
            }
        }
    }
    result.append('"');
    return result.toString();
}

void RenderTreeAsText::writeRenderObject(TextStream& ts, const RenderObject& o, RenderAsTextBehavior behavior)
{
    ts << o.renderName();

    if (behavior & RenderAsTextShowAddresses)
        ts << " " << static_cast<const void*>(&o);

    if (o.style().zIndex())
        ts << " zI: " << o.style().zIndex();

    if (o.node()) {
        String tagName = getTagName(o.node());
        // FIXME: Temporary hack to make tests pass by simulating the old generated content output.
        if (o.isPseudoElement() || (o.parent() && o.parent()->isPseudoElement()))
            tagName = emptyAtom;
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
        if (!text.firstTextBox() && !text.simpleLineLayout())
            adjustForTableCells = false;
    } else if (o.isBR()) {
        const RenderLineBreak& br = downcast<RenderLineBreak>(o);
        IntRect linesBox = br.linesBoundingBox();
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
        r.move(0, -downcast<RenderTableCell>(*o.containingBlock()).intrinsicPaddingBefore());

    // FIXME: Convert layout test results to report sub-pixel values, in the meantime using enclosingIntRect
    // for consistency with old results.
    ts << " " << enclosingIntRect(r);

    if (!is<RenderText>(o)) {
        if (is<RenderFileUploadControl>(o))
            ts << " " << quoteAndEscapeNonPrintables(downcast<RenderFileUploadControl>(o).fileTextValue());

        if (o.parent()) {
            Color color = o.style().visitedDependentColor(CSSPropertyColor);
            if (o.parent()->style().visitedDependentColor(CSSPropertyColor) != color)
                ts << " [color=" << color.nameForRenderTreeAsText() << "]";

            // Do not dump invalid or transparent backgrounds, since that is the default.
            Color backgroundColor = o.style().visitedDependentColor(CSSPropertyBackgroundColor);
            if (o.parent()->style().visitedDependentColor(CSSPropertyBackgroundColor) != backgroundColor
                && backgroundColor.isValid() && backgroundColor.rgb())
                ts << " [bgcolor=" << backgroundColor.nameForRenderTreeAsText() << "]";
            
            Color textFillColor = o.style().visitedDependentColor(CSSPropertyWebkitTextFillColor);
            if (o.parent()->style().visitedDependentColor(CSSPropertyWebkitTextFillColor) != textFillColor
                && textFillColor.isValid() && textFillColor != color && textFillColor.rgb())
                ts << " [textFillColor=" << textFillColor.nameForRenderTreeAsText() << "]";

            Color textStrokeColor = o.style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor);
            if (o.parent()->style().visitedDependentColor(CSSPropertyWebkitTextStrokeColor) != textStrokeColor
                && textStrokeColor.isValid() && textStrokeColor != color && textStrokeColor.rgb())
                ts << " [textStrokeColor=" << textStrokeColor.nameForRenderTreeAsText() << "]";

            if (o.parent()->style().textStrokeWidth() != o.style().textStrokeWidth() && o.style().textStrokeWidth() > 0)
                ts << " [textStrokeWidth=" << o.style().textStrokeWidth() << "]";
        }

        if (!is<RenderBoxModelObject>(o) || is<RenderLineBreak>(o))
            return;

        const RenderBoxModelObject& box = downcast<RenderBoxModelObject>(o);
        if (box.borderTop() || box.borderRight() || box.borderBottom() || box.borderLeft()) {
            ts << " [border:";

            BorderValue prevBorder = o.style().borderTop();
            if (!box.borderTop())
                ts << " none";
            else {
                ts << " (" << box.borderTop() << "px ";
                printBorderStyle(ts, o.style().borderTopStyle());
                Color col = o.style().borderTopColor();
                if (!col.isValid())
                    col = o.style().color();
                ts << col.nameForRenderTreeAsText() << ")";
            }

            if (o.style().borderRight() != prevBorder) {
                prevBorder = o.style().borderRight();
                if (!box.borderRight())
                    ts << " none";
                else {
                    ts << " (" << box.borderRight() << "px ";
                    printBorderStyle(ts, o.style().borderRightStyle());
                    Color col = o.style().borderRightColor();
                    if (!col.isValid())
                        col = o.style().color();
                    ts << col.nameForRenderTreeAsText() << ")";
                }
            }

            if (o.style().borderBottom() != prevBorder) {
                prevBorder = box.style().borderBottom();
                if (!box.borderBottom())
                    ts << " none";
                else {
                    ts << " (" << box.borderBottom() << "px ";
                    printBorderStyle(ts, o.style().borderBottomStyle());
                    Color col = o.style().borderBottomColor();
                    if (!col.isValid())
                        col = o.style().color();
                    ts << col.nameForRenderTreeAsText() << ")";
                }
            }

            if (o.style().borderLeft() != prevBorder) {
                prevBorder = o.style().borderLeft();
                if (!box.borderLeft())
                    ts << " none";
                else {
                    ts << " (" << box.borderLeft() << "px ";
                    printBorderStyle(ts, o.style().borderLeftStyle());
                    Color col = o.style().borderLeftColor();
                    if (!col.isValid())
                        col = o.style().color();
                    ts << col.nameForRenderTreeAsText() << ")";
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

#if ENABLE(DETAILS_ELEMENT)
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
#endif

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

void writeDebugInfo(TextStream& ts, const RenderObject& object, RenderAsTextBehavior behavior)
{
    if (behavior & RenderAsTextShowIDAndClass) {
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

    if (behavior & RenderAsTextShowLayoutState) {
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

    if (behavior & RenderAsTextShowOverflow && is<RenderBox>(object)) {
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

static void writeTextRun(TextStream& ts, const RenderText& o, const InlineTextBox& run)
{
    // FIXME: For now use an "enclosingIntRect" model for x, y and logicalWidth, although this makes it harder
    // to detect any changes caused by the conversion to floating point. :(
    int x = run.x();
    int y = run.y();
    int logicalWidth = ceilf(run.left() + run.logicalWidth()) - x;

    // FIXME: Table cell adjustment is temporary until results can be updated.
    if (is<RenderTableCell>(*o.containingBlock()))
        y -= floorToInt(downcast<RenderTableCell>(*o.containingBlock()).intrinsicPaddingBefore());
        
    ts << "text run at (" << x << "," << y << ") width " << logicalWidth;
    if (!run.isLeftToRightDirection() || run.dirOverride()) {
        ts << (!run.isLeftToRightDirection() ? " RTL" : " LTR");
        if (run.dirOverride())
            ts << " override";
    }
    ts << ": "
        << quoteAndEscapeNonPrintables(String(o.text()).substring(run.start(), run.len()));
    if (run.hasHyphen())
        ts << " + hyphen string " << quoteAndEscapeNonPrintables(o.style().hyphenString().string());
    ts << "\n";
}

static void writeSimpleLine(TextStream& ts, const RenderText& o, const FloatRect& rect, StringView text)
{
    int x = rect.x();
    int y = rect.y();
    int logicalWidth = ceilf(rect.x() + rect.width()) - x;

    if (is<RenderTableCell>(*o.containingBlock()))
        y -= floorToInt(downcast<RenderTableCell>(*o.containingBlock()).intrinsicPaddingBefore());
        
    ts << "text run at (" << x << "," << y << ") width " << logicalWidth;
    ts << ": "
        << quoteAndEscapeNonPrintables(text);
    ts << "\n";
}

void write(TextStream& ts, const RenderObject& o, int indent, RenderAsTextBehavior behavior)
{
    if (is<RenderSVGShape>(o)) {
        write(ts, downcast<RenderSVGShape>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGGradientStop>(o)) {
        writeSVGGradientStop(ts, downcast<RenderSVGGradientStop>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGResourceContainer>(o)) {
        writeSVGResourceContainer(ts, downcast<RenderSVGResourceContainer>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGContainer>(o)) {
        writeSVGContainer(ts, downcast<RenderSVGContainer>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGRoot>(o)) {
        write(ts, downcast<RenderSVGRoot>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGText>(o)) {
        writeSVGText(ts, downcast<RenderSVGText>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGInlineText>(o)) {
        writeSVGInlineText(ts, downcast<RenderSVGInlineText>(o), indent, behavior);
        return;
    }
    if (is<RenderSVGImage>(o)) {
        writeSVGImage(ts, downcast<RenderSVGImage>(o), indent, behavior);
        return;
    }

    writeIndent(ts, indent);

    RenderTreeAsText::writeRenderObject(ts, o, behavior);
    ts << "\n";

    if (is<RenderText>(o)) {
        auto& text = downcast<RenderText>(o);
        if (auto layout = text.simpleLineLayout()) {
            ASSERT(!text.firstTextBox());
            auto resolver = runResolver(downcast<RenderBlockFlow>(*text.parent()), *layout);
            for (const auto& run : resolver.rangeForRenderer(text)) {
                writeIndent(ts, indent + 1);
                writeSimpleLine(ts, text, run.rect(), run.text());
            }
        } else {
            for (auto* box = text.firstTextBox(); box; box = box->nextTextBox()) {
                writeIndent(ts, indent + 1);
                writeTextRun(ts, text, *box);
            }
        }

    } else {
        for (auto& child : childrenOfType<RenderObject>(downcast<RenderElement>(o))) {
            if (child.hasLayer())
                continue;
            write(ts, child, indent + 1, behavior);
        }
    }

    if (is<RenderWidget>(o)) {
        Widget* widget = downcast<RenderWidget>(o).widget();
        if (is<FrameView>(widget)) {
            FrameView& view = downcast<FrameView>(*widget);
            if (RenderView* root = view.frame().contentRenderer()) {
                if (!(behavior & RenderAsTextDontUpdateLayout))
                    view.layout();
                if (RenderLayer* layer = root->layer())
                    writeLayers(ts, layer, layer, layer->rect(), indent + 1, behavior);
            }
        }
    }
}

enum LayerPaintPhase {
    LayerPaintPhaseAll = 0,
    LayerPaintPhaseBackground = -1,
    LayerPaintPhaseForeground = 1
};

static void write(TextStream& ts, const RenderLayer& layer, const LayoutRect& layerBounds, const LayoutRect& backgroundClipRect, const LayoutRect& clipRect,
    LayerPaintPhase paintPhase = LayerPaintPhaseAll, int indent = 0, RenderAsTextBehavior behavior = RenderAsTextBehaviorNormal)
{
    IntRect adjustedLayoutBounds = snappedIntRect(layerBounds);
    IntRect adjustedBackgroundClipRect = snappedIntRect(backgroundClipRect);
    IntRect adjustedClipRect = snappedIntRect(clipRect);

    writeIndent(ts, indent);

    ts << "layer ";
    
    if (behavior & RenderAsTextShowAddresses)
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

    if (behavior & RenderAsTextShowCompositedLayers) {
        if (layer.isComposited()) {
            ts << " (composited, bounds=" << layer.backing()->compositedBounds() << ", drawsContent=" << layer.backing()->graphicsLayer()->drawsContent()
                << ", paints into ancestor=" << layer.backing()->paintsIntoCompositedAncestor() << ")";
        }
    }

#if ENABLE(CSS_COMPOSITING)
    if (layer.isolatesBlending())
        ts << " isolatesBlending";
    if (layer.hasBlendMode())
        ts << " blendMode: " << compositeOperatorName(CompositeSourceOver, layer.blendMode());
#endif
    
    ts << "\n";

    if (paintPhase != LayerPaintPhaseBackground)
        write(ts, layer.renderer(), indent + 1, behavior);
}

static void writeRenderRegionList(const RenderRegionList& flowThreadRegionList, TextStream& ts, int indent)
{
    for (const auto& renderRegion : flowThreadRegionList) {
        writeIndent(ts, indent);
        ts << static_cast<const RenderObject*>(renderRegion)->renderName();

        Element* generatingElement = renderRegion->generatingElement();
        if (generatingElement) {
            bool isRenderNamedFlowFragment = is<RenderNamedFlowFragment>(*renderRegion);
            if (isRenderNamedFlowFragment && downcast<RenderNamedFlowFragment>(*renderRegion).hasCustomRegionStyle())
                ts << " region style: 1";
            if (renderRegion->hasAutoLogicalHeight())
                ts << " hasAutoLogicalHeight";

            if (isRenderNamedFlowFragment)
                ts << " (anonymous child of";

            StringBuilder tagName;
            tagName.append(generatingElement->nodeName());

            RenderElement* renderElementForRegion = isRenderNamedFlowFragment ? renderRegion->parent() : renderRegion;
            if (renderElementForRegion->isPseudoElement()) {
                if (renderElementForRegion->element()->isBeforePseudoElement())
                    tagName.appendLiteral("::before");
                else if (renderElementForRegion->element()->isAfterPseudoElement())
                    tagName.appendLiteral("::after");
            }

            ts << " {" << tagName.toString() << "}";

            auto& generatingElementId = generatingElement->idForStyleResolution();
            if (!generatingElementId.isNull())
                ts << " #" << generatingElementId;

            if (isRenderNamedFlowFragment)
                ts << ")";
        }

        ts << "\n";
    }
}

static void writeRenderNamedFlowThreads(TextStream& ts, RenderView& renderView, const RenderLayer* rootLayer,
    const LayoutRect& paintRect, int indent, RenderAsTextBehavior behavior)
{
    if (!renderView.hasRenderNamedFlowThreads())
        return;

    const RenderNamedFlowThreadList* list = renderView.flowThreadController().renderNamedFlowThreadList();

    writeIndent(ts, indent);
    ts << "Named flows\n";

    for (RenderNamedFlowThreadList::const_iterator iter = list->begin(); iter != list->end(); ++iter) {
        const RenderNamedFlowThread* renderFlowThread = *iter;

        writeIndent(ts, indent + 1);
        ts << "Named flow '" << renderFlowThread->flowThreadName() << "'\n";

        RenderLayer* layer = renderFlowThread->layer();
        writeLayers(ts, rootLayer, layer, paintRect, indent + 2, behavior);

        // Display the valid and invalid render regions attached to this flow thread.
        const RenderRegionList& validRegionsList = renderFlowThread->renderRegionList();
        const RenderRegionList& invalidRegionsList = renderFlowThread->invalidRenderRegionList();
        if (!validRegionsList.isEmpty()) {
            writeIndent(ts, indent + 2);
            ts << "Regions for named flow '" << renderFlowThread->flowThreadName() << "'\n";
            writeRenderRegionList(validRegionsList, ts, indent + 3);
        }
        if (!invalidRegionsList.isEmpty()) {
            writeIndent(ts, indent + 2);
            ts << "Invalid regions for named flow '" << renderFlowThread->flowThreadName() << "'\n";
            writeRenderRegionList(invalidRegionsList, ts, indent + 3);
        }
    }
}

static LayoutSize maxLayoutOverflow(const RenderBox* box)
{
    LayoutRect overflowRect = box->layoutOverflowRect();
    return LayoutSize(overflowRect.maxX(), overflowRect.maxY());
}

static void writeLayers(TextStream& ts, const RenderLayer* rootLayer, RenderLayer* l,
                        const LayoutRect& paintRect, int indent, RenderAsTextBehavior behavior)
{
    // FIXME: Apply overflow to the root layer to not break every test.  Complete hack.  Sigh.
    LayoutRect paintDirtyRect(paintRect);
    if (rootLayer == l) {
        paintDirtyRect.setWidth(std::max<LayoutUnit>(paintDirtyRect.width(), rootLayer->renderBox()->layoutOverflowRect().maxX()));
        paintDirtyRect.setHeight(std::max<LayoutUnit>(paintDirtyRect.height(), rootLayer->renderBox()->layoutOverflowRect().maxY()));
        l->setSize(l->size().expandedTo(snappedIntSize(maxLayoutOverflow(l->renderBox()), LayoutPoint(0, 0))));
    }
    
    // Calculate the clip rects we should use.
    LayoutRect layerBounds;
    ClipRect damageRect;
    ClipRect clipRectToApply;
    l->calculateRects(RenderLayer::ClipRectsContext(rootLayer, TemporaryClipRects), paintDirtyRect, layerBounds, damageRect, clipRectToApply, l->offsetFromAncestor(rootLayer));

    // Ensure our lists are up-to-date.
    l->updateLayerListsIfNeeded();

    bool shouldPaint = (behavior & RenderAsTextShowAllLayers) ? true : l->intersectsDamageRect(layerBounds, damageRect.rect(), rootLayer, l->offsetFromAncestor(rootLayer));
    Vector<RenderLayer*>* negList = l->negZOrderList();
    bool paintsBackgroundSeparately = negList && negList->size() > 0;
    if (shouldPaint && paintsBackgroundSeparately)
        write(ts, *l, layerBounds, damageRect.rect(), clipRectToApply.rect(), LayerPaintPhaseBackground, indent, behavior);

    if (negList) {
        int currIndent = indent;
        if (behavior & RenderAsTextShowLayerNesting) {
            writeIndent(ts, indent);
            ts << " negative z-order list(" << negList->size() << ")\n";
            ++currIndent;
        }
        for (unsigned i = 0; i != negList->size(); ++i)
            writeLayers(ts, rootLayer, negList->at(i), paintDirtyRect, currIndent, behavior);
    }

    if (shouldPaint)
        write(ts, *l, layerBounds, damageRect.rect(), clipRectToApply.rect(), paintsBackgroundSeparately ? LayerPaintPhaseForeground : LayerPaintPhaseAll, indent, behavior);

    if (Vector<RenderLayer*>* normalFlowList = l->normalFlowList()) {
        int currIndent = indent;
        if (behavior & RenderAsTextShowLayerNesting) {
            writeIndent(ts, indent);
            ts << " normal flow list(" << normalFlowList->size() << ")\n";
            ++currIndent;
        }
        for (unsigned i = 0; i != normalFlowList->size(); ++i)
            writeLayers(ts, rootLayer, normalFlowList->at(i), paintDirtyRect, currIndent, behavior);
    }

    if (Vector<RenderLayer*>* posList = l->posZOrderList()) {
        size_t layerCount = 0;
        for (unsigned i = 0; i != posList->size(); ++i)
            if (!posList->at(i)->isFlowThreadCollectingGraphicsLayersUnderRegions())
                ++layerCount;
        if (layerCount) {
            int currIndent = indent;
            // We only print the header if there's at list a non-RenderNamedFlowThread part of the list.
            if (!posList->size() || !posList->at(0)->isFlowThreadCollectingGraphicsLayersUnderRegions()) {
                if (behavior & RenderAsTextShowLayerNesting) {
                    writeIndent(ts, indent);
                    ts << " positive z-order list(" << posList->size() << ")\n";
                    ++currIndent;
                }
                for (unsigned i = 0; i != posList->size(); ++i) {
                    // Do not print named flows twice.
                    if (!posList->at(i)->isFlowThreadCollectingGraphicsLayersUnderRegions())
                        writeLayers(ts, rootLayer, posList->at(i), paintDirtyRect, currIndent, behavior);
                }
            }
        }
    }
    
    // Altough the RenderFlowThread requires a layer, it is not collected by its parent,
    // so we have to treat it as a special case.
    if (is<RenderView>(l->renderer()))
        writeRenderNamedFlowThreads(ts, downcast<RenderView>(l->renderer()), rootLayer, paintDirtyRect, indent, behavior);
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

static void writeSelection(TextStream& ts, const RenderObject* renderer)
{
    if (!renderer->isRenderView())
        return;

    Frame* frame = renderer->document().frame();
    if (!frame)
        return;

    VisibleSelection selection = frame->selection().selection();
    if (selection.isCaret()) {
        ts << "caret: position " << selection.start().deprecatedEditingOffset() << " of " << nodePosition(selection.start().deprecatedNode());
        if (selection.affinity() == UPSTREAM)
            ts << " (upstream affinity)";
        ts << "\n";
    } else if (selection.isRange())
        ts << "selection start: position " << selection.start().deprecatedEditingOffset() << " of " << nodePosition(selection.start().deprecatedNode()) << "\n"
           << "selection end:   position " << selection.end().deprecatedEditingOffset() << " of " << nodePosition(selection.end().deprecatedNode()) << "\n";
}

static String externalRepresentation(RenderBox* renderer, RenderAsTextBehavior behavior)
{
    TextStream ts;
    if (!renderer->hasLayer())
        return ts.release();
        
    RenderLayer* layer = renderer->layer();
    writeLayers(ts, layer, layer, layer->rect(), 0, behavior);
    writeSelection(ts, renderer);
    return ts.release();
}

String externalRepresentation(Frame* frame, RenderAsTextBehavior behavior)
{
    RenderView* renderer = frame->contentRenderer();
    if (!renderer)
        return String();

    PrintContext printContext(frame);
    if (behavior & RenderAsTextPrintingMode)
        printContext.begin(renderer->width());
    if (!(behavior & RenderAsTextDontUpdateLayout))
        frame->document()->updateLayout();

    return externalRepresentation(renderer, behavior);
}

String externalRepresentation(Element* element, RenderAsTextBehavior behavior)
{
    RenderElement* renderer = element->renderer();
    if (!is<RenderBox>(renderer))
        return String();
    // Doesn't support printing mode.
    ASSERT(!(behavior & RenderAsTextPrintingMode));
    if (!(behavior & RenderAsTextDontUpdateLayout))
        element->document().updateLayout();
    
    return externalRepresentation(downcast<RenderBox>(renderer), behavior | RenderAsTextShowAllLayers);
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
    TextStream stream;
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
