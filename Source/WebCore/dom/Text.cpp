/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003-2018 Apple Inc. All rights reserved.
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
#include "Text.h"

#include "Event.h"
#include "RenderCombineText.h"
#include "RenderSVGInlineText.h"
#include "RenderText.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleUpdate.h"
#include "TextManipulationController.h"
#include "TextNodeTraversal.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Text);

Ref<Text> Text::create(Document& document, String&& data)
{
    return adoptRef(*new Text(document, WTFMove(data), CreateText));
}

Ref<Text> Text::createEditingText(Document& document, String&& data)
{
    return adoptRef(*new Text(document, WTFMove(data), CreateEditingText));
}

Text::~Text() = default;

ExceptionOr<Ref<Text>> Text::splitText(unsigned offset)
{
    if (offset > length())
        return Exception { IndexSizeError };

    EventQueueScope scope;
    auto oldData = data();
    auto newText = virtualCreate(oldData.substring(offset));
    setDataWithoutUpdate(oldData.left(offset));

    dispatchModifiedEvent(oldData);

    if (auto* parent = parentNode()) {
        auto insertResult = parent->insertBefore(newText, nextSibling());
        if (insertResult.hasException())
            return insertResult.releaseException();
    }

    document().textNodeSplit(*this);

    updateRendererAfterContentChange(0, oldData.length());

    return newText;
}

static const Text* earliestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->previousSibling())) {
        if (!is<Text>(*node))
            break;
        text = downcast<Text>(node);
    }
    return text;
}

static const Text* latestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->nextSibling())) {
        if (!is<Text>(*node))
            break;
        text = downcast<Text>(node);
    }
    return text;
}

String Text::wholeText() const
{
    const Text* startText = earliestLogicallyAdjacentTextNode(this);
    const Text* endText = latestLogicallyAdjacentTextNode(this);
    ASSERT(endText);
    const Node* onePastEndText = TextNodeTraversal::nextSibling(*endText);

    StringBuilder result;
    for (const Text* text = startText; text != onePastEndText; text = TextNodeTraversal::nextSibling(*text))
        result.append(text->data());
    return result.toString();
}

void Text::replaceWholeText(const String& newText)
{
    // Protect startText and endText against mutation event handlers removing the last ref
    RefPtr startText = const_cast<Text*>(earliestLogicallyAdjacentTextNode(this));
    RefPtr endText = const_cast<Text*>(latestLogicallyAdjacentTextNode(this));

    RefPtr parent = parentNode(); // Protect against mutation handlers moving this node during traversal
    for (RefPtr<Node> node = WTFMove(startText); is<Text>(node) && node != this && node->parentNode() == parent;) {
        auto nodeToRemove = node.releaseNonNull();
        node = nodeToRemove->nextSibling();
        parent->removeChild(nodeToRemove);
    }

    if (this != endText) {
        RefPtr nodePastEndText = endText->nextSibling();
        for (RefPtr node = nextSibling(); is<Text>(node) && node != nodePastEndText && node->parentNode() == parent;) {
            auto nodeToRemove = node.releaseNonNull();
            node = nodeToRemove->nextSibling();
            parent->removeChild(nodeToRemove);
        }
    }

    if (newText.isEmpty()) {
        if (parent && parentNode() == parent)
            parent->removeChild(*this);
        return;
    }

    setData(newText);
}

String Text::nodeName() const
{
    return "#text"_s;
}

Node::NodeType Text::nodeType() const
{
    return TEXT_NODE;
}

Ref<Node> Text::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    return create(targetDocument, String { data() });
}

static bool isSVGShadowText(Text* text)
{
    Node* parentNode = text->parentNode();
    ASSERT(parentNode);
    return is<ShadowRoot>(*parentNode) && downcast<ShadowRoot>(*parentNode).host()->hasTagName(SVGNames::trefTag);
}

static bool isSVGText(Text* text)
{
    Node* parentOrShadowHostNode = text->parentOrShadowHostNode();
    return parentOrShadowHostNode->isSVGElement() && !parentOrShadowHostNode->hasTagName(SVGNames::foreignObjectTag);
}

RenderPtr<RenderText> Text::createTextRenderer(const RenderStyle& style)
{
    if (isSVGText(this) || isSVGShadowText(this))
        return createRenderer<RenderSVGInlineText>(*this, data());

    if (style.hasTextCombine())
        return createRenderer<RenderCombineText>(*this, data());

    return createRenderer<RenderText>(*this, data());
}

Ref<Text> Text::virtualCreate(String&& data)
{
    return create(document(), WTFMove(data));
}

void Text::updateRendererAfterContentChange(unsigned offsetOfReplacedData, unsigned lengthOfReplacedData)
{
    if (!isConnected())
        return;

    if (styleValidity() >= Style::Validity::SubtreeAndRenderersInvalid)
        return;

    document().updateTextRenderer(*this, offsetOfReplacedData, lengthOfReplacedData);
}

static void appendTextRepresentation(StringBuilder& builder, const Text& text)
{
    String value = text.data();
    builder.append(" length="_s, value.length());

    value = makeStringByReplacingAll(value, '\\', "\\\\"_s);
    value = makeStringByReplacingAll(value, '\n', "\\n"_s);
    
    constexpr size_t maxDumpLength = 30;
    if (value.length() > maxDumpLength)
        builder.append(" \"", StringView(value).left(maxDumpLength - 10), "...\"");
    else
        builder.append(" \"", value, '\"');
}

String Text::description() const
{
    StringBuilder builder;

    builder.append(CharacterData::description());
    appendTextRepresentation(builder, *this);

    return builder.toString();
}

String Text::debugDescription() const
{
    StringBuilder builder;

    builder.append(CharacterData::debugDescription());
    appendTextRepresentation(builder, *this);

    return builder.toString();
}

void Text::setDataAndUpdate(const String& newData, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, UpdateLiveRanges updateLiveRanges)
{
    auto oldData = data();
    CharacterData::setDataAndUpdate(newData, offsetOfReplacedData, oldLength, newLength, updateLiveRanges);

    // FIXME: Does not seem correct to do this for 0 offset only.
    if (!offsetOfReplacedData) {
        auto* textManipulationController = document().textManipulationControllerIfExists();
        if (UNLIKELY(textManipulationController && oldData != newData))
            textManipulationController->didUpdateContentForNode(*this);
    }
}

} // namespace WebCore
