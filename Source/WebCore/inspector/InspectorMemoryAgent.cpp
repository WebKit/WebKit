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
#include "Page.h"
#include "ScriptGCEvent.h"
#include "ScriptProfiler.h"
#include "StyledElement.h"
#include <wtf/ArrayBuffer.h>
#include <wtf/ArrayBufferView.h>
#include <wtf/HashSet.h>
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

namespace WebCore {

namespace MemoryBlockName {
static const char jsHeapAllocated[] = "JSHeapAllocated";
static const char jsHeapUsed[] = "JSHeapUsed";
static const char jsExternalResources[] = "JSExternalResources";
static const char jsExternalArrays[] = "JSExternalArrays";
static const char jsExternalStrings[] = "JSExternalStrings";
static const char inspectorData[] = "InspectorData";
static const char inspectorDOMData[] = "InspectorDOMData";
static const char inspectorJSHeapData[] = "InspectorJSHeapData";
static const char processPrivateMemory[] = "ProcessPrivateMemory";

static const char renderTreeUsed[] = "RenderTreeUsed";
static const char renderTreeAllocated[] = "RenderTreeAllocated";

static const char domStorageCache[] = "DOMStorageCache";
}

namespace {

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
            RefPtr<NodeCount> nodeCount = NodeCount::create().setNodeName(it->first)
                                                             .setCount(it->second);
            childrenStats->addItem(nodeCount);
        }
        return childrenStats.release();
    }

    PassRefPtr<TypeBuilder::Array<TypeBuilder::Memory::ListenerCount> > listenerCount()
    {
        RefPtr<TypeBuilder::Array<TypeBuilder::Memory::ListenerCount> > listenerStats = TypeBuilder::Array<TypeBuilder::Memory::ListenerCount>::create();
        for (HashMap<AtomicString, int>::iterator it = m_eventTypeToCount.begin(); it != m_eventTypeToCount.end(); ++it) {
            RefPtr<ListenerCount> listenerCount = ListenerCount::create().setType(it->first)
                                                                         .setCount(it->second);
            listenerStats->addItem(listenerCount);
        }
        return listenerStats.release();
    }

