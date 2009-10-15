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
#include "Document.h" // For DOMConstructorWithDocument
#include <runtime/Completion.h>
#include <runtime/Lookup.h>
#include <wtf/Noncopyable.h>

namespace JSC {
    class JSGlobalData;
}

namespace WebCore {

    class Document;
    class Frame;
    class KURL;
    class Node;
    class String;
    class JSNode;

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
        JSDOMGlobalObject* globalObject() const { return m_globalObject; }

        ScriptExecutionContext* scriptExecutionContext() const
        {
            // FIXME: Should never be 0, but can be due to bug 27640.
            return m_globalObject->scriptExecutionContext();
        }

        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype)
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType, JSC::OverridesMarkChildren));
        }

    protected:
        DOMObjectWithGlobalPointer(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject* globalObject)
            : DOMObject(structure)
            , m_globalObject(globalObject)
        {
            // FIXME: This ASSERT is valid, but fires in fast/dom/gc-6.html when trying to create
            // new JavaScript objects on detached windows due to DOMWindow::document()
            // needing to reach through the frame to get to the Document*.  See bug 27640.
            // ASSERT(globalObject->scriptExecutionContext());
        }
        virtual ~DOMObjectWithGlobalPointer() { }

        void markChildren(JSC::MarkStack& markStack)
        {
            DOMObject::markChildren(markStack);
            markStack.append(m_globalObject);
        }

    private:
        JSDOMGlobalObject* m_globalObject;
    };

    // Base class for all constructor objects in the JSC bindings.
    class DOMConstructorObject : public DOMObjectWithGlobalPointer {
    public:
        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype)
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType, JSC::ImplementsHasInstance | JSC::OverridesMarkChildren));
        }

    protected:
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

    DOMObject* getCachedDOMObjectWrapper(JSC::JSGlobalData&, void* objectHandle);
    void cacheDOMObjectWrapper(JSC::JSGlobalData&, void* objectHandle, DOMObject* wrapper);
    void forgetDOMObject(JSC::JSGlobalData&, void* objectHandle);

    JSNode* getCachedDOMNodeWrapper(Document*, Node*);
    void cacheDOMNodeWrapper(Document*, Node*, JSNode* wrapper);
    void forgetDOMNode(Document*, Node*);
    void forgetAllDOMNodesForDocument(Document*);
    void updateDOMNodeDocument(Node*, Document* oldDocument, Document* newDocument);
    void markDOMNodesForDocument(JSC::MarkStack&, Document*);
    void markActiveObjectsForContext(JSC::MarkStack&, JSC::JSGlobalData&, ScriptExecutionContext*);
    void markDOMObjectWrapper(JSC::MarkStack&, JSC::JSGlobalData& globalData, void* object);

    JSC::Structure* getCachedDOMStructure(JSDOMGlobalObject*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSDOMGlobalObject*, NonNullPassRefPtr<JSC::Structure>, const JSC::ClassInfo*);
    JSC::Structure* getCachedDOMStructure(JSC::ExecState*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSC::ExecState*, NonNullPassRefPtr<JSC::Structure>, const JSC::ClassInfo*);

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
        ASSERT(!getCachedDOMObjectWrapper(exec->globalData(), object));
        // FIXME: new (exec) could use a different globalData than the globalData this wrapper is cached on.
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec, globalObject), globalObject, object);
        cacheDOMObjectWrapper(exec->globalData(), object, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), object))
            return wrapper;
        return createDOMObjectWrapper<WrapperClass>(exec, globalObject, object);
    }

#if ENABLE(SVG)
    #define CREATE_SVG_OBJECT_WRAPPER(exec, globalObject, className, object, context) createDOMObjectWrapper<JS##className>(exec, globalObject, static_cast<className*>(object), context)
    template<class WrapperClass, class DOMClass> inline DOMObject* createDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object, SVGElement* context)
    {
        ASSERT(object);
        ASSERT(!getCachedDOMObjectWrapper(exec->globalData(), object));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec, globalObject), globalObject, object, context);
        cacheDOMObjectWrapper(exec->globalData(), object, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* object, SVGElement* context)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), object))
            return wrapper;
        return createDOMObjectWrapper<WrapperClass>(exec, globalObject, object, context);
    }
#endif

    #define CREATE_DOM_NODE_WRAPPER(exec, globalObject, className, object) createDOMNodeWrapper<JS##className>(exec, globalObject, static_cast<className*>(object))
    template<class WrapperClass, class DOMClass> inline JSNode* createDOMNodeWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* node)
    {
        ASSERT(node);
        ASSERT(!getCachedDOMNodeWrapper(node->document(), node));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec, globalObject), globalObject, node);
        // FIXME: The entire function can be removed, once we fix caching.
        // This function is a one-off hack to make Nodes cache in the right global object.
        cacheDOMNodeWrapper(node->document(), node, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMNodeWrapper(JSC::ExecState* exec, JSDOMGlobalObject* globalObject, DOMClass* node)
    {
        if (!node)
            return JSC::jsNull();
        if (JSNode* wrapper = getCachedDOMNodeWrapper(node->document(), node))
            return wrapper;
        return createDOMNodeWrapper<WrapperClass>(exec, globalObject, node);
    }

    const JSC::HashTable* getHashTableForGlobalData(JSC::JSGlobalData&, const JSC::HashTable* staticTable);

    void reportException(JSC::ExecState*, JSC::JSValue exception);
    void reportCurrentException(JSC::ExecState*);

    // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
    void setDOMException(JSC::ExecState*, ExceptionCode);

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

} // namespace WebCore

#endif // JSDOMBinding_h
