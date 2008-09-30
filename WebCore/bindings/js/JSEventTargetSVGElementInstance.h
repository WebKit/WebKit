/*
 * Copyright (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSEventTargetSVGElementInstance_h
#define JSEventTargetSVGElementInstance_h

#if ENABLE(SVG)
#include "JSEventTargetBase.h"
#include "JSSVGElementInstance.h"
#include <wtf/AlwaysInline.h>

namespace WebCore {

    class EventTargetSVGElementInstance;
    class SVGElementInstance;

    class JSEventTargetSVGElementInstance : public JSSVGElementInstance {
    public:
        JSEventTargetSVGElementInstance(PassRefPtr<JSC::StructureID>, PassRefPtr<EventTargetSVGElementInstance>);

        static JSC::JSObject* createPrototype(JSC::ExecState*);

        void setListener(JSC::ExecState*, const AtomicString& eventType, JSC::JSValue* func) const;
        JSC::JSValue* getListener(const AtomicString& eventType) const;

        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier&, JSC::PropertySlot&);
        virtual void put(JSC::ExecState*, const JSC::Identifier&, JSC::JSValue*, JSC::PutPropertySlot&);

        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;
    };

    class JSEventTargetSVGElementInstancePrototype : public JSC::JSObject {
    public:
        JSEventTargetSVGElementInstancePrototype(PassRefPtr<JSC::StructureID> structure)
            : JSC::JSObject(structure)
        {
        }

        static JSC::JSObject* self(JSC::ExecState*);
        virtual bool getOwnPropertySlot(JSC::ExecState*, const JSC::Identifier& propertyName, JSC::PropertySlot&);

        virtual const JSC::ClassInfo* classInfo() const { return &s_info; }
        static const JSC::ClassInfo s_info;
    };

    EventTargetSVGElementInstance* toEventTargetSVGElementInstance(JSC::JSValue*);

} // namespace WebCore

#endif // ENABLE(SVG)
#endif // JSEventTargetSVGElementInstance_h
