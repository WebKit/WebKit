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

#include "ActiveDOMObject.h"
#include "Attr.h"
#include "DOMDataStore.h"
#include "DOMImplementation.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "MessagePort.h"
#include "PlatformBridge.h"
#include "RetainedDOMInfo.h"
#include "RetainedObjectInfo.h"
#include "V8Binding.h"
#include "V8CSSRule.h"
#include "V8CSSRuleList.h"
#include "V8CSSStyleDeclaration.h"
#include "V8DOMImplementation.h"
#include "V8MessagePort.h"
#include "V8StyleSheet.h"
#include "V8StyleSheetList.h"
#include "WrapperTypeInfo.h"

#include <algorithm>
#include <utility>
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

class DOMObjectVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
    {
        WrapperTypeInfo* type = V8DOMWrapper::domWrapperType(wrapper);
        UNUSED_PARAM(type);
        UNUSED_PARAM(object);
    }
};

class EnsureWeakDOMNodeVisitor : public DOMWrapperMap<Node>::Visitor {
public:
    void visitDOMWrapper(DOMDataStore* store, Node* object, v8::Persistent<v8::Object> wrapper)
    {
        UNUSED_PARAM(object);
        ASSERT(wrapper.IsWeak());
    }
};

#endif // NDEBUG

class GCPrologueVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
    {
        WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(wrapper);  

        // Additional handling of message port ensuring that entangled ports also
        // have their wrappers entangled. This should ideally be handled when the
        // ports are actually entangled in MessagePort::entangle, but to avoid
        // forking MessagePort.* this is postponed to GC time. Having this postponed
        // has the drawback that the wrappers are "entangled/unentangled" for each
        // GC even though their entaglement most likely is still the same.
        if (V8MessagePort::info.equals(typeInfo)) {
            // Mark each port as in-use if it's entangled. For simplicity's sake, we assume all ports are remotely entangled,
            // since the Chromium port implementation can't tell the difference.
            MessagePort* port1 = static_cast<MessagePort*>(object);
            if (port1->isEntangled() || port1->hasPendingActivity())
                wrapper.ClearWeak();
        } else {
            ActiveDOMObject* activeDOMObject = typeInfo->toActiveDOMObject(wrapper);
            if (activeDOMObject && activeDOMObject->hasPendingActivity())
                wrapper.ClearWeak();
        }
    }
};

// Implements v8::RetainedObjectInfo.
class UnspecifiedGroup : public RetainedObjectInfo {
public:
    explicit UnspecifiedGroup(void* object)
        : m_object(object)
    {
        ASSERT(m_object);
    }
    
    virtual void Dispose() { delete this; }
  
    virtual bool IsEquivalent(v8::RetainedObjectInfo* other)
    {
        ASSERT(other);
        return other == this || static_cast<WebCore::RetainedObjectInfo*>(other)->GetEquivalenceClass() == this->GetEquivalenceClass();
    }

    virtual intptr_t GetHash()
    {
        return reinterpret_cast<intptr_t>(m_object);
    }
    
    virtual const char* GetLabel()
    {
        return "Object group";
    }

    virtual intptr_t GetEquivalenceClass()
    {
        return reinterpret_cast<intptr_t>(m_object);
    }

private:
    void* m_object;
};

class GroupId {
public:
    GroupId() : m_type(NullType), m_groupId(0) {}
    GroupId(Node* node) : m_type(NodeType), m_node(node) {}
    GroupId(void* other) : m_type(OtherType), m_other(other) {}
    bool operator!() const { return m_type == NullType; }
    uintptr_t groupId() const { return m_groupId; }
    RetainedObjectInfo* createRetainedObjectInfo() const
    {
        switch (m_type) {
        case NullType:
            return 0;
        case NodeType:
            return new RetainedDOMInfo(m_node);
        case OtherType:
            return new UnspecifiedGroup(m_other);
        default:
            return 0;
        }
    }
    
private:
    enum Type {
        NullType,
        NodeType,
        OtherType
    };
    Type m_type;
    union {
        uintptr_t m_groupId;
        Node* m_node;
        void* m_other;
    };
};

class GrouperItem {
public:
    GrouperItem(GroupId groupId, v8::Persistent<v8::Object> wrapper) : m_groupId(groupId), m_wrapper(wrapper) {}
    uintptr_t groupId() const { return m_groupId.groupId(); }
    RetainedObjectInfo* createRetainedObjectInfo() const { return m_groupId.createRetainedObjectInfo(); }
    v8::Persistent<v8::Object> wrapper() const { return m_wrapper; }

private:
    GroupId m_groupId;
    v8::Persistent<v8::Object> m_wrapper;
};

bool operator<(const GrouperItem& a, const GrouperItem& b)
{
    return a.groupId() < b.groupId();
}

typedef Vector<GrouperItem> GrouperList;

