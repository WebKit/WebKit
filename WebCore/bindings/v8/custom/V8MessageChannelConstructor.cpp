/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MessageChannel.h"

#include "Document.h"
#include "Frame.h"

#include "V8Binding.h"
#include "V8Proxy.h"

#include <wtf/RefPtr.h>

namespace WebCore {

CALLBACK_FUNC_DECL(MessageChannelConstructor)
{
    INC_STATS("DOM.MessageChannel.Constructor");
    // FIXME: The logic here is almost exact duplicate of V8::ConstructDOMObject.
    // Consider refactoring to reduce duplication.
    if (!args.IsConstructCall())
        return throwError("DOM object constructor cannot be called as a function.");

    // Get the document.
    Frame* frame = V8Proxy::retrieveFrame();
    if (!frame)
        return v8::Undefined();

    Document* document = frame->document();

    // Note: it's OK to let this RefPtr go out of scope because we also call
    // SetDOMWrapper(), which effectively holds a reference to obj.
    RefPtr<MessageChannel> obj = MessageChannel::create(document);

    v8::Local<v8::Object> messageChannel = args.Holder();

    // Create references from the MessageChannel wrapper to the two
    // MessagePort wrappers to make sure that the MessagePort wrappers
    // stay alive as long as the MessageChannel wrapper is around.
    messageChannel->SetInternalField(kMessageChannelPort1Index, V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, obj->port1()));
    messageChannel->SetInternalField(kMessageChannelPort2Index, V8Proxy::ToV8Object(V8ClassIndex::MESSAGEPORT, obj->port2()));

    // Setup the standard wrapper object internal fields.
    V8Proxy::SetDOMWrapper(messageChannel, V8ClassIndex::MESSAGECHANNEL, obj.get());
    return toV8(obj.release(), messageChannel);
}


} // namespace WebCore
