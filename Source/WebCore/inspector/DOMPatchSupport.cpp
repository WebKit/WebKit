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
#include "DOMPatchSupport.h"

#include "Attribute.h"
#include "DOMEditor.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "HTMLDocument.h"
#include "HTMLDocumentParser.h"
#include "HTMLElement.h"
#include "HTMLHeadElement.h"
#include "HTMLNames.h"
#include "InspectorHistory.h"
#include "Node.h"
#include "XMLDocument.h"
#include "XMLDocumentParser.h"
#include <wtf/Deque.h>
#include <wtf/HashTraits.h>
#include <wtf/RefPtr.h>
#include <wtf/SHA1.h>
#include <wtf/text/Base64.h>
#include <wtf/text/CString.h>

namespace WebCore {

using HTMLNames::bodyTag;
using HTMLNames::headTag;
using HTMLNames::htmlTag;

struct DOMPatchSupport::Digest {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    String sha1;
    String attrsSHA1;
    Node* node;
    Vector<std::unique_ptr<Digest>> children;
};

DOMPatchSupport::DOMPatchSupport(DOMEditor& domEditor, Document& document)
    : m_domEditor(domEditor)
    , m_document(document)
{
}

void DOMPatchSupport::patchDocument(const String& markup)
{
    RefPtr<Document> newDocument;
    if (m_document.isHTMLDocument())
        newDocument = HTMLDocument::create(nullptr, URL());
    else if (m_document.isXHTMLDocument())
        newDocument = XMLDocument::createXHTML(nullptr, URL());
    else if (m_document.isSVGDocument())
        newDocument = XMLDocument::create(nullptr, URL());

    ASSERT(newDocument);
    RefPtr<DocumentParser> parser;
    if (newDocument->isHTMLDocument())
        parser = HTMLDocumentParser::create(static_cast<HTMLDocument&>(*newDocument));
    else
        parser = XMLDocumentParser::create(*newDocument, nullptr);
    parser->insert(markup); // Use insert() so that the parser will not yield.
    parser->finish();
    parser->detach();

    if (!m_document.documentElement())
        return;
    if (!newDocument->documentElement())
        return;

    std::unique_ptr<Digest> oldInfo = createDigest(*m_document.documentElement(), nullptr);
    std::unique_ptr<Digest> newInfo = createDigest(*newDocument->documentElement(), &m_unusedNodesMap);

    if (innerPatchNode(*oldInfo, *newInfo).hasException()) {
        // Fall back to rewrite.
        m_document.write(nullptr, markup);
        m_document.close();
    }
}

ExceptionOr<Node*> DOMPatchSupport::patchNode(Node& node, const String& markup)
{
    // Don't parse <html> as a fragment.
    if (node.isDocumentNode() || (node.parentNode() && node.parentNode()->isDocumentNode())) {
        patchDocument(markup);
        return nullptr;
    }

    Node* previousSibling = node.previousSibling();
    // FIXME: This code should use one of createFragment* in markup.h
    auto fragment = DocumentFragment::create(m_document);
    if (m_document.isHTMLDocument())
        fragment->parseHTML(markup, node.parentElement() ? node.parentElement() : m_document.documentElement());
    else
        fragment->parseXML(markup, node.parentElement() ? node.parentElement() : m_document.documentElement());

    // Compose the old list.
    auto* parentNode = node.parentNode();
    Vector<std::unique_ptr<Digest>> oldList;
    for (Node* child = parentNode->firstChild(); child; child = child->nextSibling())
        oldList.append(createDigest(*child, nullptr));

    // Compose the new list.
    Vector<std::unique_ptr<Digest>> newList;
    for (Node* child = parentNode->firstChild(); child != &node; child = child->nextSibling())
        newList.append(createDigest(*child, nullptr));
    for (Node* child = fragment->firstChild(); child; child = child->nextSibling()) {
        if (child->hasTagName(headTag) && !child->firstChild() && !markup.containsIgnoringASCIICase("</head>"))
            continue; // HTML5 parser inserts empty <head> tag whenever it parses <body>
        if (child->hasTagName(bodyTag) && !child->firstChild() && !markup.containsIgnoringASCIICase("</body>"))
            continue; // HTML5 parser inserts empty <body> tag whenever it parses </head>
        newList.append(createDigest(*child, &m_unusedNodesMap));
    }
    for (Node* child = node.nextSibling(); child; child = child->nextSibling())
        newList.append(createDigest(*child, nullptr));

    if (innerPatchChildren(*parentNode, oldList, newList).hasException()) {
        // Fall back to total replace.
        auto result = m_domEditor.replaceChild(*parentNode, fragment.get(), node);
        if (result.hasException())
            return result.releaseException();
    }
    return previousSibling ? previousSibling->nextSibling() : parentNode->firstChild();
}

ExceptionOr<void> DOMPatchSupport::innerPatchNode(Digest& oldDigest, Digest& newDigest)
{
    if (oldDigest.sha1 == newDigest.sha1)
        return { };

    auto& oldNode = *oldDigest.node;
    auto& newNode = *newDigest.node;

    if (newNode.nodeType() != oldNode.nodeType() || newNode.nodeName() != oldNode.nodeName())
        return m_domEditor.replaceChild(*oldNode.parentNode(), newNode, oldNode);

    if (oldNode.nodeValue() != newNode.nodeValue()) {
        auto result = m_domEditor.setNodeValue(oldNode, newNode.nodeValue());
        if (result.hasException())
            return result.releaseException();
    }

    if (!is<Element>(oldNode))
        return { };

    // Patch attributes
    auto& oldElement = downcast<Element>(oldNode);
    auto& newElement = downcast<Element>(newNode);
    if (oldDigest.attrsSHA1 != newDigest.attrsSHA1) {
        // FIXME: Create a function in Element for removing all properties. Take in account whether did/willModifyAttribute are important.
        if (oldElement.hasAttributesWithoutUpdate()) {
            while (oldElement.attributeCount()) {
                auto result = m_domEditor.removeAttribute(oldElement, oldElement.attributeAt(0).localName());
                if (result.hasException())
                    return result.releaseException();
            }
        }

        // FIXME: Create a function in Element for copying properties. cloneDataFromElement() is close but not enough for this case.
        if (newElement.hasAttributesWithoutUpdate()) {
            for (auto& attribute : newElement.attributesIterator()) {
                auto result = m_domEditor.setAttribute(oldElement, attribute.name().localName(), attribute.value());
                if (result.hasException())
                    return result.releaseException();
            }
        }
    }

    auto result = innerPatchChildren(oldElement, oldDigest.children, newDigest.children);
    m_unusedNodesMap.remove(newDigest.sha1);
    return result;
}

std::pair<DOMPatchSupport::ResultMap, DOMPatchSupport::ResultMap>
DOMPatchSupport::diff(const Vector<std::unique_ptr<Digest>>& oldList, const Vector<std::unique_ptr<Digest>>& newList)
{
    ResultMap newMap(newList.size());
    ResultMap oldMap(oldList.size());

    for (auto& result : oldMap) {
        result.first = nullptr;
        result.second = 0;
    }

    for (auto& result : newMap) {
        result.first = nullptr;
        result.second = 0;
    }

    // Trim head and tail.
    for (size_t i = 0; i < oldList.size() && i < newList.size() && oldList[i]->sha1 == newList[i]->sha1; ++i) {
        oldMap[i].first = oldList[i].get();
        oldMap[i].second = i;
        newMap[i].first = newList[i].get();
        newMap[i].second = i;
    }
    for (size_t i = 0; i < oldList.size() && i < newList.size() && oldList[oldList.size() - i - 1]->sha1 == newList[newList.size() - i - 1]->sha1; ++i) {
        size_t oldIndex = oldList.size() - i - 1;
        size_t newIndex = newList.size() - i - 1;
        oldMap[oldIndex].first = oldList[oldIndex].get();
        oldMap[oldIndex].second = newIndex;
        newMap[newIndex].first = newList[newIndex].get();
        newMap[newIndex].second = oldIndex;
    }

    typedef HashMap<String, Vector<size_t>> DiffTable;
    DiffTable newTable;
    DiffTable oldTable;

    for (size_t i = 0; i < newList.size(); ++i)
        newTable.add(newList[i]->sha1, Vector<size_t>()).iterator->value.append(i);

    for (size_t i = 0; i < oldList.size(); ++i)
        oldTable.add(oldList[i]->sha1, Vector<size_t>()).iterator->value.append(i);

    for (auto& newEntry : newTable) {
        if (newEntry.value.size() != 1)
            continue;

        auto oldIt = oldTable.find(newEntry.key);
        if (oldIt == oldTable.end() || oldIt->value.size() != 1)
            continue;

        newMap[newEntry.value[0]] = std::make_pair(newList[newEntry.value[0]].get(), oldIt->value[0]);
        oldMap[oldIt->value[0]] = std::make_pair(oldList[oldIt->value[0]].get(), newEntry.value[0]);
    }

    for (size_t i = 0; newList.size() > 0 && i < newList.size() - 1; ++i) {
        if (!newMap[i].first || newMap[i + 1].first)
            continue;

        size_t j = newMap[i].second + 1;
        if (j < oldMap.size() && !oldMap[j].first && newList[i + 1]->sha1 == oldList[j]->sha1) {
            newMap[i + 1] = std::make_pair(newList[i + 1].get(), j);
            oldMap[j] = std::make_pair(oldList[j].get(), i + 1);
        }
    }

    for (size_t i = newList.size() - 1; newList.size() > 0 && i > 0; --i) {
        if (!newMap[i].first || newMap[i - 1].first || newMap[i].second <= 0)
            continue;

        size_t j = newMap[i].second - 1;
        if (!oldMap[j].first && newList[i - 1]->sha1 == oldList[j]->sha1) {
            newMap[i - 1] = std::make_pair(newList[i - 1].get(), j);
            oldMap[j] = std::make_pair(oldList[j].get(), i - 1);
        }
    }

#ifdef DEBUG_DOM_PATCH_SUPPORT
    dumpMap(oldMap, "OLD");
    dumpMap(newMap, "NEW");
#endif

    return std::make_pair(oldMap, newMap);
}

ExceptionOr<void> DOMPatchSupport::innerPatchChildren(ContainerNode& parentNode, const Vector<std::unique_ptr<Digest>>& oldList, const Vector<std::unique_ptr<Digest>>& newList)
{
    auto resultMaps = diff(oldList, newList);
    ResultMap& oldMap = resultMaps.first;
    ResultMap& newMap = resultMaps.second;

    Digest* oldHead = nullptr;
    Digest* oldBody = nullptr;

    // 1. First strip everything except for the nodes that retain. Collect pending merges.
    HashMap<Digest*, Digest*> merges;
    HashSet<size_t, WTF::IntHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> usedNewOrdinals;
    for (size_t i = 0; i < oldList.size(); ++i) {
        if (oldMap[i].first) {
            if (!usedNewOrdinals.contains(oldMap[i].second)) {
                usedNewOrdinals.add(oldMap[i].second);
                continue;
            }
            oldMap[i].first = nullptr;
            oldMap[i].second = 0;
        }

        // Always match <head> and <body> tags with each other - we can't remove them from the DOM
        // upon patching.
        if (oldList[i]->node->hasTagName(headTag)) {
            oldHead = oldList[i].get();
            continue;
        }
        if (oldList[i]->node->hasTagName(bodyTag)) {
            oldBody = oldList[i].get();
            continue;
        }

        // Check if this change is between stable nodes. If it is, consider it as "modified".
        if (!m_unusedNodesMap.contains(oldList[i]->sha1) && (!i || oldMap[i - 1].first) && (i == oldMap.size() - 1 || oldMap[i + 1].first)) {
            size_t anchorCandidate = i ? oldMap[i - 1].second + 1 : 0;
            size_t anchorAfter = i == oldMap.size() - 1 ? anchorCandidate + 1 : oldMap[i + 1].second;
            if (anchorAfter - anchorCandidate == 1 && anchorCandidate < newList.size())
                merges.set(newList[anchorCandidate].get(), oldList[i].get());
            else {
                auto result = removeChildAndMoveToNew(*oldList[i]);
                if (result.hasException())
                    return result.releaseException();
            }
        } else {
            auto result = removeChildAndMoveToNew(*oldList[i]);
            if (result.hasException())
                return result.releaseException();
        }
    }

    // Mark retained nodes as used, do not reuse node more than once.
    HashSet<size_t, WTF::IntHash<size_t>, WTF::UnsignedWithZeroKeyHashTraits<size_t>> usedOldOrdinals;
    for (size_t i = 0; i < newList.size(); ++i) {
        if (!newMap[i].first)
            continue;
        size_t oldOrdinal = newMap[i].second;
        if (usedOldOrdinals.contains(oldOrdinal)) {
            // Do not map node more than once
            newMap[i].first = nullptr;
            newMap[i].second = 0;
            continue;
        }
        usedOldOrdinals.add(oldOrdinal);
        markNodeAsUsed(*newMap[i].first);
    }

    // Mark <head> and <body> nodes for merge.
    if (oldHead || oldBody) {
        for (size_t i = 0; i < newList.size(); ++i) {
            if (oldHead && newList[i]->node->hasTagName(headTag))
                merges.set(newList[i].get(), oldHead);
            if (oldBody && newList[i]->node->hasTagName(bodyTag))
                merges.set(newList[i].get(), oldBody);
        }
    }

    // 2. Patch nodes marked for merge.
    for (auto& merge : merges) {
        auto result = innerPatchNode(*merge.value, *merge.key);
        if (result.hasException())
            return result.releaseException();
    }

    // 3. Insert missing nodes.
    for (size_t i = 0; i < newMap.size(); ++i) {
        if (newMap[i].first || merges.contains(newList[i].get()))
            continue;
        auto result = insertBeforeAndMarkAsUsed(parentNode, *newList[i], parentNode.traverseToChildAt(i));
        if (result.hasException())
            return result.releaseException();
    }

    // 4. Then put all nodes that retained into their slots (sort by new index).
    for (size_t i = 0; i < oldMap.size(); ++i) {
        if (!oldMap[i].first)
            continue;
        RefPtr<Node> node = oldMap[i].first->node;
        auto* anchorNode = parentNode.traverseToChildAt(oldMap[i].second);
        if (node == anchorNode)
            continue;
        if (node->hasTagName(bodyTag) || node->hasTagName(headTag))
            continue; // Never move head or body, move the rest of the nodes around them.
        auto result = m_domEditor.insertBefore(parentNode, node.releaseNonNull(), anchorNode);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    CString cString = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(cString.data()), cString.length());
}

std::unique_ptr<DOMPatchSupport::Digest> DOMPatchSupport::createDigest(Node& node, UnusedNodesMap* unusedNodesMap)
{
    auto digest = std::make_unique<Digest>();
    digest->node = &node;
    SHA1 sha1;

    auto nodeType = node.nodeType();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(&nodeType), sizeof(nodeType));
    addStringToSHA1(sha1, node.nodeName());
    addStringToSHA1(sha1, node.nodeValue());

