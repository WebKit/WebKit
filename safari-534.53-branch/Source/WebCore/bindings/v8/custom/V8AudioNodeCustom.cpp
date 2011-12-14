/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEB_AUDIO)

#include "V8AudioNode.h"

#include "AudioNode.h"
#include "ExceptionCode.h"
#include "NotImplemented.h"
#include "V8Binding.h"
#include "V8Proxy.h"

namespace WebCore {

v8::Handle<v8::Value> V8AudioNode::connectCallback(const v8::Arguments& args)
{    
    if (args.Length() < 1)
        return throwError("Not enough arguments", V8Proxy::SyntaxError);

    AudioNode* destinationNode = toNative(args[0]->ToObject());
    if (!destinationNode)
        return throwError("Invalid destination node", V8Proxy::SyntaxError);
    
    unsigned output = 0;
    unsigned input = 0;
    bool ok = false;
    if (args.Length() > 1) {
        output = toInt32(args[1], ok);
        if (!ok)
            return throwError("Invalid index parameters", V8Proxy::SyntaxError);
    }

    if (args.Length() > 2) {
        input = toInt32(args[2], ok);
        if (!ok)
            return throwError("Invalid index parameters", V8Proxy::SyntaxError);
    }
        
    AudioNode* audioNode = toNative(args.Holder());
    bool success = audioNode->connect(destinationNode, output, input);
    if (!success)
        return throwError("Invalid index parameter", V8Proxy::SyntaxError);

    return v8::Undefined();
}

v8::Handle<v8::Value> V8AudioNode::disconnectCallback(const v8::Arguments& args)
{    
    unsigned output = 0;
    bool ok = false;
    if (args.Length() > 0) {
        output = toInt32(args[0], ok);
        if (!ok)
            return throwError("Invalid index parameters", V8Proxy::SyntaxError);
    }

    AudioNode* audioNode = toNative(args.Holder());
    bool success = audioNode->disconnect(output);
    if (!success)
        return throwError("Invalid index parameter", V8Proxy::SyntaxError);

    return v8::Undefined();
}

} // namespace WebCore

#endif // ENABLE(WEB_AUDIO)
