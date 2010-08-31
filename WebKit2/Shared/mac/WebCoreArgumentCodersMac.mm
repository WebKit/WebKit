/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "WebCoreArgumentCoders.h"

namespace CoreIPC {

void encodeResourceRequest(ArgumentEncoder* encoder, const WebCore::ResourceRequest& resourceRequest)
{
    NSURLRequest *nsURLRequest = resourceRequest.nsURLRequest();
    NSData *data = [NSKeyedArchiver archivedDataWithRootObject:nsURLRequest];

    encoder->encodeBytes(static_cast<const uint8_t*>([data bytes]), [data length]);
}

bool decodeResourceRequest(ArgumentDecoder* decoder, WebCore::ResourceRequest& resourceRequest)
{
    Vector<uint8_t> bytes;
    if (!decoder->decodeBytes(bytes))
        return false;

    NSData *nsData = [[NSData alloc] initWithBytesNoCopy:bytes.data() length:bytes.size() freeWhenDone:NO];
    NSURLRequest *nsURLRequest = [NSKeyedUnarchiver unarchiveObjectWithData:nsData];
    if (!nsURLRequest)
        return false;

    resourceRequest = WebCore::ResourceRequest(nsURLRequest);

    return true;
}

} // namespace CoreIPC
