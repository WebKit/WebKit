/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SelectorNodeList.h"

#include "CSSSelector.h"
#include "CSSStyleSelector.h"
#include "Document.h"
#include "Element.h"
#include "Node.h"

namespace WebCore {

SelectorNodeList::SelectorNodeList(PassRefPtr<Node> rootNode, CSSSelector* querySelector)
{
    Document* document = rootNode->document();
    CSSStyleSelector* styleSelector = document->styleSelector();
    for (Node* n = rootNode->firstChild(); n; n = n->traverseNextNode(rootNode.get())) {
        if (n->isElementNode()) {
            Element* element = static_cast<Element*>(n);
            styleSelector->initElementAndPseudoState(element);
            styleSelector->initForStyleResolve(element, 0);
            for (CSSSelector* selector = querySelector; selector; selector = selector->next()) {
                if (styleSelector->checkSelector(selector)) {
                    m_nodes.append(n);
                    break;
                }
            }
        }
    }
}

} // namespace WebCore
