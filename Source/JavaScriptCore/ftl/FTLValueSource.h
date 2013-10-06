/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef FTLValueSource_h
#define FTLValueSource_h

#include <wtf/Platform.h>

#if ENABLE(FTL_JIT)

#include "DFGNode.h"
#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace FTL {

enum ValueSourceKind {
    SourceNotSet,
    ValueInJSStack,
    Int32InJSStack,
    Int52InJSStack,
    DoubleInJSStack,
    SourceIsDead,
    HaveNode
};

class ValueSource {
public:
    ValueSource()
        : m_kind(SourceNotSet)
    {
    }
    
    explicit ValueSource(ValueSourceKind kind)
        : m_kind(kind)
    {
        ASSERT(m_kind == SourceIsDead);
    }
    
    explicit ValueSource(DFG::Node* node)
        : m_kind(HaveNode)
    {
        m_value.node = node;
    }
    
    ValueSource(ValueSourceKind kind, VirtualRegister reg)
        : m_kind(kind)
    {
        ASSERT(m_kind == ValueInJSStack || m_kind == Int32InJSStack || m_kind == Int52InJSStack || m_kind == DoubleInJSStack);
        m_value.virtualRegister = reg.offset();
    }
    
    ValueSourceKind kind() const
    {
        return m_kind;
    }
    
    bool operator!() const { return kind() == SourceNotSet; }
    
    DFG::Node* node() const
    {
        ASSERT(kind() == HaveNode);
        return m_value.node;
    }
    
    VirtualRegister virtualRegister() const
    {
        ASSERT(kind() == ValueInJSStack || kind() == Int32InJSStack || kind() == Int52InJSStack || kind() == DoubleInJSStack);
        return VirtualRegister(m_value.virtualRegister);
    }
    
    void dump(PrintStream&) const;
    void dumpInContext(PrintStream&, DumpContext*) const;

private:
    ValueSourceKind m_kind;
    union {
        DFG::Node* node;
        int virtualRegister;
    } m_value;
};

} } // namespace JSC::FTL

#endif // ENABLE(FTL_JIT)

#endif // FTLValueSource_h

