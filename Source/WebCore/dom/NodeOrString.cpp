/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NodeOrString.h"

#include "DocumentFragment.h"
#include "Text.h"

namespace WebCore {

RefPtr<Node> convertNodesOrStringsIntoNode(Node& context, Vector<NodeOrString>&& nodeOrStringVector, ExceptionCode& ec)
{
    if (nodeOrStringVector.isEmpty())
        return nullptr;

    Vector<RefPtr<Node>> nodes;
    nodes.reserveInitialCapacity(nodeOrStringVector.size());
    for (auto& nodeOrString : nodeOrStringVector) {
        switch (nodeOrString.type()) {
        case NodeOrString::Type::String:
            nodes.uncheckedAppend(Text::create(context.document(), nodeOrString.string()));
            break;
        case NodeOrString::Type::Node:
            nodes.uncheckedAppend(&nodeOrString.node());
            break;
        }
    }

    RefPtr<Node> nodeToReturn;
    if (nodes.size() == 1)
        nodeToReturn = nodes.first();
    else {
        nodeToReturn = DocumentFragment::create(context.document());
        for (auto& node : nodes) {
            nodeToReturn->appendChild(node, ec);
            if (ec)
                return nullptr;
        }
    }
    
    return nodeToReturn;
}

} // namespace WebCore