    if (node.nodeType() == Node::ELEMENT_NODE) {
        Node* child = node.firstChild();
        while (child) {
            std::unique_ptr<Digest> childInfo = createDigest(*child, unusedNodesMap);
            addStringToSHA1(sha1, childInfo->sha1);
            child = child->nextSibling();
            digest->children.append(WTFMove(childInfo));
        }
        auto& element = downcast<Element>(node);

        if (element.hasAttributesWithoutUpdate()) {
            SHA1 attrsSHA1;
            for (auto& attribute : element.attributesIterator()) {
                addStringToSHA1(attrsSHA1, attribute.name().toString());
                addStringToSHA1(attrsSHA1, attribute.value());
            }
            SHA1::Digest attrsHash;
            attrsSHA1.computeHash(attrsHash);
            digest->attrsSHA1 = base64Encode(attrsHash.data(), 10);
            addStringToSHA1(sha1, digest->attrsSHA1);
        }
    }

    SHA1::Digest hash;
    sha1.computeHash(hash);
    digest->sha1 = base64Encode(hash.data(), 10);
    if (unusedNodesMap)
        unusedNodesMap->add(digest->sha1, digest.get());

    return digest;
}

ExceptionOr<void> DOMPatchSupport::insertBeforeAndMarkAsUsed(ContainerNode& parentNode, Digest& digest, Node* anchor)
{
    ASSERT(digest.node);
    auto result = m_domEditor.insertBefore(parentNode, *digest.node, anchor);
    markNodeAsUsed(digest);
    return result;
}

