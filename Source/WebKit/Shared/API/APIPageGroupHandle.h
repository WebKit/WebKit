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

#ifndef APIPageGroupHandle_h
#define APIPageGroupHandle_h

#include "APIObject.h"
#include "WebPageGroupData.h"
#include <wtf/RefPtr.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace API {

class PageGroupHandle : public ObjectImpl<Object::Type::PageGroupHandle> {
public:
    static Ref<PageGroupHandle> create(WebKit::WebPageGroupData&&);
    virtual ~PageGroupHandle();

    const WebKit::WebPageGroupData& webPageGroupData() const { return m_webPageGroupData; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<Object>&);

private:
    explicit PageGroupHandle(WebKit::WebPageGroupData&&);

    const WebKit::WebPageGroupData m_webPageGroupData;
};

} // namespace API


#endif // APIPageGroupHandle_h
