/*
 * Copyright (C) 2007, 2009, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "JSDOMBinding.h"
#include "JSNode.h"

namespace WebCore {

WEBCORE_EXPORT JSC::JSValue createWrapper(JSC::ExecState*, JSDOMGlobalObject*, Ref<Node>&&);
WEBCORE_EXPORT JSC::JSObject* getOutOfLineCachedWrapper(JSDOMGlobalObject*, Node&);

inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, Node& node)
{
    if (LIKELY(globalObject->worldIsNormal())) {
        if (auto* wrapper = node.wrapper())
            return wrapper;
    } else {
        if (auto* wrapper = getOutOfLineCachedWrapper(globalObject, node))
            return wrapper;
    }

    return createWrapper(exec, globalObject, node);
}

// In the C++ DOM, a node tree survives as long as there is a reference to its
// root. In the JavaScript DOM, a node tree survives as long as there is a
// reference to any node in the tree. To model the JavaScript DOM on top of
// the C++ DOM, we ensure that the root of every tree has a JavaScript wrapper.
void willCreatePossiblyOrphanedTreeByRemovalSlowCase(Node* root);
inline void willCreatePossiblyOrphanedTreeByRemoval(Node* root)
{
    if (root->wrapper())
        return;

    if (!root->hasChildNodes())
        return;

    willCreatePossiblyOrphanedTreeByRemovalSlowCase(root);
}

inline void* root(Node* node)
{
    return node->opaqueRoot();
}

inline void* root(Node& node)
{
    return root(&node);
}

template<typename From>
ALWAYS_INLINE JSDynamicCastResult<JSNode, From> jsNodeCast(From* value)
{
    return value->type() >= JSNodeType ? JSC::jsCast<JSDynamicCastResult<JSNode, From>>(value) : nullptr;
}

ALWAYS_INLINE JSC::JSValue JSNode::nodeType(JSC::ExecState&) const
{
    return JSC::jsNumber(static_cast<uint8_t>(type()) & JSNodeTypeMask);
}

} // namespace WebCore
