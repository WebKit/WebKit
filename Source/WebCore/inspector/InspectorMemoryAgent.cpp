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

#if ENABLE(INSPECTOR)

#include "InspectorMemoryAgent.h"

#include "BindingVisitors.h"
#include "CharacterData.h"
#include "Document.h"
#include "EventListenerMap.h"
#include "Frame.h"
#include "HeapGraphSerializer.h"
#include "InspectorClient.h"
#include "InspectorDOMStorageAgent.h"
#include "InspectorFrontend.h"
#include "InspectorState.h"
#include "InspectorValues.h"
#include "InstrumentingAgents.h"
#include "MemoryCache.h"
#include "MemoryInstrumentationImpl.h"
#include "MemoryUsageSupport.h"
#include "Node.h"
#include "NodeTraversal.h"
#include "Page.h"
#include "ScriptGCEvent.h"
#include "ScriptProfiler.h"
#include "StyledElement.h"
#include <wtf/ArrayBufferView.h>
#include <wtf/HashSet.h>
#include <wtf/MemoryInstrumentationArrayBufferView.h>
#include <wtf/NonCopyingSort.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringImpl.h>
#include <wtf/text/WTFString.h>

using WebCore::TypeBuilder::Memory::DOMGroup;
using WebCore::TypeBuilder::Memory::ListenerCount;
using WebCore::TypeBuilder::Memory::NodeCount;
using WebCore::TypeBuilder::Memory::StringStatistics;

// Use a type alias instead of 'using' here which would cause a conflict on Mac.
typedef WebCore::TypeBuilder::Memory::MemoryBlock InspectorMemoryBlock;
typedef WebCore::TypeBuilder::Array<InspectorMemoryBlock> InspectorMemoryBlocks;

namespace WebCore {

namespace {

class MemoryUsageStatsGenerator {
public:
    MemoryUsageStatsGenerator(MemoryInstrumentationClientImpl* client) : m_client(client) { }

    void dump(InspectorMemoryBlocks* children)
    {
        m_sizesMap = m_client->sizesMap();

        // FIXME: We filter out Rendering type because the coverage is not good enough at the moment
        // and report RenderArena size instead.
        for (TypeNameToSizeMap::iterator i = m_sizesMap.begin(); i != m_sizesMap.end(); ++i) {
            if (i->key == PlatformMemoryTypes::Rendering) {
                m_sizesMap.remove(i);
                break;
            }
        }
        Vector<String> objectTypes;
        objectTypes.appendRange(m_sizesMap.keys().begin(), m_sizesMap.keys().end());

        for (Vector<String>::const_iterator i = objectTypes.begin(); i != objectTypes.end(); ++i)
            updateParentSizes(*i, m_sizesMap.get(*i));

        objectTypes.clear();
        objectTypes.appendRange(m_sizesMap.keys().begin(), m_sizesMap.keys().end());
        nonCopyingSort(objectTypes.begin(), objectTypes.end(), stringCompare);

        size_t index = 0;
        while (index < objectTypes.size())
            index = buildObjectForIndex(index, objectTypes, children);

        addMemoryInstrumentationDebugData(children);
    }

private:
    static bool stringCompare(const String& a, const String& b) { return WTF::codePointCompare(a, b) < 0; }

    void updateParentSizes(String objectType, const size_t size)
    {
        for (size_t dotPosition = objectType.reverseFind('.'); dotPosition != notFound; dotPosition = objectType.reverseFind('.', dotPosition)) {
            objectType = objectType.substring(0, dotPosition);
            TypeNameToSizeMap::AddResult result = m_sizesMap.add(objectType, size);
            if (!result.isNewEntry)
                result.iterator->value += size;
        }
    }

