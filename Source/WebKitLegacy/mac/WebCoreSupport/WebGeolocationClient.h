/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#import <WebCore/GeolocationClient.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/TZoneMalloc.h>

namespace WebCore {
class Geolocation;
class GeolocationPositionData;
}

@class WebView;

class WebGeolocationClient : public WebCore::GeolocationClient, public RefCounted<WebGeolocationClient> {
    WTF_MAKE_TZONE_ALLOCATED(WebGeolocationClient);
public:
    static Ref<WebGeolocationClient> create(WebView * webView) { return adoptRef(*new WebGeolocationClient(webView)); }

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    WebView *webView() { return m_webView; }

    void geolocationDestroyed() final { }
    void startUpdating(const String& authorizationToken, bool enableHighAccuracy) override;
    void stopUpdating() override;
#if PLATFORM(IOS_FAMILY)
    // FIXME: unify this with Mac on OpenSource.
    void setEnableHighAccuracy(bool) override;
#else
    void setEnableHighAccuracy(bool) override { }
#endif

    std::optional<WebCore::GeolocationPositionData> lastPosition() override;

    void requestPermission(WebCore::Geolocation&) override;
    void cancelPermissionRequest(WebCore::Geolocation&) override { };

private:
    explicit WebGeolocationClient(WebView *);

    WebView *m_webView;
};
