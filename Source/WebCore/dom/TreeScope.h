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

// A class which inherits both Node and TreeScope must call clearRareData() in its destructor
// so that the Node destructor no longer does problematic NodeList cache manipulation in
// the destructor.
class TreeScope {
    friend class Document;
    friend class TreeScopeAdopter;

public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }
    void setParentTreeScope(TreeScope*);

    Element* focusedElement();
    Element* getElementById(const AtomicString&) const;
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

    Document* documentScope() const { return m_documentScope; }

    Node* ancestorInThisScope(Node*) const;

    void addImageMap(HTMLMapElement&);
    void removeImageMap(HTMLMapElement&);
    HTMLMapElement* getImageMap(const String& url) const;

    Element* elementFromPoint(int x, int y) const;

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

    virtual bool applyAuthorStyles() const;

    // Used by the basic DOM mutation methods (e.g., appendChild()).
    void adoptIfNeeded(Node*);

    ContainerNode* rootNode() const { return m_rootNode; }

    IdTargetObserverRegistry& idTargetObserverRegistry() const { return *m_idTargetObserverRegistry.get(); }

    static TreeScope& noDocumentInstance()
    {
        DEFINE_STATIC_LOCAL(TreeScope, instance, ());
        return instance;
    }

    // Nodes belonging to this scope hold self-only references -
    // these are enough to keep the scope from being destroyed, but
    // not enough to keep it from removing its children. This allows a
    // node that outlives its scope to still have a valid document
    // pointer without introducing reference cycles.
    void selfOnlyRef()
    {
        ASSERT(!deletionHasBegun());
        ++m_selfOnlyRefCount;
    }

    void selfOnlyDeref()
    {
        ASSERT(!deletionHasBegun());
        --m_selfOnlyRefCount;
        if (!m_selfOnlyRefCount && !refCount() && this != &noDocumentInstance()) {
            beginDeletion();
            delete this;
        }
    }

    void removedLastRefToScope();

protected:
    TreeScope(ContainerNode*, Document*);
    explicit TreeScope(Document*);
    virtual ~TreeScope();

    void destroyTreeScopeData();
    void clearDocumentScope();
    void setDocumentScope(Document* document)
    {
        ASSERT(document);
        ASSERT(this != &noDocumentInstance());
        m_documentScope = document;
    }

private:
    TreeScope();

    virtual void dropChildren() { }

    int refCount() const;
#ifndef NDEBUG
    bool deletionHasBegun();
    void beginDeletion();
#else
    bool deletionHasBegun() { return false; }
    void beginDeletion() { }
#endif

    ContainerNode* m_rootNode;
    Document* m_documentScope;
    TreeScope* m_parentTreeScope;
    unsigned m_selfOnlyRefCount;

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

Node* nodeFromPoint(Document*, int x, int y, LayoutPoint* localPoint = 0);
TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore

#endif // TreeScope_h
