/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SimpleLineLayoutCoverage.h"

#include "Logging.h"
#include "RenderBlockFlow.h"
#include "RenderChildIterator.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "Settings.h"
#include "SimpleLineLayout.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace SimpleLineLayout {

#ifndef NDEBUG
static void printReason(AvoidanceReason reason, TextStream& stream)
{
    switch (reason) {
    case FlowIsInsideANonMultiColumnThread:
        stream << "flow is inside a non-multicolumn container";
        break;
    case FlowHasHorizonalWritingMode:
        stream << "horizontal writing mode";
        break;
    case FlowHasOutline:
        stream << "outline";
        break;
    case FlowIsRuby:
        stream << "ruby";
        break;
    case FlowHasHangingPunctuation:
        stream << "hanging punctuation";
        break;
    case FlowIsPaginated:
        stream << "paginated";
        break;
    case FlowHasTextOverflow:
        stream << "text-overflow";
        break;
    case FlowIsDepricatedFlexBox:
        stream << "depricatedFlexBox";
        break;
    case FlowParentIsPlaceholderElement:
        stream << "placeholder element";
        break;
    case FlowParentIsTextAreaWithWrapping:
        stream << "wrapping textarea";
        break;
    case FlowHasNonSupportedChild:
        stream << "nested renderers";
        break;
    case FlowHasUnsupportedFloat:
        stream << "complicated float";
        break;
    case FlowHasUnsupportedUnderlineDecoration:
        stream << "text-underline-position: under";
        break;
    case FlowHasJustifiedNonLatinText:
        stream << "text-align: justify with non-latin text";
        break;
    case FlowHasOverflowNotVisible:
        stream << "overflow: hidden | scroll | auto";
        break;
    case FlowHasWebKitNBSPMode:
        stream << "-webkit-nbsp-mode: space";
        break;
    case FlowIsNotLTR:
        stream << "dir is not LTR";
        break;
    case FlowHasLineBoxContainProperty:
        stream << "line-box-contain value indicates variable line height";
        break;
    case FlowIsNotTopToBottom:
        stream << "non top-to-bottom flow";
        break;
    case FlowHasLineBreak:
        stream << "line-break property";
        break;
    case FlowHasNonNormalUnicodeBiDi:
        stream << "non-normal Unicode bidi";
        break;
    case FlowHasRTLOrdering:
        stream << "-webkit-rtl-ordering";
        break;
    case FlowHasLineAlignEdges:
        stream << "-webkit-line-align edges";
        break;
    case FlowHasLineSnap:
        stream << "-webkit-line-snap property";
        break;
    case FlowHasTextEmphasisFillOrMark:
        stream << "text-emphasis (fill/mark)";
        break;
    case FlowHasPseudoFirstLine:
        stream << "first-line";
        break;
    case FlowHasPseudoFirstLetter:
        stream << "first-letter";
        break;
    case FlowHasTextCombine:
        stream << "text combine";
        break;
    case FlowHasTextFillBox:
        stream << "background-color (text-fill)";
        break;
    case FlowHasBorderFitLines:
        stream << "-webkit-border-fit";
        break;
    case FlowHasNonAutoLineBreak:
        stream << "line-break is not auto";
        break;
    case FlowHasSVGFont:
        stream << "SVG font";
        break;
    case FlowTextHasSoftHyphen:
        stream << "soft hyphen character";
        break;
    case FlowTextHasDirectionCharacter:
        stream << "direction character";
        break;
    case FlowIsMissingPrimaryFont:
        stream << "missing primary font";
        break;
    case FlowPrimaryFontIsInsufficient:
        stream << "missing glyph or glyph needs another font";
        break;
    case FlowTextIsCombineText:
        stream << "text is combine";
        break;
    case FlowTextIsRenderCounter:
        stream << "unsupported RenderCounter";
        break;
    case FlowTextIsRenderQuote:
        stream << "unsupported RenderQuote";
        break;
    case FlowTextIsTextFragment:
        stream << "unsupported TextFragment";
        break;
    case FlowTextIsSVGInlineText:
        stream << "unsupported SVGInlineText";
        break;
    case FlowHasComplexFontCodePath:
        stream << "text with complex font codepath";
        break;
    case FlowHasTextShadow:
        stream << "text-shadow";
        break;
    case FlowChildIsSelected:
        stream << "selected content";
        break;
    case FlowFontHasOverflowGlyph:
        stream << "-webkit-line-box-contain: glyphs with overflowing text.";
        break;
    case FlowTextHasSurrogatePair:
        stream << "surrogate pair";
        break;
    case MultiColumnFlowIsNotTopLevel:
        stream << "non top level column";
        break;
    case MultiColumnFlowHasColumnSpanner:
        stream << "column has spanner";
        break;
    case MultiColumnFlowVerticalAlign:
        stream << "column with vertical-align != baseline";
        break;
    case MultiColumnFlowIsFloating:
        stream << "column with floating objects";
        break;
    case FlowIncludesDocumentMarkers:
        stream << "text includes document markers";
        break;
    case FlowTextIsEmpty:
    case FlowHasNoChild:
    case FlowHasNoParent:
    case FeatureIsDisabled:
    default:
        break;
    }
}

