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

#import "config.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR)

#import "PlatformUtilities.h"
#import "TestWKWebView.h"
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKProcessPoolPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/WKWebViewPrivateForTesting.h>
#import <WebKit/_WKFeature.h>
#import <WebKit/_WKProcessPoolConfiguration.h>
#import <wtf/BlockPtr.h>
#import <wtf/HashSet.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringHash.h>

@interface _WKMockMediaSessionCoordinator : NSObject <_WKMediaSessionCoordinator>
@property (nonatomic, readonly) NSString *lastStateChange;
@property (nonatomic, readonly) NSString *lastMethodCalled;
@property (nonatomic) BOOL failsCommands;

- (void)seekSessionToTime:(double)time;
- (void)playSession;
- (void)pauseSession;
- (void)setSessionTrack:(NSString*)trackIdentifier;
@end

@implementation _WKMockMediaSessionCoordinator {
    RetainPtr<NSString> _lastStateChange;
    RetainPtr<NSString> _lastMethodCalled;
    id <_WKMediaSessionCoordinatorDelegate> _delegate;
}

@synthesize delegate;

- (NSString *)lastStateChange
{
    return std::exchange(_lastStateChange, @"").get();
}

- (NSString *)lastMethodCalled
{
    return std::exchange(_lastMethodCalled, @"").get();
}

- (NSString *)identifier
{
    return @"TestCoordinator";
}

- (void)joinWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler
{
    _lastMethodCalled = @"join";
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        completionHandler(!_failsCommands);
    });
}

- (void)leave
{
    _lastMethodCalled = @"leave";
}

- (void)seekTo:(double)time withCompletion:(void(^ _Nonnull)(BOOL))completionHandler
{
    _lastMethodCalled = @"seekTo";
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        completionHandler(!_failsCommands);
    });
}

- (void)playWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler
{
    _lastMethodCalled = @"play";
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        completionHandler(!_failsCommands);
    });
}

- (void)pauseWithCompletion:(void(^ _Nonnull)(BOOL))completionHandler
{
    _lastMethodCalled = @"pause";
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        completionHandler(!_failsCommands);
    });
}

- (void)setTrack:(NSString*)trackIdentifier withCompletion:(void(^ _Nonnull)(BOOL))completionHandler
{
    _lastMethodCalled = @"setTrack";
    dispatch_async(mainDispatchQueueSingleton(), ^() {
        completionHandler(!_failsCommands);
    });
}

- (void)positionStateChanged:(nullable _WKMediaPositionState *)state
{
    _lastStateChange = @"positionStateChanged";
}

- (void)readyStateChanged:(_WKMediaSessionReadyState)state
{
    _lastStateChange = @"readyStateChanged";
}

- (void)playbackStateChanged:(_WKMediaSessionPlaybackState)state
{
    _lastStateChange = @"playbackStateChanged";
}

- (void)coordinatorStateChanged:(_WKMediaSessionCoordinatorState)state
{
    _lastStateChange = @"coordinatorStateChanged";
}

- (void)trackIdentifierChanged:(NSString *)trackIdentifier
{
    _lastStateChange = @"trackIdentifierChanged";
}

- (void)seekSessionToTime:(double)time
{
    [self.delegate seekSessionToTime:time withCompletion:^(BOOL result) {
        _lastMethodCalled = @"seekSessionToTime";
    }];
}

- (void)playSession
{
    [self.delegate playSessionWithCompletion:^(BOOL result) {
        _lastMethodCalled = @"playSession";
    }];
}

- (void)pauseSession
{
    [self.delegate pauseSessionWithCompletion:^(BOOL result) {
        _lastMethodCalled = @"pauseSession";
    }];
}

- (void)setSessionTrack:(NSString*)trackIdentifier
{
    [self.delegate setSessionTrack:trackIdentifier withCompletion:^(BOOL result) {
        _lastMethodCalled = @"setSessionTrack";
    }];
}

- (void)sessionStateChanged:(_WKMediaSessionCoordinatorState)state
{
    [self.delegate coordinatorStateChanged:state];
}

@end

