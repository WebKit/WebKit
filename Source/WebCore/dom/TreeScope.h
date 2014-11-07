/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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

#ifndef TreeScope_h
#define TreeScope_h

#include "DocumentOrderedMap.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class ContainerNode;
class DOMSelection;
class Document;
class Element;
class HTMLLabelElement;
class HTMLMapElement;
class LayoutPoint;
class IdTargetObserverRegistry;
class Node;
class ShadowRoot;

class TreeScope {
    friend class Document;
    friend class TreeScopeAdopter;

public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }
    void setParentTreeScope(TreeScope*);

    Element* focusedElement();
    Element* getElementById(const AtomicString&) const;
    WEBCORE_EXPORT Element* getElementById(const String&) const;
    const Vector<Element*>* getAllElementsById(const AtomicString&) const;
    bool hasElementWithId(const AtomicStringImpl&) const;
    bool containsMultipleElementsWithId(const AtomicString& id) const;
    void addElementById(const AtomicStringImpl& elementId, Element&);
    void removeElementById(const AtomicStringImpl& elementId, Element&);

    Element* getElementByName(const AtomicString&) const;
    bool hasElementWithName(const AtomicStringImpl&) const;
    bool containsMultipleElementsWithName(const AtomicString&) const;
    void addElementByName(const AtomicStringImpl&, Element&);
    void removeElementByName(const AtomicStringImpl&, Element&);

    Document& documentScope() const { return *m_documentScope; }
    static ptrdiff_t documentScopeMemoryOffset() { return OBJECT_OFFSETOF(TreeScope, m_documentScope); }

    Node* ancestorInThisScope(Node*) const;

    void addImageMap(HTMLMapElement&);
    void removeImageMap(HTMLMapElement&);
    HTMLMapElement* getImageMap(const String& url) const;

    // For accessibility.
    bool shouldCacheLabelsByForAttribute() const { return !!m_labelsByForAttribute; }
    void addLabel(const AtomicStringImpl& forAttributeValue, HTMLLabelElement&);
    void removeLabel(const AtomicStringImpl& forAttributeValue, HTMLLabelElement&);
    HTMLLabelElement* labelElementForId(const AtomicString& forAttributeValue);

    DOMSelection* getSelection() const;

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    // Used by the basic DOM mutation methods (e.g., appendChild()).
    void adoptIfNeeded(Node*);

    ContainerNode& rootNode() const { return m_rootNode; }

    IdTargetObserverRegistry& idTargetObserverRegistry() const { return *m_idTargetObserverRegistry.get(); }

protected:
    TreeScope(ShadowRoot&, Document&);
    explicit TreeScope(Document&);
    ~TreeScope();

    void destroyTreeScopeData();
    void setDocumentScope(Document* document)
    {
        ASSERT(document);
        m_documentScope = document;
    }

private:
    ContainerNode& m_rootNode;
    Document* m_documentScope;
    TreeScope* m_parentTreeScope;

    std::unique_ptr<DocumentOrderedMap> m_elementsById;
    std::unique_ptr<DocumentOrderedMap> m_elementsByName;
    std::unique_ptr<DocumentOrderedMap> m_imageMapsByName;
    std::unique_ptr<DocumentOrderedMap> m_labelsByForAttribute;

    std::unique_ptr<IdTargetObserverRegistry> m_idTargetObserverRegistry;

    mutable RefPtr<DOMSelection> m_selection;
};

inline bool TreeScope::hasElementWithId(const AtomicStringImpl& id) const
{
    return m_elementsById && m_elementsById->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithId(const AtomicString& id) const
{
    return m_elementsById && id.impl() && m_elementsById->containsMultiple(*id.impl());
}

inline bool TreeScope::hasElementWithName(const AtomicStringImpl& id) const
{
    return m_elementsByName && m_elementsByName->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithName(const AtomicString& name) const
{
    return m_elementsByName && name.impl() && m_elementsByName->containsMultiple(*name.impl());
}

TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore

#endif // TreeScope_h
