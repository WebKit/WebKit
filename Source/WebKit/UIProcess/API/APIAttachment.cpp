/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "APIAttachment.h"

#include <WebCore/AttachmentTypes.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/BlockPtr.h>
#include <wtf/text/WTFString.h>

namespace API {

Ref<Attachment> Attachment::create(const WTF::String& identifier, WebKit::WebPageProxy& webPage)
{
    return adoptRef(*new Attachment(identifier, webPage));
}

Attachment::Attachment(const WTF::String& identifier, WebKit::WebPageProxy& webPage)
    : m_identifier(identifier)
    , m_webPage(webPage.createWeakPtr())
{
}

Attachment::~Attachment()
{
}

void Attachment::requestInfo(Function<void(const WebCore::AttachmentInfo&, WebKit::CallbackBase::Error)>&& callback)
{
    if (m_webPage)
        m_webPage->requestAttachmentInfo(m_identifier, WTFMove(callback));
    else
        callback({ }, WebKit::CallbackBase::Error::OwnerWasInvalidated);
}

void Attachment::setDisplayOptions(WebCore::AttachmentDisplayOptions options, Function<void(WebKit::CallbackBase::Error)>&& callback)
{
    if (m_webPage)
        m_webPage->setAttachmentDisplayOptions(m_identifier, options, WTFMove(callback));
    else
        callback(WebKit::CallbackBase::Error::OwnerWasInvalidated);
}

void Attachment::setDataAndContentType(WebCore::SharedBuffer& data, const WTF::String& newContentType, const WTF::String& newFilename, Function<void(WebKit::CallbackBase::Error)>&& callback)
{
    auto optionalNewContentType = newContentType.isNull() ? std::nullopt : std::optional<WTF::String> { newContentType };
    auto optionalNewFilename = newFilename.isNull() ? std::nullopt : std::optional<WTF::String> { newFilename };
    if (m_webPage)
        m_webPage->setAttachmentDataAndContentType(m_identifier, data, WTFMove(optionalNewContentType), WTFMove(optionalNewFilename), WTFMove(callback));
    else
        callback(WebKit::CallbackBase::Error::OwnerWasInvalidated);
}

}
