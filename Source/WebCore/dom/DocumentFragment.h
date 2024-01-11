/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2009 Apple Inc. All rights reserved.
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
#include "ParserContentPolicy.h"

namespace WebCore {

class DocumentFragment : public ContainerNode {
    WTF_MAKE_ISO_ALLOCATED(DocumentFragment);
public:
    WEBCORE_EXPORT static Ref<DocumentFragment> create(Document&);
    static Ref<DocumentFragment> createForInnerOuterHTML(Document&);

    void parseHTML(const String&, Element& contextElement, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent });
    WEBCORE_EXPORT bool parseXML(const String&, Element* contextElement, OptionSet<ParserContentPolicy> = { ParserContentPolicy::AllowScriptingContent });

    bool canContainRangeEndPoint() const final { return true; }
    virtual bool isTemplateContent() const { return false; }

    // From the NonElementParentNode interface - https://dom.spec.whatwg.org/#interface-nonelementparentnode
    WEBCORE_EXPORT Element* getElementById(const AtomString&) const;

protected:
    DocumentFragment(Document&, OptionSet<TypeFlag> = { });
    String nodeName() const final;

private:
    Ref<Node> cloneNodeInternal(Document&, CloningOperation) override;
    bool childTypeAllowed(NodeType) const override;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DocumentFragment)
    static bool isType(const WebCore::Node& node) { return node.isDocumentFragment(); }
SPECIALIZE_TYPE_TRAITS_END()
