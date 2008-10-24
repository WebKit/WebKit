/*
 * Copyright (C) 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "SVGElementInstance.h"
#include "JSSVGElementInstance.h"

#include "JSEventListener.h"
#include "JSDOMWindow.h"

using namespace JSC;

namespace WebCore {

JSValue* JSSVGElementInstance::addEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->associatedFrame();
    if (!frame)
        return jsUndefined();

    if (RefPtr<JSEventListener> listener = toJSDOMWindow(frame)->findOrCreateJSEventListener(exec, args.at(exec, 1)))
        impl()->addEventListener(args.at(exec, 0)->toString(exec), listener.release(), args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

JSValue* JSSVGElementInstance::removeEventListener(ExecState* exec, const ArgList& args)
{
    Frame* frame = impl()->associatedFrame();
    if (!frame)
        return jsUndefined();

    if (JSEventListener* listener = toJSDOMWindow(frame)->findJSEventListener(args.at(exec, 1)))
        impl()->removeEventListener(args.at(exec, 0)->toString(exec), listener, args.at(exec, 2)->toBoolean(exec));

    return jsUndefined();
}

void JSSVGElementInstance::pushEventHandlerScope(ExecState*, ScopeChain&) const
{
}

}

#endif