    size_t buildObjectForIndex(size_t index, const Vector<String>& objectTypes, InspectorMemoryBlocks* array)
    {
        String typeName = objectTypes[index];
        size_t dotPosition = typeName.reverseFind('.');
        String blockName = (dotPosition == notFound) ? typeName : typeName.substring(dotPosition + 1);
        RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(blockName);
        block->setSize(m_sizesMap.get(typeName));
        String prefix = typeName;
        prefix.append('.');
        array->addItem(block);
        ++index;
        RefPtr<InspectorMemoryBlocks> children;
        while (index < objectTypes.size() && objectTypes[index].startsWith(prefix)) {
            if (!children)
                children = InspectorMemoryBlocks::create();
            index = buildObjectForIndex(index, objectTypes, children.get());
        }
        if (children)
            block->setChildren(children.release());
        return index;
    }

    void addMemoryInstrumentationDebugData(InspectorMemoryBlocks* children)
    {
        if (m_client->checkInstrumentedObjects()) {
            RefPtr<InspectorMemoryBlock> totalInstrumented = InspectorMemoryBlock::create().setName("InstrumentedObjectsCount");
            totalInstrumented->setSize(m_client->totalCountedObjects());

            RefPtr<InspectorMemoryBlock> incorrectlyInstrumented = InspectorMemoryBlock::create().setName("InstrumentedButNotAllocatedObjectsCount");
            incorrectlyInstrumented->setSize(m_client->totalObjectsNotInAllocatedSet());

            children->addItem(totalInstrumented);
            children->addItem(incorrectlyInstrumented);
        }
    }

    MemoryInstrumentationClientImpl* m_client;
    TypeNameToSizeMap m_sizesMap;
};

String nodeName(Node* node)
{
    if (node->document()->isXHTMLDocument())
         return node->nodeName();
    return node->nodeName().lower();
}

typedef HashSet<StringImpl*, PtrHash<StringImpl*> > StringImplIdentitySet;

class CharacterDataStatistics {
    WTF_MAKE_NONCOPYABLE(CharacterDataStatistics);
public:
    CharacterDataStatistics() : m_characterDataSize(0) { }

    void collectCharacterData(Node* node)
    {
        if (!node->isCharacterDataNode())
            return;

        CharacterData* characterData = static_cast<CharacterData*>(node);
        StringImpl* dataImpl = characterData->dataImpl();
        if (m_domStringImplSet.contains(dataImpl))
            return;
        m_domStringImplSet.add(dataImpl);

        m_characterDataSize += dataImpl->sizeInBytes();
    }

    bool contains(StringImpl* s) { return m_domStringImplSet.contains(s); }

    int characterDataSize() { return m_characterDataSize; }

private:
    StringImplIdentitySet m_domStringImplSet;
    int m_characterDataSize;
};

class DOMTreeStatistics {
    WTF_MAKE_NONCOPYABLE(DOMTreeStatistics);
public:
    DOMTreeStatistics(Node* rootNode, CharacterDataStatistics& characterDataStatistics)
        : m_totalNodeCount(0)
        , m_characterDataStatistics(characterDataStatistics)
    {
        collectTreeStatistics(rootNode);
    }

    int totalNodeCount() { return m_totalNodeCount; }

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Memory::NodeCount> > nodeCount()
    {
        RefPtr<TypeBuilder::Array<TypeBuilder::Memory::NodeCount> > childrenStats = TypeBuilder::Array<TypeBuilder::Memory::NodeCount>::create();
        for (HashMap<String, int>::iterator it = m_nodeNameToCount.begin(); it != m_nodeNameToCount.end(); ++it) {
            RefPtr<NodeCount> nodeCount = NodeCount::create().setNodeName(it->key)
                                                             .setCount(it->value);
            childrenStats->addItem(nodeCount);
        }
        return childrenStats.release();
    }

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Memory::ListenerCount> > listenerCount()
    {
        RefPtr<TypeBuilder::Array<TypeBuilder::Memory::ListenerCount> > listenerStats = TypeBuilder::Array<TypeBuilder::Memory::ListenerCount>::create();
        for (HashMap<AtomicString, int>::iterator it = m_eventTypeToCount.begin(); it != m_eventTypeToCount.end(); ++it) {
            RefPtr<ListenerCount> listenerCount = ListenerCount::create().setType(it->key)
                                                                         .setCount(it->value);
            listenerStats->addItem(listenerCount);
        }
        return listenerStats.release();
    }

private:
    void collectTreeStatistics(Node* rootNode)
    {
        Node* currentNode = rootNode;
        collectListenersInfo(rootNode);
        while ((currentNode = NodeTraversal::next(currentNode, rootNode))) {
            ++m_totalNodeCount;
            collectNodeStatistics(currentNode);
        }
    }
    void collectNodeStatistics(Node* node)
    {
        m_characterDataStatistics.collectCharacterData(node);
        collectNodeNameInfo(node);
        collectListenersInfo(node);
    }

