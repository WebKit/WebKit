/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "V8GCController.h"

#include "DOMDataStore.h"
#include "DOMObjectsInclude.h"
#include "V8DOMMap.h"
#include "V8Index.h"
#include "V8Proxy.h"

#include <algorithm>
#include <utility>
#include <v8.h>
#include <v8-debug.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UnusedParam.h>

namespace WebCore {

#ifndef NDEBUG
// Keeps track of global handles created (not JS wrappers
// of DOM objects). Often these global handles are source
// of leaks.
//
// If you want to let a C++ object hold a persistent handle
// to a JS object, you should register the handle here to
// keep track of leaks.
//
// When creating a persistent handle, call:
//
// #ifndef NDEBUG
//    V8GCController::registerGlobalHandle(type, host, handle);
// #endif
//
// When releasing the handle, call:
//
// #ifndef NDEBUG
//    V8GCController::unregisterGlobalHandle(type, host, handle);
// #endif
//
typedef HashMap<v8::Value*, GlobalHandleInfo*> GlobalHandleMap;

static GlobalHandleMap& globalHandleMap()
{
    DEFINE_STATIC_LOCAL(GlobalHandleMap, staticGlobalHandleMap, ());
    return staticGlobalHandleMap;
}

// The function is the place to set the break point to inspect
// live global handles. Leaks are often come from leaked global handles.
static void enumerateGlobalHandles()
{
    for (GlobalHandleMap::iterator it = globalHandleMap().begin(), end = globalHandleMap().end(); it != end; ++it) {
        GlobalHandleInfo* info = it->second;
        UNUSED_PARAM(info);
        v8::Value* handle = it->first;
        UNUSED_PARAM(handle);
    }
}

void V8GCController::registerGlobalHandle(GlobalHandleType type, void* host, v8::Persistent<v8::Value> handle)
{
    ASSERT(!globalHandleMap().contains(*handle));
    globalHandleMap().set(*handle, new GlobalHandleInfo(host, type));
}

void V8GCController::unregisterGlobalHandle(void* host, v8::Persistent<v8::Value> handle)
{
    ASSERT(globalHandleMap().contains(*handle));
    GlobalHandleInfo* info = globalHandleMap().take(*handle);
    ASSERT(info->m_host == host);
    delete info;
}
#endif // ifndef NDEBUG

typedef HashMap<Node*, v8::Object*> DOMNodeMap;
typedef HashMap<void*, v8::Object*> DOMObjectMap;

#ifndef NDEBUG

static void enumerateDOMObjectMap(DOMObjectMap& wrapperMap)
{
    for (DOMObjectMap::iterator it = wrapperMap.begin(), end = wrapperMap.end(); it != end; ++it) {
        v8::Persistent<v8::Object> wrapper(it->second);
        V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
        void* object = it->first;
        UNUSED_PARAM(type);
        UNUSED_PARAM(object);
    }
}

class DOMObjectVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(void* object, v8::Persistent<v8::Object> wrapper)
    {
        V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
        UNUSED_PARAM(type);
        UNUSED_PARAM(object);
    }
};

class EnsureWeakDOMNodeVisitor : public DOMWrapperMap<Node>::Visitor {
public:
    void visitDOMWrapper(Node* object, v8::Persistent<v8::Object> wrapper)
    {
        UNUSED_PARAM(object);
        ASSERT(wrapper.IsWeak());
    }
};

#endif // NDEBUG

// A map from a DOM node to its JS wrapper, the wrapper
// is kept as a strong reference to survive GCs.
static DOMObjectMap& gcProtectedMap()
{
    DEFINE_STATIC_LOCAL(DOMObjectMap, staticGcProtectedMap, ());
    return staticGcProtectedMap;
}

void V8GCController::gcProtect(void* domObject)
{
    if (!domObject)
        return;
    if (gcProtectedMap().contains(domObject))
        return;
    if (!getDOMObjectMap().contains(domObject))
        return;

    // Create a new (strong) persistent handle for the object.
    v8::Persistent<v8::Object> wrapper = getDOMObjectMap().get(domObject);
    if (wrapper.IsEmpty())
        return;

    gcProtectedMap().set(domObject, *v8::Persistent<v8::Object>::New(wrapper));
}

void V8GCController::gcUnprotect(void* domObject)
{
    if (!domObject)
        return;
    if (!gcProtectedMap().contains(domObject))
        return;

    // Dispose the strong reference.
    v8::Persistent<v8::Object> wrapper(gcProtectedMap().take(domObject));
    wrapper.Dispose();
}

class GCPrologueVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(void* object, v8::Persistent<v8::Object> wrapper)
    {
        ASSERT(wrapper.IsWeak());
        V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
        switch (type) {
#define MAKE_CASE(TYPE, NAME)                             \
        case V8ClassIndex::TYPE: {                    \
            NAME* impl = static_cast<NAME*>(object);  \
            if (impl->hasPendingActivity())           \
                wrapper.ClearWeak();                  \
            break;                                    \
        }
    ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
    default:
        ASSERT_NOT_REACHED();
#undef MAKE_CASE
        }

    // Additional handling of message port ensuring that entangled ports also
    // have their wrappers entangled. This should ideally be handled when the
    // ports are actually entangled in MessagePort::entangle, but to avoid
    // forking MessagePort.* this is postponed to GC time. Having this postponed
    // has the drawback that the wrappers are "entangled/unentangled" for each
    // GC even though their entaglement most likely is still the same.
    if (type == V8ClassIndex::MESSAGEPORT) {
        // Mark each port as in-use if it's entangled. For simplicity's sake, we assume all ports are remotely entangled,
        // since the Chromium port implementation can't tell the difference.
        MessagePort* port1 = static_cast<MessagePort*>(object);
        if (port1->isEntangled())
            wrapper.ClearWeak();
    }
}
};

class GrouperItem {
public:
    GrouperItem(uintptr_t groupId, Node* node, v8::Persistent<v8::Object> wrapper) 
        : m_groupId(groupId)
        , m_node(node)
        , m_wrapper(wrapper) 
        {
        }

    uintptr_t groupId() const { return m_groupId; }
    Node* node() const { return m_node; }
    v8::Persistent<v8::Object> wrapper() const { return m_wrapper; }

private:
    uintptr_t m_groupId;
    Node* m_node;
    v8::Persistent<v8::Object> m_wrapper;
};

bool operator<(const GrouperItem& a, const GrouperItem& b)
{
    return a.groupId() < b.groupId();
}

typedef Vector<GrouperItem> GrouperList;

class ObjectGrouperVisitor : public DOMWrapperMap<Node>::Visitor {
public:
    ObjectGrouperVisitor()
    {
        // FIXME: grouper_.reserveCapacity(node_map.size());  ?
    }

    void visitDOMWrapper(Node* node, v8::Persistent<v8::Object> wrapper)
    {

        // If the node is in document, put it in the ownerDocument's object group.
        //
        // If an image element was created by JavaScript "new Image",
        // it is not in a document. However, if the load event has not
        // been fired (still onloading), it is treated as in the document.
        //
        // Otherwise, the node is put in an object group identified by the root
        // element of the tree to which it belongs.
        uintptr_t groupId;
        if (node->inDocument() || (node->hasTagName(HTMLNames::imgTag) && !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent()))
            groupId = reinterpret_cast<uintptr_t>(node->document());
        else {
            Node* root = node;
            if (node->isAttributeNode()) {
                root = static_cast<Attr*>(node)->ownerElement();
                // If the attribute has no element, no need to put it in the group,
                // because it'll always be a group of 1.
                if (!root)
                    return;
            } else {
                while (root->parent())
                    root = root->parent();

                // If the node is alone in its DOM tree (doesn't have a parent or any
                // children) then the group will be filtered out later anyway.
                if (root == node && !node->hasChildNodes() && !node->hasAttributes())
                    return;
            }
            groupId = reinterpret_cast<uintptr_t>(root);
        }
        m_grouper.append(GrouperItem(groupId, node, wrapper));
    }

    void applyGrouping()
    {
        // Group by sorting by the group id.
        std::sort(m_grouper.begin(), m_grouper.end());

        // FIXME Should probably work in iterators here, but indexes were easier for my simple mind.
        for (size_t i = 0; i < m_grouper.size(); ) {
            // Seek to the next key (or the end of the list).
            size_t nextKeyIndex = m_grouper.size();
            for (size_t j = i; j < m_grouper.size(); ++j) {
                if (m_grouper[i].groupId() != m_grouper[j].groupId()) {
                    nextKeyIndex = j;
                    break;
                }
            }

            ASSERT(nextKeyIndex > i);

            // We only care about a group if it has more than one object. If it only
            // has one object, it has nothing else that needs to be kept alive.
            if (nextKeyIndex - i <= 1) {
                i = nextKeyIndex;
                continue;
            }

            Vector<v8::Persistent<v8::Value> > group;
            group.reserveCapacity(nextKeyIndex - i);
            for (; i < nextKeyIndex; ++i) {
                v8::Persistent<v8::Value> wrapper = m_grouper[i].wrapper();
                if (!wrapper.IsEmpty())
                    group.append(wrapper);
                /* FIXME: Re-enabled this code to avoid GCing these wrappers!
                             Currently this depends on looking up the wrapper
                             during a GC, but we don't know which isolated world
                             we're in, so it's unclear which map to look in...

                // If the node is styled and there is a wrapper for the inline
                // style declaration, we need to keep that style declaration
                // wrapper alive as well, so we add it to the object group.
                if (node->isStyledElement()) {
                  StyledElement* element = reinterpret_cast<StyledElement*>(node);
                  CSSStyleDeclaration* style = element->inlineStyleDecl();
                  if (style != NULL) {
                    wrapper = getDOMObjectMap().get(style);
                    if (!wrapper.IsEmpty())
                      group.append(wrapper);
                  }
                }
                */
            }

            if (group.size() > 1)
                v8::V8::AddObjectGroup(&group[0], group.size());

            ASSERT(i == nextKeyIndex);
        }
    }

private:
    GrouperList m_grouper;
};

