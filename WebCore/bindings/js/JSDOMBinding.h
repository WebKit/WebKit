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
#include "JSDOMWrapper.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include <runtime/Completion.h>
#include <runtime/Lookup.h>
#include <runtime/WeakGCMap.h>
#include <wtf/Forward.h>
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
    class ScriptController;
    class ScriptCachedFrameData;

    typedef int ExceptionCode;

    // FIXME: This class should collapse into DOMObject once all DOMObjects are
    // updated to store a globalObject pointer.
    class DOMObjectWithGlobalPointer : public DOMObject {
    public:
        JSDOMGlobalObject* globalObject() const
        {
            return static_cast<JSDOMGlobalObject*>(DOMObject::globalObject());
        }

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
        DOMObjectWithGlobalPointer(NonNullPassRefPtr<JSC::Structure> structure, JSDOMGlobalObject* globalObject)
            : DOMObject(globalObject, structure)
        {
            // FIXME: This ASSERT is valid, but fires in fast/dom/gc-6.html when trying to create
            // new JavaScript objects on detached windows due to DOMWindow::document()
            // needing to reach through the frame to get to the Document*.  See bug 27640.
            // ASSERT(globalObject->scriptExecutionContext());
        }
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

    DOMObject* getCachedDOMObjectWrapper(JSC::ExecState*, void* objectHandle);
    bool hasCachedDOMObjectWrapper(JSC::JSGlobalData*, void* objectHandle);
    void cacheDOMObjectWrapper(JSC::ExecState*, void* objectHandle, DOMObject* wrapper);
    void forgetDOMNode(JSNode* wrapper, Node* node, Document* document);
    void forgetDOMObject(DOMObject* wrapper, void* objectHandle);

    JSNode* getCachedDOMNodeWrapper(JSC::ExecState*, Document*, Node*);
    void cacheDOMNodeWrapper(JSC::ExecState*, Document*, Node*, JSNode* wrapper);
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
        if (JSC::JSCell* wrapper = getCachedDOMNodeWrapper(exec, node->document(), node))
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
    JSC::JSValue jsOwnedStringOrNull(JSC::ExecState*, const String&); 
    JSC::JSValue jsOwnedStringOrNull(JSC::ExecState*, const JSC::UString&); 

    String identifierToString(const JSC::Identifier&);
    String ustringToString(const JSC::UString&);
    JSC::UString stringToUString(const String&);

    AtomicString identifierToAtomicString(const JSC::Identifier&);
    AtomicString ustringToAtomicString(const JSC::UString&);
    AtomicStringImpl* findAtomicString(const JSC::Identifier&);

    String valueToStringWithNullCheck(JSC::ExecState*, JSC::JSValue); // null if the value is null
    String valueToStringWithUndefinedOrNullCheck(JSC::ExecState*, JSC::JSValue); // null if the value is null or undefined

    inline int32_t finiteInt32Value(JSC::JSValue value, JSC::ExecState* exec, bool& okay)
    {
        double number = value.toNumber(exec);
        okay = isfinite(number);
        return JSC::toInt32(number);
    }

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
    bool allowSettingSrcToJavascriptURL(JSC::ExecState*, Element*, const String&, const String&);

    void printErrorMessageForFrame(Frame*, const String& message);
    JSC::JSValue objectToStringFunctionGetter(JSC::ExecState*, JSC::JSValue, const JSC::Identifier& propertyName);

    Frame* toLexicalFrame(JSC::ExecState*);
    Frame* toDynamicFrame(JSC::ExecState*);
    bool processingUserGesture();
    KURL completeURL(JSC::ExecState*, const String& relativeURL);
    
    inline JSC::JSValue jsString(JSC::ExecState* exec, const String& s)
    {
        StringImpl* stringImpl = s.impl();
        if (!stringImpl || !stringImpl->length())
            return jsEmptyString(exec);

        if (stringImpl->length() == 1 && stringImpl->characters()[0] <= 0xFF)
            return jsString(exec, stringToUString(s));

        JSStringCache& stringCache = currentWorld(exec)->m_stringCache;
        if (JSC::JSString* wrapper = stringCache.get(stringImpl))
            return wrapper;

        return jsStringSlowCase(exec, stringCache, stringImpl);
    }

    inline DOMObjectWrapperMap& domObjectWrapperMapFor(JSC::ExecState* exec)
    {
        return currentWorld(exec)->m_wrappers;
    }

    inline String ustringToString(const JSC::UString& u)
    {
        return u.impl();
    }

    inline JSC::UString stringToUString(const String& s)
    {
        return JSC::UString(s.impl());
    }

    inline String identifierToString(const JSC::Identifier& i)
    {
        return i.impl();
    }

    inline AtomicString ustringToAtomicString(const JSC::UString& u)
    {
        return AtomicString(u.impl());
    }

    inline AtomicString identifierToAtomicString(const JSC::Identifier& identifier)
    {
        return AtomicString(identifier.impl());
    }

} // namespace WebCore

#endif // JSDOMBinding_h
