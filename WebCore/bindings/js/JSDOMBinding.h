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
    class DOMObject : public KJS::JSObject {
    protected:
        explicit DOMObject(KJS::JSValue* prototype) // FIXME: This could take a JSObject if JSDOMWindowShell had a prototype.
            : JSObject(prototype)
        {
        }

#ifndef NDEBUG
        virtual ~DOMObject();
#endif
    };

    class ScriptInterpreter : public KJS::Interpreter {
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
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline KJS::JSValue* cacheDOMObject(KJS::ExecState* exec, DOMObj* domObj)
    {
        if (!domObj)
            return KJS::jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new (exec) JSDOMObj(JSDOMObjPrototype::self(exec), domObj);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }

#if ENABLE(SVG)
    // Retrieve from cache, or create, a JS wrapper for an SVG DOM object.
    template<class DOMObj, class JSDOMObj, class JSDOMObjPrototype> inline KJS::JSValue* cacheSVGDOMObject(KJS::ExecState* exec, DOMObj* domObj, SVGElement* context)
    {
        if (!domObj)
            return KJS::jsNull();
        if (DOMObject* ret = ScriptInterpreter::getDOMObject(domObj))
            return ret;
        DOMObject* ret = new (exec) JSDOMObj(JSDOMObjPrototype::self(exec), domObj, context);
        ScriptInterpreter::putDOMObject(domObj, ret);
        return ret;
    }
#endif

    // Convert a DOM implementation exception code into a JavaScript exception in the execution state.
    void setDOMException(KJS::ExecState*, ExceptionCode);

    // Helper class to call setDOMException on exit without adding lots of separate calls to that function.
    class DOMExceptionTranslator : Noncopyable {
    public:
        explicit DOMExceptionTranslator(KJS::ExecState* exec) : m_exec(exec), m_code(0) { }
        ~DOMExceptionTranslator() { setDOMException(m_exec, m_code); }
        operator ExceptionCode&() { return m_code; }
    private:
        KJS::ExecState* m_exec;
        ExceptionCode m_code;
    };

    KJS::JSValue* jsStringOrNull(KJS::ExecState*, const String&); // null if the string is null
    KJS::JSValue* jsStringOrNull(KJS::ExecState*, const KURL&); // null if the URL is null

    KJS::JSValue* jsStringOrUndefined(KJS::ExecState*, const String&); // undefined if the string is null
    KJS::JSValue* jsStringOrUndefined(KJS::ExecState*, const KURL&); // undefined if the URL is null

    KJS::JSValue* jsStringOrFalse(KJS::ExecState*, const String&); // boolean false if the string is null
    KJS::JSValue* jsStringOrFalse(KJS::ExecState*, const KURL&); // boolean false if the URL is null

    // See JavaScriptCore for explanation: Should be used for any UString that is already owned by another
    // object, to let the engine know that collecting the JSString wrapper is unlikely to save memory.
    KJS::JSValue* jsOwnedStringOrNull(KJS::ExecState*, const KJS::UString&); 

    KJS::UString valueToStringWithNullCheck(KJS::ExecState*, KJS::JSValue*); // null if the value is null
    KJS::UString valueToStringWithUndefinedOrNullCheck(KJS::ExecState*, KJS::JSValue*); // null if the value is null or undefined

    template <typename T> inline KJS::JSValue* toJS(KJS::ExecState* exec, PassRefPtr<T> ptr) { return toJS(exec, ptr.get()); }

    bool checkNodeSecurity(KJS::ExecState*, Node*);

    // Helpers for Window, History, and Location classes to implement cross-domain policy.
    // Besides the cross-domain check, they need non-caching versions of staticFunctionGetter for
    // because we do not want current property values involved at all.
    bool allowsAccessFromFrame(KJS::ExecState*, Frame*);
    bool allowsAccessFromFrame(KJS::ExecState*, Frame*, String& message);
    void printErrorMessageForFrame(Frame*, const String& message);
    KJS::JSValue* nonCachingStaticFunctionGetter(KJS::ExecState*, const KJS::Identifier& propertyName, const KJS::PropertySlot&);
    KJS::JSValue* objectToStringFunctionGetter(KJS::ExecState*, const KJS::Identifier& propertyName, const KJS::PropertySlot&);

} // namespace WebCore

#endif // JSDOMBinding_h
