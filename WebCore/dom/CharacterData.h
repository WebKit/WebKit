/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#ifndef CharacterData_h
#define CharacterData_h

#include "EventTargetNode.h"

namespace WebCore {

class CharacterData : public EventTargetNode {
public:
    CharacterData(Document*, const String& text);
    CharacterData(Document*);
    virtual ~CharacterData();

    // DOM methods & attributes for CharacterData

    String data() const { return m_data; }
    void setData(const String&, ExceptionCode&);
    unsigned length() const { return m_data->length(); }
    String substringData(unsigned offset, unsigned count, ExceptionCode&);
    void appendData(const String&, ExceptionCode&);
    void insertData(unsigned offset, const String&, ExceptionCode&);
    void deleteData(unsigned offset, unsigned count, ExceptionCode&);
    void replaceData(unsigned offset, unsigned count, const String &arg, ExceptionCode&);

    bool containsOnlyWhitespace() const;

    // DOM methods overridden from parent classes

    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);
    
    // Other methods (not part of DOM)

    virtual bool isCharacterDataNode() const { return true; }
    virtual int maxCharacterOffset() const;
    StringImpl* string() { return m_data.get(); }
    virtual void checkCharDataOperation(unsigned offset, ExceptionCode&);

    virtual bool offsetInCharacters() const;
    virtual bool rendererIsNeeded(RenderStyle*);
    
#ifndef NDEBUG
    virtual void dump(TextStream*, DeprecatedString indent = "") const;
#endif

protected:
    RefPtr<StringImpl> m_data;

    void dispatchModifiedEvent(StringImpl* oldValue);
};

} // namespace WebCore

#endif // CharacterData_h

