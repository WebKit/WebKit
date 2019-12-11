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

#pragma once

#include "TreeScopeOrderedMap.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class FloatPoint;
class HTMLImageElement;
class HTMLLabelElement;
class HTMLMapElement;
class LayoutPoint;
class IdTargetObserverRegistry;
class Node;
class RadioButtonGroups;
class ShadowRoot;

class TreeScope {
    friend class Document;

public:
    TreeScope* parentTreeScope() const { return m_parentTreeScope; }
    void setParentTreeScope(TreeScope&);

    Element* focusedElementInScope();
    Element* pointerLockElement() const;

    WEBCORE_EXPORT Element* getElementById(const AtomString&) const;
    WEBCORE_EXPORT Element* getElementById(const String&) const;
    Element* getElementById(StringView) const;
    const Vector<Element*>* getAllElementsById(const AtomString&) const;
    bool hasElementWithId(const AtomStringImpl&) const;
    bool containsMultipleElementsWithId(const AtomString& id) const;
    void addElementById(const AtomStringImpl& elementId, Element&, bool notifyObservers = true);
    void removeElementById(const AtomStringImpl& elementId, Element&, bool notifyObservers = true);

    WEBCORE_EXPORT Element* getElementByName(const AtomString&) const;
    bool hasElementWithName(const AtomStringImpl&) const;
    bool containsMultipleElementsWithName(const AtomString&) const;
    void addElementByName(const AtomStringImpl&, Element&);
    void removeElementByName(const AtomStringImpl&, Element&);

    Document& documentScope() const { return m_documentScope.get(); }
    static ptrdiff_t documentScopeMemoryOffset() { return OBJECT_OFFSETOF(TreeScope, m_documentScope); }

    // https://dom.spec.whatwg.org/#retarget
    Node& retargetToScope(Node&) const;

    WEBCORE_EXPORT Node* ancestorNodeInThisScope(Node*) const;
    WEBCORE_EXPORT Element* ancestorElementInThisScope(Element*) const;

    void addImageMap(HTMLMapElement&);
    void removeImageMap(HTMLMapElement&);
    HTMLMapElement* getImageMap(const AtomString&) const;

    void addImageElementByUsemap(const AtomStringImpl&, HTMLImageElement&);
    void removeImageElementByUsemap(const AtomStringImpl&, HTMLImageElement&);
    HTMLImageElement* imageElementByUsemap(const AtomStringImpl&) const;

    // For accessibility.
    bool shouldCacheLabelsByForAttribute() const { return !!m_labelsByForAttribute; }
    void addLabel(const AtomStringImpl& forAttributeValue, HTMLLabelElement&);
    void removeLabel(const AtomStringImpl& forAttributeValue, HTMLLabelElement&);
    HTMLLabelElement* labelElementForId(const AtomString& forAttributeValue);

    WEBCORE_EXPORT RefPtr<Element> elementFromPoint(double clientX, double clientY);
    WEBCORE_EXPORT Vector<RefPtr<Element>> elementsFromPoint(double clientX, double clientY);
    WEBCORE_EXPORT Vector<RefPtr<Element>> elementsFromPoint(const FloatPoint&);

    // Find first anchor with the given name.
    // First searches for an element with the given ID, but if that fails, then looks
    // for an anchor with the given name. ID matching is always case sensitive, but
    // Anchor name matching is case sensitive in strict mode and not case sensitive in
    // quirks mode for historical compatibility reasons.
    Element* findAnchor(const String& name);

    ContainerNode& rootNode() const { return m_rootNode; }

    IdTargetObserverRegistry& idTargetObserverRegistry() const { return *m_idTargetObserverRegistry.get(); }

    RadioButtonGroups& radioButtonGroups();

protected:
    TreeScope(ShadowRoot&, Document&);
    explicit TreeScope(Document&);
    ~TreeScope();

    void destroyTreeScopeData();
    void setDocumentScope(Document& document)
    {
        m_documentScope = document;
    }

    Node* nodeFromPoint(const LayoutPoint& clientPoint, LayoutPoint* localPoint);

private:

    ContainerNode& m_rootNode;
    std::reference_wrapper<Document> m_documentScope;
    TreeScope* m_parentTreeScope;

    std::unique_ptr<TreeScopeOrderedMap> m_elementsById;
    std::unique_ptr<TreeScopeOrderedMap> m_elementsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imageMapsByName;
    std::unique_ptr<TreeScopeOrderedMap> m_imagesByUsemap;
    std::unique_ptr<TreeScopeOrderedMap> m_labelsByForAttribute;

    std::unique_ptr<IdTargetObserverRegistry> m_idTargetObserverRegistry;
    
    std::unique_ptr<RadioButtonGroups> m_radioButtonGroups;
};

inline bool TreeScope::hasElementWithId(const AtomStringImpl& id) const
{
    return m_elementsById && m_elementsById->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithId(const AtomString& id) const
{
    return m_elementsById && id.impl() && m_elementsById->containsMultiple(*id.impl());
}

inline bool TreeScope::hasElementWithName(const AtomStringImpl& id) const
{
    return m_elementsByName && m_elementsByName->contains(id);
}

inline bool TreeScope::containsMultipleElementsWithName(const AtomString& name) const
{
    return m_elementsByName && name.impl() && m_elementsByName->containsMultiple(*name.impl());
}

TreeScope* commonTreeScope(Node*, Node*);

} // namespace WebCore
