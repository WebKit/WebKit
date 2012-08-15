/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "IntentData.h"

#if ENABLE(WEB_INTENTS)

#include "APIObject.h"
#include "DataReference.h"
#include "WebCoreArgumentCoders.h"
#include "WebProcess.h"
#include <WebCore/Intent.h>
#include <WebCore/MessagePortChannel.h>
#include <WebCore/PlatformMessagePortChannel.h>

using namespace WebCore;

namespace WebKit {

IntentData::IntentData(Intent* coreIntent)
    : action(coreIntent->action())
    , type(coreIntent->type())
    , service(coreIntent->service())
    , data(coreIntent->data()->data())
    , extras(coreIntent->extras())
    , suggestions(coreIntent->suggestions())
{
    MessagePortChannelArray* coreMessagePorts = coreIntent->messagePorts();
    if (coreMessagePorts) {
        size_t numMessagePorts = coreMessagePorts->size();
        for (size_t i = 0; i < numMessagePorts; ++i)
            messagePorts.append(WebProcess::shared().addMessagePortChannel((*coreMessagePorts)[i]->channel()));
    }
}

void IntentData::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(action);
    encoder->encode(type);
    encoder->encode(service);
    encoder->encode(CoreIPC::DataReference(data));
    encoder->encode(extras);
    encoder->encode(suggestions);
    encoder->encode(messagePorts);
}

bool IntentData::decode(CoreIPC::ArgumentDecoder* decoder, IntentData& intentData)
{
    if (!decoder->decode(intentData.action))
        return false;
    if (!decoder->decode(intentData.type))
        return false;
    if (!decoder->decode(intentData.service))
        return false;
    CoreIPC::DataReference data;
    if (!decoder->decode(data))
        return false;
    intentData.data.append(data.data(), data.size());
    if (!decoder->decode(intentData.extras))
        return false;
    if (!decoder->decode(intentData.suggestions))
        return false;
    if (!decoder->decode(intentData.messagePorts))
        return false;

    return true;
}

} // namespace WebKit

#endif // ENABLE(WEB_INTENTS)

