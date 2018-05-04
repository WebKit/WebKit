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

#pragma once

#include "APIObject.h"
#include "WKBase.h"
#include "WebPageProxy.h"
#include <wtf/RefPtr.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
class SharedBuffer;
struct AttachmentDisplayOptions;
struct AttachmentInfo;
}

namespace WebKit {
class WebPageProxy;
}

namespace API {

class Attachment final : public ObjectImpl<Object::Type::Attachment> {
public:
    static Ref<Attachment> create(const WTF::String& identifier, WebKit::WebPageProxy&);
    virtual ~Attachment();

    const WTF::String& identifier() const { return m_identifier; }
    void requestInfo(Function<void(const WebCore::AttachmentInfo&, WebKit::CallbackBase::Error)>&&);
    void setDisplayOptions(WebCore::AttachmentDisplayOptions, Function<void(WebKit::CallbackBase::Error)>&&);
    void setDataAndContentType(WebCore::SharedBuffer&, const WTF::String& newContentType, const WTF::String& newFilename, Function<void(WebKit::CallbackBase::Error)>&&);

private:
    explicit Attachment(const WTF::String& identifier, WebKit::WebPageProxy&);

    WTF::String m_identifier;
    WeakPtr<WebKit::WebPageProxy> m_webPage;
};

} // namespace API
