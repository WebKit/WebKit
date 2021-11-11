/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)

#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <wtf/Compiler.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS ASVInlinePreview;

namespace WebKit {

class ARKitInlinePreviewModelPlayer final : public WebCore::ModelPlayer, public CanMakeWeakPtr<ARKitInlinePreviewModelPlayer> {
public:
    static Ref<ARKitInlinePreviewModelPlayer> create(WebPage&, WebCore::ModelPlayerClient&);
    virtual ~ARKitInlinePreviewModelPlayer();

    static void setModelElementCacheDirectory(const String&);

private:
    ARKitInlinePreviewModelPlayer(WebPage&, WebCore::ModelPlayerClient&);

    // WebCore::ModelPlayer overrides.
    virtual void load(WebCore::Model&, WebCore::LayoutSize) override;
    virtual PlatformLayer* layer() override;

    void createFile(WebCore::Model&);
    void clearFile();

    WeakPtr<WebPage> m_page;
    WeakPtr<WebCore::ModelPlayerClient> m_client;

    String m_filePath;
    RetainPtr<ASVInlinePreview> m_inlinePreview;
};

}

#endif
