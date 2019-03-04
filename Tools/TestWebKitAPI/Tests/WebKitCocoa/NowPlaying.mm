/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if USE(MEDIAREMOTE) && !PLATFORM(IOS_FAMILY_SIMULATOR) && (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101304))

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <pal/spi/mac/MediaRemoteSPI.h>
#import <wtf/Function.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(MediaRemote)
SOFT_LINK(MediaRemote, MRMediaRemoteSetWantsNowPlayingNotifications, void, (bool wantsNotifications), (wantsNotifications))
SOFT_LINK(MediaRemote, MRMediaRemoteGetNowPlayingClient, void, (dispatch_queue_t queue, void(^completion)(MRNowPlayingClientRef, CFErrorRef)), (queue, completion))
SOFT_LINK(MediaRemote, MRNowPlayingClientGetProcessIdentifier, pid_t, (MRNowPlayingClientRef client), (client))
SOFT_LINK_CONSTANT(MediaRemote, kMRMediaRemoteNowPlayingApplicationDidChangeNotification, CFStringRef)
SOFT_LINK_CONSTANT(MediaRemote, kMRMediaRemoteNowPlayingApplicationPIDUserInfoKey, CFStringRef)
#define MRMediaRemoteSetWantsNowPlayingNotifications softLinkMRMediaRemoteSetWantsNowPlayingNotifications
#define MRMediaRemoteGetNowPlayingClient softLinkMRMediaRemoteGetNowPlayingClient
#define MRNowPlayingClientGetProcessIdentifier softLinkMRNowPlayingClientGetProcessIdentifier
#define kMRMediaRemoteNowPlayingApplicationDidChangeNotification getkMRMediaRemoteNowPlayingApplicationDidChangeNotification()
#define kMRMediaRemoteNowPlayingApplicationPIDUserInfoKey getkMRMediaRemoteNowPlayingApplicationPIDUserInfoKey()

static bool userInfoHasNowPlayingApplicationPID(CFDictionaryRef userInfo, int32_t pid)
{
    CFNumberRef nowPlayingPidCF = (CFNumberRef)CFDictionaryGetValue(userInfo, kMRMediaRemoteNowPlayingApplicationPIDUserInfoKey);
    if (!nowPlayingPidCF)
        return false;

    int32_t nowPlayingPid = 0;
    if (!CFNumberGetValue(nowPlayingPidCF, kCFNumberSInt32Type, &nowPlayingPid))
        return false;

    return pid == nowPlayingPid;
}

static RetainPtr<MRNowPlayingClientRef> getNowPlayingClient()
{
    bool gotNowPlaying = false;
    RetainPtr<MRNowPlayingClientRef> nowPlayingClient;
    MRMediaRemoteGetNowPlayingClient(dispatch_get_main_queue(), [&] (MRNowPlayingClientRef player, CFErrorRef error) {
        if (!error && player)
            nowPlayingClient = player;
        gotNowPlaying = true;
    });
    TestWebKitAPI::Util::run(&gotNowPlaying);
    return nowPlayingClient;
}

static pid_t getNowPlayingClientPid()
{
    return MRNowPlayingClientGetProcessIdentifier(getNowPlayingClient().get());
}

class NowPlayingTest : public testing::Test {
public:
    void SetUp() override
    {
        addObserver(*this);

        _configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [_configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeAudio];
#if PLATFORM(IOS_FAMILY)
        [_configuration setAllowsInlineMediaPlayback:YES];
        [_configuration _setInlineMediaPlaybackRequiresPlaysInlineAttribute:NO];
#endif

        _webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:_configuration.get() addToWindow:YES]);
    }

    void TearDown() override
    {
        removeObserver(*this);
    }

    TestWKWebView* webView() { return _webView.get(); }
    WKWebViewConfiguration* configuration() { return _configuration.get(); }

    pid_t webViewPid() { return [_webView _webProcessIdentifier]; }

    void loadPage(const String& name)
    {
        [_webView synchronouslyLoadTestPageNamed:name];
    }

    void runScriptWithUserGesture(const String& script)
    {
        bool complete = false;
        [_webView evaluateJavaScript:script completionHandler:[&] (id, NSError *) { complete = true; }];
        TestWebKitAPI::Util::run(&complete);
    }

    void runScriptWithoutUserGesture(const String& script)
    {
        bool complete = false;
        [_webView _evaluateJavaScriptWithoutUserGesture:script completionHandler:[&] (id, NSError *) { complete = true; }];
        TestWebKitAPI::Util::run(&complete);
    }

    void executeAndWaitForPlaying(Function<void()>&& callback)
    {
        bool isPlaying = false;
        [_webView performAfterReceivingMessage:@"playing" action:[&] { isPlaying = true; }];
        callback();
        TestWebKitAPI::Util::run(&isPlaying);
    }

    void executeAndWaitForWebViewToBecomeNowPlaying(Function<void()>&& callback)
    {
        bool becameNowPlaying = false;
        performAfterReceivingNotification(kMRMediaRemoteNowPlayingApplicationDidChangeNotification, [&] (CFDictionaryRef userInfo) -> bool {
            if (!userInfoHasNowPlayingApplicationPID(userInfo, webViewPid()))
                return false;

            becameNowPlaying = true;
            return true;
        });
        callback();
        TestWebKitAPI::Util::run(&becameNowPlaying);
    }

