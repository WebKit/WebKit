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
#include "RenderSVGInlineText.h"
#include "RenderText.h"
#include "SVGElementInlines.h"
#include "SVGNames.h"
#include "ScopedEventQueue.h"
#include "SerializedNode.h"
#include "ShadowRoot.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "StyleResolver.h"
#include "StyleUpdate.h"
#include "TextManipulationController.h"
#include "TextNodeTraversal.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Text);

Ref<Text> Text::createEditingText(Document& document, RefPtr<StringImpl>&& data)
{
    auto node = adoptRef(*new Text(document, WTF::move(data), NodeType::Text, { TypeFlag::IsPseudoElementOrSpecialInternalNode }));
    ASSERT(node->isEditingText());
    return node;
}

Text::~Text() = default;

RenderText* Text::renderer() const
{
    return downcast<RenderText>(Node::renderer());
}

ExceptionOr<Ref<Text>> Text::splitText(unsigned offset)
{
    if (offset > length())
        return Exception { ExceptionCode::IndexSizeError };

    EventQueueScope scope;
    auto oldData = data();
    Ref newText = virtualCreate(substring(oldData, offset));
    setDataWithoutUpdate(left(oldData, offset));

    dispatchModifiedEvent(oldData);

    if (RefPtr parent = parentNode()) {
        auto insertResult = parent->insertBefore(newText, protect(nextSibling()));
        if (insertResult.hasException())
            return insertResult.releaseException();
    }

    protect(document())->textNodeSplit(*this);

    updateRendererAfterContentChange(0, WTF::length(oldData));

    return newText;
}

NODELETE static const Text* earliestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->previousSibling())) {
        if (auto* maybeText = dynamicDowncast<Text>(*node))
            text = maybeText;
        else
            break;
    }
    return text;
}

NODELETE static const Text* latestLogicallyAdjacentTextNode(const Text* text)
{
    const Node* node = text;
    while ((node = node->nextSibling())) {
        if (auto* maybeText = dynamicDowncast<Text>(*node))
            text = maybeText;
        else
            break;
    }
    return text;
}

RefPtr<StringImpl> Text::wholeText() const
{
    RefPtr startText = earliestLogicallyAdjacentTextNode(this);
    RefPtr endText = latestLogicallyAdjacentTextNode(this);
    ASSERT(endText);
    RefPtr<const Node> onePastEndText = TextNodeTraversal::nextSibling(*endText);

    StringBuilder result;
    for (RefPtr text = startText; text != onePastEndText; text = TextNodeTraversal::nextSibling(*text))
        result.append(text->data());
    return result.toString();
}

void Text::replaceWholeText(StringImpl* newText)
{
    // Protect startText and endText against mutation event handlers removing the last ref
    RefPtr startText = const_cast<Text*>(earliestLogicallyAdjacentTextNode(this));
    RefPtr endText = const_cast<Text*>(latestLogicallyAdjacentTextNode(this));

    RefPtr parent = parentNode(); // Protect against mutation handlers moving this node during traversal
    for (RefPtr<Node> node = WTF::move(startText); is<Text>(node) && node != this && node->parentNode() == parent;) {
        Ref nodeToRemove = node.releaseNonNull();
        node = nodeToRemove->nextSibling();
        parent->removeChild(nodeToRemove);
    }

    if (this != endText) {
        RefPtr nodePastEndText = endText->nextSibling();
        for (RefPtr node = nextSibling(); is<Text>(node) && node != nodePastEndText && node->parentNode() == parent;) {
            Ref nodeToRemove = node.releaseNonNull();
            node = nodeToRemove->nextSibling();
            parent->removeChild(nodeToRemove);
        }
    }

    if (isEmpty(newText)) {
        if (parent && parentNode() == parent)
            parent->removeChild(*this);
        return;
    }

    setData(newText);
}

RefPtr<StringImpl> Text::nodeName() const
{
    return "#text"_si;
}

Ref<Node> Text::cloneNodeInternal(Document& document, CloningOperation, CustomElementRegistry*) const
{
    return create(document, RefPtr<StringImpl> { data() });
}

SerializedNode Text::serializeNode(CloningOperation) const
{
    return { SerializedNode::Text { data() } };
}

NODELETE static bool isSVGShadowText(const Text& text)
{
    ASSERT(text.parentNode());
    auto* parentShadowRoot = dynamicDowncast<ShadowRoot>(*text.parentNode());
    return parentShadowRoot && parentShadowRoot->host()->hasTagName(SVGNames::trefTag);
}

NODELETE static bool isSVGText(const Text& text)
{
    ASSERT(text.parentNode());
    auto* parentElement = dynamicDowncast<SVGElement>(*text.parentNode());
    return parentElement && !parentElement->hasTagName(SVGNames::foreignObjectTag);
}

RenderPtr<RenderText> Text::createTextRenderer()
{
    if (isSVGText(*this) || isSVGShadowText(*this))
        return createRenderer<RenderSVGInlineText>(*this, data());

    return createRenderer<RenderText>(RenderObject::Type::Text, *this, data());
}

Ref<Text> Text::virtualCreate(RefPtr<StringImpl>&& data)
{
    return create(protect(document()), WTF::move(data));
}

void Text::updateRendererAfterContentChange(unsigned offsetOfReplacedData, unsigned lengthOfReplacedData)
{
    if (!isConnected())
        return;

    if (hasInvalidRenderer())
        return;

    protect(document())->updateTextRenderer(*this, offsetOfReplacedData, lengthOfReplacedData);
}

static void appendTextRepresentation(StringBuilder& builder, const Text& text)
{
    RefPtr<StringImpl> value = text.data();
    builder.append(" length="_s, length(value));

    value = makeStringByReplacingAll(value, '\\', "\\\\"_s);
    value = makeStringByReplacingAll(value, '\n', "\\n"_s);
    
    constexpr size_t maxDumpLength = 30;
    if (length(value) > maxDumpLength)
        builder.append(" \""_s, StringView(value).left(maxDumpLength - 10), "...\""_s);
    else
        builder.append(" \""_s, value, '\"');
}

RefPtr<StringImpl> Text::description() const
{
    StringBuilder builder;

    builder.append(CharacterData::description());
    appendTextRepresentation(builder, *this);

    return builder.toString();
}

RefPtr<StringImpl> Text::debugDescription() const
{
    StringBuilder builder;

    builder.append(CharacterData::debugDescription());
    appendTextRepresentation(builder, *this);

    return builder.toString();
}

void Text::setDataAndUpdate(StringImpl* newData, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, UpdateLiveRanges updateLiveRanges)
{
    auto oldData = data();
    CharacterData::setDataAndUpdate(newData, offsetOfReplacedData, oldLength, newLength, updateLiveRanges);

    // FIXME: Does not seem correct to do this for 0 offset only.
    if (!offsetOfReplacedData) {
        Ref document = this->document();
        CheckedPtr textManipulationController = document->textManipulationControllerIfExists();
        if (textManipulationController && !equal(oldData, newData)) [[unlikely]]
            textManipulationController->didUpdateContentForNode(*this);
    }
}

} // namespace WebCore
