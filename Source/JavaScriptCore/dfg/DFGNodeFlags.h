/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#if ENABLE(DFG_JIT)

#include <wtf/PrintStream.h>
#include <wtf/StdLibExtras.h>

namespace JSC { namespace DFG {

// Entries in the NodeType enum (below) are composed of an id, a result type (possibly none)
// and some additional informative flags (must generate, is constant, etc).
#define NodeResultMask                   0x0007
#define NodeResultJS                     0x0001
#define NodeResultNumber                 0x0002
#define NodeResultInt32                  0x0003
#define NodeResultInt52                  0x0004
#define NodeResultBoolean                0x0005
#define NodeResultStorage                0x0006
                                
#define NodeMustGenerate                 0x0008 // set on nodes that have side effects, and may not trivially be removed by DCE.
#define NodeHasVarArgs                   0x0010
#define NodeClobbersWorld                0x0020
#define NodeMightClobber                 0x0040
                                
#define NodeBehaviorMask                 0x0180
#define NodeMayOverflow                  0x0080
#define NodeMayNegZero                   0x0100
                                
#define NodeBytecodeBackPropMask         0x1E00
#define NodeBytecodeUseBottom            0x0000
#define NodeBytecodeUsesAsNumber         0x0200 // The result of this computation may be used in a context that observes fractional, or bigger-than-int32, results.
#define NodeBytecodeNeedsNegZero         0x0400 // The result of this computation may be used in a context that observes -0.
#define NodeBytecodeUsesAsOther          0x0800 // The result of this computation may be used in a context that distinguishes between NaN and other things (like undefined).
#define NodeBytecodeUsesAsValue          (NodeBytecodeUsesAsNumber | NodeBytecodeNeedsNegZero | NodeBytecodeUsesAsOther)
#define NodeBytecodeUsesAsInt            0x1000 // The result of this computation is known to be used in a context that prefers, but does not require, integer values.

#define NodeArithFlagsMask               (NodeBehaviorMask | NodeBytecodeBackPropMask)

#define NodeDoesNotExit                  0x2000 // This flag is negated to make it natural for the default to be that a node does exit.

#define NodeRelevantToOSR                0x4000

#define NodeIsStaticConstant             0x8000 // Used only by the parser, to determine if a constant arose statically and hence could be folded at parse-time.
#define NodeIsFlushed                   0x10000 // Used by Graph::computeIsFlushed(), will tell you which local nodes are backwards-reachable from a Flush.

typedef uint32_t NodeFlags;

static inline bool bytecodeUsesAsNumber(NodeFlags flags)
{
    return !!(flags & NodeBytecodeUsesAsNumber);
}

static inline bool bytecodeCanTruncateInteger(NodeFlags flags)
{
    return !bytecodeUsesAsNumber(flags);
}

static inline bool bytecodeCanIgnoreNegativeZero(NodeFlags flags)
{
    return !(flags & NodeBytecodeNeedsNegZero);
}

static inline bool nodeMayOverflow(NodeFlags flags)
{
    return !!(flags & NodeMayOverflow);
}

static inline bool nodeMayNegZero(NodeFlags flags)
{
    return !!(flags & NodeMayNegZero);
}

static inline bool nodeCanSpeculateInt32(NodeFlags flags)
{
    if (nodeMayOverflow(flags))
        return !bytecodeUsesAsNumber(flags);
    
    if (nodeMayNegZero(flags))
        return bytecodeCanIgnoreNegativeZero(flags);
    
    return true;
}

static inline bool nodeCanSpeculateInt52(NodeFlags flags)
{
    if (nodeMayNegZero(flags))
        return bytecodeCanIgnoreNegativeZero(flags);
    
    return true;
}

void dumpNodeFlags(PrintStream&, NodeFlags);
MAKE_PRINT_ADAPTOR(NodeFlagsDump, NodeFlags, dumpNodeFlags);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGNodeFlags_h