    void collectNodeNameInfo(Node* node)
    {
        String name = nodeName(node);
        int currentCount = m_nodeNameToCount.get(name);
        m_nodeNameToCount.set(name, currentCount + 1);
    }

    void collectListenersInfo(Node* node)
    {
        EventTargetData* d = node->eventTargetData();
        if (!d)
            return;
        EventListenerMap& eventListenerMap = d->eventListenerMap;
        if (eventListenerMap.isEmpty())
            return;
        Vector<AtomicString> eventNames = eventListenerMap.eventTypes();
        for (Vector<AtomicString>::iterator it = eventNames.begin(); it != eventNames.end(); ++it) {
            AtomicString name = *it;
            EventListenerVector* listeners = eventListenerMap.find(name);
            int count = 0;
            for (EventListenerVector::iterator j = listeners->begin(); j != listeners->end(); ++j) {
                if (j->listener->type() == EventListener::JSEventListenerType)
                    ++count;
            }
            if (count)
                m_eventTypeToCount.set(name, m_eventTypeToCount.get(name) + count);
        }
    }

    int m_totalNodeCount;
    HashMap<AtomicString, int> m_eventTypeToCount;
    HashMap<String, int> m_nodeNameToCount;
    CharacterDataStatistics& m_characterDataStatistics;
};

class CounterVisitor : public WrappedNodeVisitor, public ExternalStringVisitor {
public:
    CounterVisitor(Page* page)
        : m_page(page)
        , m_domGroups(TypeBuilder::Array<TypeBuilder::Memory::DOMGroup>::create())
        , m_jsExternalStringSize(0)
        , m_sharedStringSize(0) { }

    TypeBuilder::Array<TypeBuilder::Memory::DOMGroup>* domGroups() { return m_domGroups.get(); }

    PassRefPtr<StringStatistics> strings()
    {
        RefPtr<StringStatistics> stringStatistics = StringStatistics::create()
            .setDom(m_characterDataStatistics.characterDataSize())
            .setJs(m_jsExternalStringSize)
            .setShared(m_sharedStringSize);
        return stringStatistics.release();
    }

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

        DOMTreeStatistics domTreeStats(rootNode, m_characterDataStatistics);

        RefPtr<DOMGroup> domGroup = DOMGroup::create()
            .setSize(domTreeStats.totalNodeCount())
            .setTitle(rootNode->nodeType() == Node::ELEMENT_NODE ? elementTitle(static_cast<Element*>(rootNode)) : rootNode->nodeName())
            .setNodeCount(domTreeStats.nodeCount())
            .setListenerCount(domTreeStats.listenerCount());
        if (rootNode->nodeType() == Node::DOCUMENT_NODE)
            domGroup->setDocumentURI(static_cast<Document*>(rootNode)->documentURI());

        m_domGroups->addItem(domGroup);
    }

    virtual void visitJSExternalString(StringImpl* string)
    {
        int size = string->sizeInBytes();
        m_jsExternalStringSize += size;
        if (m_characterDataStatistics.contains(string))
            m_sharedStringSize += size;
    }

private:
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
    RefPtr<TypeBuilder::Array<TypeBuilder::Memory::DOMGroup> > m_domGroups;
    CharacterDataStatistics m_characterDataStatistics;
    int m_jsExternalStringSize;
    int m_sharedStringSize;
};

class ExternalStringsRoot : public ExternalStringVisitor {
public:
    ExternalStringsRoot() : m_memoryClassInfo(0) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::ExternalStrings);
        m_memoryClassInfo = &info;
        ScriptProfiler::visitExternalStrings(const_cast<ExternalStringsRoot*>(this));
        m_memoryClassInfo = 0;
        info.ignoreMember(m_memoryClassInfo);
    }