static void printReasons(AvoidanceReasonFlags reasons, TextStream& stream)
{
    bool first = true;
    for (auto reasonItem = EndOfReasons >> 1; reasonItem != NoReason; reasonItem >>= 1) {
        if (!(reasons & reasonItem))
            continue;
        stream << (first ? " " : ", ");
        first = false;
        printReason(reasonItem, stream);
    }
}

static void printTextForSubtree(const RenderObject& renderer, unsigned& charactersLeft, TextStream& stream)
{
    if (!charactersLeft)
        return;
    if (is<RenderText>(renderer)) {
        String text = downcast<RenderText>(renderer).text();
        text = text.stripWhiteSpace();
        unsigned len = std::min(charactersLeft, text.length());
        stream << text.left(len);
        charactersLeft -= len;
        return;
    }
    if (!is<RenderElement>(renderer))
        return;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        printTextForSubtree(*child, charactersLeft, stream);
}

static unsigned textLengthForSubtree(const RenderObject& renderer)
{
    if (is<RenderText>(renderer))
        return downcast<RenderText>(renderer).text().length();
    if (!is<RenderElement>(renderer))
        return 0;
    unsigned textLength = 0;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        textLength += textLengthForSubtree(*child);
    return textLength;
}

static void collectNonEmptyLeafRenderBlockFlows(const RenderObject& renderer, HashSet<const RenderBlockFlow*>& leafRenderers)
{
    if (is<RenderText>(renderer)) {
        if (!downcast<RenderText>(renderer).text().length())
            return;
        // Find RenderBlockFlow ancestor.
        for (const auto* current = renderer.parent(); current; current = current->parent()) {
            if (!is<RenderBlockFlow>(current))
                continue;
            leafRenderers.add(downcast<RenderBlockFlow>(current));
            break;
        }
        return;
    }
    if (!is<RenderElement>(renderer))
        return;
    for (const auto* child = downcast<RenderElement>(renderer).firstChild(); child; child = child->nextSibling())
        collectNonEmptyLeafRenderBlockFlows(*child, leafRenderers);
}

static void collectNonEmptyLeafRenderBlockFlowsForCurrentPage(HashSet<const RenderBlockFlow*>& leafRenderers)
{
    for (const auto* document : Document::allDocuments()) {
        if (!document->renderView() || document->backForwardCacheState() != Document::NotInBackForwardCache)
            continue;
        if (!document->isHTMLDocument() && !document->isXHTMLDocument())
            continue;
        collectNonEmptyLeafRenderBlockFlows(*document->renderView(), leafRenderers);
    }
}

void toggleSimpleLineLayout()
{
    for (auto* document : Document::allDocuments()) {
        auto& settings = document->mutableSettings();
        settings.setSimpleLineLayoutEnabled(!settings.simpleLineLayoutEnabled());
    }
}

