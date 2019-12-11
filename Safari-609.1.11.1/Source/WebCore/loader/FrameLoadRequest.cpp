/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoadRequest.h"

#include "Document.h"
#include "Frame.h"
#include "SecurityOrigin.h"

namespace WebCore {

FrameLoadRequest::FrameLoadRequest(Document& requester, SecurityOrigin& requesterSecurityOrigin, const ResourceRequest& resourceRequest, const String& frameName, LockHistory lockHistory, LockBackForwardList lockBackForwardList, ShouldSendReferrer shouldSendReferrer, AllowNavigationToInvalidURL allowNavigationToInvalidURL, NewFrameOpenerPolicy newFrameOpenerPolicy, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, InitiatedByMainFrame initiatedByMainFrame, ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL, const AtomString& downloadAttribute, const SystemPreviewInfo& systemPreviewInfo)
    : m_requester { makeRef(requester) }
    , m_requesterSecurityOrigin { makeRef(requesterSecurityOrigin) }
    , m_resourceRequest { resourceRequest }
    , m_frameName { frameName }
    , m_lockHistory { lockHistory }
    , m_lockBackForwardList { lockBackForwardList }
    , m_shouldSendReferrer { shouldSendReferrer }
    , m_allowNavigationToInvalidURL { allowNavigationToInvalidURL }
    , m_newFrameOpenerPolicy { newFrameOpenerPolicy }
    , m_shouldReplaceDocumentIfJavaScriptURL { shouldReplaceDocumentIfJavaScriptURL }
    , m_shouldOpenExternalURLsPolicy { shouldOpenExternalURLsPolicy }
    , m_downloadAttribute { downloadAttribute }
    , m_initiatedByMainFrame { initiatedByMainFrame }
    , m_systemPreviewInfo { systemPreviewInfo }
{
}

FrameLoadRequest::FrameLoadRequest(Frame& frame, const ResourceRequest& resourceRequest, ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy, const SubstituteData& substituteData)
    : m_requester { makeRef(*frame.document()) }
    , m_requesterSecurityOrigin { makeRef(frame.document()->securityOrigin()) }
    , m_resourceRequest { resourceRequest }
    , m_substituteData { substituteData }
    , m_lockHistory { LockHistory::No }
    , m_lockBackForwardList { LockBackForwardList::No }
    , m_shouldSendReferrer { MaybeSendReferrer }
    , m_allowNavigationToInvalidURL { AllowNavigationToInvalidURL::Yes }
    , m_newFrameOpenerPolicy { NewFrameOpenerPolicy::Allow }
    , m_shouldReplaceDocumentIfJavaScriptURL { ReplaceDocumentIfJavaScriptURL }
    , m_shouldOpenExternalURLsPolicy { shouldOpenExternalURLsPolicy }
{
}

FrameLoadRequest::~FrameLoadRequest() = default;

FrameLoadRequest::FrameLoadRequest(FrameLoadRequest&&) = default;
FrameLoadRequest& FrameLoadRequest::operator=(FrameLoadRequest&&) = default;

Document& FrameLoadRequest::requester()
{
    return m_requester.get();
}

const SecurityOrigin& FrameLoadRequest::requesterSecurityOrigin() const
{
    return m_requesterSecurityOrigin.get();
}

} // namespace WebCore
