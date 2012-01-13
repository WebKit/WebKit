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

#include "Attribute.h"
#include "Base64.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "HTMLDocument.h"
#include "HTMLDocumentParser.h"
#include "HTMLElement.h"
#include "HTMLHeadElement.h"
#include "Node.h"

#include <wtf/Deque.h>
#include <wtf/RefPtr.h>
#include <wtf/SHA1.h>
#include <wtf/text/CString.h>

using namespace std;

namespace WebCore {

struct DOMEditor::Digest {
    explicit Digest(Node* node) : m_node(node) { }

    String m_sha1;
    String m_attrsSHA1;
    Node* m_node;
    Vector<OwnPtr<Digest> > m_children;
};

DOMEditor::DOMEditor(Document* document) : m_document(document) { }

DOMEditor::~DOMEditor() { }

void DOMEditor::patchDocument(const String& markup)
{
    RefPtr<HTMLDocument> newDocument = HTMLDocument::create(0, KURL());
    RefPtr<DocumentParser> parser = HTMLDocumentParser::create(newDocument.get(), false);
    parser->insert(markup); // Use insert() so that the parser will not yield.
    parser->finish();
    parser->detach();

    ExceptionCode ec = 0;
    innerPatchHTMLElement(m_document->head(), newDocument->head(), ec);
    if (!ec)
        innerPatchHTMLElement(m_document->body(), newDocument->body(), ec);

    if (ec) {
        // Fall back to rewrite.
        m_document->write(markup);
        m_document->close();
    }
}

Node* DOMEditor::patchNode(Node* node, const String& markup, ExceptionCode& ec)
{
    Element* parentElement = node->parentElement();
    Node* previousSibling = node->previousSibling();
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(m_document);
    fragment->parseHTML(markup, parentElement);

    // Compose the old list.
    Vector<OwnPtr<Digest> > oldList;
    for (Node* child = parentElement->firstChild(); child; child = child->nextSibling())
        oldList.append(createDigest(child, 0));

    // Compose the new list.
    Vector<OwnPtr<Digest> > newList;
    for (Node* child = parentElement->firstChild(); child != node; child = child->nextSibling())
        newList.append(createDigest(child, 0));
    for (Node* child = fragment->firstChild(); child; child = child->nextSibling())
        newList.append(createDigest(child, &m_unusedNodesMap));
    for (Node* child = node->nextSibling(); child; child = child->nextSibling())
        newList.append(createDigest(child, 0));

    innerPatchChildren(parentElement, oldList, newList, ec);
    if (ec) {
        // Fall back to total replace.
        ec = 0;
        parentElement->replaceChild(fragment.release(), node, ec);
        if (ec)
            return 0;
    }
    return previousSibling ? previousSibling->nextSibling() : parentElement->firstChild();
}

void DOMEditor::innerPatchHTMLElement(HTMLElement* oldElement, HTMLElement* newElement, ExceptionCode& ec)
{
    if (oldElement)  {
        if (newElement) {
            OwnPtr<Digest> oldInfo = createDigest(oldElement, 0);
            OwnPtr<Digest> newInfo = createDigest(newElement, &m_unusedNodesMap);
            innerPatchNode(oldInfo.get(), newInfo.get(), ec);
            if (ec)
                return;
        }
        oldElement->removeAllChildren();
        return;
    }
    if (newElement)
        m_document->documentElement()->appendChild(newElement, ec);
}

void DOMEditor::innerPatchNode(Digest* oldDigest, Digest* newDigest, ExceptionCode& ec)
{
    if (oldDigest->m_sha1 == newDigest->m_sha1)
        return;

    Node* oldNode = oldDigest->m_node;
    Node* newNode = newDigest->m_node;

    if (newNode->nodeType() != oldNode->nodeType() || newNode->nodeName() != oldNode->nodeName()) {
        oldNode->parentNode()->replaceChild(newNode, oldNode, ec);
        return;
    }

    if (oldNode->nodeValue() != newNode->nodeValue())
        oldNode->setNodeValue(newNode->nodeValue(), ec);
    if (ec)
        return;

    if (oldNode->nodeType() != Node::ELEMENT_NODE)
        return;

    // Patch attributes
    Element* oldElement = static_cast<Element*>(oldNode);
    Element* newElement = static_cast<Element*>(newNode);
    if (oldDigest->m_attrsSHA1 != newDigest->m_attrsSHA1) {
        NamedNodeMap* oldAttributeMap = oldElement->attributeMap();

        while (oldAttributeMap && oldAttributeMap->length())
            oldAttributeMap->removeAttribute(0);

        NamedNodeMap* newAttributeMap = newElement->attributeMap();
        if (newAttributeMap) {
            size_t numAttrs = newAttributeMap->length();
            for (size_t i = 0; i < numAttrs; ++i) {
                const Attribute* attribute = newAttributeMap->attributeItem(i);
                oldElement->setAttribute(attribute->name(), attribute->value());
            }
        }
    }

    innerPatchChildren(oldElement, oldDigest->m_children, newDigest->m_children, ec);
    m_unusedNodesMap.remove(newDigest->m_sha1);
}

pair<DOMEditor::ResultMap, DOMEditor::ResultMap>
DOMEditor::diff(const Vector<OwnPtr<Digest> >& oldList, const Vector<OwnPtr<Digest> >& newList)
{
    ResultMap newMap(newList.size());
    ResultMap oldMap(oldList.size());

    for (size_t i = 0; i < oldMap.size(); ++i) {
        oldMap[i].first = 0;
        oldMap[i].second = -1;
    }

    for (size_t i = 0; i < newMap.size(); ++i) {
        newMap[i].first = 0;
        newMap[i].second = -1;
    }

    // Trim head and tail.
    for (size_t i = 0; i < oldList.size() && i < newList.size() && oldList[i]->m_sha1 == newList[i]->m_sha1; ++i) {
        oldMap[i].first = oldList[i].get();
        oldMap[i].second = i;
        newMap[i].first = newList[i].get();
        newMap[i].second = i;
    }
    for (size_t i = 0; i < oldList.size() && i < newList.size() && oldList[oldList.size() - i - 1]->m_sha1 == newList[newList.size() - i - 1]->m_sha1; ++i) {
        size_t oldIndex = oldList.size() - i - 1;
        size_t newIndex = newList.size() - i - 1;
        oldMap[oldIndex].first = oldList[oldIndex].get();
        oldMap[oldIndex].second = newIndex;
        newMap[newIndex].first = newList[newIndex].get();
        newMap[newIndex].second = oldIndex;
    }

    typedef HashMap<String, Vector<size_t> > DiffTable;
    DiffTable newTable;
    DiffTable oldTable;

    for (size_t i = 0; i < newList.size(); ++i) {
        DiffTable::iterator it = newTable.add(newList[i]->m_sha1, Vector<size_t>()).first;
        it->second.append(i);
    }

    for (size_t i = 0; i < oldList.size(); ++i) {
        DiffTable::iterator it = oldTable.add(oldList[i]->m_sha1, Vector<size_t>()).first;
        it->second.append(i);
    }

    for (DiffTable::iterator newIt = newTable.begin(); newIt != newTable.end(); ++newIt) {
        if (newIt->second.size() != 1)
            continue;

        DiffTable::iterator oldIt = oldTable.find(newIt->first);
        if (oldIt == oldTable.end() || oldIt->second.size() != 1)
            continue;

        newMap[newIt->second[0]] = make_pair(newList[newIt->second[0]].get(), oldIt->second[0]);
        oldMap[oldIt->second[0]] = make_pair(oldList[oldIt->second[0]].get(), newIt->second[0]);
    }

    for (size_t i = 0; newList.size() > 0 && i < newList.size() - 1; ++i) {
        if (!newMap[i].first || newMap[i + 1].first)
            continue;

        size_t j = newMap[i].second + 1;
        if (j < oldMap.size() && !oldMap[j].first && newList[i + 1]->m_sha1 == oldList[j]->m_sha1) {
            newMap[i + 1] = make_pair(newList[i + 1].get(), j);
            oldMap[j] = make_pair(oldList[j].get(), i + 1);
        }
    }

    for (size_t i = newList.size() - 1; newList.size() > 0 && i > 0; --i) {
        if (!newMap[i].first || newMap[i - 1].first || newMap[i].second <= 0)
            continue;

        size_t j = newMap[i].second - 1;
        if (!oldMap[j].first && newList[i - 1]->m_sha1 == oldList[j]->m_sha1) {
            newMap[i - 1] = make_pair(newList[i - 1].get(), j);
            oldMap[j] = make_pair(oldList[j].get(), i - 1);
        }
    }

    return make_pair(oldMap, newMap);
}

void DOMEditor::innerPatchChildren(Element* element, const Vector<OwnPtr<Digest> >& oldList, const Vector<OwnPtr<Digest> >& newList, ExceptionCode& ec)
{
    pair<ResultMap, ResultMap> resultMaps = diff(oldList, newList);
    ResultMap& oldMap = resultMaps.first;
    ResultMap& newMap = resultMaps.second;

    // 1. First strip everything except for the nodes that retain. Collect pending merges.
    HashMap<Digest*, Digest*> merges;
    for (size_t i = 0; i < oldList.size(); ++i) {
        if (oldMap[i].first)
            continue;

        // Check if this change is between stable nodes. If it is, consider it as "modified".
        if (!m_unusedNodesMap.contains(oldList[i]->m_sha1) && (!i || oldMap[i - 1].first) && (i == oldMap.size() - 1 || oldMap[i + 1].first)) {
            size_t anchorCandidate = i ? oldMap[i - 1].second + 1 : 0;
            size_t anchorAfter = i == oldMap.size() - 1 ? anchorCandidate + 1 : oldMap[i + 1].second;
            if (anchorAfter - anchorCandidate == 1 && anchorCandidate < newList.size())
                merges.set(newList[anchorCandidate].get(), oldList[i].get());
            else {
                removeChild(oldList[i].get(), ec);
                if (ec)
                    return;
            }
        } else {
            removeChild(oldList[i].get(), ec);
            if (ec)
                return;
        }
    }

    // Mark retained nodes as used.
    for (size_t i = 0; i < newList.size(); ++i) {
        if (newMap[i].first)
            markNodeAsUsed(newMap[i].first);
    }

    // 2. Patch nodes marked for merge.
    for (HashMap<Digest*, Digest*>::iterator it = merges.begin(); it != merges.end(); ++it) {
        innerPatchNode(it->second, it->first, ec);
        if (ec)
            return;
    }

    // 3. Insert missing nodes.
    for (size_t i = 0; i < newMap.size(); ++i) {
        if (newMap[i].first || merges.contains(newList[i].get()))
            continue;

        ExceptionCode ec = 0;
        insertBefore(element, newList[i].get(), element->childNode(i), ec);
        if (ec)
            return;
    }

    // 4. Then put all nodes that retained into their slots (sort by new index).
    for (size_t i = 0; i < oldMap.size(); ++i) {
        if (!oldMap[i].first)
            continue;
        RefPtr<Node> node = oldMap[i].first->m_node;
        Node* anchorNode = element->childNode(oldMap[i].second);
        if (node.get() == anchorNode)
            continue;

        element->insertBefore(node, anchorNode, ec);
        if (ec)
            return;
    }
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    CString cString = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(cString.data()), cString.length());
}

