/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "APIPageGroupHandle.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"

namespace API {

Ref<PageGroupHandle> PageGroupHandle::create(WebKit::WebPageGroupData&& webPageGroupData)
{
    return adoptRef(*new PageGroupHandle(WTFMove(webPageGroupData)));
}

PageGroupHandle::PageGroupHandle(WebKit::WebPageGroupData&& webPageGroupData)
    : m_webPageGroupData(WTFMove(webPageGroupData))
{
}

PageGroupHandle::~PageGroupHandle()
{
}

void PageGroupHandle::encode(IPC::Encoder& encoder) const
{
    encoder << m_webPageGroupData;
}

bool PageGroupHandle::decode(IPC::Decoder& decoder, RefPtr<Object>& result)
{
    std::optional<WebKit::WebPageGroupData> webPageGroupData;
    decoder >> webPageGroupData;
    if (!webPageGroupData)
        return false;

    result = create(WTFMove(*webPageGroupData));
    return true;
}

}
