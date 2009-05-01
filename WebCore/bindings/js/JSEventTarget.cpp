/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "JSEventTarget.h"

#include "DOMWindow.h"
#include "Document.h"
#include "JSDOMWindow.h"
#include "JSDOMWindowShell.h"
#include "JSEventListener.h"
#include "JSMessagePort.h"
#include "JSNode.h"
#include "JSXMLHttpRequest.h"
#include "JSXMLHttpRequestUpload.h"
#include "MessagePort.h"
#include "XMLHttpRequest.h"
#include "XMLHttpRequestUpload.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
#include "DOMApplicationCache.h"
#include "JSDOMApplicationCache.h"
#endif

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "JSSVGElementInstance.h"
#endif

#if ENABLE(WORKERS)
#include "JSWorker.h"
#include "JSWorkerContext.h"
#include "Worker.h"
#include "WorkerContext.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue toJS(ExecState* exec, EventTarget* target)
{
    if (!target)
        return jsNull();
    
#if ENABLE(SVG)
    // SVGElementInstance supports both toSVGElementInstance and toNode since so much mouse handling code depends on toNode returning a valid node.
    if (SVGElementInstance* instance = target->toSVGElementInstance())
        return toJS(exec, instance);
#endif
    
    if (Node* node = target->toNode())
        return toJS(exec, node);

    if (DOMWindow* domWindow = target->toDOMWindow())
        return toJS(exec, domWindow);

    if (XMLHttpRequest* xhr = target->toXMLHttpRequest())
        // XMLHttpRequest is always created via JS, so we don't need to use cacheDOMObject() here.
        return getCachedDOMObjectWrapper(exec->globalData(), xhr);

    if (XMLHttpRequestUpload* upload = target->toXMLHttpRequestUpload())
        return toJS(exec, upload);

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    if (DOMApplicationCache* cache = target->toDOMApplicationCache())
        // DOMApplicationCache is always created via JS, so we don't need to use cacheDOMObject() here.
        return getCachedDOMObjectWrapper(exec->globalData(), cache);
#endif

    if (MessagePort* messagePort = target->toMessagePort())
        return toJS(exec, messagePort);

#if ENABLE(WORKERS)
    if (Worker* worker = target->toWorker())
        return toJS(exec, worker);

    if (WorkerContext* workerContext = target->toWorkerContext())
        return toJSDOMGlobalObject(workerContext);
#endif

    ASSERT_NOT_REACHED();
    return jsNull();
}

EventTarget* toEventTarget(JSC::JSValue value)
{
    #define CONVERT_TO_EVENT_TARGET(type) \
        if (value.isObject(&JS##type::s_info)) \
            return static_cast<JS##type*>(asObject(value))->impl();

    CONVERT_TO_EVENT_TARGET(Node)
    CONVERT_TO_EVENT_TARGET(XMLHttpRequest)
    CONVERT_TO_EVENT_TARGET(XMLHttpRequestUpload)
    CONVERT_TO_EVENT_TARGET(MessagePort)

    if (value.isObject(&JSDOMWindowShell::s_info))
        return static_cast<JSDOMWindowShell*>(asObject(value))->impl();

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    CONVERT_TO_EVENT_TARGET(DOMApplicationCache)
#endif

#if ENABLE(SVG)
    CONVERT_TO_EVENT_TARGET(SVGElementInstance)
#endif

#if ENABLE(WORKERS)
    CONVERT_TO_EVENT_TARGET(Worker)
    CONVERT_TO_EVENT_TARGET(WorkerContext)
#endif

    return 0;
}

} // namespace WebCore
