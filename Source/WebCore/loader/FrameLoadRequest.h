/*
 * Copyright (C) 2003-2017 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "FrameLoaderTypes.h"
#include "ReferrerPolicy.h"
#include "ResourceRequest.h"
#include "ShouldTreatAsContinuingLoad.h"
#include "SubstituteData.h"
#include <wtf/Forward.h>

namespace WebCore {

class Document;
class Frame;
class SecurityOrigin;

class FrameLoadRequest {
public:
    WEBCORE_EXPORT FrameLoadRequest(Document&, SecurityOrigin&, ResourceRequest&&, const String& frameName, InitiatedByMainFrame, const AtomString& downloadAttribute = { }, const SystemPreviewInfo& = { });
    WEBCORE_EXPORT FrameLoadRequest(Frame&, const ResourceRequest&, const SubstituteData& = SubstituteData());

    WEBCORE_EXPORT ~FrameLoadRequest();

    WEBCORE_EXPORT FrameLoadRequest(FrameLoadRequest&&);
    WEBCORE_EXPORT FrameLoadRequest& operator=(FrameLoadRequest&&);

    bool isEmpty() const { return m_resourceRequest.isEmpty(); }

    Document& requester();
    const SecurityOrigin& requesterSecurityOrigin() const;

    ResourceRequest& resourceRequest() { return m_resourceRequest; }
    const ResourceRequest& resourceRequest() const { return m_resourceRequest; }

    const String& frameName() const { return m_frameName; }
    void setFrameName(const String& frameName) { m_frameName = frameName; }

    void setShouldCheckNewWindowPolicy(bool checkPolicy) { m_shouldCheckNewWindowPolicy = checkPolicy; }
    bool shouldCheckNewWindowPolicy() const { return m_shouldCheckNewWindowPolicy; }

    void setShouldTreatAsContinuingLoad(ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad) { m_shouldTreatAsContinuingLoad = shouldTreatAsContinuingLoad; }
    ShouldTreatAsContinuingLoad shouldTreatAsContinuingLoad() const { return m_shouldTreatAsContinuingLoad; }

    const SubstituteData& substituteData() const { return m_substituteData; }
    void setSubstituteData(const SubstituteData& data) { m_substituteData = data; }
    bool hasSubstituteData() { return m_substituteData.isValid(); }

    LockHistory lockHistory() const { return m_lockHistory; }
    void setLockHistory(LockHistory value) { m_lockHistory = value; }

    LockBackForwardList lockBackForwardList() const { return m_lockBackForwardList; }
    void setLockBackForwardList(LockBackForwardList value) { m_lockBackForwardList = value; }

    const String& clientRedirectSourceForHistory() const { return m_clientRedirectSourceForHistory; }
    void setClientRedirectSourceForHistory(const String& clientRedirectSourceForHistory) { m_clientRedirectSourceForHistory = clientRedirectSourceForHistory; }

    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }
    void setReferrerPolicy(const ReferrerPolicy& referrerPolicy) { m_referrerPolicy = referrerPolicy; }

    AllowNavigationToInvalidURL allowNavigationToInvalidURL() const { return m_allowNavigationToInvalidURL; }
    void disableNavigationToInvalidURL() { m_allowNavigationToInvalidURL = AllowNavigationToInvalidURL::No; }

    NewFrameOpenerPolicy newFrameOpenerPolicy() const { return m_newFrameOpenerPolicy; }
    void setNewFrameOpenerPolicy(NewFrameOpenerPolicy newFrameOpenerPolicy) { m_newFrameOpenerPolicy = newFrameOpenerPolicy; }

    // The shouldReplaceDocumentIfJavaScriptURL parameter will go away when the FIXME to eliminate the
    // corresponding parameter from ScriptController::executeIfJavaScriptURL() is addressed.
    ShouldReplaceDocumentIfJavaScriptURL shouldReplaceDocumentIfJavaScriptURL() const { return m_shouldReplaceDocumentIfJavaScriptURL; }
    void disableShouldReplaceDocumentIfJavaScriptURL() { m_shouldReplaceDocumentIfJavaScriptURL = DoNotReplaceDocumentIfJavaScriptURL; }

    void setShouldOpenExternalURLsPolicy(ShouldOpenExternalURLsPolicy policy) { m_shouldOpenExternalURLsPolicy = policy; }
    ShouldOpenExternalURLsPolicy shouldOpenExternalURLsPolicy() const { return m_shouldOpenExternalURLsPolicy; }

    const AtomString& downloadAttribute() const { return m_downloadAttribute; }

    InitiatedByMainFrame initiatedByMainFrame() const { return m_initiatedByMainFrame; }

    bool isSystemPreview() const { return m_systemPreviewInfo.isPreview; }
    const SystemPreviewInfo& systemPreviewInfo() const { return m_systemPreviewInfo; }

    void setIsRequestFromClientOrUserInput() { m_isRequestFromClientOrUserInput = true; }
    bool isRequestFromClientOrUserInput() const { return m_isRequestFromClientOrUserInput; }

private:
    Ref<Document> m_requester;
    Ref<SecurityOrigin> m_requesterSecurityOrigin;
    ResourceRequest m_resourceRequest;
    String m_frameName;
    SubstituteData m_substituteData;
    String m_clientRedirectSourceForHistory;

    bool m_shouldCheckNewWindowPolicy { false };
    ShouldTreatAsContinuingLoad m_shouldTreatAsContinuingLoad { ShouldTreatAsContinuingLoad::No };
    LockHistory m_lockHistory { LockHistory::No };
    LockBackForwardList m_lockBackForwardList { LockBackForwardList::No };
    ReferrerPolicy m_referrerPolicy { ReferrerPolicy::EmptyString };
    AllowNavigationToInvalidURL m_allowNavigationToInvalidURL { AllowNavigationToInvalidURL::Yes };
    NewFrameOpenerPolicy m_newFrameOpenerPolicy { NewFrameOpenerPolicy::Allow };
    ShouldReplaceDocumentIfJavaScriptURL m_shouldReplaceDocumentIfJavaScriptURL { ReplaceDocumentIfJavaScriptURL };
    ShouldOpenExternalURLsPolicy m_shouldOpenExternalURLsPolicy { ShouldOpenExternalURLsPolicy::ShouldNotAllow };
    AtomString m_downloadAttribute;
    InitiatedByMainFrame m_initiatedByMainFrame { InitiatedByMainFrame::Unknown };
    SystemPreviewInfo m_systemPreviewInfo;
    bool m_isRequestFromClientOrUserInput { false };
};

} // namespace WebCore
