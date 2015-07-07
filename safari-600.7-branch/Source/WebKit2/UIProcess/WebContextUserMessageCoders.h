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
#include "WebFrameProxy.h"
#include "WebPageGroup.h"
#include "WebPageGroupData.h"
#include "WebPageProxy.h"

#if PLATFORM(COCOA)
#include "ObjCObjectGraphCoders.h"
#endif

namespace WebKit {

// Adds
// - Page -> BundlePage
// - Frame -> BundleFrame
// - PageGroup -> BundlePageGroup

class WebContextUserMessageEncoder : public UserMessageEncoder<WebContextUserMessageEncoder> {
public:
    typedef UserMessageEncoder<WebContextUserMessageEncoder> Base;

    explicit WebContextUserMessageEncoder(API::Object* root, WebProcessProxy& process)
        : Base(root)
        , m_process(process)
    {
    }

    WebContextUserMessageEncoder(const WebContextUserMessageEncoder& userMessageEncoder, API::Object* root)
        : Base(root)
        , m_process(userMessageEncoder.m_process)
    {
    }

    void encode(IPC::ArgumentEncoder& encoder) const
    {
        API::Object::Type type = API::Object::Type::Null;
        if (baseEncode(encoder, *this, type))
            return;

        switch (type) {
        case API::Object::Type::Page: {
            WebPageProxy* page = static_cast<WebPageProxy*>(m_root);
            encoder << page->pageID();
            break;
        }
        case API::Object::Type::Frame: {
            WebFrameProxy* frame = static_cast<WebFrameProxy*>(m_root);
            encoder << frame->frameID();
            break;
        }
        case API::Object::Type::PageGroup: {
            WebPageGroup* pageGroup = static_cast<WebPageGroup*>(m_root);
            encoder << pageGroup->data();
            break;
        }
#if PLATFORM(COCOA)
        case API::Object::Type::ObjCObjectGraph: {
            ObjCObjectGraph* objectGraph = static_cast<ObjCObjectGraph*>(m_root);
            encoder << WebContextObjCObjectGraphEncoder(objectGraph, m_process);
            break;
        }
#endif
        default:
            ASSERT_NOT_REACHED();
            break;
        }
    }

private:
    WebProcessProxy& m_process;
};

// Adds
//   - Page -> BundlePage
//   - Frame -> BundleFrame
//   - PageGroup -> BundlePageGroup

class WebContextUserMessageDecoder : public UserMessageDecoder<WebContextUserMessageDecoder> {
public:
    typedef UserMessageDecoder<WebContextUserMessageDecoder> Base;

    WebContextUserMessageDecoder(RefPtr<API::Object>& root, WebProcessProxy& process)
        : Base(root)
        , m_process(process)
    {
    }

    WebContextUserMessageDecoder(WebContextUserMessageDecoder& userMessageDecoder, RefPtr<API::Object>& root)
        : Base(root)
        , m_process(userMessageDecoder.m_process)
    {
    }

    static bool decode(IPC::ArgumentDecoder& decoder, WebContextUserMessageDecoder& coder)
    {
        API::Object::Type type = API::Object::Type::Null;
        if (!Base::baseDecode(decoder, coder, type))
            return false;

        if (coder.m_root || type == API::Object::Type::Null)
            return true;

        switch (type) {
        case API::Object::Type::BundlePage: {
            uint64_t pageID;
            if (!decoder.decode(pageID))
                return false;
            coder.m_root = coder.m_process.webPage(pageID);
            break;
        }
        case API::Object::Type::BundleFrame: {
            uint64_t frameID;
            if (!decoder.decode(frameID))
                return false;
            coder.m_root = coder.m_process.webFrame(frameID);
            break;
        }
        case API::Object::Type::BundlePageGroup: {
            uint64_t pageGroupID;
            if (!decoder.decode(pageGroupID))
                return false;
            coder.m_root = WebPageGroup::get(pageGroupID);
            break;
        }
#if PLATFORM(COCOA)
        case API::Object::Type::ObjCObjectGraph: {
            RefPtr<ObjCObjectGraph> objectGraph;
            WebContextObjCObjectGraphDecoder objectGraphDecoder(objectGraph, coder.m_process);
            if (!decoder.decode(objectGraphDecoder))
                return false;
            coder.m_root = objectGraph.get();
            break;
        }
#endif
        default:
            return false;
        }

        return true;
    }

private:
    WebProcessProxy& m_process;
};

} // namespace WebKit
