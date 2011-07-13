/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#include "config.h"
#include "ShadowContentSelector.h"

#include "ShadowContentElement.h"
#include "ShadowRoot.h"


namespace WebCore {

ShadowContentSelector* ShadowContentSelector::s_currentInstance = 0;

ShadowContentSelector::ShadowContentSelector(ShadowRoot* shadowRoot)
    : m_parent(s_currentInstance)
    , m_shadowRoot(shadowRoot)
    , m_activeElement(0)
{
    s_currentInstance = this;
    for (Node* node = shadowRoot->shadowHost()->firstChild(); node; node = node->nextSibling())
        m_children.append(node);
}

ShadowContentSelector::~ShadowContentSelector()
{
    ASSERT(s_currentInstance == this);
    s_currentInstance = m_parent;
}

void ShadowContentSelector::selectInclusion(ShadowInclusionList* inclusions)
{
    inclusions->clear();

    for (size_t i = 0; i < m_children.size(); ++i) {
        Node* child = m_children[i].get();
        if (!child)
            continue;
        if (!m_activeElement->shouldInclude(child))
            continue;

        inclusions->append(m_activeElement, child);
        m_children[i] = 0;
    }

}

void ShadowContentSelector::willAttachContentFor(ShadowContentElement* element)
{
    ASSERT(!m_activeElement);
    m_activeElement = element;
}

void ShadowContentSelector::didAttachContent()
{
    ASSERT(m_activeElement);
    m_activeElement = 0;
}

ShadowContentElement* ShadowContentSelector::activeElement() const
{
    return m_activeElement;
}

}
