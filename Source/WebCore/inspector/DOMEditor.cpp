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

#include "config.h"
#include "DOMEditor.h"

#if ENABLE(INSPECTOR)

#include "Base64.h"
#include "Document.h"
#include "HTMLDocument.h"
#include "HTMLDocumentParser.h"
#include "HTMLElement.h"
#include "HTMLHeadElement.h"
#include "Node.h"

#include <wtf/RefPtr.h>
#include <wtf/SHA1.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

struct DOMEditor::NodeDigest {
    NodeDigest(Node* node) : m_node(node) { }

    String m_digest;
    String m_attrsDigest;
    Node* m_node;
    Vector<OwnPtr<NodeDigest> > m_children;
};

DOMEditor::DOMEditor(Document* document) : m_document(document) { }

DOMEditor::~DOMEditor() { }

void DOMEditor::patch(const String& markup)
{
    RefPtr<HTMLDocument> newDocument = HTMLDocument::create(0, KURL());
    RefPtr<DocumentParser> parser = HTMLDocumentParser::create(newDocument.get(), false);
    parser->insert(markup); // Use insert() so that the parser will not yield.
    parser->finish();
    parser->detach();

    if (!patchElement(m_document->head(), newDocument->head()) || !patchElement(m_document->body(), newDocument->body())) {
        // Fall back to rewrite.
        m_document->write(markup);
        m_document->close();
    }
}

bool DOMEditor::patchElement(Element* oldElement, Element* newElement)
{
    if (oldElement)  {
        if (newElement) {
            OwnPtr<NodeDigest> oldInfo = createNodeDigest(oldElement);
            OwnPtr<NodeDigest> newInfo = createNodeDigest(newElement);
            return patchNode(oldInfo.get(), newInfo.get());
        }
        oldElement->removeAllChildren();
        return true;
    }
    if (newElement) {
        ExceptionCode ec = 0;
        m_document->documentElement()->appendChild(newElement, ec);
        return !ec;
    }
    return true;
}

bool DOMEditor::patchNode(NodeDigest* oldDigest, NodeDigest* newDigest)
{
    if (oldDigest->m_digest == newDigest->m_digest)
        return true;

    Node* oldNode = oldDigest->m_node;
    Node* newNode = newDigest->m_node;

    if (newNode->nodeType() != oldNode->nodeType() || newNode->nodeName() != oldNode->nodeName()) {
        ExceptionCode ec = 0;
        oldNode->parentNode()->replaceChild(newNode, oldNode, ec);
        if (ec)
            return false;
        return true;
    }

    ExceptionCode ec = 0;
    if (oldNode->nodeValue() != newNode->nodeValue())
        oldNode->setNodeValue(newNode->nodeValue(), ec);
    if (ec)
        return false;

    if (oldNode->nodeType() == Node::ELEMENT_NODE && oldDigest->m_attrsDigest != newDigest->m_attrsDigest) {
        Element* oldElement = static_cast<Element*>(oldNode);
        Element* newElement = static_cast<Element*>(newNode);
        oldElement->setAttributesFromElement(*newElement);
    }
    if (oldDigest->m_node->nodeType() != Node::ELEMENT_NODE)
        return true;

    return patchChildren(static_cast<ContainerNode*>(oldNode), oldDigest->m_children, newDigest->m_children);
}

