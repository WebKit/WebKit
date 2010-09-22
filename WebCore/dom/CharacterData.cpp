/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#include "EventNames.h"
#include "ExceptionCode.h"
#include "InspectorController.h"
#include "MutationEvent.h"
#include "RenderText.h"

namespace WebCore {

void CharacterData::setData(const String& data, ExceptionCode&)
{
    StringImpl* dataImpl = data.impl() ? data.impl() : StringImpl::empty();
    if (equal(m_data.get(), dataImpl))
        return;

    int oldLength = length();
    RefPtr<StringImpl> oldStr = m_data;
    m_data = dataImpl;

    if ((!renderer() || !rendererIsNeeded(renderer()->style())) && attached()) {
        detach();
        attach();
    } else if (renderer())
        toRenderText(renderer())->setTextWithOffset(m_data, 0, oldLength);

    dispatchModifiedEvent(oldStr.get());

    document()->textRemoved(this, 0, oldLength);
}

String CharacterData::substringData(unsigned offset, unsigned count, ExceptionCode& ec)
{
    checkCharDataOperation(offset, ec);
    if (ec)
        return String();

    return m_data->substring(offset, count);
}

PassRefPtr<StringImpl> CharacterData::appendDataCommon(const String& data)
{
    String newStr = m_data;
    newStr.append(data);

    RefPtr<StringImpl> oldStr = m_data;
    m_data = newStr.impl();

    if ((!renderer() || !rendererIsNeeded(renderer()->style())) && attached()) {
        detach();
        attach();
    } else if (renderer())
        toRenderText(renderer())->setTextWithOffset(m_data, oldStr->length(), 0);

    return oldStr.release();
}

void CharacterData::parserAppendData(const String& data)
{
    appendDataCommon(data);
    // We don't call dispatchModifiedEvent here because we don't want the
    // parser to dispatch DOM mutation events.
    if (parentNode())
        parentNode()->childrenChanged();
}

void CharacterData::appendData(const String& data, ExceptionCode&)
{
    RefPtr<StringImpl> oldStr = appendDataCommon(data);
    dispatchModifiedEvent(oldStr.get());
}

void CharacterData::insertData(unsigned offset, const String& data, ExceptionCode& ec)
{
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    String newStr = m_data;
    newStr.insert(data, offset);

    RefPtr<StringImpl> oldStr = m_data;
    m_data = newStr.impl();

    if ((!renderer() || !rendererIsNeeded(renderer()->style())) && attached()) {
        detach();
        attach();
    } else if (renderer())
        toRenderText(renderer())->setTextWithOffset(m_data, offset, 0);

    dispatchModifiedEvent(oldStr.get());
    
    document()->textInserted(this, offset, data.length());
}

void CharacterData::deleteData(unsigned offset, unsigned count, ExceptionCode& ec)
{
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    unsigned realCount;
    if (offset + count > length())
        realCount = length() - offset;
    else
        realCount = count;

    String newStr = m_data;
    newStr.remove(offset, realCount);

    RefPtr<StringImpl> oldStr = m_data;
    m_data = newStr.impl();
    
    if ((!renderer() || !rendererIsNeeded(renderer()->style())) && attached()) {
        detach();
        attach();
    } else if (renderer())
        toRenderText(renderer())->setTextWithOffset(m_data, offset, count);

    dispatchModifiedEvent(oldStr.get());

    document()->textRemoved(this, offset, realCount);
}

void CharacterData::replaceData(unsigned offset, unsigned count, const String& data, ExceptionCode& ec)
{
    checkCharDataOperation(offset, ec);
    if (ec)
        return;

    unsigned realCount;
    if (offset + count > length())
        realCount = length() - offset;
    else
        realCount = count;

    String newStr = m_data;
    newStr.remove(offset, realCount);
    newStr.insert(data, offset);

    RefPtr<StringImpl> oldStr = m_data;
    m_data = newStr.impl();

    if ((!renderer() || !rendererIsNeeded(renderer()->style())) && attached()) {
        detach();
        attach();
    } else if (renderer())
        toRenderText(renderer())->setTextWithOffset(m_data, offset, count);
    
    dispatchModifiedEvent(oldStr.get());
    
    // update the markers for spell checking and grammar checking
    document()->textRemoved(this, offset, realCount);
    document()->textInserted(this, offset, data.length());
}

String CharacterData::nodeValue() const
{
    return m_data;
}

bool CharacterData::containsOnlyWhitespace() const
{
    return !m_data || m_data->containsOnlyWhitespace();
}

void CharacterData::setNodeValue(const String& nodeValue, ExceptionCode& ec)
{
    setData(nodeValue, ec);
}

void CharacterData::dispatchModifiedEvent(StringImpl* prevValue)
{
    if (parentNode())
        parentNode()->childrenChanged();
    if (document()->hasListenerType(Document::DOMCHARACTERDATAMODIFIED_LISTENER))
        dispatchEvent(MutationEvent::create(eventNames().DOMCharacterDataModifiedEvent, true, 0, prevValue, m_data));
    dispatchSubtreeModifiedEvent();
#if ENABLE(INSPECTOR)
    InspectorController::characterDataModified(this);
#endif
}

void CharacterData::checkCharDataOperation(unsigned offset, ExceptionCode& ec)
{
    ec = 0;

    // INDEX_SIZE_ERR: Raised if the specified offset is negative or greater than the number of 16-bit
    // units in data.
    if (offset > length()) {
        ec = INDEX_SIZE_ERR;
        return;
    }
}

int CharacterData::maxCharacterOffset() const
{
    return static_cast<int>(length());
}

bool CharacterData::rendererIsNeeded(RenderStyle *style)
{
    if (!m_data || !length())
        return false;
    return Node::rendererIsNeeded(style);
}

bool CharacterData::offsetInCharacters() const
{
    return true;
}

} // namespace WebCore