PassOwnPtr<DOMEditor::Digest> DOMEditor::createDigest(Node* node, UnusedNodesMap* unusedNodesMap)
{
    Digest* digest = new Digest(node);

    SHA1 sha1;

    Node::NodeType nodeType = node->nodeType();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(&nodeType), sizeof(nodeType));
    addStringToSHA1(sha1, node->nodeName());
    addStringToSHA1(sha1, node->nodeValue());

    if (node->nodeType() == Node::ELEMENT_NODE) {
        Node* child = node->firstChild();
        while (child) {
            OwnPtr<Digest> childInfo = createDigest(child, unusedNodesMap);
            addStringToSHA1(sha1, childInfo->m_sha1);
            child = child->nextSibling();
            digest->m_children.append(childInfo.release());
        }
        Element* element = static_cast<Element*>(node);

        NamedNodeMap* attrMap = element->attributeMap();
        if (attrMap && attrMap->length()) {
            size_t numAttrs = attrMap->length();
            SHA1 attrsSHA1;
            for (size_t i = 0; i < numAttrs; ++i) {
                const Attribute* attribute = attrMap->attributeItem(i);
                addStringToSHA1(attrsSHA1, attribute->name().toString());
                addStringToSHA1(attrsSHA1, attribute->value());
            }
            Vector<uint8_t, 20> attrsHash;
            attrsSHA1.computeHash(attrsHash);
            digest->m_attrsSHA1 = base64Encode(reinterpret_cast<const char*>(attrsHash.data()), 10);
            addStringToSHA1(sha1, digest->m_attrsSHA1);
        }
    }

    Vector<uint8_t, 20> hash;
    sha1.computeHash(hash);
    digest->m_sha1 = base64Encode(reinterpret_cast<const char*>(hash.data()), 10);
    if (unusedNodesMap)
        unusedNodesMap->add(digest->m_sha1, digest);
    return adoptPtr(digest);
}