namespace TestWebKitAPI {

class MediaSessionCoordinatorTest : public testing::Test {
public:
    void SetUp() final
    {
        auto configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        auto preferences = [configuration preferences];

        for (_WKFeature *feature in [WKPreferences _features]) {
            if ([feature.key isEqualToString:@"MediaSessionCoordinatorEnabled"])
                [preferences _setEnabled:YES forFeature:feature];
            if ([feature.key isEqualToString:@"MediaSessionEnabled"])
                [preferences _setEnabled:YES forFeature:feature];
        }

        _webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:configuration.get()]);
    }

    void TearDown() override
    {
        [_webView clearMessageHandlers:_messageHandlers.get()];
    }

    void createCoordinator()
    {
        _coordinator = [[_WKMockMediaSessionCoordinator alloc] init];

        __block bool result = false;
        __block bool done = false;
        [webView() _createMediaSessionCoordinatorForTesting:(id <_WKMediaSessionCoordinator>)_coordinator.get() completionHandler:^(BOOL success) {
            result = success;
            done = true;
        }];
        TestWebKitAPI::Util::run(&done);

        listenForEventMessages({ "coordinatorstatechange"_s });

        EXPECT_TRUE(result);
        if (!result)
            NSLog(@"-[_createMediaSessionCoordinatorForTesting:completionHandler:] failed!");

        waitForEventListenerToBeCalled("coordinatorstatechange"_s);
        ASSERT_TRUE(eventListenerWasCalled("coordinatorstatechange"_s));
    }

    TestWKWebView* webView() const { return _webView.get(); }
    _WKMockMediaSessionCoordinator* coordinator() const { return _coordinator.get(); }

    void loadPageAndBecomeReady(const String& pageName)
    {
        [_webView synchronouslyLoadTestPageNamed:pageName.createNSString().get()];

        bool canplaythrough = false;
        [webView() performAfterReceivingMessage:@"canplaythrough event" action:[&] {
            canplaythrough = true;
        }];
        runScriptWithUserGesture("load()"_s);
        Util::run(&canplaythrough);
    }

    void runScriptWithUserGesture(const String& script)
    {
        [_webView objectByEvaluatingJavaScriptWithUserGesture:script.createNSString().get()];
    }

    void play()
    {
        bool playing = false;
        [_webView performAfterReceivingMessage:@"play event" action:[&] { playing = true; }];
        runScriptWithUserGesture("audio.play()"_s);
        Util::run(&playing);
    }

    void pause()
    {
        bool paused = false;
        [_webView performAfterReceivingMessage:@"pause event" action:[&] { paused = true; }];
        runScriptWithUserGesture("audio.pause()"_s);
        Util::run(&paused);
    }

    void listenForEventMessages(std::initializer_list<ASCIILiteral> events)
    {
        for (auto event : events) {
            auto eventMessage = makeString(event, " event"_s);
            [webView() performAfterReceivingMessage:eventMessage.createNSString().get() action:[this, eventMessage = WTFMove(eventMessage)] {
                _eventListenersCalled.add(eventMessage);
            }];
        }
    }

    bool eventListenerWasCalled(const String& event)
    {
        return _eventListenersCalled.contains(makeString(event, " event"_s));
    }

    void clearEventListenerState()
    {
        _eventListenersCalled.clear();
    }

    void executeUntil(Function<bool()>&& callback, int retries = 50)
    {
        int tries = 0;
        do {
            if (callback())
                return;
            Util::runFor(0.1_s);
        } while (++tries <= retries);

        return;
    }

    void waitForEventListenerToBeCalled(const String& event)
    {
        executeUntil([&] {
            return eventListenerWasCalled(event);
        });
    }

    void listenForMessagesPosted(std::initializer_list<ASCIILiteral> handlers, ASCIILiteral suffix)
    {
        for (auto handler : handlers) {
            auto handlerMessage = makeString(handler, suffix);
            RetainPtr nsHandlerMessage = handlerMessage.createNSString();
            [_messageHandlers addObject:nsHandlerMessage.get()];
            [webView() performAfterReceivingMessage:nsHandlerMessage.get() action:[this, handlerMessage = WTFMove(handlerMessage)] {
                _sessionMessagesPosted.add(handlerMessage);
            }];
        }
    }

    void clearMessagesPosted()
    {
        _sessionMessagesPosted.clear();
    }

    void listenForSessionHandlerMessages(std::initializer_list<ASCIILiteral> handlers)
    {
        listenForMessagesPosted(handlers, " handler"_s);
    }

    bool sessionHandlerWasCalled(const String& handler)
    {
        return _sessionMessagesPosted.contains(makeString(handler, " handler"_s));
    }

    void waitForSessionHandlerToBeCalled(const String& handler)
    {
        executeUntil([&] {
            return sessionHandlerWasCalled(handler);
        });
    }

    void listenForPromiseMessages(std::initializer_list<ASCIILiteral> handlers)
    {
        listenForMessagesPosted(handlers, " resolved"_s);
        listenForMessagesPosted(handlers, " rejected"_s);
    }

    void clearPromiseMessages(const String& promise)
    {
        _sessionMessagesPosted.remove(makeString(promise, " resolved"_s));
        _sessionMessagesPosted.remove(makeString(promise, " rejected"_s));
    }

    bool promiseWasResolved(const String& promise)
    {
        return _sessionMessagesPosted.contains(makeString(promise, " resolved"_s));
    }

    bool promiseWasRejected(const String& promise)
    {
        return _sessionMessagesPosted.contains(makeString(promise, " rejected"_s));
    }

    void waitForPromise(const String& promise)
    {
        executeUntil([&] {
            return promiseWasResolved(promise) || promiseWasRejected(promise);
        }, 200);
    }

