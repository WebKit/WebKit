/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "LocalDOMWindowMediaControls.h"

#if ENABLE(VIDEO)

#include "MediaControlsUtils.h"
#include "RenderTheme.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LocalDOMWindowMediaControls);

LocalDOMWindowMediaControls::LocalDOMWindowMediaControls(DOMWindow& window)
    : LocalDOMWindowProperty(dynamicDowncast<LocalDOMWindow>(&window))
{
}

LocalDOMWindowMediaControls::~LocalDOMWindowMediaControls() = default;

LocalDOMWindowMediaControls* LocalDOMWindowMediaControls::from(DOMWindow& window)
{
    RefPtr localWindow = dynamicDowncast<LocalDOMWindow>(window);
    if (!localWindow)
        return nullptr;

    auto* supplement = downcast<LocalDOMWindowMediaControls>(Supplement<LocalDOMWindow>::from(localWindow.get(), supplementName()));
    if (!supplement) {
        auto newSupplement = makeUnique<LocalDOMWindowMediaControls>(window);
        supplement = newSupplement.get();
        provideTo(localWindow.get(), supplementName(), WTFMove(newSupplement));
    }
    return supplement;
}

RefPtr<MediaControlsUtils> LocalDOMWindowMediaControls::utils(Document& document, DOMWindow& window)
{
    if (auto* supplement = from(window))
        return supplement->ensureUtils(document);
    ASSERT_NOT_REACHED();
    return nullptr;
}

Ref<MediaControlsUtils> LocalDOMWindowMediaControls::ensureUtils(Document& document)
{
    if (!m_utils)
        m_utils = MediaControlsUtils::create(document);
    return *m_utils;
}

ASCIILiteral LocalDOMWindowMediaControls::supplementName()
{
    return "LocalDOMWindowMediaControls"_s;
}

}

#endif
