/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "APIPageHandle.h"

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"

namespace API {

Ref<PageHandle> PageHandle::create(WebKit::WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier webPageID)
{
    return adoptRef(*new PageHandle(pageProxyID, webPageID, false));
}

Ref<PageHandle> PageHandle::createAutoconverting(WebKit::WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier webPageID)
{
    return adoptRef(*new PageHandle(pageProxyID, webPageID, true));
}

PageHandle::PageHandle(WebKit::WebPageProxyIdentifier pageProxyID, WebCore::PageIdentifier webPageID, bool isAutoconverting)
    : m_pageProxyID(pageProxyID)
    , m_webPageID(webPageID)
    , m_isAutoconverting(isAutoconverting)
{
}

PageHandle::~PageHandle()
{
}

void PageHandle::encode(IPC::Encoder& encoder) const
{
    encoder << m_pageProxyID;
    encoder << m_webPageID;
    encoder << m_isAutoconverting;
}

bool PageHandle::decode(IPC::Decoder& decoder, RefPtr<Object>& result)
{
    std::optional<WebKit::WebPageProxyIdentifier> pageProxyID;
    decoder >> pageProxyID;
    if (!pageProxyID)
        return false;

    std::optional<WebCore::PageIdentifier> webPageID;
    decoder >> webPageID;
    if (!webPageID)
        return false;

    bool isAutoconverting;
    if (!decoder.decode(isAutoconverting))
        return false;

    result = isAutoconverting ? createAutoconverting(*pageProxyID, *webPageID) : create(*pageProxyID, *webPageID);
    return true;
}

} // namespace API
