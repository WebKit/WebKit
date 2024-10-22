/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"

#if PLATFORM(MAC) && ENABLE(MEDIA_SESSION)

#import "PlatformUtilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WebViewPrivate.h>
#import <pal/spi/mac/MediaRemoteSPI.h>
#import <wtf/Function.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(MediaRemote)

SOFT_LINK(MediaRemote, MRMediaRemoteSendCommandToApp, Boolean, (MRMediaRemoteCommand command, CFDictionaryRef options, MROriginRef origin, CFStringRef appDisplayID, MRSendCommandAppOptions appOptions, dispatch_queue_t replyQ, void(^completion)(MRSendCommandError err, CFArrayRef handlerReturnStatuses)), (command, options, origin, appDisplayID, appOptions, replyQ, completion));
#define MRMediaRemoteSendCommandToApp softLinkMRMediaRemoteSendCommandToApp

SOFT_LINK(MediaRemote, MRMediaRemoteGetLocalOrigin, MROriginRef, (), ());
#define MRMediaRemoteGetLocalOrigin softLinkMRMediaRemoteGetLocalOrigin

SOFT_LINK(MediaRemote, MRMediaRemoteGetSupportedCommandsForOrigin, void, (MROriginRef origin, dispatch_queue_t queue, void(^completion)(CFArrayRef commands)), (origin, queue, completion));
#define MRMediaRemoteGetSupportedCommandsForOrigin softLinkMRMediaRemoteGetSupportedCommandsForOrigin

SOFT_LINK(MediaRemote, MRMediaRemoteGetNowPlayingClient, void, (dispatch_queue_t queue, void(^completion)(MRNowPlayingClientRef, CFErrorRef)), (queue, completion))
#define MRMediaRemoteGetNowPlayingClient softLinkMRMediaRemoteGetNowPlayingClient

SOFT_LINK(MediaRemote, MRNowPlayingClientGetProcessIdentifier, pid_t, (MRNowPlayingClientRef client), (client))
#define MRNowPlayingClientGetProcessIdentifier softLinkMRNowPlayingClientGetProcessIdentifier

SOFT_LINK_CONSTANT(MediaRemote, kMRMediaRemoteOptionSkipInterval, CFStringRef)
#define kMRMediaRemoteOptionSkipInterval getkMRMediaRemoteOptionSkipInterval()

SOFT_LINK_CONSTANT(MediaRemote, kMRMediaRemoteOptionPlaybackPosition, CFStringRef)
#define kMRMediaRemoteOptionPlaybackPosition getkMRMediaRemoteOptionPlaybackPosition()

#if !USE(APPLE_INTERNAL_SDK)
@interface MRCommandInfo : NSObject
@property (nonatomic, readonly) MRMediaRemoteCommand command;
@property (nonatomic, readonly, getter=isEnabled) BOOL enabled;
@property (nonatomic, readonly, copy) NSDictionary *options;
@end
#endif

