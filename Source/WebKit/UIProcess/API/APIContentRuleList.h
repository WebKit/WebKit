/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebCompiledContentRuleList;
}

namespace API {

class ContentRuleList final : public ObjectImpl<Object::Type::ContentRuleList> {
public:
#if ENABLE(CONTENT_EXTENSIONS)
    static Ref<ContentRuleList> create(const WTF::String& name, Ref<WebKit::WebCompiledContentRuleList>&& contentRuleList)
    {
        return adoptRef(*new ContentRuleList(name, WTFMove(contentRuleList)));
    }

    ContentRuleList(const WTF::String& name, Ref<WebKit::WebCompiledContentRuleList>&&);
    virtual ~ContentRuleList();

    const WTF::String& name() const { return m_name; }
    const WebKit::WebCompiledContentRuleList& compiledRuleList() const { return m_compiledRuleList.get(); }

    bool usesCopiedMemory() const;

private:
    WTF::String m_name;
    Ref<WebKit::WebCompiledContentRuleList> m_compiledRuleList;
#endif // ENABLE(CONTENT_EXTENSIONS)
};

} // namespace API
