/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef DFGPhantomCanonicalizationPhase_h
#define DFGPhantomCanonicalizationPhase_h

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

class Graph;

// Replaces all pre-existing Phantoms with Checks or removes them if the Check is unnecessary. If
// the Phantom was necessary (it uses a node that is relevant to OSR) then the Phantom is hoisted
// to just below the node.
//
// This phase is only valid in SSA, because it's only in SSA that Phantoms are ignored for the
// purpose of liveness-at-some-point and are only used for absolute liveness.
//
// This phase makes a lot of things easier, like CFG simplification: you don't have to insert any
// phantoms when jettisoning a CFG edge.

bool performPhantomCanonicalization(Graph&);

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGPhantomCanonicalizationPhase_h
