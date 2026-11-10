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
#include "CDATASection.h"

#include "Document.h"
#include "NodeDocument.h"
#include "SerializedNode.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDATASection);

inline CDATASection::CDATASection(Document& document, RefPtr<StringImpl>&& data)
    : Text(document, WTF::move(data), NodeType::CDATASection, { })
{
}

Ref<CDATASection> CDATASection::create(Document& document, RefPtr<StringImpl>&& data)
{
    return adoptRef(*new CDATASection(document, WTF::move(data)));
}

RefPtr<StringImpl> CDATASection::nodeName() const
{
    return "#cdata-section"_si;
}

Ref<Node> CDATASection::cloneNodeInternal(Document& document, CloningOperation, CustomElementRegistry*) const
{
    return create(document, RefPtr<StringImpl> { data() });
}

SerializedNode CDATASection::serializeNode(CloningOperation) const
{
    return { SerializedNode::CDATASection { data() } };
}

Ref<Text> CDATASection::virtualCreate(RefPtr<StringImpl>&& data)
{
    return create(protect(document()), WTF::move(data));
}

} // namespace WebCore