private:
    RetainPtr<_WKMockMediaSessionCoordinator> _coordinator;
    RetainPtr<WKWebViewConfiguration> _configuration;
    RetainPtr<TestWKWebView> _webView;
    HashSet<String> _sessionMessagesPosted;
    HashSet<String> _eventListenersCalled;
    RetainPtr<NSMutableArray> _messageHandlers;
};

// rdar://136550811
#if PLATFORM(MAC)
TEST_F(MediaSessionCoordinatorTest, DISABLED_JoinAndLeave)
#else
TEST_F(MediaSessionCoordinatorTest, JoinAndLeave)
#endif
{
    loadPageAndBecomeReady("media-remote"_s);
    listenForPromiseMessages({ "join"_s });

    createCoordinator();

    RetainPtr<NSString> state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("waiting", [state UTF8String]);

    // 'join()' should fail if the remote coordinator refuses.
    coordinator().failsCommands = YES;
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasRejected("join"_s));
    clearPromiseMessages("join"_s);
    EXPECT_STREQ("join", coordinator().lastMethodCalled.UTF8String);

    // And it shoud succeed if it allows it.
    coordinator().failsCommands = NO;
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasResolved("join"_s));
    clearPromiseMessages("join"_s);
    EXPECT_STREQ("join", coordinator().lastMethodCalled.UTF8String);

    state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("joined", [state UTF8String]);

    [webView() objectByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.leave()"];
    String lastMethodCalled;
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "leave"_s;
    });
    EXPECT_STREQ("leave", lastMethodCalled.utf8().data());

    state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("closed", [state UTF8String]);

    // It shouldn't be possible to re-join a close coordinator.
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasRejected("join"_s));
    EXPECT_STREQ("", coordinator().lastMethodCalled.UTF8String);
}

// rdar://136550811
#if PLATFORM(MAC)
TEST_F(MediaSessionCoordinatorTest, DISABLED_StateChanges)
#else
TEST_F(MediaSessionCoordinatorTest, StateChanges)
#endif
{
    loadPageAndBecomeReady("media-remote"_s);

    listenForPromiseMessages({ "join"_s });
    createCoordinator();
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasResolved("join"_s));
    clearPromiseMessages("join"_s);
    EXPECT_STREQ("join", coordinator().lastMethodCalled.UTF8String);

    [webView() objectByEvaluatingJavaScript:@"navigator.mediaSession.setPositionState({ duration: 1, playbackRate: 1, position: 0 })"];
    String lastStateChange;
    executeUntil([&] {
        lastStateChange = coordinator().lastStateChange;
        return lastStateChange == "positionStateChanged"_s;
    });
    EXPECT_STREQ("positionStateChanged", lastStateChange.utf8().data());

    for (NSString *state in @[ @"havemetadata", @"havecurrentdata", @"havefuturedata", @"haveenoughdata", @"havenothing" ]) {
        [webView() objectByEvaluatingJavaScript:[NSString stringWithFormat:@"navigator.mediaSession.readyState = '%@'", state]];
        executeUntil([&] {
            lastStateChange = coordinator().lastStateChange;
            return lastStateChange == "readyStateChanged"_s;
        });
        EXPECT_STREQ("readyStateChanged", lastStateChange.utf8().data());

        RetainPtr<NSString> currentState = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.readyState"];
        EXPECT_STREQ(state.UTF8String, currentState.get().UTF8String);
    }

    for (NSString *state in @[ @"paused", @"playing", @"none" ]) {
        [webView() objectByEvaluatingJavaScript:[NSString stringWithFormat:@"navigator.mediaSession.playbackState = '%@'", state]];
        executeUntil([&] {
            lastStateChange = coordinator().lastStateChange;
            return lastStateChange == "playbackStateChanged"_s;
        });
        EXPECT_STREQ("playbackStateChanged", lastStateChange.utf8().data());

        RetainPtr<NSString> currentState = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.playbackState"];
        EXPECT_STREQ(state.UTF8String, currentState.get().UTF8String);
    }

    [webView() objectByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.leave()"];
    String lastMethodCalled;
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "leave"_s;
    });
    EXPECT_STREQ("leave", lastMethodCalled.utf8().data());

    RetainPtr<NSString> state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("closed", [state UTF8String]);
}

