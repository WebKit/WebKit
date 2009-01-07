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

#include "Document.h"
#include "JSEventListener.h"
#include "JSEventTargetNode.h"
#include "JSMessagePort.h"
#include "JSWorker.h"
#include "JSWorkerContext.h"
#include "JSXMLHttpRequestUpload.h"
#include "Worker.h"
#include "WorkerContext.h"

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "JSSVGElementInstance.h"
#endif

using namespace JSC;

namespace WebCore {

JSValuePtr toJS(ExecState* exec, EventTarget* target)
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

} // namespace WebCore