namespace TestWebKitAPI {

class MediaSessionTest : public testing::Test {
public:
    void SetUp() final
    {
        _configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [_configuration setMediaTypesRequiringUserActionForPlayback:WKAudiovisualMediaTypeAudio];

        _webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 300) configuration:_configuration.get() addToWindow:YES]);

        WKPreferences *preferences = [_webView configuration].preferences;
        preferences._mediaSessionEnabled = YES;

        _messageHandlers = adoptNS([[NSMutableArray alloc] init]);
    }

    void TearDown() override
    {
        [_webView clearMessageHandlers:_messageHandlers.get()];
    }

    TestWKWebView* webView() { return _webView.get(); }

    pid_t gpuProcessPID() { return [_webView _gpuProcessIdentifier]; }

    RetainPtr<MRNowPlayingClientRef> getNowPlayingClient()
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

    pid_t getNowPlayingClientPid()
    {
        return MRNowPlayingClientGetProcessIdentifier(getNowPlayingClient().get());
    }

    void loadPageAndBecomeNowPlaying(NSString *pageName)
    {
        [_webView synchronouslyLoadTestPageNamed:pageName];

        bool canplaythrough = false;
        [webView() performAfterReceivingMessage:@"canplaythrough event" action:[&] {
            canplaythrough = true;
        }];
        runScriptWithUserGesture(@"load()");
        Util::run(&canplaythrough);

        play();
        pause();
        ASSERT_EQ(gpuProcessPID(), getNowPlayingClientPid());
    }

    void runScriptWithUserGesture(NSString *script)
    {
        bool complete = false;
        [_webView evaluateJavaScript:script completionHandler:[&] (id, NSError *) { complete = true; }];
        TestWebKitAPI::Util::run(&complete);
    }

    void play()
    {
        bool playing = false;
        [_webView performAfterReceivingMessage:@"play event" action:[&] { playing = true; }];
        runScriptWithUserGesture(@"audio.play()");
        Util::run(&playing);
    }

    void pause()
    {
        bool paused = false;
        [_webView performAfterReceivingMessage:@"pause event" action:[&] { paused = true; }];
        runScriptWithUserGesture(@"audio.pause()");
        Util::run(&paused);
    }

    bool sendMediaRemoteCommand(MRMediaRemoteCommand command, CFDictionaryRef options = nullptr)
    {
        bool completed = false;
        bool success;

        MRMediaRemoteSendCommandToApp(command, options, NULL, NULL, static_cast<MRSendCommandAppOptions>(0), NULL, [&] (MRSendCommandError error, CFArrayRef) {
            success = !error;
            completed = true;
        });
        TestWebKitAPI::Util::run(&completed);

        return success;
    }

    bool sendMediaRemoteSeekCommand(MRMediaRemoteCommand command, double interval)
    {
        CFStringRef seekInterval = (command == MRMediaRemoteCommandSeekToPlaybackPosition) ? kMRMediaRemoteOptionPlaybackPosition : kMRMediaRemoteOptionSkipInterval;
        NSDictionary *options = @{(__bridge NSString *)seekInterval : @(interval)};
        return sendMediaRemoteCommand(command, (__bridge CFDictionaryRef)options);
    }

    void listenForEventMessages(std::initializer_list<ASCIILiteral> events)
    {
        for (auto event : events) {
            auto eventMessage = makeString(event, " event"_s);
            [_messageHandlers addObject:eventMessage];
            [webView() performAfterReceivingMessage:eventMessage action:[this, eventMessage = WTFMove(eventMessage)] {
                _eventListenersCalled.add(eventMessage);
            }];
        }
    }

    bool eventListenerWasCalled(StringView event)
    {
        return _eventListenersCalled.contains(makeString(event, " event"_s));
    }

    void clearEventListenerState()
    {
        _eventListenersCalled.clear();
    }

    void waitForEventListenerToBeCalled(StringView event)
    {
        int tries = 0;
        do {
            if (eventListenerWasCalled(event))
                return;
            Util::runFor(0.1_s);
        } while (++tries <= 50);

        return;
    }

    void listenForSessionHandlerMessages(std::initializer_list<ASCIILiteral> handlers)
    {
        for (auto handler : handlers) {
            auto handlerMessage = makeString(handler, " handler"_s);
            [_messageHandlers addObject:handlerMessage];
            [webView() performAfterReceivingMessage:handlerMessage action:[this, handlerMessage = WTFMove(handlerMessage)] {
                _mediaSessionHandlersCalled.add(handlerMessage);
            }];
        }
    }

    bool sessionHandlerWasCalled(StringView handler)
    {
        return _mediaSessionHandlersCalled.contains(makeString(handler, " handler"_s));
    }

    void waitForSessionHandlerToBeCalled(StringView handler)
    {
        int tries = 0;
        do {
            if (sessionHandlerWasCalled(handler))
                return;
            Util::runFor(0.1_s);
        } while (++tries <= 50);

        return;
    }

    RetainPtr<NSArray> getSupportedCommands()
    {
        bool completed = false;
        RetainPtr<NSArray> result;

        MRMediaRemoteGetSupportedCommandsForOrigin(MRMediaRemoteGetLocalOrigin(), dispatch_get_main_queue(), [&] (CFArrayRef commands) {
            result = (__bridge NSArray *)commands;
            completed = true;
        });

        TestWebKitAPI::Util::run(&completed);

        return result;
    }

