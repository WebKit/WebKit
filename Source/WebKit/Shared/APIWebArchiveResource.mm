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

#import "config.h"
#import "APIWebArchiveResource.h"

#if PLATFORM(COCOA)

#import "APIData.h"
#import <WebCore/ArchiveResource.h>
#import <wtf/RetainPtr.h>
#import <wtf/URL.h>
#import <wtf/cf/VectorCF.h>
#import <wtf/text/WTFString.h>

namespace API {
using namespace WebCore;

Ref<WebArchiveResource> WebArchiveResource::create(API::Data* data, const WTF::String& url, const WTF::String& mimeType, const WTF::String& textEncoding)
{
    return adoptRef(*new WebArchiveResource(data, url, mimeType, textEncoding));
}

Ref<WebArchiveResource> WebArchiveResource::create(RefPtr<ArchiveResource>&& archiveResource)
{
    return adoptRef(*new WebArchiveResource(WTFMove(archiveResource)));
}

WebArchiveResource::WebArchiveResource(API::Data* data, const WTF::String& url, const WTF::String& mimeType, const WTF::String& textEncoding)
    : m_archiveResource(ArchiveResource::create(SharedBuffer::create(data->span()), WTF::URL { url }, mimeType, textEncoding, WTF::String()))
{
}

WebArchiveResource::WebArchiveResource(RefPtr<ArchiveResource>&& archiveResource)
    : m_archiveResource(WTFMove(archiveResource))
{
}

WebArchiveResource::~WebArchiveResource() = default;

Ref<API::Data> WebArchiveResource::data()
{
    RetainPtr cfData = m_archiveResource->data().makeContiguous()->createCFData();
    auto cfDataSpan = span(cfData.get());
    return API::Data::createWithoutCopying(cfDataSpan, [cfData = WTFMove(cfData)] { });
}

WTF::String WebArchiveResource::url()
{
    return m_archiveResource->url().string();
}

WTF::String WebArchiveResource::mimeType()
{
    return m_archiveResource->mimeType();
}

WTF::String WebArchiveResource::textEncoding()
{
    return m_archiveResource->textEncoding();
}

ArchiveResource* WebArchiveResource::coreArchiveResource()
{
    return m_archiveResource.get();
}

} // namespace WebKit

#endif // PLATFORM(COCOA)
