/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include "config.h"
#include "TextInputContext.h"

#include "WebCoreArgumentCoders.h"
#include <WebCore/Element.h>

namespace WebKit {

TextInputContext::~TextInputContext() = default;

}

namespace IPC {

void ArgumentCoder<WebKit::TextInputContext>::encode(Encoder& encoder, const WebKit::TextInputContext& context)
{
    encoder << context.boundingRect;
    encoder << context.webPageIdentifier;
    encoder << context.documentIdentifier;
    encoder << context.elementIdentifier;
}

Optional<WebKit::TextInputContext> ArgumentCoder<WebKit::TextInputContext>::decode(Decoder& decoder)
{
    WebKit::TextInputContext context;
    if (!decoder.decode(context.boundingRect))
        return WTF::nullopt;
    if (!decoder.decode(context.webPageIdentifier))
        return WTF::nullopt;
    auto documentIdentifier = WebCore::DocumentIdentifier::decode(decoder);
    if (!documentIdentifier)
        return WTF::nullopt;
    context.documentIdentifier = *documentIdentifier;
    auto elementIdentifier = WebCore::ElementIdentifier::decode(decoder);
    if (!elementIdentifier)
        return WTF::nullopt;
    context.elementIdentifier = *elementIdentifier;

    return context;
}

}
