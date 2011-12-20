/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "InspectorMemoryAgent.h"

#if ENABLE(INSPECTOR)

#include "DOMWrapperVisitor.h"
#include "Document.h"
#include "Frame.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "Node.h"
#include "Page.h"
#include "ScriptProfiler.h"
#include "StyledElement.h"
#include <wtf/HashSet.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace {

class CounterVisitor : public DOMWrapperVisitor {
public:
    CounterVisitor(Page* page) : m_page(page), m_counters(InspectorArray::create()) { }

    InspectorArray* counters() { return m_counters.get(); }

    virtual void visitNode(Node* node)
    {
        if (node->document()->frame() && m_page != node->document()->frame()->page())
            return;

        Node* rootNode = node;
        while (rootNode->parentNode())
            rootNode = rootNode->parentNode();

        if (m_roots.contains(rootNode))
            return;
        m_roots.add(rootNode);

        RefPtr<InspectorObject> entry = InspectorObject::create();
        entry->setString("title", rootNode->nodeType() == Node::ELEMENT_NODE ? elementTitle(static_cast<Element*>(rootNode)) : rootNode->nodeName());
        if (rootNode->nodeType() == Node::DOCUMENT_NODE)
            entry->setString("documentURI", static_cast<Document*>(rootNode)->documentURI());
        collectTreeStatistics(rootNode, entry.get());
        m_counters->pushObject(entry);
    }

private:
    static void collectTreeStatistics(Node* rootNode, InspectorObject* result)
    {
        unsigned count = 0;
        HashMap<String, int> nameToCount;
        Node* currentNode = rootNode;
        while ((currentNode = currentNode->traverseNextNode(rootNode))) {
            ++count;
            String name = nodeName(currentNode);
            int currentCount = nameToCount.get(name);
            nameToCount.set(name, currentCount + 1);
        }

        RefPtr<InspectorArray> childrenStats = InspectorArray::create();
        for (HashMap<String, int>::iterator it = nameToCount.begin(); it != nameToCount.end(); ++it) {
            RefPtr<InspectorObject> nodeCount = InspectorObject::create();
            nodeCount->setString("nodeName", it->first);
            nodeCount->setNumber("count", it->second);
            childrenStats->pushObject(nodeCount);
        }

        result->setNumber("size", count);
        result->setArray("nodeCount", childrenStats);
    }

    static String nodeName(Node* node)
    {
        if (node->document()->isXHTMLDocument())
             return node->nodeName();
        return node->nodeName().lower();
    }

    String elementTitle(Element* element)
    {
        StringBuilder result;
        result.append(nodeName(element));

        const AtomicString& idValue = element->getIdAttribute();
        String idString;
        if (!idValue.isNull() && !idValue.isEmpty()) {
            result.append("#");
            result.append(idValue);
        }

        HashSet<AtomicString> usedClassNames;
        if (element->hasClass() && element->isStyledElement()) {
            const SpaceSplitString& classNamesString = static_cast<StyledElement*>(element)->classNames();
            size_t classNameCount = classNamesString.size();
            for (size_t i = 0; i < classNameCount; ++i) {
                const AtomicString& className = classNamesString[i];
                if (usedClassNames.contains(className))
                    continue;
                usedClassNames.add(className);
                result.append(".");
                result.append(className);
            }
        }
        return result.toString();
    }

    HashSet<Node*> m_roots;
    Page* m_page;
    RefPtr<InspectorArray> m_counters;
};

} // namespace

InspectorMemoryAgent::~InspectorMemoryAgent()
{
}

void InspectorMemoryAgent::getDOMNodeCount(ErrorString*, RefPtr<InspectorArray>& result)
{
    CounterVisitor counterVisitor(m_page);
    ScriptProfiler::visitJSDOMWrappers(&counterVisitor);

    // Make sure all documents reachable from the main frame are accounted.
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* doc = frame->document())
            counterVisitor.visitNode(doc);
    }

    result = counterVisitor.counters();
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents, InspectorState* state, Page* page, InspectorDOMAgent* domAgent)
    : InspectorBaseAgent<InspectorMemoryAgent>("Memory", instrumentingAgents, state)
    , m_page(page)
    , m_domAgent(domAgent)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