private:
    using ObserverSet = HashSet<NowPlayingTest*>;
    static ObserverSet& observers()
    {
        static NeverDestroyed<ObserverSet> observers { };
        return observers.get();
    }

    static void addObserver(NowPlayingTest& test)
    {
        observers().add(&test);
        if (observers().size() == 1) {
            CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), nullptr, &notificationCallback, kMRMediaRemoteNowPlayingApplicationDidChangeNotification, nullptr, CFNotificationSuspensionBehaviorDeliverImmediately);
            MRMediaRemoteSetWantsNowPlayingNotifications(true);
        }
    }

    static void removeObserver(NowPlayingTest& test)
    {
        observers().remove(&test);
        if (!observers().size()) {
            CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), nullptr, kMRMediaRemoteNowPlayingApplicationDidChangeNotification, nullptr);
            MRMediaRemoteSetWantsNowPlayingNotifications(false);
        }
    }

    static void notificationCallback(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object, CFDictionaryRef userInfo)
    {
        ObserverSet observersCopy = observers();
        for (auto* observer : observersCopy)
            observer->receivedNotification(name, userInfo);
    }

    void receivedNotification(CFNotificationName name, CFDictionaryRef userInfo)
    {
        auto callbackIter = _callbacks.find(name);
        if (callbackIter == _callbacks.end())
            return;
        auto& callback = callbackIter->value;
        if (callback(userInfo))
            _callbacks.remove(callbackIter);
    }

    using NotificationCallback = Function<bool(CFDictionaryRef)>;
    void performAfterReceivingNotification(CFNotificationName name, NotificationCallback&& callback)
    {
        _callbacks.add(name, WTFMove(callback));
    }

    RetainPtr<WKWebViewConfiguration> _configuration;
    RetainPtr<TestWKWebView> _webView;
    HashMap<CFNotificationName, NotificationCallback> _callbacks;
};

TEST_F(NowPlayingTest, AudioElement)
{
    executeAndWaitForWebViewToBecomeNowPlaying([&] {
        executeAndWaitForPlaying([&] {
            loadPage("now-playing");
            runScriptWithoutUserGesture("createMediaElement({type:'audio', hasAudio:true})");
            runScriptWithUserGesture("play()");
        });
    });
}

TEST_F(NowPlayingTest, VideoElementWithAudio)
{
    executeAndWaitForWebViewToBecomeNowPlaying([&] {
        executeAndWaitForPlaying([&] {
            loadPage("now-playing");
            runScriptWithoutUserGesture("createMediaElement({type:'video', hasAudio:true})");
            runScriptWithUserGesture("play()");
        });
    });
}

TEST_F(NowPlayingTest, VideoElementWithoutAudio)
{
    executeAndWaitForPlaying([&] {
        loadPage("now-playing");
        runScriptWithoutUserGesture("createMediaElement({type:'video', hasAudio:false})");
        runScriptWithoutUserGesture("play()");
    });

    ASSERT_NE(webViewPid(), getNowPlayingClientPid());
}

TEST_F(NowPlayingTest, VideoElementWithMutedAudio)
{
    executeAndWaitForPlaying([&] {
        loadPage("now-playing");
        runScriptWithoutUserGesture("createMediaElement({type:'video', hasAudio:true, muted:true})");
        runScriptWithoutUserGesture("play()");
    });
    ASSERT_NE(webViewPid(), getNowPlayingClientPid());
}

TEST_F(NowPlayingTest, VideoElementWithMutedAudioUnmutedWithUserGesture)
{
    executeAndWaitForPlaying([&] {
        loadPage("now-playing");
        runScriptWithoutUserGesture("createMediaElement({type:'video', hasAudio:true, muted:true})");
        runScriptWithoutUserGesture("play()");
    });
    ASSERT_NE(webViewPid(), getNowPlayingClientPid());

    executeAndWaitForWebViewToBecomeNowPlaying([&] {
        runScriptWithUserGesture("unmute()");
    });
}

TEST_F(NowPlayingTest, VideoElementWithoutAudioPlayWithUserGesture)
{
    executeAndWaitForPlaying([&] {
        loadPage("now-playing");
        runScriptWithoutUserGesture("createMediaElement({type:'video', hasAudio:true, muted:true})");
        runScriptWithoutUserGesture("play()");
    });
    ASSERT_NE(webViewPid(), getNowPlayingClientPid());

    executeAndWaitForPlaying([&] {
        runScriptWithUserGesture("play()");
    });

    ASSERT_NE(webViewPid(), getNowPlayingClientPid());
}

#endif // USE(MEDIAREMOTE) && (PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && __MAC_OS_X_VERSION_MAX_ALLOWED >= 101304))
