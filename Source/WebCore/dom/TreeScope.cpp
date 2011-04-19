/*
 * Copyright (C) 2011 Google Inc. All Rights Reserved.
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

#include "config.h"
#include "TreeScope.h"

#include "Element.h"
#include "HTMLAnchorElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "NodeRareData.h"

namespace WebCore {

using namespace HTMLNames;

TreeScope::TreeScope(Document* document, ConstructionType constructionType)
    : ContainerNode(0, constructionType)
    , m_parentTreeScope(0)
    , m_accessKeyMapValid(false)
    , m_numNodeListCaches(0)
{
    m_document = document;
    if (document != this) {
        // Assume document as parent scope
        m_parentTreeScope = document;
        // FIXME: This branch should be inert until shadow scopes are landed.
        ASSERT_NOT_REACHED();
    }
}

TreeScope::~TreeScope()
{
    if (hasRareData())
        rareData()->setTreeScope(0);
}

void TreeScope::destroyTreeScopeData()
{
    m_elementsById.clear();
    m_imageMapsByName.clear();
    m_elementsByAccessKey.clear();
}

void TreeScope::setParentTreeScope(TreeScope* newParentScope)
{
    // A document node cannot be re-parented.
    ASSERT(!isDocumentNode());
    // Every scope other than document needs a parent scope.
    ASSERT(m_parentTreeScope);
    ASSERT(newParentScope);

    m_parentTreeScope = newParentScope;
}

Element* TreeScope::getElementById(const AtomicString& elementId) const
{
    if (elementId.isEmpty())
        return 0;
    return m_elementsById.getElementById(elementId.impl(), this);
}

void TreeScope::addElementById(const AtomicString& elementId, Element* element)
{
    m_elementsById.add(elementId.impl(), element);
}

void TreeScope::removeElementById(const AtomicString& elementId, Element* element)
{
    m_elementsById.remove(elementId.impl(), element);
}

Element* TreeScope::getElementByAccessKey(const String& key) const
{
    if (key.isEmpty())
        return 0;
    if (!m_accessKeyMapValid) {
        for (Node* n = firstChild(); n; n = n->traverseNextNode()) {
            if (!n->isElementNode())
                continue;
            Element* element = static_cast<Element*>(n);
            const AtomicString& accessKey = element->getAttribute(accesskeyAttr);
            if (!accessKey.isEmpty())
                m_elementsByAccessKey.set(accessKey.impl(), element);
        }
        m_accessKeyMapValid = true;
    }
    return m_elementsByAccessKey.get(key.impl());
}

void TreeScope::invalidateAccessKeyMap()
{
    m_accessKeyMapValid = false;
    m_elementsByAccessKey.clear();
}

void TreeScope::addImageMap(HTMLMapElement* imageMap)
{
    AtomicStringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    m_imageMapsByName.add(name, imageMap);
}

void TreeScope::removeImageMap(HTMLMapElement* imageMap)
{
    AtomicStringImpl* name = imageMap->getName().impl();
    if (!name)
        return;
    m_imageMapsByName.remove(name, imageMap);
}

HTMLMapElement* TreeScope::getImageMap(const String& url) const
{
    if (url.isNull())
        return 0;
    size_t hashPos = url.find('#');
    String name = (hashPos == notFound ? url : url.substring(hashPos + 1)).impl();
    if (document()->isHTMLDocument())
        return static_cast<HTMLMapElement*>(m_imageMapsByName.getElementByLowercasedMapName(AtomicString(name.lower()).impl(), this));
    return static_cast<HTMLMapElement*>(m_imageMapsByName.getElementByMapName(AtomicString(name).impl(), this));
}

Element* TreeScope::findAnchor(const String& name)
{
    if (name.isEmpty())
        return 0;
    if (Element* element = getElementById(name))
        return element;
    for (Node* node = this; node; node = node->traverseNextNode()) {
        if (node->hasTagName(aTag)) {
            HTMLAnchorElement* anchor = static_cast<HTMLAnchorElement*>(node);
            if (document()->inQuirksMode()) {
                // Quirks mode, case insensitive comparison of names.
                if (equalIgnoringCase(anchor->name(), name))
                    return anchor;
            } else {
                // Strict mode, names need to match exactly.
                if (anchor->name() == name)
                    return anchor;
            }
        }
    }
    return 0;
}

} // namespace WebCore

