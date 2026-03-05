/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "APIFrameInfo.h"
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

namespace API {

class FormInfo final : public ObjectImpl<Object::Type::FormInfo> {
public:
    template <typename... Args> static Ref<FormInfo> create(Args&&... args)
    {
        return adoptRef(*new FormInfo(std::forward<Args>(args)...));
    }

    virtual ~FormInfo();

    API::FrameInfo* NODELETE targetFrame() const;
    API::FrameInfo* NODELETE sourceFrame() const;
    const WTF::URL& NODELETE submissionURL() const LIFETIME_BOUND;
    const WTF::String& NODELETE httpMethod() const LIFETIME_BOUND;
    const Vector<std::pair<WTF::String, WTF::String>>& NODELETE formValues() const LIFETIME_BOUND;

private:
    FormInfo(API::FrameInfo&, API::FrameInfo& sourceFrame, const WTF::URL& submissionURL, const WTF::String& httpMethod, const Vector<std::pair<WTF::String, WTF::String>>& formValues);

    Ref<API::FrameInfo> m_targetFrame;
    Ref<API::FrameInfo> m_sourceFrame;
    WTF::URL m_submissionURL;
    WTF::String m_httpMethod;
    Vector<std::pair<WTF::String, WTF::String>> m_formValues;
};

} // namespace API

SPECIALIZE_TYPE_TRAITS_API_OBJECT(FormInfo);
