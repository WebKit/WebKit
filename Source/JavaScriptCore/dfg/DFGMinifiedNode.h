/*
 * Copyright (C) 2012, 2014, 2015 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include "DFGMinifiedID.h"
#include "DFGNodeType.h"
#include "InlineCallFrame.h"
#include "JSCJSValue.h"

namespace JSC { namespace DFG {

struct Node;

inline bool belongsInMinifiedGraph(NodeType type)
{
    switch (type) {
    case JSConstant:
    case Int52Constant:
    case DoubleConstant:
    case PhantomDirectArguments:
    case PhantomClonedArguments:
        return true;
    default:
        return false;
    }
}

class MinifiedNode {
public:
    MinifiedNode() { }
    
    static MinifiedNode fromNode(Node*);
    
    MinifiedID id() const { return m_id; }
    
    bool hasConstant() const { return m_hasConstant; }
    
    JSValue constant() const
    {
        return JSValue::decode(bitwise_cast<EncodedJSValue>(m_info.get()));
    }
    
    bool isPhantomDirectArguments() const { return m_isPhantomDirectArguments; }
    bool isPhantomClonedArguments() const { return m_isPhantomClonedArguments; }
    bool hasInlineCallFrame() const { return m_isPhantomDirectArguments || m_isPhantomClonedArguments; }
    
    InlineCallFrame* inlineCallFrame() const
    {
        return bitwise_cast<InlineCallFrame*>(static_cast<uintptr_t>(m_info.get()));
    }
    
    static MinifiedID getID(MinifiedNode* node) { return node->id(); }
    static bool compareByNodeIndex(const MinifiedNode& a, const MinifiedNode& b)
    {
        return a.m_id < b.m_id;
    }
    
private:
    static bool hasConstant(NodeType type)
    {
        return type == JSConstant || type == Int52Constant || type == DoubleConstant;
    }
    
    Packed<uint64_t> m_info;
    MinifiedID m_id;
    bool m_hasConstant : 1;
    bool m_isPhantomDirectArguments : 1;
    bool m_isPhantomClonedArguments : 1;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