private:
    virtual void visitJSExternalString(StringImpl* string)
    {
        m_memoryClassInfo->addMember(string);
    }

    mutable MemoryClassInfo* m_memoryClassInfo;
};

class ExternalArraysRoot : public ExternalArrayVisitor {
public:
    ExternalArraysRoot() : m_memoryClassInfo(0) { }

    void reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
    {
        MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::ExternalArrays);
        m_memoryClassInfo = &info;
        ScriptProfiler::visitExternalArrays(const_cast<ExternalArraysRoot*>(this));
        m_memoryClassInfo = 0;
        info.ignoreMember(m_memoryClassInfo);
    }

private:
    virtual void visitJSExternalArray(ArrayBufferView* arrayBufferView)
    {
        m_memoryClassInfo->addMember(arrayBufferView);
    }

    mutable MemoryClassInfo* m_memoryClassInfo;
};

} // namespace

InspectorMemoryAgent::~InspectorMemoryAgent()
{
}

void InspectorMemoryAgent::getDOMNodeCount(ErrorString*, RefPtr<TypeBuilder::Array<TypeBuilder::Memory::DOMGroup> >& domGroups, RefPtr<TypeBuilder::Memory::StringStatistics>& strings)
{
    CounterVisitor counterVisitor(m_page);
    ScriptProfiler::visitNodeWrappers(&counterVisitor);

    // Make sure all documents reachable from the main frame are accounted.
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* doc = frame->document())
            counterVisitor.visitNode(doc);
    }

    ScriptProfiler::visitExternalStrings(&counterVisitor);

    domGroups = counterVisitor.domGroups();
    strings = counterVisitor.strings();
}

static void reportJSHeapInfo(WTF::MemoryInstrumentationClient& memoryInstrumentationClient)
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);

    memoryInstrumentationClient.countObjectSize(0, WebCoreMemoryTypes::JSHeapUsed, info.usedJSHeapSize);
    memoryInstrumentationClient.countObjectSize(0, WebCoreMemoryTypes::JSHeapUnused, info.totalJSHeapSize - info.usedJSHeapSize);
}

static void reportRenderTreeInfo(WTF::MemoryInstrumentationClient& memoryInstrumentationClient, Page* page)
{
    ArenaSize arenaSize = page->renderTreeSize();

    memoryInstrumentationClient.countObjectSize(0, WebCoreMemoryTypes::RenderTreeUsed, arenaSize.treeSize);
    memoryInstrumentationClient.countObjectSize(0, WebCoreMemoryTypes::RenderTreeUnused, arenaSize.allocated - arenaSize.treeSize);
}

namespace {

class DOMTreesIterator : public WrappedNodeVisitor {
public:
    DOMTreesIterator(MemoryInstrumentationImpl& memoryInstrumentation, Page* page)
        : m_page(page)
        , m_memoryInstrumentation(memoryInstrumentation)
    {
    }

    virtual void visitNode(Node* node) OVERRIDE
    {
        if (node->document() && node->document()->frame() && m_page != node->document()->frame()->page())
            return;

        m_memoryInstrumentation.addRootObject(node);
    }

    void visitFrame(Frame* frame)
    {
        m_memoryInstrumentation.addRootObject(frame);
    }

    void visitBindings()
    {
        ScriptProfiler::collectBindingMemoryInfo(&m_memoryInstrumentation);
    }

    void visitMemoryCache()
    {
        m_memoryInstrumentation.addRootObject(memoryCache());
    }


private:
    Page* m_page;
    MemoryInstrumentationImpl& m_memoryInstrumentation;
};

}

