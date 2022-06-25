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

#ifndef WebArchiveResource_h
#define WebArchiveResource_h

#if PLATFORM(COCOA)

#include "APIObject.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace API {
class Data;
class URL;
}

namespace WebCore {
class ArchiveResource;
}

namespace API {

class WebArchiveResource : public API::ObjectImpl<API::Object::Type::WebArchiveResource> {
public:
    virtual ~WebArchiveResource();

    static Ref<WebArchiveResource> create(API::Data*, const WTF::String& URL, const WTF::String& MIMEType, const WTF::String& textEncoding);
    static Ref<WebArchiveResource> create(RefPtr<WebCore::ArchiveResource>&&);

    Ref<API::Data> data();
    WTF::String URL();
    WTF::String MIMEType();
    WTF::String textEncoding();

    WebCore::ArchiveResource* coreArchiveResource();

private:
    WebArchiveResource(API::Data*, const WTF::String& URL, const WTF::String& MIMEType, const WTF::String& textEncoding);
    WebArchiveResource(RefPtr<WebCore::ArchiveResource>&&);

    RefPtr<WebCore::ArchiveResource> m_archiveResource;
};

} // namespace API

#endif // PLATFORM(COCOA)

#endif // WebArchiveResource_h
