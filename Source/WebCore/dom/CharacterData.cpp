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
#include "CharacterData.h"

#include "Attr.h"
#include "ElementTraversal.h"
#include "EventNames.h"
#include "FrameSelection.h"
#include "InspectorInstrumentation.h"
#include "MutationEvent.h"
#include "MutationObserverInterestGroup.h"
#include "MutationRecord.h"
#include "ProcessingInstruction.h"
#include "RenderText.h"
#include "StyleInheritedData.h"
#include <unicode/ubrk.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/Ref.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(CharacterData);

static bool canUseSetDataOptimization(const CharacterData& node)
{
    auto& document = node.document();
    return !document.hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER) && !document.hasMutationObserversOfType(MutationObserver::CharacterData)
        && !document.hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER);
}

void CharacterData::setData(const String& data)
{
    const String& nonNullData = !data.isNull() ? data : emptyString();
    unsigned oldLength = length();

    if (m_data == nonNullData && canUseSetDataOptimization(*this)) {
        document().textRemoved(*this, 0, oldLength);
        if (document().frame())
            document().frame()->selection().textWasReplaced(this, 0, oldLength, oldLength);
        return;
    }

    Ref<CharacterData> protectedThis(*this);

    setDataAndUpdate(nonNullData, 0, oldLength, nonNullData.length());
}

ExceptionOr<String> CharacterData::substringData(unsigned offset, unsigned count)
{
    if (offset > length())
        return Exception { IndexSizeError };

    return m_data.substring(offset, count);
}

unsigned CharacterData::parserAppendData(const String& string, unsigned offset, unsigned lengthLimit)
{
    unsigned oldLength = m_data.length();

    ASSERT(lengthLimit >= oldLength);

    unsigned characterLength = string.length() - offset;
    unsigned characterLengthLimit = std::min(characterLength, lengthLimit - oldLength);

    // Check that we are not on an unbreakable boundary.
    // Some text break iterator implementations work best if the passed buffer is as small as possible,
    // see <https://bugs.webkit.org/show_bug.cgi?id=29092>.
    // We need at least two characters look-ahead to account for UTF-16 surrogates.
    if (characterLengthLimit < characterLength) {
        NonSharedCharacterBreakIterator it(StringView(string).substring(offset, (characterLengthLimit + 2 > characterLength) ? characterLength : characterLengthLimit + 2));
        if (!ubrk_isBoundary(it, characterLengthLimit))
            characterLengthLimit = ubrk_preceding(it, characterLengthLimit);
    }

    if (!characterLengthLimit)
        return 0;

    String oldData = m_data;
    if (string.is8Bit())
        m_data.append(string.characters8() + offset, characterLengthLimit);
    else
        m_data.append(string.characters16() + offset, characterLengthLimit);

    ASSERT(!renderer() || is<Text>(*this));
    if (is<Text>(*this))
        downcast<Text>(*this).updateRendererAfterContentChange(oldLength, 0);

    notifyParentAfterChange(ContainerNode::ChildChange::Source::Parser);

    auto mutationRecipients = MutationObserverInterestGroup::createForCharacterDataMutation(*this);
    if (UNLIKELY(mutationRecipients))
        mutationRecipients->enqueueMutationRecord(MutationRecord::createCharacterData(*this, oldData));

    return characterLengthLimit;
}

void CharacterData::appendData(const String& data)
{
    setDataAndUpdate(m_data + data, m_data.length(), 0, data.length(), UpdateLiveRanges::No);
}

ExceptionOr<void> CharacterData::insertData(unsigned offset, const String& data)
{
    if (offset > length())
        return Exception { IndexSizeError };

    String newData = m_data;
    newData.insert(data, offset);
    setDataAndUpdate(newData, offset, 0, data.length());

    return { };
}

ExceptionOr<void> CharacterData::deleteData(unsigned offset, unsigned count)
{
    if (offset > length())
        return Exception { IndexSizeError };

    count = std::min(count, length() - offset);

    String newData = m_data;
    newData.remove(offset, count);
    setDataAndUpdate(newData, offset, count, 0);

    return { };
}

ExceptionOr<void> CharacterData::replaceData(unsigned offset, unsigned count, const String& data)
{
    if (offset > length())
        return Exception { IndexSizeError };

    count = std::min(count, length() - offset);

    String newData = m_data;
    newData.remove(offset, count);
    newData.insert(data, offset);
    setDataAndUpdate(newData, offset, count, data.length());

    return { };
}

String CharacterData::nodeValue() const
{
    return m_data;
}

ExceptionOr<void> CharacterData::setNodeValue(const String& nodeValue)
{
    setData(nodeValue);
    return { };
}

void CharacterData::setDataAndUpdate(const String& newData, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength, UpdateLiveRanges shouldUpdateLiveRanges)
{
    String oldData = m_data;
    m_data = newData;

    if (oldLength && shouldUpdateLiveRanges != UpdateLiveRanges::No)
        document().textRemoved(*this, offsetOfReplacedData, oldLength);
    if (newLength && shouldUpdateLiveRanges != UpdateLiveRanges::No)
        document().textInserted(*this, offsetOfReplacedData, newLength);

    ASSERT(!renderer() || is<Text>(*this));
    if (is<Text>(*this))
        downcast<Text>(*this).updateRendererAfterContentChange(offsetOfReplacedData, oldLength);

    if (is<ProcessingInstruction>(*this))
        downcast<ProcessingInstruction>(*this).checkStyleSheet();

    if (document().frame())
        document().frame()->selection().textWasReplaced(this, offsetOfReplacedData, oldLength, newLength);

    notifyParentAfterChange(ContainerNode::ChildChange::Source::API);

    dispatchModifiedEvent(oldData);
}

void CharacterData::notifyParentAfterChange(ContainerNode::ChildChange::Source source)
{
    document().incDOMTreeVersion();

    if (!parentNode())
        return;

    ContainerNode::ChildChange change = {
        ContainerNode::ChildChange::Type::TextChanged,
        ElementTraversal::previousSibling(*this),
        ElementTraversal::nextSibling(*this),
        source
    };

    parentNode()->childrenChanged(change);
}

void CharacterData::dispatchModifiedEvent(const String& oldData)
{
    if (auto mutationRecipients = MutationObserverInterestGroup::createForCharacterDataMutation(*this))
        mutationRecipients->enqueueMutationRecord(MutationRecord::createCharacterData(*this, oldData));

    if (!isInShadowTree()) {
        if (document().hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER))
            dispatchScopedEvent(MutationEvent::create(eventNames().DOMCharacterDataModifiedEvent, Event::CanBubble::Yes, nullptr, oldData, m_data));
        dispatchSubtreeModifiedEvent();
    }

    InspectorInstrumentation::characterDataModified(document(), *this);
}

} // namespace WebCore
