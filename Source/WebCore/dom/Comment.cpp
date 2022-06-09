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
#include "Comment.h"

#include "Document.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Comment);

inline Comment::Comment(Document& document, const String& text)
    : CharacterData(document, text)
{
}

Ref<Comment> Comment::create(Document& document, const String& text)
{
    return adoptRef(*new Comment(document, text));
}

String Comment::nodeName() const
{
    return "#comment"_s;
}

Node::NodeType Comment::nodeType() const
{
    return COMMENT_NODE;
}

Ref<Node> Comment::cloneNodeInternal(Document& targetDocument, CloningOperation)
{
    return create(targetDocument, data());
}

bool Comment::childTypeAllowed(NodeType) const
{
    return false;
}

} // namespace WebCore