// Create object groups for DOM tree nodes.
void V8GCController::gcPrologue()
{
    v8::HandleScope scope;

#ifndef NDEBUG
    DOMObjectVisitor domObjectVisitor;
    visitDOMObjectsInCurrentThread(&domObjectVisitor);
#endif

    // Run through all objects with possible pending activity making their
    // wrappers non weak if there is pending activity.
    GCPrologueVisitor prologueVisitor;
    visitActiveDOMObjectsInCurrentThread(&prologueVisitor);

    // Create object groups.
    ObjectGrouperVisitor objectGrouperVisitor;
    visitDOMNodesInCurrentThread(&objectGrouperVisitor);
    objectGrouperVisitor.applyGrouping();
}

class GCEpilogueVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(void* object, v8::Persistent<v8::Object> wrapper)
    {
        V8ClassIndex::V8WrapperType type = V8DOMWrapper::domWrapperType(wrapper);
        switch (type) {
#define MAKE_CASE(TYPE, NAME)                                                   \
        case V8ClassIndex::TYPE: {                                              \
          NAME* impl = static_cast<NAME*>(object);                              \
          if (impl->hasPendingActivity()) {                                     \
            ASSERT(!wrapper.IsWeak());                                          \
            wrapper.MakeWeak(impl, &DOMDataStore::weakActiveDOMObjectCallback); \
          }                                                                     \
          break;                                                                \
        }
ACTIVE_DOM_OBJECT_TYPES(MAKE_CASE)
        default:
            ASSERT_NOT_REACHED();
#undef MAKE_CASE
        }

        if (type == V8ClassIndex::MESSAGEPORT) {
            MessagePort* port1 = static_cast<MessagePort*>(object);
            // We marked this port as reachable in GCPrologueVisitor.  Undo this now since the
            // port could be not reachable in the future if it gets disentangled (and also
            // GCPrologueVisitor expects to see all handles marked as weak).
            if (!wrapper.IsWeak() && !wrapper.IsNearDeath())
                wrapper.MakeWeak(port1, &DOMDataStore::weakActiveDOMObjectCallback);
        }
    }
};

int V8GCController::workingSetEstimateMB = 0;

namespace {

int getMemoryUsageInMB()
{
#if PLATFORM(CHROMIUM)
    return ChromiumBridge::memoryUsageMB();
#else
    return 0;
#endif
}

}  // anonymous namespace

void V8GCController::gcEpilogue()
{
    v8::HandleScope scope;

    // Run through all objects with pending activity making their wrappers weak
    // again.
    GCEpilogueVisitor epilogueVisitor;
    visitActiveDOMObjectsInCurrentThread(&epilogueVisitor);

    workingSetEstimateMB = getMemoryUsageInMB();

#ifndef NDEBUG
    // Check all survivals are weak.
    DOMObjectVisitor domObjectVisitor;
    visitDOMObjectsInCurrentThread(&domObjectVisitor);

    EnsureWeakDOMNodeVisitor weakDOMNodeVisitor;
    visitDOMNodesInCurrentThread(&weakDOMNodeVisitor);

    enumerateDOMObjectMap(gcProtectedMap());
    enumerateGlobalHandles();
#endif
}

void V8GCController::checkMemoryUsage()
{
#if PLATFORM(CHROMIUM)
    // These values are appropriate for Chromium only.
    const int lowUsageMB = 256;  // If memory usage is below this threshold, do not bother forcing GC.
    const int highUsageMB = 1024;  // If memory usage is above this threshold, force GC more aggresively.
    const int highUsageDeltaMB = 128;  // Delta of memory usage growth (vs. last workingSetEstimateMB) to force GC when memory usage is high.

    int memoryUsageMB = getMemoryUsageInMB();
    if ((memoryUsageMB > lowUsageMB && memoryUsageMB > 2 * workingSetEstimateMB) || (memoryUsageMB > highUsageMB && memoryUsageMB > workingSetEstimateMB + highUsageDeltaMB))
        v8::V8::LowMemoryNotification();
#endif
}


}  // namespace WebCore
