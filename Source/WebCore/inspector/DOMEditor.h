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

#ifndef DOMEditor_h
#define DOMEditor_h

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class HTMLElement;
class NamedNodeMap;
class Node;

typedef String ErrorString;

#if ENABLE(INSPECTOR)

class DOMEditor {
public:
    explicit DOMEditor(Document*);
    virtual ~DOMEditor();

    void patchDocument(const String& markup);
    Node* patchNode(ErrorString*, Node*, const String& markup);

private:
    struct Digest;
    typedef Vector<pair<Digest*, size_t> > ResultMap;

    void innerPatchHTMLElement(ErrorString*, HTMLElement* oldElement, HTMLElement* newElement);
    void innerPatchNode(ErrorString*, Digest* oldNode, Digest* newNode);
    std::pair<ResultMap, ResultMap> diff(Vector<OwnPtr<Digest> >& oldChildren, Vector<OwnPtr<Digest> >& newChildren);
    void innerPatchChildren(ErrorString*, Element*, Vector<OwnPtr<Digest> >& oldChildren, Vector<OwnPtr<Digest> >& newChildren);
    PassOwnPtr<Digest> createDigest(Node*);
    Document* m_document;
};

#endif // ENABLE(INSPECTOR)

} // namespace WebCore

#endif // !defined(DOMEditor_h)
