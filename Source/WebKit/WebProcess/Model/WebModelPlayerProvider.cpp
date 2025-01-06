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
#include "WebProcess.h"
#include <WebCore/ModelPlayer.h>
#include <WebCore/Page.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
#include "ARKitInlinePreviewModelPlayerMac.h"
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
#include "ARKitInlinePreviewModelPlayerIOS.h"
#endif

#if HAVE(SCENEKIT)
#include <WebCore/SceneKitModelPlayer.h>
#endif

#if ENABLE(MODEL_PROCESS)
#include "ModelProcessModelPlayer.h"
#include "ModelProcessModelPlayerManager.h"
#endif

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebModelPlayerProvider);

WebModelPlayerProvider::WebModelPlayerProvider(WebPage& page)
    : m_page { page }
{
}

WebModelPlayerProvider::~WebModelPlayerProvider() = default;

// MARK: - WebCore::ModelPlayerProvider overrides.

RefPtr<WebCore::ModelPlayer> WebModelPlayerProvider::createModelPlayer(WebCore::ModelPlayerClient& client)
{
    Ref page = m_page.get();
    UNUSED_PARAM(page);
#if ENABLE(MODEL_PROCESS)
    if (page->corePage()->settings().modelProcessEnabled())
        return WebProcess::singleton().modelProcessModelPlayerManager().createModelProcessModelPlayer(page, client);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    if (page->useARKitForModel())
        return ARKitInlinePreviewModelPlayerMac::create(page, client);
#endif
#if HAVE(SCENEKIT)
    if (page->useSceneKitForModel())
        return WebCore::SceneKitModelPlayer::create(client);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    return ARKitInlinePreviewModelPlayerIOS::create(page, client);
#endif

    UNUSED_PARAM(client);
    return nullptr;
}

void WebModelPlayerProvider::deleteModelPlayer(WebCore::ModelPlayer& modelPlayer)
{
#if ENABLE(MODEL_PROCESS)
    Ref page = m_page.get();
    if (page->corePage()->settings().modelProcessEnabled())
        return WebProcess::singleton().modelProcessModelPlayerManager().deleteModelProcessModelPlayer(modelPlayer);
#else
    UNUSED_PARAM(modelPlayer);
#endif
}

}