ExceptionOr<void> DOMPatchSupport::removeChildAndMoveToNew(Digest& oldDigest)
{
    Ref<Node> oldNode = *oldDigest.node;
    ASSERT(oldNode->parentNode());
    auto result = m_domEditor.removeChild(*oldNode->parentNode(), oldNode);
    if (result.hasException())
        return result.releaseException();

    // Diff works within levels. In order not to lose the node identity when user
    // prepends his HTML with "<div>" (i.e. all nodes are shifted to the next nested level),
    // prior to dropping the original node on the floor, check whether new DOM has a digest
    // with matching sha1. If it does, replace it with the original DOM chunk. Chances are
    // high that it will get merged back into the original DOM during the further patching.
    auto it = m_unusedNodesMap.find(oldDigest.sha1);
    if (it != m_unusedNodesMap.end()) {
        auto& newDigest = *it->value;
        auto& newNode = *newDigest.node;
        auto result = m_domEditor.replaceChild(*newNode.parentNode(), oldNode.get(), newNode);
        if (result.hasException())
            return result.releaseException();
        newDigest.node = oldNode.ptr();
        markNodeAsUsed(newDigest);
        return { };
    }

    for (auto& child : oldDigest.children) {
        auto result = removeChildAndMoveToNew(*child);
        if (result.hasException())
            return result.releaseException();
    }
    return { };
}

void DOMPatchSupport::markNodeAsUsed(Digest& digest)
{
    Deque<Digest*> queue;
    queue.append(&digest);
    while (!queue.isEmpty()) {
        auto& first = *queue.takeFirst();
        m_unusedNodesMap.remove(first.sha1);
        for (auto& child : first.children)
            queue.append(child.get());
    }
}

#ifdef DEBUG_DOM_PATCH_SUPPORT

static String nodeName(Node* node)
{
    if (node->document().isXHTMLDocument())
         return node->nodeName();
    return node->nodeName().convertToASCIILowercase();
}

void DOMPatchSupport::dumpMap(const ResultMap& map, const String& name)
{
    fprintf(stderr, "\n\n");
    for (size_t i = 0; i < map.size(); ++i)
        fprintf(stderr, "%s[%lu]: %s (%p) - [%lu]\n", name.utf8().data(), i, map[i].first ? nodeName(map[i].first->m_node).utf8().data() : "", map[i].first, map[i].second);
}

#endif

} // namespace WebCore