// rdar://136550811
#if PLATFORM(MAC)
TEST_F(MediaSessionCoordinatorTest, DISABLED_CoordinatorMethodCallbacks)
#else
TEST_F(MediaSessionCoordinatorTest, CoordinatorMethodCallbacks)
#endif
{
    loadPageAndBecomeReady("media-remote"_s);

    listenForPromiseMessages({ "join"_s });
    createCoordinator();
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasResolved("join"_s));
    clearPromiseMessages("join"_s);
    EXPECT_STREQ("join", coordinator().lastMethodCalled.UTF8String);

    listenForPromiseMessages({ "play"_s, "pause"_s, "seekTo"_s, "setTrack"_s });
    auto methodsAndArgs = @[
        @[ @"play", @"" ],
        @[ @"pause", @"" ],
        @[ @"seekTo", @"10" ],
        @[ @"setTrack", @"'\\'test-track-1\\''" ],
    ];
    for (NSArray *methodInfo in methodsAndArgs) {
        NSString *method = methodInfo[0];
        NSString *args = methodInfo[1];
        auto str = [NSString stringWithFormat:@"callMethod('%@', %@)", method, args];
        [webView() objectByEvaluatingJavaScript:str];
        waitForPromise(method);
        ASSERT_TRUE(promiseWasResolved(method));
        clearPromiseMessages(method);
        EXPECT_STREQ(method.UTF8String, coordinator().lastMethodCalled.UTF8String);
    }
}

// rdar://136550811
#if PLATFORM(MAC)
TEST_F(MediaSessionCoordinatorTest, DISABLED_CallSessionMethods)
#else
TEST_F(MediaSessionCoordinatorTest, CallSessionMethods)
#endif
{
    loadPageAndBecomeReady("media-remote"_s);
    listenForSessionHandlerMessages({ "play"_s, "pause"_s, "seekto"_s, "nexttrack"_s });

    listenForPromiseMessages({ "join"_s });
    createCoordinator();
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForPromise("join"_s);
    ASSERT_TRUE(promiseWasResolved("join"_s));
    clearPromiseMessages("join"_s);
    EXPECT_STREQ("join", coordinator().lastMethodCalled.UTF8String);

    String lastMethodCalled;

    [coordinator() seekSessionToTime:20];
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "seekSessionToTime"_s;
    });
    EXPECT_STREQ("seekSessionToTime", lastMethodCalled.utf8().data());

    [coordinator() playSession];
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "playSession"_s;
    });
    EXPECT_STREQ("playSession", lastMethodCalled.utf8().data());

    [coordinator() pauseSession];
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "pauseSession"_s;
    });
    EXPECT_STREQ("pauseSession", lastMethodCalled.utf8().data());

    [coordinator() setSessionTrack:@"Track 0"];
    executeUntil([&] {
        lastMethodCalled = coordinator().lastMethodCalled;
        return lastMethodCalled == "setSessionTrack"_s;
    });
    EXPECT_STREQ("setSessionTrack", lastMethodCalled.utf8().data());
}

// rdar://136550811
#if PLATFORM(MAC)
TEST_F(MediaSessionCoordinatorTest, DISABLED_JoinAndPrivateLeave)
#else
TEST_F(MediaSessionCoordinatorTest, JoinAndPrivateLeave)
#endif
{
    loadPageAndBecomeReady("media-remote"_s);
    listenForPromiseMessages({ "join"_s });

    createCoordinator();

    // Check that when a coordinator is created, its original state is 'waiting'.
    // createCoordinator has already waited for the 'coordinatorstatechange' event
    // to be fired.
    RetainPtr<NSString> state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("waiting", [state UTF8String]);

    // Check that when we join a session; the 'coordinatorstatechange' event will be
    // fired and the coordinator state changes to 'waiting'.
    [webView() objectByEvaluatingJavaScript:@"joinSession()"];
    waitForEventListenerToBeCalled("coordinatorstatechange"_s);
    ASSERT_TRUE(eventListenerWasCalled("coordinatorstatechange"_s));
    state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("joined", [state UTF8String]);

    // Check that when the MediaSessionCoordinatorPrivate changes its state to 'closed'
    // in the UI process, the 'coordinatorstatechange' event will be fired in JS (in web
    // process) and that the coordinator state will change to 'closed'.
    [coordinator() sessionStateChanged:WKMediaSessionCoordinatorStateClosed];
    waitForEventListenerToBeCalled("coordinatorstatechange"_s);
    state = [webView() stringByEvaluatingJavaScript:@"navigator.mediaSession.coordinator.state"];
    EXPECT_STREQ("closed", [state UTF8String]);
}

} // namespace TestWebKitAPI

#endif // ENABLE(MEDIA_SESSION_COORDINATOR)
