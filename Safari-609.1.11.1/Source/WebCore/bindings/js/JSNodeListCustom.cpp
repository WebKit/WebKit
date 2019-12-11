/*
 * Copyright (C) 2007-2019 Apple Inc. All rights reserved.
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
#include "JSNodeList.h"

#include "ChildNodeList.h"
#include "JSNode.h"
#include "LiveNodeList.h"
#include "Node.h"
#include "NodeList.h"
#include <wtf/text/AtomString.h>


namespace WebCore {
using namespace JSC;

bool JSNodeListOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor, const char** reason)
{
    JSNodeList* jsNodeList = jsCast<JSNodeList*>(handle.slot()->asCell());
    if (!jsNodeList->hasCustomProperties(jsNodeList->vm()))
        return false;

    if (jsNodeList->wrapped().isLiveNodeList()) {
        if (UNLIKELY(reason))
            *reason = "LiveNodeList owner is opaque root";

        return visitor.containsOpaqueRoot(root(static_cast<LiveNodeList&>(jsNodeList->wrapped()).ownerNode()));
    }

    if (jsNodeList->wrapped().isChildNodeList()) {
        if (UNLIKELY(reason))
            *reason = "ChildNodeList owner is opaque root";

        return visitor.containsOpaqueRoot(root(static_cast<ChildNodeList&>(jsNodeList->wrapped()).ownerNode()));
    }

    if (jsNodeList->wrapped().isEmptyNodeList()) {
        if (UNLIKELY(reason))
            *reason = "EmptyNodeList owner is opaque root";

        return visitor.containsOpaqueRoot(root(static_cast<EmptyNodeList&>(jsNodeList->wrapped()).ownerNode()));
    }
    return false;
}

JSC::JSValue createWrapper(JSDOMGlobalObject& globalObject, Ref<NodeList>&& nodeList)
{
    // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
    // https://bugs.webkit.org/show_bug.cgi?id=142595
    globalObject.vm().heap.deprecatedReportExtraMemory(nodeList->memoryCost());
    return createWrapper<NodeList>(&globalObject, WTFMove(nodeList));
}

JSC::JSValue toJSNewlyCreated(JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<NodeList>&& nodeList)
{
    return createWrapper(*globalObject, WTFMove(nodeList));
}

} // namespace WebCore
