// -*- c-basic-offset: 4 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
}

namespace KJS {

    /**
     * Base class for all objects in this binding.
     */
    class DOMObject : public JSObject {
    protected:
        DOMObject()
        {
            // DOMObject destruction is not thread-safe because DOMObjects wrap 
            // unsafe WebCore DOM data structures.
            Collector::collectOnMainThreadOnly(this);
        }
#ifndef NDEBUG
        virtual ~DOMObject();
#endif
    };

    /**
     * We inherit from Interpreter, to save a pointer to the HTML part
     * that the interpreter runs for.
     * The interpreter also stores the DOM object -> KJS::DOMObject cache.
     */
    class ScriptInterpreter : public Interpreter {
    public:
        ScriptInterpreter(JSObject* global, WebCore::Frame*);

        static DOMObject* getDOMObject(void* objectHandle);
        static void putDOMObject(void* objectHandle, DOMObject*);
        static void forgetDOMObject(void* objectHandle);

        static WebCore::JSNode* getDOMNodeForDocument(WebCore::Document*, WebCore::Node*);
        static void putDOMNodeForDocument(WebCore::Document*, WebCore::Node*, WebCore::JSNode* nodeWrapper);
        static void forgetDOMNodeForDocument(WebCore::Document*, WebCore::Node*);
        static void forgetAllDOMNodesForDocument(WebCore::Document*);
        static void updateDOMNodeDocument(WebCore::Node*, WebCore::Document* oldDoc, WebCore::Document* newDoc);
        static void markDOMNodesForDocument(WebCore::Document*);

        WebCore::Frame* frame() const { return m_frame; }

        /**
         * Set the event that is triggering the execution of a script, if any
         */
        void setCurrentEvent(WebCore::Event* event) { m_currentEvent = event; }
        void setInlineCode(bool inlineCode) { m_inlineCode = inlineCode; }
        void setProcessingTimerCallback(bool timerCallback) { m_timerCallback = timerCallback; }

        /**
         * "Smart" window.open policy
         */
        bool wasRunByUserGesture() const;

        virtual ExecState* globalExec();

        WebCore::Event* getCurrentEvent() const { return m_currentEvent; }

        virtual bool isGlobalObject(JSValue*);
        virtual Interpreter* interpreterForGlobalObject(const JSValue*);
        virtual bool isSafeScript(const Interpreter* target);

        virtual bool shouldInterruptScript() const;

    private:
        virtual ~ScriptInterpreter() { } // only deref on the base class should delete us
    
        WebCore::Frame* m_frame;
        WebCore::Event* m_currentEvent;
        bool m_inlineCode;
        bool m_timerCallback;
    };

    /**
     * Retrieve from cache, or create, a KJS object around a DOM object
     */
    template<class DOMObj, class KJSDOMObj> inline JSValue *cacheDOMObject(ExecState* exec, DOMObj* domObj)
    {
        if (!domObj)
            return jsNull();
        ScriptInterpreter* interp = static_cast<ScriptInterpreter*>(exec->dynamicInterpreter());
        if (DOMObject* ret = interp->getDOMObject(domObj))
            return ret;
        DOMObject* ret = new KJSDOMObj(exec, domObj);
        interp->putDOMObject(domObj, ret);
        return ret;
    }

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

} // namespace

#endif