static void collectDomTreeInfo(MemoryInstrumentationImpl& memoryInstrumentation, Page* page)
{
    ExternalStringsRoot stringsRoot;
    memoryInstrumentation.addRootObject(stringsRoot);

    ExternalArraysRoot arraysRoot;
    memoryInstrumentation.addRootObject(arraysRoot);

    DOMTreesIterator domTreesIterator(memoryInstrumentation, page);

    ScriptProfiler::visitNodeWrappers(&domTreesIterator);

    // Make sure all documents reachable from the main frame are accounted.
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        if (Document* doc = frame->document()) {
            domTreesIterator.visitNode(doc);
            domTreesIterator.visitFrame(frame);
        }
    }

    domTreesIterator.visitBindings();
    domTreesIterator.visitMemoryCache();
}

static void addPlatformComponentsInfo(PassRefPtr<InspectorMemoryBlocks> children)
{
    Vector<MemoryUsageSupport::ComponentInfo> components;
    MemoryUsageSupport::memoryUsageByComponents(components);
    for (Vector<MemoryUsageSupport::ComponentInfo>::iterator it = components.begin(); it != components.end(); ++it) {
        RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(it->m_name);
        block->setSize(it->m_sizeInBytes);
        children->addItem(block);
    }
}

void InspectorMemoryAgent::getProcessMemoryDistribution(ErrorString*, const bool* reportGraph, RefPtr<InspectorMemoryBlock>& processMemory, RefPtr<InspectorObject>& graph)
{
    OwnPtr<HeapGraphSerializer> graphSerializer;
    if (reportGraph && *reportGraph)
        graphSerializer = adoptPtr(new HeapGraphSerializer());
    MemoryInstrumentationClientImpl memoryInstrumentationClient(graphSerializer.get());
    m_inspectorClient->getAllocatedObjects(memoryInstrumentationClient.allocatedObjects());
    MemoryInstrumentationImpl memoryInstrumentation(&memoryInstrumentationClient);

    reportJSHeapInfo(memoryInstrumentationClient);
    reportRenderTreeInfo(memoryInstrumentationClient, m_page);
    collectDomTreeInfo(memoryInstrumentation, m_page); // FIXME: collect for all pages?

    PlatformMemoryInstrumentation::reportStaticMembersMemoryUsage(&memoryInstrumentation);
    WebCoreMemoryInstrumentation::reportStaticMembersMemoryUsage(&memoryInstrumentation);

    RefPtr<InspectorMemoryBlocks> children = InspectorMemoryBlocks::create();
    addPlatformComponentsInfo(children);

    memoryInstrumentation.addRootObject(this);
    memoryInstrumentation.addRootObject(memoryInstrumentation);
    memoryInstrumentation.addRootObject(memoryInstrumentationClient);
    if (graphSerializer) {
        memoryInstrumentation.addRootObject(graphSerializer.get());
        graph = graphSerializer->serialize();
    }

    m_inspectorClient->dumpUncountedAllocatedObjects(memoryInstrumentationClient.countedObjects());

    MemoryUsageStatsGenerator statsGenerator(&memoryInstrumentationClient);
    statsGenerator.dump(children.get());

    processMemory = InspectorMemoryBlock::create().setName(WebCoreMemoryTypes::ProcessPrivateMemory);
    processMemory->setChildren(children);

    size_t privateBytes = 0;
    size_t sharedBytes = 0;
    MemoryUsageSupport::processMemorySizesInBytes(&privateBytes, &sharedBytes);
    processMemory->setSize(privateBytes);
}

void InspectorMemoryAgent::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::Inspector);
    InspectorBaseAgent<InspectorMemoryAgent>::reportMemoryUsage(memoryObjectInfo);
    info.addWeakPointer(m_inspectorClient);
    info.addMember(m_page);
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents, InspectorClient* client, InspectorCompositeState* state, Page* page)
    : InspectorBaseAgent<InspectorMemoryAgent>("Memory", instrumentingAgents, state)
    , m_inspectorClient(client)
    , m_page(page)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
