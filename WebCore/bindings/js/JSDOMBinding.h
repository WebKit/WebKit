/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008, 2009 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
 *  Copyright (C) 2009 Google, Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef JSDOMBinding_h
#define JSDOMBinding_h

#include "JSDOMGlobalObject.h"
#include "JSSVGContextCache.h"
#include "Document.h"
#include <runtime/Completion.h>
#include <runtime/Lookup.h>
#include <runtime/WeakGCMap.h>
#include <wtf/Noncopyable.h>

namespace JSC {
    class JSGlobalData;
    class DebuggerCallFrame;
}

namespace WebCore {

    class Document;
    class Frame;
    class JSNode;
    class KURL;
    class Node;
    class String;
    class ScriptController;
    class ScriptCachedFrameData;

    typedef int ExceptionCode;

#if ENABLE(SVG)
    class SVGElement;
#endif

    // Base class for all objects in this binding except Window.
    class DOMObject : public JSC::JSObject {
    protected:
        explicit DOMObject(NonNullPassRefPtr<JSC::Structure> structure) 
            : JSObject(structure)
        {
        }

        virtual bool defineOwnProperty(JSC::ExecState*, const JSC::Identifier&, JSC::PropertyDescriptor&, bool);

#ifndef NDEBUG
        virtual ~DOMObject();
#endif
    };

    // FIXME: This class should collapse into DOMObject once all DOMObjects are
    // updated to store a globalObject pointer.
    class DOMObjectWithGlobalPointer : public DOMObject {
    public:
        JSDOMGlobalObject* globalObject() const { return static_cast<JSDOMGlobalObject*>(getAnonymousValue(GlobalObjectSlot).asCell()); }