// If the node is in document, put it in the ownerDocument's object group.
//
// If an image element was created by JavaScript "new Image",
// it is not in a document. However, if the load event has not
// been fired (still onloading), it is treated as in the document.
//
// Otherwise, the node is put in an object group identified by the root
// element of the tree to which it belongs.
static GroupId calculateGroupId(Node* node)
{
    if (node->inDocument() || (node->hasTagName(HTMLNames::imgTag) && !static_cast<HTMLImageElement*>(node)->haveFiredLoadEvent()))
        return GroupId(node->document());

    Node* root = node;
    if (node->isAttributeNode()) {
        root = static_cast<Attr*>(node)->ownerElement();
        // If the attribute has no element, no need to put it in the group,
        // because it'll always be a group of 1.
        if (!root)
            return GroupId();
    } else {
        while (Node* parent = root->parentOrHostNode())
            root = parent;
    }

    return GroupId(root);
}

static GroupId calculateGroupId(StyleBase* styleBase)
{
    ASSERT(styleBase);
    StyleBase* current = styleBase;
    StyleSheet* styleSheet = 0;
    while (true) {
        // Special case: CSSStyleDeclarations might be either inline and in this case
        // we need to group them with their node or regular ones.
        if (current->isMutableStyleDeclaration()) {
            CSSMutableStyleDeclaration* cssMutableStyleDeclaration = static_cast<CSSMutableStyleDeclaration*>(current);
            if (cssMutableStyleDeclaration->isInlineStyleDeclaration())
                return calculateGroupId(cssMutableStyleDeclaration->node());
            // Either we have no parent, or this parent is a CSSRule.
            ASSERT(cssMutableStyleDeclaration->parent() == cssMutableStyleDeclaration->parentRule());
        }

        if (current->isStyleSheet())
            styleSheet = static_cast<StyleSheet*>(current);

        StyleBase* parent = current->parent();
        if (!parent)
            break;
        current = parent;
    }

    if (styleSheet) {
        if (Node* ownerNode = styleSheet->ownerNode())
            return calculateGroupId(ownerNode);
        return GroupId(styleSheet);
    }

    return GroupId(current);
}

class GrouperVisitor : public DOMWrapperMap<Node>::Visitor, public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(DOMDataStore* store, Node* node, v8::Persistent<v8::Object> wrapper)
    {
        if (node->hasEventListeners()) {
            Vector<v8::Persistent<v8::Value> > listeners;
            EventListenerIterator iterator(node);
            while (EventListener* listener = iterator.nextListener()) {
                if (listener->type() != EventListener::JSEventListenerType)
                    continue;
                V8AbstractEventListener* v8listener = static_cast<V8AbstractEventListener*>(listener);
                if (!v8listener->hasExistingListenerObject())
                    continue;
                listeners.append(v8listener->existingListenerObjectPersistentHandle());
            }
            if (!listeners.isEmpty())
                v8::V8::AddImplicitReferences(wrapper, listeners.data(), listeners.size());
        }

        GroupId groupId = calculateGroupId(node);
        if (!groupId)
            return;
        m_grouper.append(GrouperItem(groupId, wrapper));
    }

    void visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
    {
        WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(wrapper);

        if (typeInfo->isSubclass(&V8StyleSheetList::info)) {
            StyleSheetList* styleSheetList = static_cast<StyleSheetList*>(object);
            GroupId groupId(styleSheetList);
            if (Document* document = styleSheetList->document())
                groupId = GroupId(document);
            m_grouper.append(GrouperItem(groupId, wrapper));

        } else if (typeInfo->isSubclass(&V8DOMImplementation::info)) {
            DOMImplementation* domImplementation = static_cast<DOMImplementation*>(object);
            GroupId groupId(domImplementation);
            if (Document* document = domImplementation->document())
                groupId = GroupId(document);
            m_grouper.append(GrouperItem(groupId, wrapper));

        } else if (typeInfo->isSubclass(&V8StyleSheet::info) || typeInfo->isSubclass(&V8CSSRule::info)) {
            m_grouper.append(GrouperItem(calculateGroupId(static_cast<StyleBase*>(object)), wrapper));

        } else if (typeInfo->isSubclass(&V8CSSStyleDeclaration::info)) {
            CSSStyleDeclaration* cssStyleDeclaration = static_cast<CSSStyleDeclaration*>(object);

            GroupId groupId = calculateGroupId(cssStyleDeclaration);
            m_grouper.append(GrouperItem(groupId, wrapper));

            // Keep alive "dirty" primitive values (i.e. the ones that
            // have user-added properties) by creating implicit
            // references between the style declaration and the values
            // in it.
            if (cssStyleDeclaration->isMutableStyleDeclaration()) {
                CSSMutableStyleDeclaration* cssMutableStyleDeclaration = static_cast<CSSMutableStyleDeclaration*>(cssStyleDeclaration);
                Vector<v8::Persistent<v8::Value> > values;
                values.reserveCapacity(cssMutableStyleDeclaration->length());
                CSSMutableStyleDeclaration::const_iterator end = cssMutableStyleDeclaration->end();
                for (CSSMutableStyleDeclaration::const_iterator it = cssMutableStyleDeclaration->begin(); it != end; ++it) {
                    v8::Persistent<v8::Object> value = store->domObjectMap().get(it->value());
                    if (!value.IsEmpty() && value->IsDirty())
                        values.append(value);
                }
                if (!values.isEmpty())
                    v8::V8::AddImplicitReferences(wrapper, values.data(), values.size());
            }

        } else if (typeInfo->isSubclass(&V8CSSRuleList::info)) {
            CSSRuleList* cssRuleList = static_cast<CSSRuleList*>(object);
            GroupId groupId(cssRuleList);
            StyleList* styleList = cssRuleList->styleList();
            if (styleList)
                groupId = calculateGroupId(styleList);
            m_grouper.append(GrouperItem(groupId, wrapper));
        }
    }

    void applyGrouping()
    {
        // Group by sorting by the group id.
        std::sort(m_grouper.begin(), m_grouper.end());

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

            size_t rootIndex = i;
            
            Vector<v8::Persistent<v8::Value> > group;
            group.reserveCapacity(nextKeyIndex - i);
            for (; i < nextKeyIndex; ++i) {
                v8::Persistent<v8::Value> wrapper = m_grouper[i].wrapper();
                if (!wrapper.IsEmpty())
                    group.append(wrapper);
            }

            if (group.size() > 1)
                v8::V8::AddObjectGroup(&group[0], group.size(), m_grouper[rootIndex].createRetainedObjectInfo());

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
    GrouperVisitor grouperVisitor;
    visitDOMNodesInCurrentThread(&grouperVisitor);
    visitDOMObjectsInCurrentThread(&grouperVisitor);
    grouperVisitor.applyGrouping();

    // Clean single element cache for string conversions.
    lastStringImpl = 0;
    lastV8String.Clear();
}

