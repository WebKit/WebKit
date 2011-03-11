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
#include "Frame.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "MessagePort.h"
#include "PlatformBridge.h"
#include "SVGElement.h"
#include "V8Binding.h"
#include "V8CSSCharsetRule.h"
#include "V8CSSFontFaceRule.h"
#include "V8CSSImportRule.h"
#include "V8CSSMediaRule.h"
#include "V8CSSRuleList.h"
#include "V8CSSStyleDeclaration.h"
#include "V8CSSStyleRule.h"
#include "V8CSSStyleSheet.h"
#include "V8DOMMap.h"
#include "V8HTMLLinkElement.h"
#include "V8HTMLStyleElement.h"
#include "V8MessagePort.h"
#include "V8ProcessingInstruction.h"
#include "V8Proxy.h"
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

class GrouperItem {
public:
    GrouperItem(uintptr_t groupId, v8::Persistent<v8::Object> wrapper) 
        : m_groupId(groupId)
        , m_wrapper(wrapper) 
        {
        }

    uintptr_t groupId() const { return m_groupId; }
    v8::Persistent<v8::Object> wrapper() const { return m_wrapper; }

private:
    uintptr_t m_groupId;
    v8::Persistent<v8::Object> m_wrapper;
};

bool operator<(const GrouperItem& a, const GrouperItem& b)
{
    return a.groupId() < b.groupId();
}

typedef Vector<GrouperItem> GrouperList;

void makeV8ObjectGroups(GrouperList& grouper)
{
    // Group by sorting by the group id.
    std::sort(grouper.begin(), grouper.end());

    // FIXME Should probably work in iterators here, but indexes were easier for my simple mind.
    for (size_t i = 0; i < grouper.size(); ) {
        // Seek to the next key (or the end of the list).
        size_t nextKeyIndex = grouper.size();
        for (size_t j = i; j < grouper.size(); ++j) {
            if (grouper[i].groupId() != grouper[j].groupId()) {
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
            v8::Persistent<v8::Value> wrapper = grouper[i].wrapper();
            if (!wrapper.IsEmpty())
                group.append(wrapper);
        }

        if (group.size() > 1)
            v8::V8::AddObjectGroup(&group[0], group.size());

        ASSERT(i == nextKeyIndex);
    }
}

class NodeGrouperVisitor : public DOMWrapperMap<Node>::Visitor {
public:
    NodeGrouperVisitor()
    {
        // FIXME: grouper_.reserveCapacity(node_map.size());  ?
    }

    void visitDOMWrapper(DOMDataStore* store, Node* node, v8::Persistent<v8::Object> wrapper)
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
                while (root->parentNode())
                    root = root->parentNode();

                // If the node is alone in its DOM tree (doesn't have a parent or any
                // children) then the group will be filtered out later anyway.
                if (root == node && !node->hasChildNodes() && !node->hasAttributes())
                    return;
            }
            groupId = reinterpret_cast<uintptr_t>(root);
        }
        m_grouper.append(GrouperItem(groupId, wrapper));

        // If the node is styled and there is a wrapper for the inline
        // style declaration, we need to keep that style declaration
        // wrapper alive as well, so we add it to the object group.
        if (node->isStyledElement()) {
            StyledElement* element = reinterpret_cast<StyledElement*>(node);
            addDOMObjectToGroup(store, groupId, element->inlineStyleDecl());
        }

        if (node->isDocumentNode()) {
            Document* document = reinterpret_cast<Document*>(node);
            addDOMObjectToGroup(store, groupId, document->styleSheets());
            addDOMObjectToGroup(store, groupId, document->implementation());
        }

        WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(wrapper);

        if (V8HTMLLinkElement::info.equals(typeInfo)) {
            HTMLLinkElement* htmlLinkElement = static_cast<HTMLLinkElement*>(node);
            addDOMObjectToGroup(store, groupId, htmlLinkElement->sheet());
        }

        if (V8HTMLStyleElement::info.equals(typeInfo)) {
            HTMLStyleElement* htmlStyleElement = static_cast<HTMLStyleElement*>(node);
            addDOMObjectToGroup(store, groupId, htmlStyleElement->sheet());
        }

        if (V8ProcessingInstruction::info.equals(typeInfo)) {
            ProcessingInstruction* processingInstruction = static_cast<ProcessingInstruction*>(node);
            addDOMObjectToGroup(store, groupId, processingInstruction->sheet());
        }
    }

    void applyGrouping()
    {
        makeV8ObjectGroups(m_grouper);
    }

private:
    GrouperList m_grouper;

    void addDOMObjectToGroup(DOMDataStore* store, uintptr_t groupId, void* object)
    {
        if (!object)
            return;
        v8::Persistent<v8::Object> wrapper = store->domObjectMap().get(object);
        if (!wrapper.IsEmpty())
            m_grouper.append(GrouperItem(groupId, wrapper));
    }
};

