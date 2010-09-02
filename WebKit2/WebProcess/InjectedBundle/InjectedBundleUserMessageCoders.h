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
#include "WebPage.h"
#include "WebProcess.h"

namespace WebKit {

// Adds
// - BundlePage -> Page

class InjectedBundleUserMessageEncoder : public UserMessageEncoder<InjectedBundleUserMessageEncoder> {
public:
    typedef UserMessageEncoder<InjectedBundleUserMessageEncoder> Base;

    InjectedBundleUserMessageEncoder(APIObject* root) 
        : Base(root)
    {
    }

    void encode(CoreIPC::ArgumentEncoder* encoder) const 
    {
        APIObject::Type type = m_root->type();
        encoder->encode(static_cast<uint32_t>(type));
        
        if (baseEncode(encoder, type))
            return;

        switch (type) {
        case APIObject::TypeBundlePage: {
            WebPage* page = static_cast<WebPage*>(m_root);
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

class InjectedBundleUserMessageDecoder : public UserMessageDecoder<InjectedBundleUserMessageDecoder> {
public:
    typedef UserMessageDecoder<InjectedBundleUserMessageDecoder> Base;

    InjectedBundleUserMessageDecoder(RefPtr<APIObject>& root)
        : Base(root)
    {
    }

    InjectedBundleUserMessageDecoder(InjectedBundleUserMessageDecoder&, RefPtr<APIObject>& root)
        : Base(root)
    {
    }

    static bool decode(CoreIPC::ArgumentDecoder* decoder, InjectedBundleUserMessageDecoder& coder)
    {
        uint32_t type;
        if (!decoder->decode(type))
            return false;

        if (!Base::baseDecode(decoder, coder, static_cast<APIObject::Type>(type)))
            return false;

        // If the base created something in root, we are done.
        if (coder.m_root)
            return true;

        switch (type) {
        case APIObject::TypePage: {
            uint64_t pageID;
            if (!decoder->decode(pageID))
                return false;
            coder.m_root = WebProcess::shared().webPage(pageID);
            break;
        }
        default:
            return false;
        }

        return true;
    }
};

} // namespace WebKit
