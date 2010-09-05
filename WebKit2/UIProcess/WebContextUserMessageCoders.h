/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "UserMessageCoders.h"
#include "WebContext.h"
#include "WebPageProxy.h"

namespace WebKit {

// Adds
// - Page -> BundlePage

class WebContextUserMessageEncoder : public UserMessageEncoder<WebContextUserMessageEncoder> {
public:
    typedef UserMessageEncoder<WebContextUserMessageEncoder> Base;

    WebContextUserMessageEncoder(APIObject* root) 
        : Base(root)
    {
    }

    void encode(CoreIPC::ArgumentEncoder* encoder) const 
    {
        APIObject::Type type = APIObject::TypeNull;
        if (baseEncode(encoder, type))
            return;

        switch (type) {
        case APIObject::TypePage: {
            WebPageProxy* page = static_cast<WebPageProxy*>(m_root);
            encoder->encode(page->pageID());
            break;
        }
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }
};

// Adds
//   - Page -> BundlePage

class WebContextUserMessageDecoder : public UserMessageDecoder<WebContextUserMessageDecoder> {
public:
    typedef UserMessageDecoder<WebContextUserMessageDecoder> Base;

    WebContextUserMessageDecoder(RefPtr<APIObject>& root, WebContext* context)
        : Base(root)
        , m_context(context)
    {
    }

    WebContextUserMessageDecoder(WebContextUserMessageDecoder& userMessageDecoder, RefPtr<APIObject>& root)
        : Base(root)
        , m_context(userMessageDecoder.m_context)
    {
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, WebContextUserMessageDecoder& coder)
    {
        APIObject::Type type = APIObject::TypeNull;
        if (!Base::baseDecode(decoder, coder, type))
            return false;

        if (coder.m_root || type == APIObject::TypeNull)
            return true;

        switch (type) {
        case APIObject::TypeBundlePage: {
            uint64_t pageID;
            if (!decoder->decode(pageID))
                return false;
            coder.m_root = coder.m_context->process()->webPage(pageID);
            break;
        }
        default:
            return false;
        }

        return true;
    }

private:
    WebContext* m_context;
};

} // namespace WebKit
