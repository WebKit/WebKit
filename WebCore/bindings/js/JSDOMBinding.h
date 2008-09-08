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

#include <kjs/JSFunction.h>
#include <kjs/interpreter.h>
#include <kjs/lookup.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

    class AtomicString;
    class Document;
    class Event;
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
        explicit DOMObject(PassRefPtr<JSC::StructureID> structureID) 
            : JSObject(structureID)
        {
        }

        explicit DOMObject(JSC::JSObject* prototype)
            : JSObject(prototype)
        {
        }

#ifndef NDEBUG
        virtual ~DOMObject();
#endif
    };

    class ScriptInterpreter : public JSC::Interpreter {
    public:
        static DOMObject* getDOMObject(void* objectHandle);
        static void putDOMObject(void* objectHandle, DOMObject*);
        static void forgetDOMObject(void* objectHandle);

        static JSNode* getDOMNodeForDocument(Document*, Node*);
        static void putDOMNodeForDocument(Document*, Node*, JSNode* nodeWrapper);
        static void forgetDOMNodeForDocument(Document*, Node*);
        static void forgetAllDOMNodesForDocument(Document*);
        static void updateDOMNodeDocument(Node*, Document* oldDoc, Document* newDoc);
        static void markDOMNodesForDocument(Document*);
    };

    // Retrieve from cache, or create, a JS wrapper for a DOM object.
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline JSC::JSValue* cacheDOMObject(JSC::ExecState* exec, DOMObj* domObj)
    {
        if (!domObj)
            return JSC::jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new (exec) JSDOMObj(JSDOMObjPrototype::self(exec), domObj);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }

#if ENABLE(SVG)
    // Retrieve from cache, or create, a JS wrapper for an SVG DOM object.
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline JSC::JSValue* cacheSVGDOMObject(JSC::ExecState* exec, DOMObj* domObj, SVGElement* context)
    {
        if (!domObj)
            return JSC::jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new (exec) JSDOMObj(JSDOMObjPrototype::self(exec), domObj, context);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }
#endif

    // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
    void setDOMException(JSC::ExecState*, ExceptionCode);

    // Helper class to call setDOMException on exit without adding lots of separate calls to that function.
    class DOMExceptionTranslator : Noncopyable {
    public:
        explicit DOMExceptionTranslator(JSC::ExecState* exec) : m_exec(exec), m_code(0) { }
        ~DOMExceptionTranslator() { setDOMException(m_exec, m_code); }
        operator ExceptionCode&() { return m_code; }
    private:
        JSC::ExecState* m_exec;
        ExceptionCode m_code;
    };

    JSC::JSValue* jsStringOrNull(JSC::ExecState*, const String&); // null if the string is null
    JSC::JSValue* jsStringOrNull(JSC::ExecState*, const KURL&); // null if the URL is null

    JSC::JSValue* jsStringOrUndefined(JSC::ExecState*, const String&); // undefined if the string is null
    JSC::JSValue* jsStringOrUndefined(JSC::ExecState*, const KURL&); // undefined if the URL is null

    JSC::JSValue* jsStringOrFalse(JSC::ExecState*, const String&); // boolean false if the string is null
    JSC::JSValue* jsStringOrFalse(JSC::ExecState*, const KURL&); // boolean false if the URL is null

    // See JavaScriptCore for explanation: Should be used for any UString that is already owned by another
    // object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
    JSC::JSValue* jsOwnedStringOrNull(JSC::ExecState*, const JSC::UString&); 

    JSC::UString valueToStringWithNullCheck(JSC::ExecState*, JSC::JSValue*); // null if the value is null
    JSC::UString valueToStringWithUndefinedOrNullCheck(JSC::ExecState*, JSC::JSValue*); // null if the value is null or undefined

    template <typename T> inline JSC::JSValue* toJS(JSC::ExecState* exec, PassRefPtr<T> ptr) { return toJS(exec, ptr.get()); }

    bool checkNodeSecurity(JSC::ExecState*, Node*);

    // Helpers for Window, History, and Location classes to implement cross-domain policy.
    // Besides the cross-domain check, they need non-caching versions of staticFunctionGetter for
    // because we do not want current property values involved at all.
    bool allowsAccessFromFrame(JSC::ExecState*, Frame*);
    bool allowsAccessFromFrame(JSC::ExecState*, Frame*, String& message);
    void printErrorMessageForFrame(Frame*, const String& message);
    JSC::JSValue* nonCachingStaticFunctionGetter(JSC::ExecState*, const JSC::Identifier& propertyName, const JSC::PropertySlot&);
    JSC::JSValue* objectToStringFunctionGetter(JSC::ExecState*, const JSC::Identifier& propertyName, const JSC::PropertySlot&);

    JSC::ExecState* execStateFromNode(Node*);

} // namespace WebCore

#endif // JSDOMBinding_h
