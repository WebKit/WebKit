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
#include "WebVibrationProxy.h"

#if ENABLE(VIBRATION)

#include "WebContext.h"
#include "WebVibrationProxyMessages.h"

namespace WebKit {

PassRefPtr<WebVibrationProxy> WebVibrationProxy::create(WebContext* context)
{
    return adoptRef(new WebVibrationProxy(context));
}

WebVibrationProxy::WebVibrationProxy(WebContext* context)
    : m_context(context)
{
    m_context->addMessageReceiver(Messages::WebVibrationProxy::messageReceiverName(), this);
}

WebVibrationProxy::~WebVibrationProxy()
{
}

void WebVibrationProxy::invalidate()
{
    cancelVibration();
}

void WebVibrationProxy::initializeProvider(const WKVibrationProvider* provider)
{
    m_provider.initialize(provider);
}

void WebVibrationProxy::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::MessageDecoder& decoder)
{
    didReceiveWebVibrationProxyMessage(connection, messageID, decoder);
}

void WebVibrationProxy::vibrate(uint64_t vibrationTime)
{
    m_provider.vibrate(this, vibrationTime);
}

void WebVibrationProxy::cancelVibration()
{
    m_provider.cancelVibration(this);
}

} // namespace WebKit

#endif // ENABLE(VIBRATION)
