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

class Text : public CharacterData {
public:
    static const unsigned defaultLengthLimit = 1 << 16;

    static Ref<Text> create(Document&, const String&);
    static Ref<Text> createWithLengthLimit(Document&, const String&, unsigned positionInString, unsigned lengthLimit = defaultLengthLimit);
    static Ref<Text> createEditingText(Document&, const String&);

    virtual ~Text();

    RefPtr<Text> splitText(unsigned offset, ExceptionCode&);

    // DOM Level 3: http://www.w3.org/TR/DOM-Level-3-Core/core.html#ID-1312295772

    String wholeText() const;
    RefPtr<Text> replaceWholeText(const String&, ExceptionCode&);
    
    RenderPtr<RenderText> createTextRenderer(const RenderStyle&);
    
    bool canContainRangeEndPoint() const override final { return true; }

    RenderText* renderer() const;

protected:
    Text(Document& document, const String& data, ConstructionType type)
        : CharacterData(document, data, type)
    {
    }

private:
    String nodeName() const override;
    NodeType nodeType() const override;
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;
    bool childTypeAllowed(NodeType) const override;

    virtual Ref<Text> virtualCreate(const String&);

#if ENABLE(TREE_DEBUGGING)
    void formatForDebugger(char* buffer, unsigned length) const override;
#endif
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Text)
    static bool isType(const WebCore::Node& node) { return node.isTextNode(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // Text_h
