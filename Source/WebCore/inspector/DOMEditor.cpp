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
#include "ExceptionCode.h"
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

static String exceptionDescription(ExceptionCode& ec)
{
    ExceptionCodeDescription description(ec);
    return description.name;
}

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

    ErrorString errorString;
    innerPatchHTMLElement(&errorString, m_document->head(), newDocument->head());
    if (errorString.isEmpty())
        innerPatchHTMLElement(&errorString, m_document->body(), newDocument->body());

    if (errorString.isEmpty())
        return;

    // Fall back to rewrite.
    m_document->write(markup);
    m_document->close();
}

Node* DOMEditor::patchNode(ErrorString* errorString, Node* node, const String& markup)
{
    Element* parentElement = node->parentElement();
    if (!parentElement) {
        *errorString = "Can only set outer HTML to nodes within Element";
        return 0;
    }

    Node* previousSibling = node->previousSibling();
    RefPtr<DocumentFragment> fragment = DocumentFragment::create(m_document);
    fragment->parseHTML(markup, parentElement);

    // Compose the old list.
    Vector<OwnPtr<Digest> > oldList;
    for (Node* child = parentElement->firstChild(); child; child = child->nextSibling())
        oldList.append(createDigest(child));

    // Compose the new list.
    Vector<OwnPtr<Digest> > newList;
    for (Node* child = parentElement->firstChild(); child != node; child = child->nextSibling())
        newList.append(createDigest(child));
    for (Node* child = fragment->firstChild(); child; child = child->nextSibling())
        newList.append(createDigest(child));
    for (Node* child = node->nextSibling(); child; child = child->nextSibling())
        newList.append(createDigest(child));

    innerPatchChildren(errorString, parentElement, oldList, newList);
    if (!errorString->isEmpty()) {
        // Fall back to total replace.
        ExceptionCode ec = 0;
        parentElement->replaceChild(fragment.release(), node, ec);
        if (ec) {
            *errorString = exceptionDescription(ec);
            return 0;
        }
    }
    return previousSibling ? previousSibling->nextSibling() : parentElement->firstChild();
}

void DOMEditor::innerPatchHTMLElement(ErrorString* errorString, HTMLElement* oldElement, HTMLElement* newElement)
{
    if (oldElement)  {
        if (newElement) {
            OwnPtr<Digest> oldInfo = createDigest(oldElement);
            OwnPtr<Digest> newInfo = createDigest(newElement);
            innerPatchNode(errorString, oldInfo.get(), newInfo.get());
            if (!errorString->isEmpty())
                return;
        }
        oldElement->removeAllChildren();
        return;
    }
    if (newElement) {
        ExceptionCode ec = 0;
        m_document->documentElement()->appendChild(newElement, ec);
        if (ec)
            *errorString = exceptionDescription(ec);
    }
}

void DOMEditor::innerPatchNode(ErrorString* errorString, Digest* oldDigest, Digest* newDigest)
{
    if (oldDigest->m_sha1 == newDigest->m_sha1)
        return;

    Node* oldNode = oldDigest->m_node;
    Node* newNode = newDigest->m_node;

    if (newNode->nodeType() != oldNode->nodeType() || newNode->nodeName() != oldNode->nodeName()) {
        ExceptionCode ec = 0;
        oldNode->parentNode()->replaceChild(newNode, oldNode, ec);
        if (ec)
            *errorString = exceptionDescription(ec);
        return;
    }

    ExceptionCode ec = 0;
    if (oldNode->nodeValue() != newNode->nodeValue())
        oldNode->setNodeValue(newNode->nodeValue(), ec);
    if (ec) {
        *errorString = exceptionDescription(ec);
        return;
    }

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

    innerPatchChildren(errorString, oldElement, oldDigest->m_children, newDigest->m_children);
}

pair<DOMEditor::ResultMap, DOMEditor::ResultMap>
DOMEditor::diff(Vector<OwnPtr<Digest> >& oldList, Vector<OwnPtr<Digest> >& newList)
{
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
        make_pair(newList[newIt->second[0]].get(), oldIt->second[0]);
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

void DOMEditor::innerPatchChildren(ErrorString* errorString, Element* element, Vector<OwnPtr<Digest> >& oldList, Vector<OwnPtr<Digest> >& newList)
{
    // Trim tail.
    size_t offset = 0;
    for (; offset < oldList.size() && offset < newList.size() && oldList[oldList.size() - offset - 1]->m_sha1 == newList[newList.size() - offset - 1]->m_sha1; ++offset) { }
    if (offset > 0) {
        oldList.resize(oldList.size() - offset);
        newList.resize(newList.size() - offset);
    }

    // Trim head.
    for (offset = 0; offset < oldList.size() && offset < newList.size() && oldList[offset]->m_sha1 == newList[offset]->m_sha1; ++offset) { }
    if (offset > 0) {
        oldList.remove(0, offset);
        newList.remove(0, offset);
    }

    pair<ResultMap, ResultMap> resultMaps = diff(oldList, newList);
    ResultMap& oldMap = resultMaps.first;
    ResultMap& newMap = resultMaps.second;

    HashSet<Digest*> merges;
    for (size_t i = 0; i < oldList.size(); ++i) {
        if (oldMap[i].first)
            continue;

        // Check if this change is between stable nodes. If it is, consider it as "modified".
        if ((!i || oldMap[i - 1].first) && (i == oldMap.size() - 1 || oldMap[i + 1].first)) {
            size_t anchorCandidate = i ? oldMap[i - 1].second + 1 : 0;
            size_t anchorAfter = i == oldMap.size() - 1 ? anchorCandidate + 1 : oldMap[i + 1].second;
            if (anchorAfter - anchorCandidate == 1 && anchorCandidate < newList.size()) {
                innerPatchNode(errorString, oldList[i].get(), newList[anchorCandidate].get());
                if (!errorString->isEmpty())
                    return;

                merges.add(newList[anchorCandidate].get());
            } else
                element->removeChild(oldList[i]->m_node);
        } else
            element->removeChild(oldList[i]->m_node);
    }

    for (size_t i = 0; i < newMap.size(); ++i) {
        if (newMap[i].first || merges.contains(newList[i].get()))
            continue;

        ExceptionCode ec = 0;
        element->insertBefore(newList[i]->m_node, element->childNode(i + offset), ec);
        if (ec) {
            *errorString = exceptionDescription(ec);
            return;
        }
    }
}

static void addStringToSHA1(SHA1& sha1, const String& string)
{
    CString cString = string.utf8();
    sha1.addBytes(reinterpret_cast<const uint8_t*>(cString.data()), cString.length());
}

PassOwnPtr<DOMEditor::Digest> DOMEditor::createDigest(Node* node)
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
            OwnPtr<Digest> childInfo = createDigest(child);
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
    return adoptPtr(digest);
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