private:
    RetainPtr<WKWebViewConfiguration> _configuration;
    RetainPtr<TestWKWebView> _webView;

    HashSet<String> _mediaSessionHandlersCalled;
    HashSet<String> _eventListenersCalled;
    RetainPtr<NSMutableArray> _messageHandlers;
};

TEST_F(MediaSessionTest, DISABLED_OnlyOneHandler)
{
    loadPageAndBecomeNowPlaying(@"media-remote");

    [webView() objectByEvaluatingJavaScript:@"setEmptyActionHandlers([ 'play' ])"];

    listenForSessionHandlerMessages({ "play"_s, "pause"_s, "seekto"_s, "seekforward"_s, "seekbackward"_s, "previoustrack"_s, "nexttrack"_s });
    listenForEventMessages({ "play"_s, "pause"_s, "seeked"_s });

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101500
    static Vector<MRMediaRemoteCommand> registeredCommands = { MRMediaRemoteCommandPlay };
    auto currentCommands = getSupportedCommands();
    for (MRCommandInfo *command in currentCommands.get()) {
        if (!command.enabled)
            continue;
        
        ASSERT_TRUE(registeredCommands.contains(command.command));
    }
#endif

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPlay));
    waitForSessionHandlerToBeCalled("play"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("play"_s));
    ASSERT_FALSE(eventListenerWasCalled("play"_s));

    // The media session only registered for Play, but no other commands should reach HTMLMediaElement.
    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipForward, 1));
    ASSERT_FALSE(sessionHandlerWasCalled("seekforward"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipBackward, 10));
    ASSERT_FALSE(sessionHandlerWasCalled("seekbackward"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSeekToPlaybackPosition, 6));
    ASSERT_FALSE(sessionHandlerWasCalled("seekto"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandNextTrack));
    ASSERT_FALSE(sessionHandlerWasCalled("nexttrack"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPreviousTrack));
    ASSERT_FALSE(sessionHandlerWasCalled("previoustrack"_s));
}

