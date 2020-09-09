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
 *
 */

#pragma once

#include "ContainerNode.h"

namespace WebCore {

class CharacterData : public Node {
    WTF_MAKE_ISO_ALLOCATED(CharacterData);
public:
    const String& data() const { return m_data; }
    static ptrdiff_t dataMemoryOffset() { return OBJECT_OFFSETOF(CharacterData, m_data); }

    WEBCORE_EXPORT void setData(const String&);
    virtual void setDataAndUpdate(const String&, unsigned offsetOfReplacedData, unsigned oldLength, unsigned newLength);
    unsigned length() const { return m_data.length(); }
    WEBCORE_EXPORT ExceptionOr<String> substringData(unsigned offset, unsigned count);
    WEBCORE_EXPORT void appendData(const String&);
    WEBCORE_EXPORT ExceptionOr<void> insertData(unsigned offset, const String&);
    WEBCORE_EXPORT ExceptionOr<void> deleteData(unsigned offset, unsigned count);
    WEBCORE_EXPORT ExceptionOr<void> replaceData(unsigned offset, unsigned count, const String&);

    // Like appendData, but optimized for the parser (e.g., no mutation events).
    // Returns how much could be added before length limit was met.
    unsigned parserAppendData(const String& string, unsigned offset, unsigned lengthLimit);

protected:
    CharacterData(Document& document, const String& text, ConstructionType type = CreateCharacterData)
        : Node(document, type)
        , m_data(!text.isNull() ? text : emptyString())
    {
        ASSERT(type == CreateCharacterData || type == CreateText || type == CreateEditingText);
    }

    void setDataWithoutUpdate(const String& data)
    {
        ASSERT(!data.isNull());
        m_data = data;
    }
    void dispatchModifiedEvent(const String& oldValue);

private:
    String nodeValue() const final;
    ExceptionOr<void> setNodeValue(const String&) final;
    void notifyParentAfterChange(ContainerNode::ChildChangeSource);

    String m_data;
};

inline unsigned Node::length() const
{
    if (is<CharacterData>(*this))
        return downcast<CharacterData>(*this).length();
    return countChildNodes();
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::CharacterData)
    static bool isType(const WebCore::Node& node) { return node.isCharacterDataNode(); }
SPECIALIZE_TYPE_TRAITS_END()
