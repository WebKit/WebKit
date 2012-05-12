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
#include <runtime/JSGlobalThis.h>

namespace WebCore {

    class DOMWindow;
    class Frame;

    class JSDOMWindowShell : public JSC::JSGlobalThis {
        typedef JSC::JSGlobalThis Base;
    public:
        JSDOMWindowShell(PassRefPtr<DOMWindow>, JSC::Structure*, DOMWrapperWorld*);
        static void destroy(JSCell*);

        JSDOMWindow* window() const { return JSC::jsCast<JSDOMWindow*>(unwrappedObject()); }
        void setWindow(JSC::JSGlobalData& globalData, JSDOMWindow* window)
        {
            ASSERT_ARG(window, window);
            setUnwrappedObject(globalData, window);
        }
        void setWindow(PassRefPtr<DOMWindow>);

        static const JSC::ClassInfo s_info;

        DOMWindow* impl() const;

        static JSDOMWindowShell* create(PassRefPtr<DOMWindow> window, JSC::Structure* structure, DOMWrapperWorld* world) 
        {
            JSC::Heap& heap = JSDOMWindow::commonJSGlobalData()->heap;
            JSDOMWindowShell* shell = new (NotNull, JSC::allocateCell<JSDOMWindowShell>(heap)) JSDOMWindowShell(structure, world);
            shell->finishCreation(*world->globalData(), window);
            return shell; 
        }

        static JSC::Structure* createStructure(JSC::JSGlobalData& globalData, JSC::JSValue prototype) 
        {
            return JSC::Structure::create(globalData, 0, prototype, JSC::TypeInfo(JSC::GlobalThisType, StructureFlags), &s_info); 
        }

        DOMWrapperWorld* world() { return m_world.get(); }

    protected:
        JSDOMWindowShell(JSC::Structure*, DOMWrapperWorld*);
        void finishCreation(JSC::JSGlobalData&, PassRefPtr<DOMWindow>);

    private:
        static const unsigned StructureFlags = JSC::OverridesGetOwnPropertySlot | JSC::OverridesGetPropertyNames | Base::StructureFlags;

        static JSC::UString className(const JSC::JSObject*);
        static bool getOwnPropertySlot(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::PropertySlot&);
        static bool getOwnPropertyDescriptor(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&);
        static void put(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, JSC::PutPropertySlot&);
        static void putDirectVirtual(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::JSValue, unsigned attributes);
        static bool deleteProperty(JSC::JSCell*, JSC::ExecState*, JSC::PropertyName);
        static void getOwnPropertyNames(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode);
        static void getPropertyNames(JSC::JSObject*, JSC::ExecState*, JSC::PropertyNameArray&, JSC::EnumerationMode);
        static bool defineOwnProperty(JSC::JSObject*, JSC::ExecState*, JSC::PropertyName, JSC::PropertyDescriptor&, bool shouldThrow);

        RefPtr<DOMWrapperWorld> m_world;
    };

    JSC::JSValue toJS(JSC::ExecState*, Frame*);
    JSDOMWindowShell* toJSDOMWindowShell(Frame*, DOMWrapperWorld*);

} // namespace WebCore

#endif // JSDOMWindowShell_h
