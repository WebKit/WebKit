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

#if ENABLE(ATTACHMENT_ELEMENT)

#include <WebCore/SharedBuffer.h>
#include <wtf/text/WTFString.h>

namespace API {

Ref<Attachment> Attachment::create(const WTF::String& identifier, WebKit::WebPageProxy& webPage)
{
    return adoptRef(*new Attachment(identifier, webPage));
}

Attachment::Attachment(const WTF::String& identifier, WebKit::WebPageProxy& webPage)
    : m_identifier(identifier)
    , m_webPage(makeWeakPtr(webPage))
{
}

Attachment::~Attachment()
{
}

void Attachment::updateAttributes(Function<void(WebKit::CallbackBase::Error)>&& callback)
{
    if (!m_webPage) {
        callback(WebKit::CallbackBase::Error::OwnerWasInvalidated);
        return;
    }

    m_webPage->updateAttachmentAttributes(*this, WTFMove(callback));
}

void Attachment::invalidate()
{
    m_identifier = { };
    m_filePath = { };
    m_contentType = { };
    m_webPage.clear();
#if PLATFORM(COCOA)
    m_fileWrapper.clear();
#endif
}

#if !PLATFORM(COCOA)

bool Attachment::isEmpty() const
{
    return true;
}

WTF::String Attachment::mimeType() const
{
    return m_contentType;
}

WTF::String Attachment::fileName() const
{
    return { };
}

std::optional<uint64_t> Attachment::fileSizeForDisplay() const
{
    return std::nullopt;
}

RefPtr<WebCore::SharedBuffer> Attachment::enclosingImageData() const
{
    return nullptr;
}

RefPtr<WebCore::SharedBuffer> Attachment::createSerializedRepresentation() const
{
    return nullptr;
}

void Attachment::updateFromSerializedRepresentation(Ref<WebCore::SharedBuffer>&&, const WTF::String&)
{
}

#endif // !PLATFORM(COCOA)

}

#endif // ENABLE(ATTACHMENT_ELEMENT)