TEST_F(MediaSessionTest, DISABLED_RemoteCommands)
{
    loadPageAndBecomeNowPlaying(@"media-remote");

    [webView() objectByEvaluatingJavaScript:@"setEmptyActionHandlers([ 'play', 'pause', 'seekto', 'seekforward', 'seekbackward', 'previoustrack', 'nexttrack' ])"];

    listenForSessionHandlerMessages({ "play"_s, "pause"_s, "seekto"_s, "seekforward"_s, "seekbackward"_s, "previoustrack"_s, "nexttrack"_s });
    listenForEventMessages({ "play"_s, "pause"_s, "seeked"_s });

#if __MAC_OS_X_VERSION_MIN_REQUIRED > 101500
    static Vector<MRMediaRemoteCommand> registeredCommands = { MRMediaRemoteCommandPlay, MRMediaRemoteCommandPause, MRMediaRemoteCommandSeekToPlaybackPosition, MRMediaRemoteCommandSkipForward, MRMediaRemoteCommandSkipBackward, MRMediaRemoteCommandPreviousTrack, MRMediaRemoteCommandNextTrack };
    auto currentCommands = getSupportedCommands();
    for (MRCommandInfo *command in currentCommands.get()) {
        if (!command.enabled)
            continue;
        
        ASSERT_TRUE(registeredCommands.contains(command.command));
    }
#endif

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPlay));
    waitForSessionHandlerToBeCalled("play"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("play"_s));
    ASSERT_FALSE(eventListenerWasCalled("play"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPause));
    waitForSessionHandlerToBeCalled("pause"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("pause"_s));
    ASSERT_FALSE(eventListenerWasCalled("pause"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipForward, 1));
    waitForSessionHandlerToBeCalled("seekforward"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("seekforward"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipBackward, 10));
    waitForSessionHandlerToBeCalled("seekbackward"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("seekbackward"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSeekToPlaybackPosition, 6));
    waitForSessionHandlerToBeCalled("seekto"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("seekto"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandNextTrack));
    waitForSessionHandlerToBeCalled("nexttrack"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("nexttrack"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPreviousTrack));
    waitForSessionHandlerToBeCalled("previoustrack"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("previoustrack"_s));

    // Unregister action handlers, supported commands should go to HTMLMediaElement.
    [webView() objectByEvaluatingJavaScript:@"clearActionHandlers()"];
    clearEventListenerState();

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPlay));
    waitForEventListenerToBeCalled("play"_s);
    ASSERT_TRUE(eventListenerWasCalled("play"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPause));
    waitForEventListenerToBeCalled("pause"_s);
    ASSERT_TRUE(eventListenerWasCalled("pause"_s));

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipForward, 1));
    waitForEventListenerToBeCalled("seeked"_s);
    ASSERT_TRUE(eventListenerWasCalled("seeked"_s));
    clearEventListenerState();

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipBackward, 10));
    waitForEventListenerToBeCalled("seeked"_s);
    ASSERT_TRUE(eventListenerWasCalled("seeked"_s));
    clearEventListenerState();

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSeekToPlaybackPosition, 6));
    waitForEventListenerToBeCalled("seeked"_s);
    ASSERT_TRUE(eventListenerWasCalled("seeked"_s));
}

TEST_F(MediaSessionTest, MinimalCommands)
{
    loadPageAndBecomeNowPlaying(@"media-remote");

    [webView() objectByEvaluatingJavaScript:@"setEmptyActionHandlers([ 'seekforward' ])"];

    listenForSessionHandlerMessages({ "seekforward"_s });
    listenForEventMessages({ "play"_s, "pause"_s, "seeked"_s });

    ASSERT_TRUE(sendMediaRemoteSeekCommand(MRMediaRemoteCommandSkipForward, 1));
    waitForSessionHandlerToBeCalled("seekforward"_s);
    ASSERT_TRUE(sessionHandlerWasCalled("seekforward"_s));
    ASSERT_FALSE(eventListenerWasCalled("seeked"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPlay));
    waitForEventListenerToBeCalled("play"_s);
    ASSERT_TRUE(eventListenerWasCalled("play"_s));
    ASSERT_FALSE(sessionHandlerWasCalled("play"_s));

    ASSERT_TRUE(sendMediaRemoteCommand(MRMediaRemoteCommandPause));
    waitForEventListenerToBeCalled("pause"_s);
    ASSERT_TRUE(eventListenerWasCalled("pause"_s));
    ASSERT_FALSE(sessionHandlerWasCalled("pause"_s));

    Vector<MRMediaRemoteCommand> expectedCommands {
        MRMediaRemoteCommandPlay,
        MRMediaRemoteCommandPause,
        MRMediaRemoteCommandSkipForward,
    };
    std::sort(expectedCommands.begin(), expectedCommands.end());

    Vector actualCommands = makeVector(getSupportedCommands().get(), [] (MRCommandInfo *command) -> std::optional<MRMediaRemoteCommand> {
        if (!command.enabled)
            return std::nullopt;
        return command.command;
    });
    std::sort(actualCommands.begin(), actualCommands.end());

    EXPECT_EQ(expectedCommands, actualCommands);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(MAC) && ENABLE(MEDIA_SESSION)
