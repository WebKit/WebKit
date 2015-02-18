/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#ifndef DFA_h
#define DFA_h

#if ENABLE(CONTENT_EXTENSIONS)

#include "ContentExtensionsDebugging.h"
#include "DFANode.h"
#include <wtf/Vector.h>

namespace WebCore {

namespace ContentExtensions {

// The DFA abstract a partial DFA graph in a compact form.
class DFA {
public:
    DFA();
    DFA(Vector<DFANode>&& nodes, unsigned rootIndex);
    DFA(const DFA& dfa);

    DFA& operator=(const DFA&);

    unsigned root() const { return m_root; }
    unsigned size() const { return m_nodes.size(); }
    const DFANode& nodeAt(unsigned i) const
    {
        return m_nodes[i];
    }

#if CONTENT_EXTENSIONS_STATE_MACHINE_DEBUGGING
    void debugPrintDot() const;
#endif

private:
    Vector<DFANode> m_nodes;
    unsigned m_root;
};

}

} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)

#endif // DFA_h
