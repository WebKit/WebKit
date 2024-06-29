/*
 * Copyright (C) 2022 Igalia S.L.
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
#include "NotificationResourcesLoader.h"

#if ENABLE(NOTIFICATIONS)

#include "BitmapImage.h"
#include "GraphicsContext.h"
#include "NotificationResources.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include <wtf/URL.h>

namespace WebCore {

// 2.5. Resources
// https://notifications.spec.whatwg.org/#resources

NotificationResourcesLoader::NotificationResourcesLoader(Notification& notification)
    : m_notification(notification)
{
}

bool NotificationResourcesLoader::resourceIsSupportedInPlatform(Resource resource)
{
    switch (resource) {
    case Resource::Icon:
#if PLATFORM(GTK) || PLATFORM(WPE)
        return true;
#else
        return false;
#endif
    case Resource::Image:
    case Resource::Badge:
    case Resource::ActionIcon:
        // FIXME: Implement other resources.
        return false;
    }

    ASSERT_NOT_REACHED();
    return false;
}

void NotificationResourcesLoader::start(CompletionHandler<void(RefPtr<NotificationResources>&&)>&& completionHandler)
{
    m_completionHandler = WTFMove(completionHandler);

    // If the notification platform supports icons, fetch notification’s icon URL, if icon URL is set.
    if (resourceIsSupportedInPlatform(Resource::Icon)) {
        const URL& iconURL = m_notification.icon();
        if (!iconURL.isEmpty()) {
            auto loader = makeUnique<ResourceLoader>(*m_notification.scriptExecutionContext(), iconURL, [this](ResourceLoader* loader, RefPtr<BitmapImage>&& image) {
                if (m_stopped)
                    return;

                if (image) {
                    if (!m_resources)
                        m_resources = NotificationResources::create();
                    m_resources->setIcon(WTFMove(image));
                }

                didFinishLoadingResource(loader);
            });

            if (!loader->finished())
                m_loaders.add(WTFMove(loader));
        }
    }

    // FIXME: Implement other resources.

    if (m_loaders.isEmpty())
        m_completionHandler(WTFMove(m_resources));
}

void NotificationResourcesLoader::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;

    auto completionHandler = std::exchange(m_completionHandler, nullptr);

    while (!m_loaders.isEmpty()) {
        auto loader = m_loaders.takeAny();
        loader->cancel();
    }

    if (completionHandler)
        completionHandler(nullptr);
}

void NotificationResourcesLoader::didFinishLoadingResource(ResourceLoader* loader)
{
    if (m_loaders.contains(loader)) {
        m_loaders.remove(loader);
        if (m_loaders.isEmpty() && m_completionHandler)
            m_completionHandler(WTFMove(m_resources));
    }
}

NotificationResourcesLoader::ResourceLoader::ResourceLoader(ScriptExecutionContext& context, const URL& url, CompletionHandler<void(ResourceLoader*, RefPtr<BitmapImage>&&)>&& completionHandler)
    : m_completionHandler(WTFMove(completionHandler))
{
    ThreadableLoaderOptions options;
    options.mode = FetchOptions::Mode::Cors;
    options.sendLoadCallbacks = SendCallbackPolicy::SendCallbacks;
    options.dataBufferingPolicy = DataBufferingPolicy::DoNotBufferData;
    options.contentSecurityPolicyEnforcement = context.shouldBypassMainWorldContentSecurityPolicy() ? ContentSecurityPolicyEnforcement::DoNotEnforce : ContentSecurityPolicyEnforcement::EnforceConnectSrcDirective;
    m_loader = ThreadableLoader::create(context, *this, ResourceRequest(url), options);
}

NotificationResourcesLoader::ResourceLoader::~ResourceLoader()
{
}

void NotificationResourcesLoader::ResourceLoader::cancel()
{
    auto completionHandler = std::exchange(m_completionHandler, nullptr);
    m_loader->cancel();
    m_loader = nullptr;
    if (completionHandler)
        completionHandler(this, nullptr);
}

void NotificationResourcesLoader::ResourceLoader::didReceiveResponse(ScriptExecutionContextIdentifier, ResourceLoaderIdentifier, const ResourceResponse& response)
{
    // If the response's internal response's type is "default", then attempt to decode the resource as image.
    if (response.type() == ResourceResponse::Type::Default)
        m_image = BitmapImage::create();
}

void NotificationResourcesLoader::ResourceLoader::didReceiveData(const SharedBuffer& buffer)
{
    if (m_image) {
        m_buffer.append(buffer);
        m_image->setData(m_buffer.get(), false);
    }
}

void NotificationResourcesLoader::ResourceLoader::didFinishLoading(ScriptExecutionContextIdentifier, ResourceLoaderIdentifier, const NetworkLoadMetrics&)
{
    m_finished = true;

    if (m_image)
        m_image->setData(m_buffer.take(), true);

    if (m_completionHandler)
        m_completionHandler(this, WTFMove(m_image));
}

void NotificationResourcesLoader::ResourceLoader::didFail(ScriptExecutionContextIdentifier, const ResourceError&)
{
    m_finished = true;

    if (m_completionHandler)
        m_completionHandler(this, nullptr);
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATIONS)
