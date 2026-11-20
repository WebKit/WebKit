/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc. All rights reserved.
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
#include "WebPageProxy.h"

#include "PageClient.h"
#include "WebKitWebResourceLoadManager.h"
#include "WebPageProxyInternals.h"
#include "WebPreferences.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/SearchPopupMenu.h>
#include <WebCore/UserAgent.h>

#if ENABLE(VIRTUAL_AUDIO_DEVICES)
#include "APIPageConfiguration.h"
#include "Logging.h"
#include "VirtualAudioDeviceDataChannel.h"
#include "VirtualAudioDeviceParameters.h"
#include "VirtualAudioDevicesGLib.h"
#include "WebKitVirtualAudioDevicePrivate.h"
#endif

namespace WebKit {

String WebPageProxy::userAgentForURL(const URL& url)
{
    if (url.isNull() || !preferences().needsSiteSpecificQuirks())
        return this->userAgent();

    auto userAgent = WebCore::standardUserAgentForURL(url);
    return userAgent.isNull() ? this->userAgent() : userAgent;
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    return WebCore::standardUserAgent(applicationNameForUserAgent);
}

void WebPageProxy::saveRecentSearches(IPC::Connection&, const String&, const Vector<WebCore::RecentSearch>&)
{
    notImplemented();
}

void WebPageProxy::loadRecentSearches(IPC::Connection&, const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&& completionHandler)
{
    notImplemented();
    completionHandler({ });
}

void WebPageProxy::didInitiateLoadForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceRequest&& request)
{
    if (auto* manager = pageClient() ? pageClient()->webResourceLoadManager() : nullptr)
        manager->didInitiateLoad(resourceID, frameID, WTF::move(request));
}

void WebPageProxy::didSendRequestForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceRequest&& request, WebCore::ResourceResponse&& redirectResponse)
{
    if (auto* manager = pageClient() ? pageClient()->webResourceLoadManager() : nullptr)
        manager->didSendRequest(resourceID, frameID, WTF::move(request), WTF::move(redirectResponse));
}

void WebPageProxy::didReceiveResponseForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceResponse&& response)
{
    if (auto* manager = pageClient() ? pageClient()->webResourceLoadManager() : nullptr)
        manager->didReceiveResponse(resourceID, frameID, WTF::move(response));
}

void WebPageProxy::didFinishLoadForResource(WebCore::ResourceLoaderIdentifier resourceID, WebCore::FrameIdentifier frameID, WebCore::ResourceError&& error)
{
    if (auto* manager = pageClient() ? pageClient()->webResourceLoadManager() : nullptr)
        manager->didFinishLoad(resourceID, frameID, WTF::move(error));
}

void WebPageProxy::scheduleActivityStateUpdate()
{
    if (internals().activityStateChangeTimer.isActive())
        return;

    internals().activityStateChangeTimer.startOneShot(0_s);
}

WebKitWebView* WebPageProxy::platformView()
{
    return internals().platformView.get();
}

void WebPageProxy::setPlatformView(WebKitWebView *view)
{
    internals().platformView = GWeakPtr(view);
}

#if ENABLE(VIRTUAL_AUDIO_DEVICES)
Vector<VirtualAudioDeviceParameters> WebPageProxy::createVirtualAudioDeviceParameters(WebProcessProxy& process)
{
    // Every call creates new channels, for the web page created in @process. The UI process keeps
    // no copy of the sender sockets: they are only owned by the returned parameters and then by the
    // web page in the web process. Receivers are kept for as long as their sender socket is open,
    // and they are removed when the sender socket is closed: the web page was destroyed, the web
    // process exited, or the parameters were discarded or ignored (for instance on reinitialization).
    //
    // A page can have web pages in several processes at once: the provisional page of a process
    // swap, a page kept in the back/forward cache, the remote pages of site isolation. Each of them
    // renders its own stream on the device, with its own mixer and timestamps, and the application
    // expects a single stream per device: only the audio of the process of the main frame is
    // delivered, the audio of the other processes is dropped. When site isolation is enabled for
    // the GLib ports, the audio of the cross-site frames rendered in other processes will be lost:
    // the streams of all the processes of the page will need to be mixed here instead.
    auto devices = virtualAudioDevices(m_configuration.get());
    if (devices.isEmpty())
        return { };

    Ref preferences = m_preferences;
    if (!preferences->exposeSpeakersEnabled() || !preferences->exposeSpeakersWithoutMicrophoneEnabled())
        RELEASE_LOG_ERROR(Media, "WebPageProxy::createVirtualAudioDeviceParameters: the ExposeSpeakers and ExposeSpeakersWithoutMicrophone features are needed to select virtual audio devices other than the default one");

    Vector<VirtualAudioDeviceParameters> parameters;
    for (auto& [device, configuration] : devices) {
        RefPtr receiver = VirtualAudioDeviceDataReceiver::create(configuration, [weakThis = WeakPtr { *this }, processIdentifier = process.coreProcessIdentifier(), device = device](std::span<const uint8_t> data, uint64_t timestamp) {
            RefPtr protectedThis = weakThis;
            if (!protectedThis || protectedThis->legacyMainFrameProcess().coreProcessIdentifier() != processIdentifier)
                return;
            webkitVirtualAudioDeviceRenderBuffer(device.get(), data, timestamp);
        }, [weakThis = WeakPtr { *this }](VirtualAudioDeviceDataReceiver& receiver) {
            if (RefPtr protectedThis = weakThis) {
                protectedThis->internals().virtualAudioDeviceReceivers.removeFirstMatching([&](auto& item) {
                    return item.ptr() == &receiver;
                });
            }
        });
        if (!receiver)
            continue;

        auto persistentId = virtualAudioDevicePersistentId(identifier(), configuration.name);
        parameters.append({ WTF::move(configuration), WTF::move(persistentId), receiver->takeSenderSocket() });
        internals().virtualAudioDeviceReceivers.append(receiver.releaseNonNull());
    }

    return parameters;
}
#endif

} // namespace WebKit
