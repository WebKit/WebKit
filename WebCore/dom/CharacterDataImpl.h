/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef DOM_CharacterDataImpl_h
#define DOM_CharacterDataImpl_h

#include "EventTargetNodeImpl.h"

namespace WebCore {

class CharacterDataImpl : public EventTargetNodeImpl {
public:
    CharacterDataImpl(DocumentImpl*, const String& text);
    CharacterDataImpl(DocumentImpl*);
    virtual ~CharacterDataImpl();

    // DOM methods & attributes for CharacterData

    virtual String data() const;
    virtual void setData(const String&, ExceptionCode&);
    virtual unsigned length() const;
    virtual String substringData(unsigned offset, unsigned count, ExceptionCode&);
    virtual void appendData(const String&, ExceptionCode&);
    virtual void insertData(unsigned offset, const String&, ExceptionCode&);
    virtual void deleteData(unsigned offset, unsigned count, ExceptionCode&);
    virtual void replaceData(unsigned offset, unsigned count, const String &arg, ExceptionCode&);

    bool containsOnlyWhitespace() const;
    bool containsOnlyWhitespace(unsigned from, unsigned len) const;
    
    // DOM methods overridden from  parent classes

    virtual String nodeValue() const;
    virtual void setNodeValue(const String&, ExceptionCode&);

    // Other methods (not part of DOM)

    StringImpl* string() { return str; }
    virtual void checkCharDataOperation(unsigned offset, ExceptionCode&);

    virtual int maxOffset() const;
    virtual int caretMinOffset() const;
    virtual int caretMaxOffset() const;
    virtual unsigned caretMaxRenderedOffset() const;
    virtual bool offsetInCharacters() const;
    virtual bool rendererIsNeeded(RenderStyle*);
    
#ifndef NDEBUG
    virtual void dump(QTextStream*, QString ind = "") const;
#endif

protected:
    // note: since DOMStrings are shared, str should always be copied when making
    // a change or returning a string
    StringImpl* str;

    void dispatchModifiedEvent(StringImpl* prevValue);
};

} // namespace WebCore

#endif // DOM_CharacterDataImpl_h

