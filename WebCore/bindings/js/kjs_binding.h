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

#ifndef kjs_binding_h
#define kjs_binding_h

#include <kjs/function.h>
#include <kjs/interpreter.h>
#include <kjs/lookup.h>
#include <wtf/Noncopyable.h>

#if PLATFORM(MAC)
#include <JavaScriptCore/runtime.h>
#endif

namespace WebCore {
    class AtomicString;
    class Document;
    class Event;
    class Frame;
    class Node;
    class String;
    class JSNode;

    typedef int ExceptionCode;

#if ENABLE(SVG)
    class SVGElement;
#endif
}

namespace KJS {

    /**
     * Base class for all objects in this binding EXCEPT Window
     */
    class DOMObject : public JSObject {
    protected:
        explicit DOMObject(JSValue* prototype) // FIXME: this should take a JSObject once JSLocation has a real prototype
            : JSObject(prototype)
        {
            // DOMObject destruction is not thread-safe because DOMObjects wrap 
            // unsafe WebCore DOM data structures.
            Collector::collectOnMainThreadOnly(this);
        }

#ifndef NDEBUG
        virtual ~DOMObject();
#endif

    private:
        DOMObject();
    };

    class ScriptInterpreter : public Interpreter {
    public:
        static DOMObject* getDOMObject(void* objectHandle);
        static void putDOMObject(void* objectHandle, DOMObject*);
        static void forgetDOMObject(void* objectHandle);

        static WebCore::JSNode* getDOMNodeForDocument(WebCore::Document*, WebCore::Node*);
        static void putDOMNodeForDocument(WebCore::Document*, WebCore::Node*, WebCore::JSNode* nodeWrapper);
        static void forgetDOMNodeForDocument(WebCore::Document*, WebCore::Node*);
        static void forgetAllDOMNodesForDocument(WebCore::Document*);
        static void updateDOMNodeDocument(WebCore::Node*, WebCore::Document* oldDoc, WebCore::Document* newDoc);
        static void markDOMNodesForDocument(WebCore::Document*);
    };

    /**
     * Retrieve from cache, or create, a JS object around a DOM object
     */
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline JSValue* cacheDOMObject(ExecState* exec, DOMObj* domObj)
    {
        if (!domObj)
            return jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new JSDOMObj(JSDOMObjPrototype::self(exec), domObj);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }

#if ENABLE(SVG)
    /**
     * Retrieve from cache, or create, a JS object around a SVG DOM object
     */
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline JSValue* cacheSVGDOMObject(ExecState* exec, DOMObj* domObj, WebCore::SVGElement* context)
    {
        if (!domObj)
            return jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new JSDOMObj(JSDOMObjPrototype::self(exec), domObj, context);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }
#endif

    // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
    void setDOMException(ExecState*, WebCore::ExceptionCode);

    // Helper class to call setDOMException on exit without adding lots of separate calls to that function.
    class DOMExceptionTranslator : Noncopyable {
    public:
        explicit DOMExceptionTranslator(ExecState* exec) : m_exec(exec), m_code(0) { }
        ~DOMExceptionTranslator() { setDOMException(m_exec, m_code); }
        operator WebCore::ExceptionCode&() { return m_code; }
    private:
        ExecState* m_exec;
        WebCore::ExceptionCode m_code;
    };

    JSValue* jsStringOrNull(const WebCore::String&); // null if the string is null
    JSValue* jsStringOrUndefined(const WebCore::String&); // undefined if the string is null
    JSValue* jsStringOrFalse(const WebCore::String&); // boolean false if the string is null

    // see JavaScriptCore for explanation should be used for UString that is already owned
    // by another object, so that collecting the JSString wrapper is unlikely to save memory.
    JSValue* jsOwnedStringOrNull(const KJS::UString&); 

    WebCore::String valueToStringWithNullCheck(ExecState*, JSValue*); // null String if the value is null
    WebCore::String valueToStringWithUndefinedOrNullCheck(ExecState*, JSValue*); // null String if the value is null or undefined

    template <typename T> inline JSValue* toJS(ExecState* exec, PassRefPtr<T> ptr) { return toJS(exec, ptr.get()); }

} // namespace KJS

namespace WebCore {

    // Helpers for Window, History, and Location classes to implement cross-domain policy.
    // Besides the cross-domain check, they need non-caching versions of staticFunctionGetter for
    // because we do not want current property values involved at all.
    bool allowsAccessFromFrame(KJS::ExecState*, Frame*);
    bool allowsAccessFromFrame(KJS::ExecState*, Frame*, String& message);
    void printErrorMessageForFrame(Frame*, const String& message);

    KJS::JSValue* nonCachingStaticFunctionGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier& propertyName, const KJS::PropertySlot&);
    KJS::JSValue* objectToStringFunctionGetter(KJS::ExecState*, KJS::JSObject*, const KJS::Identifier& propertyName, const KJS::PropertySlot&);

} // namespace WebCore

#endif // kjs_binding_h
