/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef DFGNodeFlags_h
#define DFGNodeFlags_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

// Entries in the NodeType enum (below) are composed of an id, a result type (possibly none)
// and some additional informative flags (must generate, is constant, etc).
#define NodeResultMask              0xF
#define NodeResultJS                0x1
#define NodeResultNumber            0x2
#define NodeResultInt32             0x3
#define NodeResultBoolean           0x4
#define NodeResultStorage           0x5
                                
#define NodeMustGenerate           0x10 // set on nodes that have side effects, and may not trivially be removed by DCE.
#define NodeHasVarArgs             0x20
#define NodeClobbersWorld          0x40
#define NodeMightClobber           0x80
                                
#define NodeBehaviorMask          0x300
#define NodeMayOverflow           0x100
#define NodeMayNegZero            0x200
                                
#define NodeBackPropMask         0x1C00
#define NodeUseBottom             0x000
#define NodeUsedAsNumber          0x400 // The result of this computation may be used in a context that observes fractional results.
#define NodeNeedsNegZero          0x800 // The result of this computation may be used in a context that observes -0.
#define NodeUsedAsValue           (NodeUsedAsNumber | NodeNeedsNegZero)
#define NodeUsedAsInt            0x1000 // The result of this computation is known to be used in a context that prefers, but does not require, integer values.

typedef uint16_t NodeFlags;

static inline bool nodeUsedAsNumber(NodeFlags flags)
{
    return !!(flags & NodeUsedAsNumber);
}

static inline bool nodeCanTruncateInteger(NodeFlags flags)
{
    return !nodeUsedAsNumber(flags);
}

static inline bool nodeCanIgnoreNegativeZero(NodeFlags flags)
{
    return !(flags & NodeNeedsNegZero);
}

static inline bool nodeMayOverflow(NodeFlags flags)
{
    return !!(flags & NodeMayOverflow);
}

static inline bool nodeCanSpeculateInteger(NodeFlags flags)
{
    if (flags & NodeMayOverflow)
        return !nodeUsedAsNumber(flags);
    
    if (flags & NodeMayNegZero)
        return nodeCanIgnoreNegativeZero(flags);
    
    return true;
}

const char* nodeFlagsAsString(NodeFlags);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGNodeFlags_h

