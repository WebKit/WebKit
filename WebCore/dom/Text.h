/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#ifndef Text_h
#define Text_h

#include "CharacterData.h"

namespace WebCore {
    
const unsigned cTextNodeLengthLimit = 1 << 16;

class Text : public CharacterData {
public:
    Text(Document *impl, const String &_text);
    Text(Document *impl);
    virtual ~Text();

    // DOM methods & attributes for CharacterData

    PassRefPtr<Text> splitText(unsigned offset, ExceptionCode&);

    // DOM Level 3: http://www.w3.org/TR/DOM-Level-3-Core/core.html#ID-1312295772
    String wholeText() const;
    PassRefPtr<Text> replaceWholeText(const String&, ExceptionCode&);

    // DOM methods overridden from parent classes

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual PassRefPtr<Node> cloneNode(bool deep);

    // Other methods (not part of DOM)

    virtual void attach();
    virtual bool rendererIsNeeded(RenderStyle*);
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*);
    virtual void recalcStyle(StyleChange = NoChange);
    virtual bool childTypeAllowed(NodeType);

    static PassRefPtr<Text> createWithLengthLimit(Document*, const String&, unsigned& charsLeft, unsigned maxChars = cTextNodeLengthLimit);

#if ENABLE(WML)
    virtual void insertedIntoDocument();
#endif

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const;
#endif

protected:
    virtual PassRefPtr<Text> createNew(PassRefPtr<StringImpl>);
};

} // namespace WebCore

#endif // Text_h
