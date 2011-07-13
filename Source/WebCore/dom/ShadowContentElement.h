/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef ShadowContentElement_h
#define ShadowContentElement_h

#include "StyledElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class ShadowContentElement;

class ShadowInclusion : public RefCounted<ShadowInclusion> {
public:
    static PassRefPtr<ShadowInclusion> create(ShadowContentElement*, Node*);

    ShadowContentElement* includer() const { return m_includer; }
    Node* content() const { return m_content.get(); }
    ShadowInclusion* next() const { return m_next.get(); }
    ShadowInclusion* previous() const { return m_previous.get(); }

    void append(PassRefPtr<ShadowInclusion>);
    void unlink();

private:
    explicit ShadowInclusion(ShadowContentElement* includer, Node* content)
        : m_includer(includer), m_content(content)
    { }

    ShadowContentElement* m_includer;
    RefPtr<Node> m_content;
    RefPtr<ShadowInclusion> m_next;
    RefPtr<ShadowInclusion> m_previous;
};

inline PassRefPtr<ShadowInclusion> ShadowInclusion::create(ShadowContentElement* includer, Node* content)
{
    return adoptRef(new ShadowInclusion(includer, content));
}


class ShadowInclusionList {
public:
    ShadowInclusionList();
    ~ShadowInclusionList();

    ShadowInclusion* first() const { return m_first.get(); }
    ShadowInclusion* last() const { return m_last.get(); }
    ShadowInclusion* find(Node*) const;
    bool isEmpty() const { return !m_first; }

    void clear();
    void append(PassRefPtr<ShadowInclusion>);
    void append(ShadowContentElement*, Node*);

private:
    RefPtr<ShadowInclusion> m_first;
    RefPtr<ShadowInclusion> m_last;
};

inline void ShadowInclusionList::append(ShadowContentElement* includer, Node* node)
{
    append(ShadowInclusion::create(includer, node));
}


// NOTE: Current implementation doesn't support dynamic insertion/deletion of ShadowContentElement.
// You should create ShadowContentElement during the host construction.
class ShadowContentElement : public StyledElement {
public:
    static PassRefPtr<ShadowContentElement> create(Document*);

    virtual ~ShadowContentElement();
    virtual bool shouldInclude(Node*);
    virtual void attach();
    virtual void detach();

    const ShadowInclusionList* inclusions() const { return &m_inclusions; }

protected:
    ShadowContentElement(const QualifiedName&, Document*);

private:
    virtual bool isContentElement() const { return true; }
    virtual bool rendererIsNeeded(const NodeRenderingContext&) { return false; }
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) { return 0; }

    ShadowInclusionList m_inclusions;
};

inline ShadowContentElement* toShadowContentElement(Node* node)
{
    ASSERT(!node || node->isContentElement());
    return static_cast<ShadowContentElement*>(node);
}

}

#endif
