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

#pragma once

#include "QuickLookDocumentData.h"
#include <WebCore/PageIdentifier.h>
#include <WebCore/PreviewLoaderClient.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebFrame;

class WebPreviewLoaderClient final : public WebCore::PreviewLoaderClient {
public:
    static Ref<WebPreviewLoaderClient> create(const String& fileName, const String& uti, WebCore::PageIdentifier pageID)
    {
        return adoptRef(*new WebPreviewLoaderClient(fileName, uti, pageID));
    }
    ~WebPreviewLoaderClient();

    static void didReceivePassword(const String&, WebCore::PageIdentifier);

private:
    WebPreviewLoaderClient(const String& fileName, const String& uti, WebCore::PageIdentifier);
    void didReceiveDataArray(CFArrayRef) override;
    void didFinishLoading() override;
    void didFail() override;
    bool supportsPasswordEntry() const override { return true; }
    void didRequestPassword(Function<void(const String&)>&&) override;

    const String m_fileName;
    const String m_uti;
    const WebCore::PageIdentifier m_pageID;
    QuickLookDocumentData m_data;
};

} // namespace WebKit
