/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
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

#include <WebCore/Node.h>

namespace WebCore {

class NamedNodeMap;

class DocumentType final : public Node {
    WTF_MAKE_TZONE_ALLOCATED(DocumentType);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DocumentType);
public:
    static Ref<DocumentType> create(Document& document, StringImpl* name, StringImpl* publicId, StringImpl* systemId)
    {
        return adoptRef(*new DocumentType(document, name, publicId, systemId));
    }

    const RefPtr<StringImpl>& name() const LIFETIME_BOUND { return m_name; }
    const RefPtr<StringImpl>& publicId() const LIFETIME_BOUND { return m_publicId; }
    const RefPtr<StringImpl>& systemId() const LIFETIME_BOUND { return m_systemId; }

private:
    DocumentType(Document&, StringImpl* name, StringImpl* publicId, StringImpl* systemId);

    RefPtr<StringImpl> nodeName() const override;
    Ref<Node> cloneNodeInternal(Document&, CloningOperation, CustomElementRegistry*) const override;
    SerializedNode serializeNode(CloningOperation) const override;

    void parentOrShadowHostNode() const = delete; // Call parentNode() instead.

    RefPtr<StringImpl> m_name;
    RefPtr<StringImpl> m_publicId;
    RefPtr<StringImpl> m_systemId;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::DocumentType)
    static bool isType(const WebCore::Node& node) { return node.nodeType() == WebCore::NodeType::DocumentType; }
SPECIALIZE_TYPE_TRAITS_END()
