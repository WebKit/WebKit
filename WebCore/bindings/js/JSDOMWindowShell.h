/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef JSDOMWindowShell_h
#define JSDOMWindowShell_h

#include "JSDOMWindow.h"
#include "JSDOMBinding.h"

namespace WebCore {

    class DOMWindow;
    class Frame;

    class JSDOMWindowShell : public DOMObject {
        typedef DOMObject Base;
    public:
        JSDOMWindowShell(PassRefPtr<DOMWindow>);
        virtual ~JSDOMWindowShell();

        JSDOMWindow* window() const { return m_window; }
        void setWindow(JSDOMWindow* window)
        {
            ASSERT_ARG(window, window);
            m_window = window;
            setPrototype(window->prototype());
        }
        void setWindow(PassRefPtr<DOMWindow>);

        static const JSC::ClassInfo s_info;

        DOMWindow* impl() const;

        void* operator new(size_t);

        static PassRefPtr<JSC::Structure> createStructure(JSC::JSValue prototype) 
        {
            return JSC::Structure::create(prototype, JSC::TypeInfo(JSC::ObjectType)); 
        }

    private:
        virtual void mark();
        virtual JSC::UString className() const;
        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::PropertySlot&);
        virtual void put(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSValue, JSC::PutPropertySlot&);
        virtual void putWithAttributes(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSValue, unsigned attributes);
        virtual bool deleteProperty(JSC::ExecState*, const JSC::Identifier& propertyName);
        virtual void getPropertyNames(JSC::ExecState*, JSC::PropertyNameArray&);
        virtual bool getPropertyAttributes(JSC::ExecState*, const JSC::Identifier& propertyName, unsigned& attributes) const;
        virtual void defineGetter(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSObject* getterFunction);
        virtual void defineSetter(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::JSObject* setterFunction);
        virtual JSC::JSValue lookupGetter(JSC::ExecState*, const JSC::Identifier& propertyName);
        virtual JSC::JSValue lookupSetter(JSC::ExecState*, const JSC::Identifier& propertyName);
        virtual JSC::JSObject* unwrappedObject();
        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }

        JSDOMWindow* m_window;
    };

    JSC::JSValue toJS(JSC::ExecState*, Frame*);
    JSDOMWindowShell* toJSDOMWindowShell(Frame*);

} // namespace WebCore

#endif // JSDOMWindowShell_h