bool DOMEditor::patchChildren(ContainerNode* oldParent, Vector<OwnPtr<NodeDigest> >& oldList, Vector<OwnPtr<NodeDigest> >& newList)
{
    // Trim tail.
    size_t offset = 0;
    for (; offset < oldList.size() && offset < newList.size() && oldList[oldList.size() - offset - 1]->m_digest == newList[newList.size() - offset - 1]->m_digest; ++offset) { }
    if (offset > 0) {
        oldList.resize(oldList.size() - offset);
        newList.resize(newList.size() - offset);
    }

    // Trim head.
    for (offset = 0; offset < oldList.size() && offset < newList.size() && oldList[offset]->m_digest == newList[offset]->m_digest; ++offset) { }
    if (offset > 0) {
        oldList.remove(0, offset);
        newList.remove(0, offset);
    }

    // Diff the children lists.
    typedef Vector<pair<NodeDigest*, size_t> > ResultMap;
    ResultMap newMap(newList.size());
    ResultMap oldMap(oldList.size());

    for (size_t i = 0; i < oldMap.size(); ++i)
        oldMap[i].first = 0;
    for (size_t i = 0; i < newMap.size(); ++i)
        newMap[i].first = 0;

    typedef HashMap<String, Vector<size_t> > DiffTable;
    DiffTable newTable;
    DiffTable oldTable;

    for (size_t i = 0; i < newList.size(); ++i) {
        DiffTable::iterator it = newTable.add(newList[i]->m_digest, Vector<size_t>()).first;
        it->second.append(i);
    }

    for (size_t i = 0; i < oldList.size(); ++i) {
        DiffTable::iterator it = oldTable.add(oldList[i]->m_digest, Vector<size_t>()).first;
        it->second.append(i);
    }

    for (DiffTable::iterator newIt = newTable.begin(); newIt != newTable.end(); ++newIt) {
        if (newIt->second.size() != 1)
            continue;

        DiffTable::iterator oldIt = oldTable.find(newIt->first);
        if (oldIt == oldTable.end() || oldIt->second.size() != 1)
            continue;
        make_pair(newList[newIt->second[0]].get(), oldIt->second[0]);
        newMap[newIt->second[0]] = make_pair(newList[newIt->second[0]].get(), oldIt->second[0]);
        oldMap[oldIt->second[0]] = make_pair(oldList[oldIt->second[0]].get(), newIt->second[0]);
    }

    for (size_t i = 0; newList.size() > 0 && i < newList.size() - 1; ++i) {
        if (!newMap[i].first || newMap[i + 1].first)
            continue;

        size_t j = newMap[i].second + 1;
        if (j < oldMap.size() && !oldMap[j].first && newList[i + 1]->m_digest == oldList[j]->m_digest) {
            newMap[i + 1] = make_pair(newList[i + 1].get(), j);
            oldMap[j] = make_pair(oldList[j].get(), i + 1);
        }
    }

    for (size_t i = newList.size() - 1; newList.size() > 0 && i > 0; --i) {
        if (!newMap[i].first || newMap[i - 1].first || newMap[i].second <= 0)
            continue;

        size_t j = newMap[i].second - 1;
        if (!oldMap[j].first && newList[i - 1]->m_digest == oldList[j]->m_digest) {
            newMap[i - 1] = make_pair(newList[i - 1].get(), j);
            oldMap[j] = make_pair(oldList[j].get(), i - 1);
        }
    }

    HashSet<NodeDigest*> merges;
    for (size_t i = 0; i < oldList.size(); ++i) {
        if (oldMap[i].first)
            continue;

        // Check if this change is between stable nodes. If it is, consider it as "modified".
        if ((!i || oldMap[i - 1].first) && (i == oldMap.size() - 1 || oldMap[i + 1].first)) {
            size_t anchorBefore = i ? oldMap[i - 1].second : -1;
            size_t anchorAfter = i == oldMap.size() - 1 ? anchorBefore + 2 : oldMap[i + 1].second;
            if (anchorAfter - anchorBefore == 2 && anchorBefore + 1 < newList.size()) {
                if (!patchNode(oldList[i].get(), newList[anchorBefore + 1].get()))
                    return false;

                merges.add(newList[anchorBefore + 1].get());
            } else
                oldList[i]->m_node->parentNode()->removeChild(oldList[i]->m_node);
        } else {
            ContainerNode* parentNode = static_cast<ContainerNode*>(oldList[i]->m_node->parentNode());
            parentNode->removeChild(oldList[i]->m_node);
        }
    }

    for (size_t i = 0; i < newMap.size(); ++i) {
        if (newMap[i].first || merges.contains(newList[i].get()))
            continue;

        ExceptionCode ec = 0;
        oldParent->insertBefore(newList[i]->m_node, oldParent->childNode(i + offset), ec);
        return !ec;
    }

    return true;
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    CString cString = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(cString.data()), cString.length());
}

PassOwnPtr<DOMEditor::NodeDigest> DOMEditor::createNodeDigest(Node* node)
{
    NodeDigest* nodeDigest = new NodeDigest(node);

    SHA1 sha1;

    Node::NodeType nodeType = node->nodeType();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(&nodeType), sizeof(nodeType));
    addStringToSHA1(sha1, node->nodeName());
    addStringToSHA1(sha1, node->nodeValue());

    if (node->nodeType() == Node::ELEMENT_NODE) {
        Node* child = node->firstChild();
        while (child) {
            OwnPtr<NodeDigest> childInfo = createNodeDigest(child);
            addStringToSHA1(sha1, childInfo->m_digest);
            child = child->nextSibling();
            nodeDigest->m_children.append(childInfo.release());
        }

        Element* element = static_cast<Element*>(node);
        const NamedNodeMap* attrMap = element->attributes(true);
        if (attrMap && attrMap->length()) {
            unsigned numAttrs = attrMap->length();
            SHA1 attrsSHA1;
            for (unsigned i = 0; i < numAttrs; ++i) {
                const Attribute* attribute = attrMap->attributeItem(i);
                addStringToSHA1(attrsSHA1, attribute->name().toString());
                addStringToSHA1(attrsSHA1, attribute->value());
            }
            Vector<uint8_t, 20> attrsHash;
            attrsSHA1.computeHash(attrsHash);
            nodeDigest->m_attrsDigest = base64Encode(reinterpret_cast<const char*>(attrsHash.data()), 10);
            addStringToSHA1(sha1, nodeDigest->m_attrsDigest);
        }
    }

    Vector<uint8_t, 20> hash;
    sha1.computeHash(hash);
    nodeDigest->m_digest = base64Encode(reinterpret_cast<const char*>(hash.data()), 10);
    return adoptPtr(nodeDigest);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
