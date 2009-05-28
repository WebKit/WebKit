/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Samuel Weinig <sam@webkit.org>
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
#include <runtime/Completion.h>
#include <runtime/Lookup.h>
#include <runtime/JSFunction.h>
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
        explicit DOMObject(PassRefPtr<JSC::Structure> structure) 
            : JSObject(structure)
        {
        }

#ifndef NDEBUG
        virtual ~DOMObject();
#endif
    };

    DOMObject* getCachedDOMObjectWrapper(JSC::JSGlobalData&, void* objectHandle);
    void cacheDOMObjectWrapper(JSC::JSGlobalData&, void* objectHandle, DOMObject* wrapper);
    void forgetDOMObject(JSC::JSGlobalData&, void* objectHandle);

    JSNode* getCachedDOMNodeWrapper(Document*, Node*);
    void cacheDOMNodeWrapper(Document*, Node*, JSNode* wrapper);
    void forgetDOMNode(Document*, Node*);
    void forgetAllDOMNodesForDocument(Document*);
    void updateDOMNodeDocument(Node*, Document* oldDocument, Document* newDocument);
    void markDOMNodesForDocument(Document*);
    void markActiveObjectsForContext(JSC::JSGlobalData&, ScriptExecutionContext*);
    void markDOMObjectWrapper(JSC::JSGlobalData& globalData, void* object);

    JSC::Structure* getCachedDOMStructure(JSDOMGlobalObject*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSDOMGlobalObject*, PassRefPtr<JSC::Structure>, const JSC::ClassInfo*);
    JSC::Structure* getCachedDOMStructure(JSC::ExecState*, const JSC::ClassInfo*);
    JSC::Structure* cacheDOMStructure(JSC::ExecState*, PassRefPtr<JSC::Structure>, const JSC::ClassInfo*);

    JSC::JSObject* getCachedDOMConstructor(JSC::ExecState*, const JSC::ClassInfo*);
    void cacheDOMConstructor(JSC::ExecState*, const JSC::ClassInfo*, JSC::JSObject* constructor);

    template<class WrapperClass> inline JSC::Structure* getDOMStructure(JSC::ExecState* exec, JSDOMGlobalObject* globalObject)
    {
        if (JSC::Structure* structure = getCachedDOMStructure(globalObject, &WrapperClass::s_info))
            return structure;
        return cacheDOMStructure(globalObject, WrapperClass::createStructure(WrapperClass::createPrototype(exec, globalObject)), &WrapperClass::s_info);
    }
    template<class WrapperClass> inline JSC::Structure* getDOMStructure(JSC::ExecState* exec)
    {
        return getDOMStructure<WrapperClass>(exec, static_cast<JSDOMGlobalObject*>(exec->lexicalGlobalObject()));
    }
    template<class WrapperClass> inline JSC::JSObject* getDOMPrototype(JSC::ExecState* exec, JSC::JSGlobalObject* globalObject)
    {
        return static_cast<JSC::JSObject*>(asObject(getDOMStructure<WrapperClass>(exec, static_cast<JSDOMGlobalObject*>(globalObject))->storedPrototype()));
    }
    #define CREATE_DOM_OBJECT_WRAPPER(exec, className, object) createDOMObjectWrapper<JS##className>(exec, static_cast<className*>(object))
    template<class WrapperClass, class DOMClass> inline DOMObject* createDOMObjectWrapper(JSC::ExecState* exec, DOMClass* object)
    {
        ASSERT(object);
        ASSERT(!getCachedDOMObjectWrapper(exec->globalData(), object));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec), object);
        cacheDOMObjectWrapper(exec->globalData(), object, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, DOMClass* object)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), object))
            return wrapper;
        return createDOMObjectWrapper<WrapperClass>(exec, object);
    }

#if ENABLE(SVG)
    #define CREATE_SVG_OBJECT_WRAPPER(exec, className, object, context) createDOMObjectWrapper<JS##className>(exec, static_cast<className*>(object), context)
    template<class WrapperClass, class DOMClass> inline DOMObject* createDOMObjectWrapper(JSC::ExecState* exec, DOMClass* object, SVGElement* context)
    {
        ASSERT(object);
        ASSERT(!getCachedDOMObjectWrapper(exec->globalData(), object));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec), object, context);
        cacheDOMObjectWrapper(exec->globalData(), object, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMObjectWrapper(JSC::ExecState* exec, DOMClass* object, SVGElement* context)
    {
        if (!object)
            return JSC::jsNull();
        if (DOMObject* wrapper = getCachedDOMObjectWrapper(exec->globalData(), object))
            return wrapper;
        return createDOMObjectWrapper<WrapperClass>(exec, object, context);
    }
#endif

    #define CREATE_DOM_NODE_WRAPPER(exec, className, object) createDOMNodeWrapper<JS##className>(exec, static_cast<className*>(object))
    template<class WrapperClass, class DOMClass> inline JSNode* createDOMNodeWrapper(JSC::ExecState* exec, DOMClass* node)
    {
        ASSERT(node);
        ASSERT(!getCachedDOMNodeWrapper(node->document(), node));
        WrapperClass* wrapper = new (exec) WrapperClass(getDOMStructure<WrapperClass>(exec), node);
        cacheDOMNodeWrapper(node->document(), node, wrapper);
        return wrapper;
    }
    template<class WrapperClass, class DOMClass> inline JSC::JSValue getDOMNodeWrapper(JSC::ExecState* exec, DOMClass* node)
    {
        if (!node)
            return JSC::jsNull();
        if (JSNode* wrapper = getCachedDOMNodeWrapper(node->document(), node))
            return wrapper;
        return createDOMNodeWrapper<WrapperClass>(exec, node);
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

    template <typename T> inline JSC::JSValue toJS(JSC::ExecState* exec, PassRefPtr<T> ptr) { return toJS(exec, ptr.get()); }

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