static uintptr_t calculateGroupId(StyleBase* styleBase)
{
    ASSERT(styleBase);
    StyleBase* current = styleBase;
    StyleSheet* styleSheet = 0;
    while (true) {
        // Special case: CSSStyleDeclarations should have CSSRule as a parent
        // to proceed with parent traversal, otherwise they are coming from
        // inlined style declaration and should be treated as a root.
        if (current->isMutableStyleDeclaration()) {
            CSSMutableStyleDeclaration* cssMutableStyleDeclaration = static_cast<CSSMutableStyleDeclaration*>(current);
            if (CSSRule* parentRule = cssMutableStyleDeclaration->parentRule())
                current = parentRule;
            else
                return reinterpret_cast<uintptr_t>(cssMutableStyleDeclaration);
        }

        if (current->isStyleSheet())
            styleSheet = static_cast<StyleSheet*>(current);

        StyleBase* parent = current->parent();
        if (!parent)
            break;
        current = parent;
    }

    if (styleSheet)
        return reinterpret_cast<uintptr_t>(styleSheet);

    return reinterpret_cast<uintptr_t>(current);
}

class DOMObjectGrouperVisitor : public DOMWrapperMap<void>::Visitor {
public:
    DOMObjectGrouperVisitor()
    {
    }

    void startMap()
    {
        m_grouper.shrink(0);
    }

    void endMap()
    {
        makeV8ObjectGroups(m_grouper);
    }

    void visitDOMWrapper(DOMDataStore* store, void* object, v8::Persistent<v8::Object> wrapper)
    {
        WrapperTypeInfo* typeInfo = V8DOMWrapper::domWrapperType(wrapper);
        // FIXME: extend WrapperTypeInfo with isStyle to simplify the check below or consider
        // adding a virtual method to WrapperTypeInfo which would know how to group objects.
        // FIXME: check if there are other StyleBase wrappers we should care of.
        if (V8CSSStyleSheet::info.equals(typeInfo)
            || V8CSSStyleDeclaration::info.equals(typeInfo)
            || V8CSSCharsetRule::info.equals(typeInfo)
            || V8CSSFontFaceRule::info.equals(typeInfo)
            || V8CSSStyleRule::info.equals(typeInfo)
            || V8CSSImportRule::info.equals(typeInfo)
            || V8CSSMediaRule::info.equals(typeInfo)) {
            StyleBase* styleBase = static_cast<StyleBase*>(object);

            uintptr_t groupId = calculateGroupId(styleBase);
            m_grouper.append(GrouperItem(groupId, wrapper));

            if (V8CSSStyleDeclaration::info.equals(typeInfo)) {
                CSSStyleDeclaration* cssStyleDeclaration = static_cast<CSSStyleDeclaration*>(styleBase);
                if (cssStyleDeclaration->isMutableStyleDeclaration()) {
                    CSSMutableStyleDeclaration* cssMutableStyleDeclaration = static_cast<CSSMutableStyleDeclaration*>(cssStyleDeclaration);
                    CSSMutableStyleDeclaration::const_iterator end = cssMutableStyleDeclaration->end();
                    for (CSSMutableStyleDeclaration::const_iterator it = cssMutableStyleDeclaration->begin(); it != end; ++it) {
                        wrapper = store->domObjectMap().get(it->value());
                        if (!wrapper.IsEmpty())
                            m_grouper.append(GrouperItem(groupId, wrapper));
                    }
                }
            }
        } else if (V8StyleSheetList::info.equals(typeInfo)) {
            addAllItems(store, static_cast<StyleSheetList*>(object), wrapper);
        } else if (V8CSSRuleList::info.equals(typeInfo)) {
            addAllItems(store, static_cast<CSSRuleList*>(object), wrapper);
        }
    }

private:
    GrouperList m_grouper;

    template <class C>
    void addAllItems(DOMDataStore* store, C* collection, v8::Persistent<v8::Object> wrapper)
    {
        uintptr_t groupId = reinterpret_cast<uintptr_t>(collection);
        m_grouper.append(GrouperItem(groupId, wrapper));
        for (unsigned i = 0; i < collection->length(); i++) {
            wrapper = store->domObjectMap().get(collection->item(i));
            if (!wrapper.IsEmpty())
                m_grouper.append(GrouperItem(groupId, wrapper));
        }
    }
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
    NodeGrouperVisitor nodeGrouperVisitor;
    visitDOMNodesInCurrentThread(&nodeGrouperVisitor);
    nodeGrouperVisitor.applyGrouping();

    DOMObjectGrouperVisitor domObjectGrouperVisitor;
    visitDOMObjectsInCurrentThread(&domObjectGrouperVisitor);

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