        ScriptExecutionContext* scriptExecutionContext() const
        {
            // FIXME: Should never be 0, but can be due to bug 27640.
            return globalObject()->scriptExecutionContext();
        }

        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype)
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), AnonymousSlotCount);
        }

    protected:
        static const unsigned AnonymousSlotCount = 1 + DOMObject::AnonymousSlotCount;
        static const unsigned GlobalObjectSlot = AnonymousSlotCount - 1;

        DOMObjectWithGlobalPointer(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject* globalObject)
            : DOMObject(structure)
        {
            // FIXME: This ASSERT is valid, but fires in fast/dom/gc-6.html when trying to create
            // new JavaScript objects on detached windows due to DOMWindow::document()
            // needing to reach through the frame to get to the Document*.  See bug 27640.
            // ASSERT(globalObject->scriptExecutionContext());
            putAnonymousValue(GlobalObjectSlot, globalObject);
        }
        virtual ~DOMObjectWithGlobalPointer() { }
    };

    // Base class for all constructor objects in the JSC bindings.
    class DOMConstructorObject : public DOMObjectWithGlobalPointer {
    public:
        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype)
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), AnonymousSlotCount);
        }

    protected:
        static const unsigned StructureFlags = JSC::ImplementsHasInstance | JSC::OverridesMarkChildren | DOMObjectWithGlobalPointer::StructureFlags;
        DOMConstructorObject(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject* globalObject)
            : DOMObjectWithGlobalPointer(structure, globalObject)
        {
        }
    };

    // Constructors using this base class depend on being in a Document and
    // can never be used from a WorkerContext.
    class DOMConstructorWithDocument : public DOMConstructorObject {
    public:
        Document* document() const
        {
            return static_cast<Document*>(scriptExecutionContext());
        }

    protected:
        DOMConstructorWithDocument(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject* globalObject)
            : DOMConstructorObject(structure, globalObject)
        {
            ASSERT(globalObject->scriptExecutionContext()->isDocument());
        }
    };

    typedef JSC::WeakGCMap<void*, DOMObject*> DOMObjectWrapperMap;
    typedef JSC::WeakGCMap<StringImpl*, JSC::JSString*> JSStringCache; 

    class DOMWrapperWorld : public RefCounted<DOMWrapperWorld> {
    public:
        static PassRefPtr<DOMWrapperWorld> create(JSC::JSGlobalData* globalData, bool isNormal)
        {
            return adoptRef(new DOMWrapperWorld(globalData, isNormal));
        }
        ~DOMWrapperWorld();

        void detachFromGlobalData() { m_globalData = 0; }
        void rememberDocument(Document* document) { documentsWithWrappers.add(document); }
        void forgetDocument(Document* document) { documentsWithWrappers.remove(document); }

        // FIXME: can we make this private?
        DOMObjectWrapperMap m_wrappers;
        JSStringCache m_stringCache;

        bool isNormal() const { return m_isNormal; }

    protected:
        DOMWrapperWorld(JSC::JSGlobalData*, bool isNormal);

    private:
        JSC::JSGlobalData* m_globalData;
        HashSet<Document*> documentsWithWrappers;
        bool m_isNormal;
    };

    // Map from static HashTable instances to per-GlobalData ones.
    class DOMObjectHashTableMap {
    public:
        static DOMObjectHashTableMap& mapFor(JSC::JSGlobalData&);

        ~DOMObjectHashTableMap()
        {
            HashMap<const JSC::HashTable*, JSC::HashTable>::iterator mapEnd = m_map.end();
            for (HashMap<const JSC::HashTable*, JSC::HashTable>::iterator iter = m_map.begin(); iter != m_map.end(); ++iter)
                iter->second.deleteTable();
        }

        const JSC::HashTable* get(const JSC::HashTable* staticTable)
        {
            HashMap<const JSC::HashTable*, JSC::HashTable>::iterator iter = m_map.find(staticTable);
            if (iter != m_map.end())
                return &iter->second;
            return &m_map.set(staticTable, JSC::HashTable(*staticTable)).first->second;
        }

    private:
        HashMap<const JSC::HashTable*, JSC::HashTable> m_map;
    };

    class WebCoreJSClientData : public JSC::JSGlobalData::ClientData, public Noncopyable {
        friend class JSGlobalDataWorldIterator;

    public:
        WebCoreJSClientData(JSC::JSGlobalData* globalData)
            : m_normalWorld(DOMWrapperWorld::create(globalData, true))
        {
            m_worldSet.add(m_normalWorld.get());
        }

        virtual ~WebCoreJSClientData()
        {
            ASSERT(m_worldSet.contains(m_normalWorld.get()));
            ASSERT(m_worldSet.size() == 1);
            m_normalWorld->detachFromGlobalData();
        }

        DOMWrapperWorld* normalWorld() { return m_normalWorld.get(); }

        void getAllWorlds(Vector<DOMWrapperWorld*>& worlds)
        {
            copyToVector(m_worldSet, worlds);
        }

        void rememberWorld(DOMWrapperWorld* world)
        {
            ASSERT(!m_worldSet.contains(world));
            m_worldSet.add(world);
        }
        void forgetWorld(DOMWrapperWorld* world)
        {
            ASSERT(m_worldSet.contains(world));
            m_worldSet.remove(world);
        }

        DOMObjectHashTableMap hashTableMap;
    private:
        HashSet<DOMWrapperWorld*> m_worldSet;
        RefPtr<DOMWrapperWorld> m_normalWorld;
    };

    DOMObject* getCachedDOMObjectWrapper(JSC::ExecState*, void* objectHandle);
    bool hasCachedDOMObjectWrapper(JSC::JSGlobalData*, void* objectHandle);
    void cacheDOMObjectWrapper(JSC::ExecState*, void* objectHandle, DOMObject* wrapper);
    void forgetDOMNode(JSNode* wrapper, Node* node, Document* document);
    void forgetDOMObject(DOMObject* wrapper, void* objectHandle);

    JSNode* getCachedDOMNodeWrapper(JSC::ExecState*, Document*, Node*);
    void cacheDOMNodeWrapper(JSC::ExecState*, Document*, Node*, JSNode* wrapper);
    void forgetAllDOMNodesForDocument(Document*);
    void forgetWorldOfDOMNodesForDocument(Document*, DOMWrapperWorld*);
    void updateDOMNodeDocument(Node*, Document* oldDocument, Document* newDocument);

    void markDOMNodesForDocument(JSC::MarkStack&, Document*);
    void markActiveObjectsForContext(JSC::MarkStack&, JSC::JSGlobalData&, ScriptExecutionContext*);
    void markDOMObjectWrapper(JSC::MarkStack&, JSC::JSGlobalData& globalData, void* object);
    void markDOMNodeWrapper(JSC::MarkStack& markStack, Document* document, Node* node);
    bool hasCachedDOMObjectWrapperUnchecked(JSC::JSGlobalData*, void* objectHandle);
    bool hasCachedDOMNodeWrapperUnchecked(Document*, Node*);

    JSC::Structure* getCachedDOMStructure(JSDOMGlobalObject*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSDOMGlobalObject*, NonNullPassRefPtr<JSC::Structure>, const JSC::ClassInfo*);
    JSC::Structure* getCachedDOMStructure(JSC::ExecState*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSC::ExecState*, NonNullPassRefPtr<JSC::Structure>, const JSC::ClassInfo*);

    DOMWrapperWorld* currentWorld(JSC::ExecState*);
    DOMWrapperWorld* normalWorld(JSC::JSGlobalData&);
    DOMWrapperWorld* mainThreadNormalWorld();
    inline DOMWrapperWorld* debuggerWorld() { return mainThreadNormalWorld(); }
    inline DOMWrapperWorld* pluginWorld() { return mainThreadNormalWorld(); }

    JSC::JSObject* getCachedDOMConstructor(JSC::ExecState*, const JSC::ClassInfo*);
    void cacheDOMConstructor(JSC::ExecState*, const JSC::ClassInfo*, JSC::JSObject* constructor);

    inline JSDOMGlobalObject* deprecatedGlobalObjectForPrototype(JSC::ExecState* exec)
    {
        // FIXME: Callers to this function should be using the global object
        // from which the object is being created, instead of assuming the lexical one.
        // e.g. subframe.document.body should use the subframe's global object, not the lexical one.
        return static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject());
    }

    template<class WrapperClass> inline JSC::Structure* getDOMStructure(JSC::ExecState* exec, JSDOMGlobalObject* globalObject)
    {
        if (JSC::Structure* structure = getCachedDOMStructure(globalObject, &WrapperClass::s_info))
            return structure;
        return cacheDOMStructure(globalObject, WrapperClass::createStructure(WrapperClass::createPrototype(exec, globalObject)), &WrapperClass::s_info);
    }
    template<class WrapperClass> inline JSC::Structure* deprecatedGetDOMStructure(JSC::ExecState* exec)
    {
        // FIXME: This function is wrong.  It uses the wrong global object for creating the prototype structure.
        return getDOMStructure<WrapperClass>(exec, deprecatedGlobalObjectForPrototype(exec));
    }
    template<class WrapperClass> inline JSC::JSObject* getDOMPrototype(JSC::ExecState* exec, JSC::JSGlobalObject* globalObject)
    {
        return static_cast<JSC::JSObject*>(asObject(getDOMStructure<WrapperClass>(exec, static_cast<JSDOMGlobalObject*>(globalObject))->storedPrototype()));
    }
    #define CREATE_DOM_OBJECT_WRAPPER(exec, globalObject, className, object) createDOMObjectWrapper<JS##className>(exec, globalObject, static_cast<className*>(object))
    template<class WrapperClass, class DOMClass> inline DOMObject* createDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object)
    {
        ASSERT(object);
        ASSERT(!getCachedDOMObjectWrapper(exec, object));
        // FIXME: new (exec) could use a different globalData than the globalData this wrapper is cached on.
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec, globalObject), globalObject, object);
        cacheDOMObjectWrapper(exec, object, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec, object))
            return wrapper;
        return createDOMObjectWrapper<WrapperClass>(exec, globalObject, object);
    }

