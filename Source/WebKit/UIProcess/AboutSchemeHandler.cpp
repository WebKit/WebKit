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
#include "AboutSchemeHandler.h"

#include <WebCore/ResourceError.h>

#if USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/AboutSchemeHandlerAdditions.h>)
#include <WebKitAdditions/AboutSchemeHandlerAdditions.h>
#endif

namespace WebKit {

Ref<AboutSchemeHandler> AboutSchemeHandler::create()
{
    return adoptRef(*new AboutSchemeHandler());
}

AboutSchemeHandler::AboutSchemeHandler()
{
    platformInitialize();
}

AboutSchemeHandler::OpaquePathHandler* AboutSchemeHandler::handlerForURL(URL& url) const
{
    if (!url.hasOpaquePath())
        return nullptr;

    return m_handlers.get<StringViewHashTranslator>(url.path());
}

void AboutSchemeHandler::platformStartTask(WebPageProxy&, WebURLSchemeTask& task)
{
    auto url = task.request().url();

    if (auto* handler = handlerForURL(url)) {
        handler->loadContent(url, [task = Ref { task }](auto&& response, auto&& buffer) mutable {
            task->didReceiveResponse(WTF::move(response));
            task->didReceiveData(WTF::move(buffer));
            task->didComplete({ });
        });
        return;
    }

    WebCore::ResourceResponse response(WTF::URL { url }, "text/html"_s, 0, "UTF-8"_s);
    task.didReceiveResponse(WTF::move(response));
    task.didComplete({ });
}

bool AboutSchemeHandler::canHandleURL(const URL& url) const
{
    return url.protocolIsAbout() && url.hasOpaquePath() && m_handlers.contains<StringViewHashTranslator>(url.path());
}

void AboutSchemeHandler::platformInitialize()
{
#if PLATFORM(COCOA) && HAVE(CUSTOM_ABOUT_SCHEME_HANDLER)
    registerCocoaAboutHandlers(*this);
#endif
}

void AboutSchemeHandler::registerHandler(const String& opaquePath, std::unique_ptr<OpaquePathHandler>&& handler)
{
    auto addResult = m_handlers.set(opaquePath, WTF::move(handler));
    ASSERT_UNUSED(addResult, addResult.isNewEntry);
}

} // namespace WebKit
