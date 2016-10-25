/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include <domjit/DOMJITHeapRange.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>

#if ENABLE(JIT)

namespace WebCore { namespace DOMJIT {

// Describe your abstract heap hierarchy here.
// V(AbstractHeapName, Parent)
#define DOMJIT_ABSTRACT_HEAP_LIST(V) \
    V(Node, DOM) \
    V(Node_firstChild, Node) \
    V(Node_lastChild, Node) \
    V(Node_parentNode, Node) \
    V(Node_nextSibling, Node) \
    V(Node_previousSibling, Node) \


class AbstractHeapRepository {
    WTF_MAKE_NONCOPYABLE(AbstractHeapRepository);
public:
    static const AbstractHeapRepository& shared();

    JSC::DOMJIT::HeapRange DOM;

#define DOMJIT_DEFINE_MEMBER(name, parent) JSC::DOMJIT::HeapRange name;
    DOMJIT_ABSTRACT_HEAP_LIST(DOMJIT_DEFINE_MEMBER)
#undef DOMJIT_DEFINE_MEMBER

    AbstractHeapRepository();
};

} }

#endif
