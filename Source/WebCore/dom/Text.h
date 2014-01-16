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
 *
 */

#ifndef Text_h
#define Text_h

#include "CharacterData.h"
#include "RenderPtr.h"

namespace WebCore {

class RenderText;
class ScriptExecutionContext;

class Text : public CharacterData {
public:
    static const unsigned defaultLengthLimit = 1 << 16;

    static PassRefPtr<Text> create(Document&, const String&);
    static PassRefPtr<Text> create(ScriptExecutionContext&, const String&);
    static PassRefPtr<Text> createWithLengthLimit(Document&, const String&, unsigned positionInString, unsigned lengthLimit = defaultLengthLimit);
    static PassRefPtr<Text> createEditingText(Document&, const String&);

    virtual ~Text();

    PassRefPtr<Text> splitText(unsigned offset, ExceptionCode&);

    // DOM Level 3: http://www.w3.org/TR/DOM-Level-3-Core/core.html#ID-1312295772

    String wholeText() const;
    PassRefPtr<Text> replaceWholeText(const String&, ExceptionCode&);
    
    RenderPtr<RenderText> createTextRenderer(const RenderStyle&);
    
    virtual bool canContainRangeEndPoint() const override FINAL { return true; }

    RenderText* renderer() const;

protected:
    Text(Document& document, const String& data, ConstructionType type)
        : CharacterData(document, data, type)
    {
    }

private:
    virtual String nodeName() const override;
    virtual NodeType nodeType() const override;
    virtual PassRefPtr<Node> cloneNode(bool deep) override;
    virtual bool childTypeAllowed(NodeType) const override;

    virtual PassRefPtr<Text> virtualCreate(const String&);

#ifndef NDEBUG
    virtual void formatForDebugger(char* buffer, unsigned length) const override;
#endif
};

inline bool isText(const Node& node) { return node.isTextNode(); }
void isText(const Text&); // Catch unnecessary runtime check of type known at compile time.
void isText(const ContainerNode&); // Catch unnecessary runtime check of type known at compile time.

NODE_TYPE_CASTS(Text)

} // namespace WebCore

#endif // Text_h