#if ENABLE(SVG)
    #define CREATE_SVG_OBJECT_WRAPPER(exec, globalObject, className, object, context) createDOMObjectWrapper<JS##className>(exec, globalObject, static_cast<className*>(object), context)
    template<class WrapperClass, class DOMClass> inline DOMObject* createDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object, SVGElement* context)
    {
        DOMObject* wrapper = createDOMObjectWrapper<WrapperClass, DOMClass>(exec, globalObject, object);
        ASSERT(wrapper);
        if (context)
            JSSVGContextCache::addWrapper(wrapper, context);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object, SVGElement* context)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec, object)) {
            ASSERT(JSSVGContextCache::svgContextForDOMObject(wrapper) == context);
            return wrapper;
        }
        return createDOMObjectWrapper<WrapperClass, DOMClass>(exec, globalObject, object, context);
    }
#endif

    #define CREATE_DOM_NODE_WRAPPER(exec, globalObject, className, object) createDOMNodeWrapper<JS##className>(exec, globalObject, static_cast<className*>(object))
    template<class WrapperClass, class DOMClass> inline JSNode* createDOMNodeWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* node)
    {
        ASSERT(node);
        ASSERT(!getCachedDOMNodeWrapper(exec, node->document(), node));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec, globalObject), globalObject, node);
        // FIXME: The entire function can be removed, once we fix caching.
        // This function is a one-off hack to make Nodes cache in the right global object.
        cacheDOMNodeWrapper(exec, node->document(), node, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMNodeWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* node)
    {
        if (!node)
            return JSC::jsNull();
        if (JSNode* wrapper = getCachedDOMNodeWrapper(exec, node->document(), node))
            return wrapper;
        return createDOMNodeWrapper<WrapperClass>(exec, globalObject, node);
    }

    const JSC::HashTable* getHashTableForGlobalData(JSC::JSGlobalData&, const JSC::HashTable* staticTable);

    void reportException(JSC::ExecState*, JSC::JSValue exception);
    void reportCurrentException(JSC::ExecState*);

    // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
    void setDOMException(JSC::ExecState*, ExceptionCode);

    JSC::JSValue jsString(JSC::ExecState*, const String&); // empty if the string is null
    JSC::JSValue jsStringSlowCase(JSC::ExecState*, JSStringCache&, StringImpl*);
    JSC::JSValue jsString(JSC::ExecState*, const KURL&); // empty if the URL is null
    inline JSC::JSValue jsString(JSC::ExecState* exec, const AtomicString& s)
    { 
        return jsString(exec, s.string());
    }
        
    JSC::JSValue jsStringOrNull(JSC::ExecState*, const String&); // null if the string is null
    JSC::JSValue jsStringOrNull(JSC::ExecState*, const KURL&); // null if the URL is null

    JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const String&); // undefined if the string is null
    JSC::JSValue jsStringOrUndefined(JSC::ExecState*, const KURL&); // undefined if the URL is null

    JSC::JSValue jsStringOrFalse(JSC::ExecState*, const String&); // boolean false if the string is null
    JSC::JSValue jsStringOrFalse(JSC::ExecState*, const KURL&); // boolean false if the URL is null

    // See JavaScriptCore for explanation: Should be used for any UString that is already owned by another
    // object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
    JSC::JSValue jsOwnedStringOrNull(JSC::ExecState*, const JSC::UString&); 

    JSC::UString valueToStringWithNullCheck(JSC::ExecState*, JSC::JSValue); // null if the value is null
    JSC::UString valueToStringWithUndefinedOrNullCheck(JSC::ExecState*, JSC::JSValue); // null if the value is null or undefined

    // Returns a Date instance for the specified value, or null if the value is NaN or infinity.
    JSC::JSValue jsDateOrNull(JSC::ExecState*, double);
    // NaN if the value can't be converted to a date.
    double valueToDate(JSC::ExecState*, JSC::JSValue);

    // FIXME: These are a stop-gap until all toJS calls can be converted to pass a globalObject
    template <typename T>
    inline JSC::JSValue toJS(JSC::ExecState* exec, T* ptr)
    {
        return toJS(exec, deprecatedGlobalObjectForPrototype(exec), ptr);
    }
    template <typename T>
    inline JSC::JSValue toJS(JSC::ExecState* exec, PassRefPtr<T> ptr)
    {
        return toJS(exec, deprecatedGlobalObjectForPrototype(exec), ptr.get());
    }
    template <typename T>
    inline JSC::JSValue toJSNewlyCreated(JSC::ExecState* exec, T* ptr)
    {
        return toJSNewlyCreated(exec, deprecatedGlobalObjectForPrototype(exec), ptr);
    }

    template <typename T>
    inline JSC::JSValue toJS(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, PassRefPtr<T> ptr)
    {
        return toJS(exec, globalObject, ptr.get());
    }

    // Validates that the passed object is a sequence type per section 4.1.13 of the WebIDL spec.
    JSC::JSObject* toJSSequence(JSC::ExecState*, JSC::JSValue, unsigned&);

    bool checkNodeSecurity(JSC::ExecState*, Node*);

    // Helpers for Window, History, and Location classes to implement cross-domain policy.
    // Besides the cross-domain check, they need non-caching versions of staticFunctionGetter for
    // because we do not want current property values involved at all.
    bool allowsAccessFromFrame(JSC::ExecState*, Frame*);
    bool allowsAccessFromFrame(JSC::ExecState*, Frame*, String& message);
    bool shouldAllowNavigation(JSC::ExecState*, Frame*);
    void printErrorMessageForFrame(Frame*, const String& message);
    JSC::JSValue objectToStringFunctionGetter(JSC::ExecState*, const JSC::Identifier& propertyName, const JSC::PropertySlot&);

    Frame* toLexicalFrame(JSC::ExecState*);
    Frame* toDynamicFrame(JSC::ExecState*);
    bool processingUserGesture(JSC::ExecState*);
    KURL completeURL(JSC::ExecState*, const String& relativeURL);

    inline DOMWrapperWorld* currentWorld(JSC::ExecState* exec)
    {
        return static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject())->world();
    }
    
    inline JSC::JSValue jsString(JSC::ExecState* exec, const String& s)
    {
        StringImpl* stringImpl = s.impl();
        if (!stringImpl || !stringImpl->length())
            return jsEmptyString(exec);

        if (stringImpl->length() == 1 && stringImpl->characters()[0] <= 0xFF)
            return jsString(exec, stringImpl->ustring());

        JSStringCache& stringCache = currentWorld(exec)->m_stringCache;
        if (JSC::JSString* wrapper = stringCache.get(stringImpl))
            return wrapper;

        return jsStringSlowCase(exec, stringCache, stringImpl);
    }

} // namespace WebCore

#endif // JSDOMBinding_h
