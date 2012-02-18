/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef DFGArithNodeFlagsInferencePhase_h
#define DFGArithNodeFlagsInferencePhase_h

#include <wtf/Platform.h>

#if ENABLE(DFG_JIT)

namespace JSC { namespace DFG {

class Graph;

// Determine which arithmetic nodes' results are only used in a context that
// truncates to integer anyway. This is great for optimizing away checks for
// overflow and negative zero. NB the way this phase integrates into the rest
// of the DFG makes it non-optional. Instead of proving that a node is only
// used in integer context, it actually does the opposite: finds nodes that
// are used in non-integer contexts. Hence failing to run this phase will make
// the compiler assume that all nodes are just used as integers!

void performArithNodeFlagsInference(Graph&);

} } // namespace JSC::DFG::Phase

#endif // ENABLE(DFG_JIT)

#endif // DFGArithNodeFlagsInferencePhase_h
