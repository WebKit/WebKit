/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#if ENABLE(CONTEXT_MENUS)

#include "APIClient.h"
#include "APIInjectedBundlePageContextMenuClient.h"
#include "WKBundlePageContextMenuClient.h"

namespace API {
class Object;

template<> struct ClientTraits<WKBundlePageContextMenuClientBase> {
    typedef std::tuple<WKBundlePageContextMenuClientV0, WKBundlePageContextMenuClientV1> Versions;
};
}

namespace WebCore {
class ContextMenuContext;
class ContextMenuItem;
class HitTestResult;
}

namespace WebKit {
class WebContextMenuItemData;
class WebPage;

class InjectedBundlePageContextMenuClient : public API::Client<WKBundlePageContextMenuClientBase>, public API::InjectedBundle::PageContextMenuClient {
public:
    explicit InjectedBundlePageContextMenuClient(const WKBundlePageContextMenuClientBase*);

private:
    bool getCustomMenuFromDefaultItems(WebPage&, const WebCore::HitTestResult&, const Vector<WebCore::ContextMenuItem>& defaultMenu, Vector<WebContextMenuItemData>& newMenu, const WebCore::ContextMenuContext&, RefPtr<API::Object>& userData) override;
    void prepareForImmediateAction(WebPage&, const WebCore::HitTestResult&, RefPtr<API::Object>& userData) override;
};

} // namespace WebKit

#endif // ENABLE(CONTEXT_MENUS)