class GCEpilogueVisitor : public DOMWrapperMap<void>::Visitor {
public:
    void visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
    {
        WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(wrapper);
        if (V8MessagePort::info.equals(typeInfo)) {
            MessagePort* port1 = static_cast<MessagePort*>(object);
            // We marked this port as reachable in GCPrologueVisitor.  Undo this now since the
            // port could be not reachable in the future if it gets disentangled (and also
            // GCPrologueVisitor expects to see all handles marked as weak).
            if ((!wrapper.IsWeak() && !wrapper.IsNearDeath()) || port1->hasPendingActivity())
                wrapper.MakeWeak(port1, &DOMDataStore::weakActiveDOMObjectCallback);
        } else {
            ActiveDOMObject* activeDOMObject = typeInfo->toActiveDOMObject(wrapper);
            if (activeDOMObject && activeDOMObject->hasPendingActivity()) {
                ASSERT(!wrapper.IsWeak());
                // NOTE: To re-enable weak status of the active object we use
                // |object| from the map and not |activeDOMObject|. The latter
                // may be a different pointer (in case ActiveDOMObject is not
                // the main base class of the object's class) and pointer
                // identity is required by DOM map functions.
                wrapper.MakeWeak(object, &DOMDataStore::weakActiveDOMObjectCallback);
            }
        }
    }
};

int V8GCController::workingSetEstimateMB = 0;

namespace {

int getMemoryUsageInMB()
{
#if PLATFORM(CHROMIUM) || PLATFORM(ANDROID)
    return PlatformBridge::memoryUsageMB();
#else
    return 0;
#endif
}

int getActualMemoryUsageInMB()
{
#if PLATFORM(CHROMIUM) || PLATFORM(ANDROID)
    return PlatformBridge::actualMemoryUsageMB();
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

    workingSetEstimateMB = getActualMemoryUsageInMB();

#ifndef NDEBUG
    // Check all survivals are weak.
    DOMObjectVisitor domObjectVisitor;
    visitDOMObjectsInCurrentThread(&domObjectVisitor);

    EnsureWeakDOMNodeVisitor weakDOMNodeVisitor;
    visitDOMNodesInCurrentThread(&weakDOMNodeVisitor);

    enumerateGlobalHandles();
#endif
}

void V8GCController::checkMemoryUsage()
{
#if PLATFORM(CHROMIUM) || PLATFORM(QT) && !OS(SYMBIAN)
    // These values are appropriate for Chromium only.
    const int lowUsageMB = 256;  // If memory usage is below this threshold, do not bother forcing GC.
    const int highUsageMB = 1024;  // If memory usage is above this threshold, force GC more aggresively.
    const int highUsageDeltaMB = 128;  // Delta of memory usage growth (vs. last workingSetEstimateMB) to force GC when memory usage is high.
#elif PLATFORM(ANDROID)
    // Query the PlatformBridge for memory thresholds as these vary device to device.
    static const int lowUsageMB = PlatformBridge::lowMemoryUsageMB();
    static const int highUsageMB = PlatformBridge::highMemoryUsageMB();
    // We use a delta of -1 to ensure that when we are in a low memory situation we always trigger a GC.
    static const int highUsageDeltaMB = -1;
#else
    return;
#endif

    int memoryUsageMB = getMemoryUsageInMB();
    if ((memoryUsageMB > lowUsageMB && memoryUsageMB > 2 * workingSetEstimateMB) || (memoryUsageMB > highUsageMB && memoryUsageMB > workingSetEstimateMB + highUsageDeltaMB))
        v8::V8::LowMemoryNotification();
}


}  // namespace WebCore