private:
    void collectTreeStatistics(Node* rootNode)
    {
        Node* currentNode = rootNode;
        collectListenersInfo(rootNode);
        while ((currentNode = currentNode->traverseNextNode(rootNode))) {
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

class CounterVisitor : public NodeWrapperVisitor, public ExternalStringVisitor {
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

class ExternalResourceVisitor : public ExternalStringVisitor, public ExternalArrayVisitor {
public:
    explicit ExternalResourceVisitor(VisitedObjects& visitedObjects)
        : m_visitedObjects(visitedObjects)
        , m_jsExternalStringSize(0)
        , m_externalArraySize(0)
    { }

    size_t externalStringSize() const { return m_jsExternalStringSize; }
    size_t externalArraySize() const { return m_externalArraySize; }

private:
    virtual void visitJSExternalArray(ArrayBufferView* bufferView)
    {
        ArrayBuffer* buffer = bufferView->buffer().get();
        if (m_visitedObjects.add(buffer).isNewEntry)
            m_externalArraySize += buffer->byteLength();
    }
    virtual void visitJSExternalString(StringImpl* string)
    {
        if (m_visitedObjects.add(string).isNewEntry)
            m_jsExternalStringSize += string->sizeInBytes();
    }

    VisitedObjects& m_visitedObjects;
    size_t m_jsExternalStringSize;
    size_t m_externalArraySize;
};

class InspectorDataCounter {
public:
    void addComponent(const String& name, size_t size)
    {
        m_components.append(ComponentInfo(name, size));
    }

    PassRefPtr<InspectorMemoryBlock> dumpStatistics()
    {
        size_t totalSize = 0;
        RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(MemoryBlockName::inspectorData);
        for (Vector<ComponentInfo>::iterator it = m_components.begin(); it != m_components.end(); ++it) {
            RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(it->m_name);
            block->setSize(it->m_size);
            totalSize += it->m_size;
        }
        block->setSize(totalSize);
        return block;
    }

private:
    class ComponentInfo {
    public:
        ComponentInfo(const String& name, size_t size) : m_name(name), m_size(size) { }

        const String m_name;
        size_t m_size;
    };

    Vector<ComponentInfo> m_components;
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

static PassRefPtr<InspectorMemoryBlock> jsHeapInfo()
{
    HeapInfo info;
    ScriptGCEvent::getHeapSize(info);

    RefPtr<InspectorMemoryBlock> jsHeapAllocated = InspectorMemoryBlock::create().setName(MemoryBlockName::jsHeapAllocated);
    jsHeapAllocated->setSize(static_cast<int>(info.totalJSHeapSize));

    RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children = TypeBuilder::Array<InspectorMemoryBlock>::create();
    RefPtr<InspectorMemoryBlock> jsHeapUsed = InspectorMemoryBlock::create().setName(MemoryBlockName::jsHeapUsed);
    jsHeapUsed->setSize(static_cast<int>(info.usedJSHeapSize));
    children->addItem(jsHeapUsed);

    jsHeapAllocated->setChildren(children);
    return jsHeapAllocated.release();
}

static PassRefPtr<InspectorMemoryBlock> renderTreeInfo(Page* page)
{
    ArenaSize arenaSize = page->renderTreeSize();

    RefPtr<InspectorMemoryBlock> renderTreeAllocated = InspectorMemoryBlock::create().setName(MemoryBlockName::renderTreeAllocated);
    renderTreeAllocated->setSize(arenaSize.allocated);

    RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children = TypeBuilder::Array<InspectorMemoryBlock>::create();
    RefPtr<InspectorMemoryBlock> renderTreeUsed = InspectorMemoryBlock::create().setName(MemoryBlockName::renderTreeUsed);
    renderTreeUsed->setSize(arenaSize.treeSize);
    children->addItem(renderTreeUsed);

    renderTreeAllocated->setChildren(children);
    return renderTreeAllocated.release();
}

namespace {

class DOMTreesIterator : public NodeWrapperVisitor {
public:
    DOMTreesIterator(Page* page, MemoryInstrumentationImpl& memoryInstrumentation)
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

    PassRefPtr<InspectorMemoryBlock> buildObjectForMemoryCache() const
    {
        size_t totalSize = 0;

        RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children = TypeBuilder::Array<InspectorMemoryBlock>::create();
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::MemoryCacheStructures);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceRaw);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceCSS);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceFont);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceImage);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceScript);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceSVG);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceShader);
        totalSize += addMemoryBlockFor(children.get(), WebCoreMemoryTypes::CachedResourceXSLT);

        RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(WebCoreMemoryTypes::MemoryCache);
        block->setSize(totalSize);
        block->setChildren(children.release());
        return block.release();
    }

    PassRefPtr<InspectorMemoryBlock> buildObjectForPage() const
    {
        size_t totalSize = 0;

        RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > domChildren = TypeBuilder::Array<InspectorMemoryBlock>::create();
        totalSize += addMemoryBlockFor(domChildren.get(), WebCoreMemoryTypes::DOM);
        totalSize += addMemoryBlockFor(domChildren.get(), WebCoreMemoryTypes::Image);
        totalSize += addMemoryBlockFor(domChildren.get(), WebCoreMemoryTypes::CSS);
        totalSize += addMemoryBlockFor(domChildren.get(), WebCoreMemoryTypes::Binding);
        totalSize += addMemoryBlockFor(domChildren.get(), WebCoreMemoryTypes::Loader);

        RefPtr<InspectorMemoryBlock> dom = InspectorMemoryBlock::create().setName(WebCoreMemoryTypes::Page);
        dom->setSize(totalSize);
        dom->setChildren(domChildren.release());
        return dom.release();
    }

    void dumpStatistics(TypeBuilder::Array<InspectorMemoryBlock>* children, InspectorDataCounter* inspectorData)
    {
        children->addItem(buildObjectForMemoryCache());
        children->addItem(buildObjectForPage());

        inspectorData->addComponent(MemoryBlockName::inspectorDOMData, m_memoryInstrumentation.selfSize());
    }

private:
    size_t addMemoryBlockFor(TypeBuilder::Array<InspectorMemoryBlock>* array, MemoryObjectType typeName) const
    {
        RefPtr<InspectorMemoryBlock> result = InspectorMemoryBlock::create().setName(typeName);
        size_t size = m_memoryInstrumentation.totalSize(typeName);
        result->setSize(size);
        array->addItem(result);
        return size;
    }

    Page* m_page;
    MemoryInstrumentationImpl& m_memoryInstrumentation;
};

}

