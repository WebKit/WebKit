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

#include "ARKitInlinePreviewModelPlayer.h"
#include <WebCore/ModelPlayer.h>
#include <WebCore/ModelPlayerClient.h>
#include <wtf/Compiler.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS ASVInlinePreview;

namespace WebKit {

class ARKitInlinePreviewModelPlayerMac final : public ARKitInlinePreviewModelPlayer {
public:
    static Ref<ARKitInlinePreviewModelPlayerMac> create(WebPage&, WebCore::ModelPlayerClient&);
    virtual ~ARKitInlinePreviewModelPlayerMac();

    static void setModelElementCacheDirectory(const String&);
    static const String& modelElementCacheDirectory();

private:
    ARKitInlinePreviewModelPlayerMac(WebPage&, WebCore::ModelPlayerClient&);

    std::optional<ModelIdentifier> modelIdentifier() override;

    // WebCore::ModelPlayer overrides.
    void load(WebCore::Model&, WebCore::LayoutSize) override;
    void sizeDidChange(WebCore::LayoutSize) override;
    PlatformLayer* layer() override;
    bool supportsMouseInteraction() override;
    bool supportsDragging() override;
    void handleMouseDown(const WebCore::LayoutPoint&, MonotonicTime) override;
    void handleMouseMove(const WebCore::LayoutPoint&, MonotonicTime) override;
    void handleMouseUp(const WebCore::LayoutPoint&, MonotonicTime) override;
    String inlinePreviewUUIDForTesting() const override;

    void createFile(WebCore::Model&);
    void clearFile();

    void createPreviewsForModelWithURL(const URL&);
    void didCreateRemotePreviewForModelWithURL(const URL&);

    WebCore::LayoutSize m_size;
    String m_filePath;
    RetainPtr<ASVInlinePreview> m_inlinePreview;
};

}

#endif
