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

#include "config.h"

#if ENABLE(SVG)
#include "JSEventTargetSVGElementInstance.h"

#include "Document.h"
#include "JSDOMWindow.h"
#include "JSEventListener.h"

namespace WebCore {

using namespace JSC;

ASSERT_CLASS_FITS_IN_CELL(JSEventTargetSVGElementInstance)

const ClassInfo JSEventTargetSVGElementInstance::s_info = { "EventTargetSVGElementInstance", &JSSVGElementInstance::s_info, 0, 0 };

JSEventTargetSVGElementInstance::JSEventTargetSVGElementInstance(PassRefPtr<StructureID> structure, PassRefPtr<EventTargetSVGElementInstance> node)
    : JSSVGElementInstance(structure, node)
{
}

JSObject* JSEventTargetSVGElementInstance::createPrototype(ExecState* exec)
{
    return new (exec) JSEventTargetSVGElementInstancePrototype(JSEventTargetSVGElementInstancePrototype::createStructureID(JSSVGElementInstancePrototype::self(exec)));
}

void JSEventTargetSVGElementInstance::setListener(ExecState* exec, const AtomicString& eventType, JSValue* func) const
{
    SVGElement* element = impl()->correspondingElement();
    if (Frame* frame = element->document()->frame())
        element->setEventListenerForType(eventType, toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, func, true));
}

JSValue* JSEventTargetSVGElementInstance::getListener(const AtomicString& eventType) const
{
    SVGElement* element = impl()->correspondingElement();
    EventListener* listener = element->eventListenerForType(eventType);

    JSEventListener* jsListener = static_cast<JSEventListener*>(listener);
    if (jsListener && jsListener->listenerObj())
        return jsListener->listenerObj();

    return jsNull();
}

EventTargetSVGElementInstance* toEventTargetSVGElementInstance(JSValue* val)
{
    if (!val || !val->isObject(&JSSVGElementInstance::s_info))
        return 0;

    return static_cast<EventTargetSVGElementInstance*>(static_cast<JSEventTargetSVGElementInstance*>(val)->impl());
}

} // namespace WebCore

#endif // ENABLE(SVG)
