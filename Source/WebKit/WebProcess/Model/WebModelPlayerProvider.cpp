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

#include "config.h"
#include "WebModelPlayerProvider.h"

#include "WebPage.h"
#include <WebCore/ModelPlayer.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#include "ARKitInlinePreviewModelPlayerMac.h"
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#include "ARKitInlinePreviewModelPlayerIOS.h"
#endif

#if HAVE(SCENEKIT)
#include <WebCore/SceneKitModelPlayer.h>
#endif

namespace WebKit {

WebModelPlayerProvider::WebModelPlayerProvider(WebPage& page)
    : m_page { page }
{
    UNUSED_PARAM(m_page);
}

WebModelPlayerProvider::~WebModelPlayerProvider() = default;

// MARK: - WebCore::ModelPlayerProvider overrides.

RefPtr<WebCore::ModelPlayer> WebModelPlayerProvider::createModelPlayer(WebCore::ModelPlayerClient& client)
{
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    if (m_page.useARKitForModel())
        return ARKitInlinePreviewModelPlayerMac::create(m_page, client);
#endif
#if HAVE(SCENEKIT)
    if (m_page.useSceneKitForModel())
        return WebCore::SceneKitModelPlayer::create(client);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    return ARKitInlinePreviewModelPlayerIOS::create(m_page, client);
#endif

    UNUSED_PARAM(client);
    return nullptr;
}

}
