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

#ifndef APIInjectedBundlePageUIClient_h
#define APIInjectedBundlePageUIClient_h

#include "WebEvent.h"

namespace WebCore {
class HitTestResult;
}

namespace WebKit {
class WebFrame;
class WebPage;
class WebSecurityOrigin;
}

namespace API {

class Object;

namespace InjectedBundle {

class PageUIClient {
public:
    virtual ~PageUIClient() { }

    virtual void willAddMessageToConsole(WebKit::WebPage*, const WTF::String& message, int32_t lineNumber) { UNUSED_PARAM(message); UNUSED_PARAM(lineNumber); }
    virtual void willSetStatusbarText(WebKit::WebPage*, const WTF::String&) { }
    virtual void willRunJavaScriptAlert(WebKit::WebPage*, const WTF::String&, WebKit::WebFrame*) { }
    virtual void willRunJavaScriptConfirm(WebKit::WebPage*, const WTF::String&, WebKit::WebFrame*) { }
    virtual void willRunJavaScriptPrompt(WebKit::WebPage*, const WTF::String&, const WTF::String&, WebKit::WebFrame*) { }
    virtual void mouseDidMoveOverElement(WebKit::WebPage*, const WebCore::HitTestResult&, WebKit::WebEvent::Modifiers, RefPtr<API::Object>& userData) { UNUSED_PARAM(userData); }
    virtual void pageDidScroll(WebKit::WebPage*) { }

    virtual WTF::String shouldGenerateFileForUpload(WebKit::WebPage*, const WTF::String& originalFilePath) { UNUSED_PARAM(originalFilePath); return WTF::String(); }
    virtual WTF::String generateFileForUpload(WebKit::WebPage*, const WTF::String& originalFilePath) { UNUSED_PARAM(originalFilePath); return emptyString(); }

    enum class UIElementVisibility {
        Unknown,
        Visible,
        Hidden,
    };

    virtual UIElementVisibility statusBarIsVisible(WebKit::WebPage*) { return UIElementVisibility::Unknown; }
    virtual UIElementVisibility menuBarIsVisible(WebKit::WebPage*) { return UIElementVisibility::Unknown; }
    virtual UIElementVisibility toolbarsAreVisible(WebKit::WebPage*) { return UIElementVisibility::Unknown; }

    virtual void didReachApplicationCacheOriginQuota(WebKit::WebPage*, WebKit::WebSecurityOrigin*, int64_t totalBytesNeeded) { UNUSED_PARAM(totalBytesNeeded); }
    virtual uint64_t didExceedDatabaseQuota(WebKit::WebPage*, WebKit::WebSecurityOrigin*, const WTF::String& databaseName, const WTF::String& databaseDisplayName, uint64_t currentQuotaBytes, uint64_t currentOriginUsageBytes, uint64_t currentDatabaseUsageBytes, uint64_t expectedUsageBytes)
    {
        UNUSED_PARAM(databaseName);
        UNUSED_PARAM(databaseDisplayName);
        UNUSED_PARAM(currentQuotaBytes);
        UNUSED_PARAM(currentOriginUsageBytes);
        UNUSED_PARAM(currentDatabaseUsageBytes);
        UNUSED_PARAM(expectedUsageBytes);
        return 0;
    }

    virtual WTF::String plugInStartLabelTitle(const WTF::String& mimeType) const { UNUSED_PARAM(mimeType); return emptyString(); }
    virtual WTF::String plugInStartLabelSubtitle(const WTF::String& mimeType) const { UNUSED_PARAM(mimeType); return emptyString(); }
    virtual WTF::String plugInExtraStyleSheet() const { return emptyString(); }
    virtual WTF::String plugInExtraScript() const { return emptyString(); }
};

} // namespace InjectedBundle

} // namespace API

#endif // APIInjectedBundlePageUIClient_h