static void collectDomTreeInfo(Page* page, InspectorClient* inspectorClient, VisitedObjects& visitedObjects, TypeBuilder::Array<InspectorMemoryBlock>* children, InspectorDataCounter* inspectorData)
{
    HashSet<const void*> allocatedObjects;
    inspectorClient->getAllocatedObjects(allocatedObjects);
    MemoryInstrumentationImpl memoryInstrumentation(visitedObjects, allocatedObjects.isEmpty() ? 0 : &allocatedObjects);

    DOMTreesIterator domTreesIterator(page, memoryInstrumentation);

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

    domTreesIterator.dumpStatistics(children, inspectorData);
}

static void addPlatformComponentsInfo(PassRefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children)
{
    Vector<MemoryUsageSupport::ComponentInfo> components;
    MemoryUsageSupport::memoryUsageByComponents(components);
    for (Vector<MemoryUsageSupport::ComponentInfo>::iterator it = components.begin(); it != components.end(); ++it) {
        RefPtr<InspectorMemoryBlock> block = InspectorMemoryBlock::create().setName(it->m_name);
        block->setSize(it->m_sizeInBytes);
        children->addItem(block);
    }
}

static PassRefPtr<InspectorMemoryBlock> jsExternalResourcesInfo(VisitedObjects& visitedObjects)
{
    ExternalResourceVisitor visitor(visitedObjects);
    ScriptProfiler::visitExternalStrings(&visitor);
    ScriptProfiler::visitExternalArrays(&visitor);

    RefPtr<InspectorMemoryBlock> externalResourcesStats = InspectorMemoryBlock::create().setName(MemoryBlockName::jsExternalResources);
    externalResourcesStats->setSize(visitor.externalStringSize() + visitor.externalArraySize());
    RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children = TypeBuilder::Array<InspectorMemoryBlock>::create();

    RefPtr<InspectorMemoryBlock> externalStringStats = InspectorMemoryBlock::create().setName(MemoryBlockName::jsExternalStrings);
    externalStringStats->setSize(visitor.externalStringSize());
    children->addItem(externalStringStats);

    RefPtr<InspectorMemoryBlock> externalArrayStats = InspectorMemoryBlock::create().setName(MemoryBlockName::jsExternalArrays);
    externalArrayStats->setSize(visitor.externalArraySize());
    children->addItem(externalArrayStats);

    return externalResourcesStats.release();
}

static PassRefPtr<InspectorMemoryBlock> dumpDOMStorageCache(size_t cacheSize)
{
    RefPtr<InspectorMemoryBlock> domStorageCache = InspectorMemoryBlock::create().setName(MemoryBlockName::domStorageCache);
    domStorageCache->setSize(cacheSize);
    return domStorageCache;
}

void InspectorMemoryAgent::getProcessMemoryDistribution(ErrorString*, RefPtr<InspectorMemoryBlock>& processMemory)
{
    processMemory = InspectorMemoryBlock::create().setName(MemoryBlockName::processPrivateMemory);

    InspectorDataCounter inspectorData;
    inspectorData.addComponent(MemoryBlockName::inspectorJSHeapData, ScriptProfiler::profilerSnapshotsSize());

    VisitedObjects visitedObjects;
    RefPtr<TypeBuilder::Array<InspectorMemoryBlock> > children = TypeBuilder::Array<InspectorMemoryBlock>::create();
    children->addItem(jsHeapInfo());
    children->addItem(renderTreeInfo(m_page)); // FIXME: collect for all pages?
    children->addItem(jsExternalResourcesInfo(visitedObjects));
    collectDomTreeInfo(m_page, m_inspectorClient, visitedObjects, children.get(), &inspectorData); // FIXME: collect for all pages?
    children->addItem(inspectorData.dumpStatistics());
    children->addItem(dumpDOMStorageCache(m_domStorageAgent->memoryBytesUsedByStorageCache()));
    addPlatformComponentsInfo(children);
    processMemory->setChildren(children);

    size_t privateBytes = 0;
    size_t sharedBytes = 0;
    MemoryUsageSupport::processMemorySizesInBytes(&privateBytes, &sharedBytes);
    processMemory->setSize(privateBytes);
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents, InspectorClient* client, InspectorState* state, Page* page, InspectorDOMStorageAgent* domStorageAgent)
    : InspectorBaseAgent<InspectorMemoryAgent>("Memory", instrumentingAgents, state)
    , m_inspectorClient(client)
    , m_page(page)
    , m_domStorageAgent(domStorageAgent)
{
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