void printSimpleLineLayoutBlockList()
{
    HashSet<const RenderBlockFlow*> leafRenderers;
    collectNonEmptyLeafRenderBlockFlowsForCurrentPage(leafRenderers);
    if (!leafRenderers.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    stream << "---------------------------------------------------\n";
    for (const auto* flow : leafRenderers) {
        auto reason = canUseForWithReason(*flow, IncludeReasons::All);
        if (reason == NoReason)
            continue;
        unsigned printedLength = 30;
        stream << "\"";
        printTextForSubtree(*flow, printedLength, stream);
        for (;printedLength > 0; --printedLength)
            stream << " ";
        stream << "\"(" << textLengthForSubtree(*flow) << "):";
        printReasons(reason, stream);
        stream << "\n";
    }
    stream << "---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}

void printSimpleLineLayoutCoverage()
{
    HashSet<const RenderBlockFlow*> leafRenderers;
    collectNonEmptyLeafRenderBlockFlowsForCurrentPage(leafRenderers);
    if (!leafRenderers.size()) {
        WTFLogAlways("No text found in this document\n");
        return;
    }
    TextStream stream;
    HashMap<AvoidanceReason, unsigned> flowStatistics;
    unsigned textLength = 0;
    unsigned unsupportedTextLength = 0;
    unsigned numberOfUnsupportedLeafBlocks = 0;
    unsigned supportedButForcedToLineLayoutTextLength = 0;
    unsigned numberOfSupportedButForcedToLineLayoutLeafBlocks = 0;
    for (const auto* flow : leafRenderers) {
        auto flowLength = textLengthForSubtree(*flow);
        textLength += flowLength;
        auto reasons = canUseForWithReason(*flow, IncludeReasons::All);
        if (reasons == NoReason) {
            if (flow->lineLayoutPath() == RenderBlockFlow::ForceLineBoxesPath) {
                supportedButForcedToLineLayoutTextLength += flowLength;
                ++numberOfSupportedButForcedToLineLayoutLeafBlocks;
            }
            continue;
        }
        ++numberOfUnsupportedLeafBlocks;
        unsupportedTextLength += flowLength;
        for (auto reasonItem = EndOfReasons >> 1; reasonItem != NoReason; reasonItem >>= 1) {
            if (!(reasons & reasonItem))
                continue;
            auto result = flowStatistics.add(reasonItem, flowLength);
            if (!result.isNewEntry)
                result.iterator->value += flowLength;
        }
    }
    stream << "---------------------------------------------------\n";
    stream << "Number of blocks: total(" <<  leafRenderers.size() << ") non-simple(" << numberOfUnsupportedLeafBlocks << ")\nContent length: total(" <<
        textLength << ") non-simple(" << unsupportedTextLength << ")\n";
    for (const auto& reasonEntry : flowStatistics) {
        printReason(reasonEntry.key, stream);
        stream << ": " << (float)reasonEntry.value / (float)textLength * 100 << "%\n";
    }
    if (supportedButForcedToLineLayoutTextLength) {
        stream << "Simple line layout potential coverage: " << (float)(textLength - unsupportedTextLength) / (float)textLength * 100 << "%\n\n";
        stream << "Simple line layout actual coverage: " << (float)(textLength - unsupportedTextLength - supportedButForcedToLineLayoutTextLength) / (float)textLength * 100 << "%\nForced line layout blocks: " << numberOfSupportedButForcedToLineLayoutLeafBlocks << " content length: " << supportedButForcedToLineLayoutTextLength << "(" << (float)supportedButForcedToLineLayoutTextLength / (float)textLength * 100 << "%)";
    } else
        stream << "Simple line layout coverage: " << (float)(textLength - unsupportedTextLength) / (float)textLength * 100 << "%";
    stream << "\n---------------------------------------------------\n";
    WTFLogAlways("%s", stream.release().utf8().data());
}
#endif

}
}
