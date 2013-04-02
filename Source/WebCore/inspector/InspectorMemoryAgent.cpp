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

// Use a type alias instead of 'using' here which would cause a conflict on Mac.
typedef WebCore::TypeBuilder::Memory::MemoryBlock InspectorMemoryBlock;
typedef WebCore::TypeBuilder::Array<InspectorMemoryBlock> InspectorMemoryBlocks;

namespace WebCore {

namespace {

class MemoryUsageStatsGenerator {
public:
    MemoryUsageStatsGenerator() { }

    void dump(const TypeNameToSizeMap& sizesMap, InspectorMemoryBlocks* children)
    {
        m_sizesMap = sizesMap;

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

    TypeNameToSizeMap m_sizesMap;
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
        m_memoryClassInfo->addMember(string, "externalString");
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
        m_memoryClassInfo->addMember(arrayBufferView, "externalArray", WTF::RetainingPointer);
    }

    mutable MemoryClassInfo* m_memoryClassInfo;
};

} // namespace

InspectorMemoryAgent::~InspectorMemoryAgent()
{
}

void InspectorMemoryAgent::getDOMCounters(ErrorString*, int* documents, int* nodes, int* jsEventListeners)
{
    *documents = InspectorCounters::counterValue(InspectorCounters::DocumentCounter);
    *nodes = InspectorCounters::counterValue(InspectorCounters::NodeCounter);
    *jsEventListeners = ThreadLocalInspectorCounters::current().counterValue(ThreadLocalInspectorCounters::JSEventListenerCounter);
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

        while (Node* parentNode = node->parentNode())
            node = parentNode;

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

static void addPlatformComponentsInfo(TypeNameToSizeMap* memoryInfo)
{
    Vector<MemoryUsageSupport::ComponentInfo> components;
    MemoryUsageSupport::memoryUsageByComponents(components);
    for (Vector<MemoryUsageSupport::ComponentInfo>::iterator it = components.begin(); it != components.end(); ++it)
        memoryInfo->add(it->m_name, it->m_sizeInBytes);
}

static void addMemoryInstrumentationDebugData(MemoryInstrumentationClientImpl* client, TypeNameToSizeMap* memoryInfo)
{
    if (client->checkInstrumentedObjects()) {
        memoryInfo->add("InstrumentedObjectsCount", client->totalCountedObjects());
        memoryInfo->add("InstrumentedButNotAllocatedObjectsCount", client->totalObjectsNotInAllocatedSet());
    }
}

void InspectorMemoryAgent::getProcessMemoryDistributionMap(TypeNameToSizeMap* memoryInfo)
{
    getProcessMemoryDistributionImpl(false, memoryInfo);
}

void InspectorMemoryAgent::getProcessMemoryDistribution(ErrorString*, const bool* reportGraph, RefPtr<InspectorMemoryBlock>& processMemory, RefPtr<InspectorObject>& graphMetaInformation)
{
    TypeNameToSizeMap memoryInfo;
    graphMetaInformation = getProcessMemoryDistributionImpl(reportGraph && *reportGraph, &memoryInfo);

    MemoryUsageStatsGenerator statsGenerator;
    RefPtr<InspectorMemoryBlocks> children = InspectorMemoryBlocks::create();
    statsGenerator.dump(memoryInfo, children.get());

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
    info.addMember(m_page, "page");
}

namespace {

class FrontendWrapper : public HeapGraphSerializer::Client {
public:
    explicit FrontendWrapper(InspectorFrontend::Memory* frontend) : m_frontend(frontend) { }
    virtual void addNativeSnapshotChunk(PassRefPtr<TypeBuilder::Memory::HeapSnapshotChunk> heapSnapshotChunk) OVERRIDE
    {
        m_frontend->addNativeSnapshotChunk(heapSnapshotChunk);
    }
private:
    InspectorFrontend::Memory* m_frontend;
};

}

PassRefPtr<InspectorObject> InspectorMemoryAgent::getProcessMemoryDistributionImpl(bool reportGraph, TypeNameToSizeMap* memoryInfo)
{
    RefPtr<InspectorObject> meta;
    OwnPtr<HeapGraphSerializer> graphSerializer;
    OwnPtr<FrontendWrapper> frontendWrapper;

    if (reportGraph) {
        frontendWrapper = adoptPtr(new FrontendWrapper(m_frontend));
        graphSerializer = adoptPtr(new HeapGraphSerializer(frontendWrapper.get()));
    }

    MemoryInstrumentationClientImpl memoryInstrumentationClient(graphSerializer.get());
    m_inspectorClient->getAllocatedObjects(memoryInstrumentationClient.allocatedObjects());
    MemoryInstrumentationImpl memoryInstrumentation(&memoryInstrumentationClient);

    reportJSHeapInfo(memoryInstrumentationClient);
    reportRenderTreeInfo(memoryInstrumentationClient, m_page);
    collectDomTreeInfo(memoryInstrumentation, m_page); // FIXME: collect for all pages?

    PlatformMemoryInstrumentation::reportStaticMembersMemoryUsage(&memoryInstrumentation);
    WebCoreMemoryInstrumentation::reportStaticMembersMemoryUsage(&memoryInstrumentation);

    memoryInstrumentation.addRootObject(this);
    memoryInstrumentation.addRootObject(memoryInstrumentation);
    memoryInstrumentation.addRootObject(memoryInstrumentationClient);
    if (graphSerializer) {
        memoryInstrumentation.addRootObject(graphSerializer.get());
        meta = graphSerializer->finish();
        graphSerializer.release(); // Release it earlier than frontendWrapper
        frontendWrapper.release();
    }

    m_inspectorClient->dumpUncountedAllocatedObjects(memoryInstrumentationClient.countedObjects());

    *memoryInfo = memoryInstrumentationClient.sizesMap();
    addPlatformComponentsInfo(memoryInfo);
    addMemoryInstrumentationDebugData(&memoryInstrumentationClient, memoryInfo);
    return meta.release();
}

InspectorMemoryAgent::InspectorMemoryAgent(InstrumentingAgents* instrumentingAgents, InspectorClient* client, InspectorCompositeState* state, Page* page)
    : InspectorBaseAgent<InspectorMemoryAgent>("Memory", instrumentingAgents, state)
    , m_inspectorClient(client)
    , m_page(page)
    , m_frontend(0)
{
}

void InspectorMemoryAgent::setFrontend(InspectorFrontend* frontend)
{
    ASSERT(!m_frontend);
    m_frontend = frontend->memory();
}

void InspectorMemoryAgent::clearFrontend()
{
    m_frontend = 0;
}

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