void DOMEditor::insertBefore(Element* parentElement, Digest* digest, Node* anchor, ExceptionCode& ec)
{
    parentElement->insertBefore(digest->m_node, anchor, ec);
    markNodeAsUsed(digest);
}

void DOMEditor::removeChild(Digest* oldDigest, ExceptionCode& ec)
{
    RefPtr<Node> oldNode = oldDigest->m_node;
    oldNode->parentNode()->removeChild(oldNode.get(), ec);

    // Diff works within levels. In order not to lose the node identity when user
    // prepends his HTML with "<div>" (i.e. all nodes are shifted to the next nested level),
    // prior to dropping the original node on the floor, check whether new DOM has a digest
    // with matching sha1. If it does, replace it with the original DOM chunk. Chances are
    // high that it will get merged back into the original DOM during the further patching.

    UnusedNodesMap::iterator it = m_unusedNodesMap.find(oldDigest->m_sha1);
    if (it != m_unusedNodesMap.end()) {
        Digest* newDigest = it->second;
        Node* newNode = newDigest->m_node;
        newNode->parentNode()->replaceChild(oldNode, newNode, ec);
        newDigest->m_node = oldNode.get();
        markNodeAsUsed(newDigest);
        return;
    }

    for (size_t i = 0; i < oldDigest->m_children.size(); ++i)
        removeChild(oldDigest->m_children[i].get(), ec);
}

void DOMEditor::markNodeAsUsed(Digest* digest)
{
    Deque<Digest*> queue;
    queue.append(digest);
    while (!queue.isEmpty()) {
        Digest* first = queue.takeFirst();
        m_unusedNodesMap.remove(first->m_sha1);
        for (size_t i = 0; i < first->m_children.size(); ++i)
            queue.append(first->m_children[i].get());
    }
}

void DOMEditor::dumpMap(const String& name, const DOMEditor::ResultMap& map, const Vector<OwnPtr<Digest> >& list)
{
    for (size_t i = 0; i < map.size(); ++i)
        fprintf(stderr, "%s[%ld] %s - %ld\n", name.utf8().data(), i, map[i].first ? map[i].first->m_sha1.utf8().data() : list[i]->m_sha1.utf8().data(), map[i].second);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
