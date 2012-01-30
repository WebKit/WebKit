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

#ifndef HTMLContentElement_h
#define HTMLContentElement_h

#include "ContentInclusionSelector.h"
#include "HTMLElement.h"
#include <wtf/Forward.h>

namespace WebCore {

class ContentSelectorQuery;
class ShadowInclusionList;

// NOTE: Current implementation doesn't support dynamic insertion/deletion of HTMLContentElement.
// You should create HTMLContentElement during the host construction.
class HTMLContentElement : public HTMLElement {
public:
    static PassRefPtr<HTMLContentElement> create(const QualifiedName&, Document*);
    static PassRefPtr<HTMLContentElement> create(Document*);

    virtual ~HTMLContentElement();
    virtual void attach();
    virtual void detach();

    const AtomicString& select() const;

    // FIXME: Currently this constructor accepts wider query than shadow dom spec.
    // For example, a selector query should not include contextual selectors.
    // See https://bugs.webkit.org/show_bug.cgi?id=75946
    // FIXME: Currently we don't support setting select value dynamically.
    // See https://bugs.webkit.org/show_bug.cgi?id=76261
    void setSelect(const AtomicString&);

    const ShadowInclusionList* inclusions() const { return m_inclusions.get(); }
    bool hasInclusion() const { return inclusions()->first(); }

    virtual bool isSelectValid() const;

protected:
    HTMLContentElement(const QualifiedName&, Document*);

private:
    virtual bool isContentElement() const { return true; }
    virtual bool rendererIsNeeded(const NodeRenderingContext&) { return false; }
    virtual RenderObject* createRenderer(RenderArena*, RenderStyle*) { return 0; }

    OwnPtr<ShadowInclusionList> m_inclusions;
};

inline HTMLContentElement* toHTMLContentElement(Node* node)
{
    ASSERT(!node || node->isContentElement());
    return static_cast<HTMLContentElement*>(node);
}

}

#endif
