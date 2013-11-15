/*
 * Copyright (C) 2013 Adobe Systems Inc. All rights reserved.
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef InspectorNodeFinder_h
#define InspectorNodeFinder_h

#include <wtf/ListHashSet.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Attribute;
class Element;
class Node;

class InspectorNodeFinder {
public:
    InspectorNodeFinder(String whitespaceTrimmedQuery);
    void performSearch(Node*);
    const ListHashSet<Node*>& results() const { return m_results; }

private:
    bool matchesAttribute(const Attribute&);
    bool matchesElement(const Element&);

    void searchUsingDOMTreeTraversal(Node*);
    void searchUsingXPath(Node*);
    void searchUsingCSSSelectors(Node*);

    bool m_startTagFound;
    bool m_endTagFound;
    bool m_exactAttributeMatch;

    String m_whitespaceTrimmedQuery;
    String m_tagNameQuery;
    String m_attributeQuery;

    ListHashSet<Node*> m_results;
};

} // namespace WebCore

#endif // InspectorNodeFinder_h
