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

#include "config.h"
#include "APIFormInfo.h"

namespace API {

class FrameInfo;

FormInfo::FormInfo(API::FrameInfo& frame, API::FrameInfo& sourceFrame, const WTF::URL& submissionURL, const WTF::String& httpMethod, const Vector<std::pair<WTF::String, WTF::String>>& formValues)
    : m_targetFrame(frame)
    , m_sourceFrame(sourceFrame)
    , m_submissionURL(submissionURL)
    , m_httpMethod(httpMethod)
    , m_formValues(formValues)
{
}

FormInfo::~FormInfo()
{
}

API::FrameInfo* FormInfo::targetFrame() const
{
    return m_targetFrame.ptr();
}

API::FrameInfo* FormInfo::sourceFrame() const
{
    return m_sourceFrame.ptr();
}

const WTF::URL& FormInfo::submissionURL() const
{
    return m_submissionURL;
}

const WTF::String& FormInfo::httpMethod() const
{
    return m_httpMethod;
}

const Vector<std::pair<WTF::String, WTF::String>>& FormInfo::formValues() const
{
    return m_formValues;
}

} // namespace API
