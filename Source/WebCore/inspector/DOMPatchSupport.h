/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ExceptionOr.h"
#include <wtf/HashMap.h>
#include <wtf/Vector.h>

namespace WebCore {

class ContainerNode;
class DOMEditor;
class Document;
class Node;

class DOMPatchSupport final {
public:
    DOMPatchSupport(DOMEditor&, Document&);

    void patchDocument(const String& markup);
    ExceptionOr<Node*> patchNode(Node&, const String& markup);

private:
    struct Digest;

    using ResultMap = Vector<std::pair<Digest*, size_t>>;
    using UnusedNodesMap = HashMap<String, Digest*>;

    ExceptionOr<void> innerPatchNode(Digest& oldNode, Digest& newNode);
    std::pair<ResultMap, ResultMap> diff(const Vector<std::unique_ptr<Digest>>& oldChildren, const Vector<std::unique_ptr<Digest>>& newChildren);
    ExceptionOr<void> innerPatchChildren(ContainerNode&, const Vector<std::unique_ptr<Digest>>& oldChildren, const Vector<std::unique_ptr<Digest>>& newChildren);
    std::unique_ptr<Digest> createDigest(Node&, UnusedNodesMap*);
    ExceptionOr<void> insertBeforeAndMarkAsUsed(ContainerNode&, Digest&, Node* anchor);
    ExceptionOr<void> removeChildAndMoveToNew(Digest&);
    void markNodeAsUsed(Digest&);

#ifdef DEBUG_DOM_PATCH_SUPPORT
    void dumpMap(const ResultMap&, const String& name);
#endif

    DOMEditor& m_domEditor;
    Document& m_document;

    UnusedNodesMap m_unusedNodesMap;
};

} // namespace WebCore
