/*
 * Copyright (C) 2009, 2012, 2014 Apple Inc. All rights reserved.
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

#import "WebGeolocationClient.h"

#if ENABLE(GEOLOCATION)

#import "WebDelegateImplementationCaching.h"
#import "WebFrameInternal.h"
#import "WebGeolocationPositionInternal.h"
#import "WebSecurityOriginInternal.h"
#import "WebUIDelegatePrivate.h"
#import "WebViewInternal.h"
#import <WebCore/Document.h>
#import <WebCore/Frame.h>
#import <WebCore/Geolocation.h>
#import <wtf/BlockObjCExceptions.h>

#if PLATFORM(IOS_FAMILY)
#import <WebCore/WAKResponder.h>
#import <WebKitLegacy/WebCoreThreadRun.h>
#endif

using namespace WebCore;

#if !PLATFORM(IOS_FAMILY)
@interface WebGeolocationPolicyListener : NSObject <WebAllowDenyPolicyListener>
{
    RefPtr<Geolocation> _geolocation;
}
- (id)initWithGeolocation:(Geolocation&)geolocation;
@end
#else
@interface WebGeolocationPolicyListener : NSObject <WebAllowDenyPolicyListener>
{
    RefPtr<Geolocation> _geolocation;
    RetainPtr<WebView> _webView;
}
- (id)initWithGeolocation:(Geolocation*)geolocation forWebView:(WebView*)webView;
@end
#endif

#if PLATFORM(IOS_FAMILY)
@interface WebGeolocationProviderInitializationListener : NSObject <WebGeolocationProviderInitializationListener> {
@private
    RefPtr<Geolocation> m_geolocation;
}
- (id)initWithGeolocation:(Geolocation&)geolocation;
@end
#endif

WebGeolocationClient::WebGeolocationClient(WebView *webView)
    : m_webView(webView)
{
}

void WebGeolocationClient::geolocationDestroyed()
{
    delete this;
}

void WebGeolocationClient::startUpdating()
{
    [[m_webView _geolocationProvider] registerWebView:m_webView];
}

void WebGeolocationClient::stopUpdating()
{
    [[m_webView _geolocationProvider] unregisterWebView:m_webView];
}

#if PLATFORM(IOS_FAMILY)
void WebGeolocationClient::setEnableHighAccuracy(bool wantsHighAccuracy)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    [[m_webView _geolocationProvider] setEnableHighAccuracy:wantsHighAccuracy];
    END_BLOCK_OBJC_EXCEPTIONS;
}
#endif

void WebGeolocationClient::requestPermission(Geolocation& geolocation)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    SEL selector = @selector(webView:decidePolicyForGeolocationRequestFromOrigin:frame:listener:);
    if (![[m_webView UIDelegate] respondsToSelector:selector]) {
        geolocation.setIsAllowed(false);
        return;
    }

#if !PLATFORM(IOS_FAMILY)
    Frame *frame = geolocation.frame();

    if (!frame) {
        geolocation.setIsAllowed(false);
        return;
    }

    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:&frame->document()->securityOrigin()];
    WebGeolocationPolicyListener* listener = [[WebGeolocationPolicyListener alloc] initWithGeolocation:geolocation];

    CallUIDelegate(m_webView, selector, webOrigin, kit(frame), listener);

    [webOrigin release];
    [listener release];
#else
    RetainPtr<WebGeolocationProviderInitializationListener> listener = adoptNS([[WebGeolocationProviderInitializationListener alloc] initWithGeolocation:geolocation]);
    [[m_webView _geolocationProvider] initializeGeolocationForWebView:m_webView listener:listener.get()];
#endif
    END_BLOCK_OBJC_EXCEPTIONS;
}

Optional<GeolocationPosition> WebGeolocationClient::lastPosition()
{
    return core([[m_webView _geolocationProvider] lastPosition]);
}

#if !PLATFORM(IOS_FAMILY)
@implementation WebGeolocationPolicyListener

- (id)initWithGeolocation:(Geolocation&)geolocation
{
    if (!(self = [super init]))
        return nil;
    _geolocation = &geolocation;
    return self;
}

- (void)allow
{
    _geolocation->setIsAllowed(true);
}

- (void)deny
{
    _geolocation->setIsAllowed(false);
}

@end

#else
@implementation WebGeolocationPolicyListener
- (id)initWithGeolocation:(Geolocation*)geolocation forWebView:(WebView*)webView
{
    self = [super init];
    if (!self)
        return nil;
    _geolocation = geolocation;
    _webView = webView;
    return self;
}

- (void)allow
{
#if USE(WEB_THREAD)
    WebThreadRun(^{
        _geolocation->setIsAllowed(true);
    });
#else
    _geolocation->setIsAllowed(true);
#endif
}

- (void)deny
{
#if USE(WEB_THREAD)
    WebThreadRun(^{
        _geolocation->setIsAllowed(false);
    });
#else
    _geolocation->setIsAllowed(false);
#endif
}

- (void)denyOnlyThisRequest
{
#if USE(WEB_THREAD)
    WebThreadRun(^{
#endif
        // A soft deny does not prevent subsequent request from the Geolocation object.
        [self deny];
        _geolocation->resetIsAllowed();
#if USE(WEB_THREAD)
    });
#endif
}

- (BOOL)shouldClearCache
{
    // Theoretically, WebView could changes the WebPreferences after we get the pointer.
    // We lock to be on the safe side.
    WebThreadLock();

    return [[_webView.get() preferences] _alwaysRequestGeolocationPermission];
}
@end

@implementation WebGeolocationProviderInitializationListener
- (id)initWithGeolocation:(Geolocation&)geolocation
{
    self = [super init];
    if (self)
        m_geolocation = &geolocation;
    return self;
}

- (void)initializationAllowedWebView:(WebView *)webView
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    Frame* frame = m_geolocation->frame();
    if (!frame)
        return;
    WebSecurityOrigin *webOrigin = [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:&frame->document()->securityOrigin()];
    WebGeolocationPolicyListener *listener = [[WebGeolocationPolicyListener alloc] initWithGeolocation:m_geolocation.get() forWebView:webView];
    SEL selector = @selector(webView:decidePolicyForGeolocationRequestFromOrigin:frame:listener:);
    CallUIDelegate(webView, selector, webOrigin, kit(frame), listener);
    [webOrigin release];
    [listener release];

    END_BLOCK_OBJC_EXCEPTIONS;
}

- (void)initializationDeniedWebView:(WebView *)webView
{
    m_geolocation->setIsAllowed(false);
}
@end
#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(GEOLOCATION)
