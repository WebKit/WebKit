/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#import "Helpers/Utilities.h"
#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <wtf/BlockPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/Vector.h>
#import <wtf/darwin/DispatchExtras.h>

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#import <objc/runtime.h>
#import <pal/cocoa/WebPrivacySoftLink.h>
#endif

#if defined(ENGINEERING_BUILD) && ENGINEERING_BUILD

static NSString * const testSecurityFlagName = @"radar184485266";

namespace TestWebKitAPI {

enum class FlagState : uint8_t { Enforced, Disabled, NoSuchFlag };

// [nil boolValue] is NO, so handing back the NSNumber would let a dead network process pass for a disabled flag.
static FlagState flagStateInNetworkProcess(WKWebsiteDataStore *dataStore, NSString *flagName)
{
    RetainPtr<NSNumber> reply;
    bool done = false;
    [dataStore _isSecurityFlagEnabledInNetworkProcessForTesting:flagName completionHandler:[&] (NSNumber *enabled) {
        reply = enabled;
        done = true;
    }];
    Util::run(&done);

    EXPECT_GT([dataStore _networkProcessIdentifier], 0) << "network process died while answering";
    if (!reply)
        return FlagState::NoSuchFlag;
    return [reply boolValue] ? FlagState::Enforced : FlagState::Disabled;
}

static FlagState flagStateInUIProcess(NSString *flagName)
{
    RetainPtr enabled = [WKWebsiteDataStore _isSecurityFlagEnabledInUIProcessForTesting:flagName];
    if (!enabled)
        return FlagState::NoSuchFlag;
    return [enabled boolValue] ? FlagState::Enforced : FlagState::Disabled;
}

static RetainPtr<WKWebsiteDataStore> launchNetworkProcess()
{
    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [configuration setWebsiteDataStore:dataStore.get()];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:NSMakeRect(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<body>hello</body>"];
    return dataStore;
}

class SecurityFlagsTest : public testing::Test {
public:
    void SetUp() final { [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[]]; }

    void TearDown() final { [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[]]; }
};

TEST_F(SecurityFlagsTest, SecureByDefault)
{
    auto dataStore = launchNetworkProcess();
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Enforced);
}

TEST_F(SecurityFlagsTest, UIProcessReadsItsOwnValue)
{
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Enforced);
    EXPECT_EQ(flagStateInUIProcess(@"radar99999999"), FlagState::NoSuchFlag);

    [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[testSecurityFlagName]];

    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsTest, DidChangeReachesRunningProcess)
{
    auto dataStore = launchNetworkProcess();
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Enforced);

    [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[testSecurityFlagName]];

    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

// Terminating is what makes this discriminating: an already-running process would have learned the value over
// SecurityFlagsDidChange, passing with no creation-parameter plumbing at all.
TEST_F(SecurityFlagsTest, CreationParametersReachNewProcess)
{
    auto dataStore = launchNetworkProcess();
    auto originalPID = [dataStore _networkProcessIdentifier];
    EXPECT_GT(originalPID, 0);

    [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[testSecurityFlagName]];

    [dataStore _terminateNetworkProcess];
    while ([dataStore _networkProcessIdentifier] <= 0)
        Util::spinRunLoop();

    auto relaunchedPID = [dataStore _networkProcessIdentifier];
    EXPECT_NE(relaunchedPID, originalPID);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsTest, UnknownFlagNameIsIgnored)
{
    [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[@"radar99999999", testSecurityFlagName, @"notARadarNumber"]];

    auto dataStore = launchNetworkProcess();
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), @"radar99999999"), FlagState::NoSuchFlag);
}

static NSString * const disabledSecurityFlagsDefaultsKey = @"WebKitDebugDisabledSecurityFlags";

class SecurityFlagsUserDefaultTest : public testing::Test {
public:
    void SetUp() final { [NSUserDefaults.standardUserDefaults setObject:@[testSecurityFlagName] forKey:disabledSecurityFlagsDefaultsKey]; }

    void TearDown() final { [NSUserDefaults.standardUserDefaults removeObjectForKey:disabledSecurityFlagsDefaultsKey]; }
};

TEST_F(SecurityFlagsUserDefaultTest, UIProcessAppliesTheUserDefault)
{
    // Creating an API object is what initializes WebKit, which is what applies the user default.
    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];

    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsUserDefaultTest, UIProcessValueIsReadableOffTheMainThread)
{
    RetainPtr dataStore = [WKWebsiteDataStore nonPersistentDataStore];

    auto state = FlagState::NoSuchFlag;
    bool done = false;
    dispatch_async(globalDispatchQueueSingleton(QOS_CLASS_DEFAULT, 0), makeBlockPtr([&] {
        state = flagStateInUIProcess(testSecurityFlagName);
        done = true;
    }).get());
    Util::run(&done);

    EXPECT_EQ(state, FlagState::Disabled);
}

TEST_F(SecurityFlagsUserDefaultTest, DisablesFlagAtStartup)
{
    auto dataStore = launchNetworkProcess();
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsUserDefaultTest, TestingSPIIgnoresTheUserDefault)
{
    auto dataStore = launchNetworkProcess();
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);

    [WKWebsiteDataStore _setDisabledSecurityFlagsForTesting:@[]];
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Enforced);
}

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

// Stands in for WebPrivacy. A reply is answered asynchronously, as the real service does, and can be held so that a
// test can act while a request is in flight; a held reply carries the answer that was set when it was requested.
class MockDisabledSecurityFlagsService {
public:
    static MockDisabledSecurityFlagsService& singleton()
    {
        static NeverDestroyed<MockDisabledSecurityFlagsService> service;
        return service.get();
    }

    void answerWith(NSArray<NSString *> *names, NSError *error = nil)
    {
        m_names = names;
        m_error = error;
    }

    void holdReplies() { m_holdReplies = true; }

    void releaseHeldReplies()
    {
        m_holdReplies = false;
        for (auto& reply : std::exchange(m_heldReplies, { }))
            answer(reply);
    }

    unsigned requestCount() const { return m_requestCount; }
    unsigned replyCount() const { return m_replyCount; }

    using Completion = void (^)(NSArray<NSString *> *, NSError *);

    void handleRequest(Completion completion)
    {
        ++m_requestCount;

        Reply reply { makeBlockPtr(completion), m_names, m_error };
        if (m_holdReplies) {
            m_heldReplies.append(WTF::move(reply));
            return;
        }

        RunLoop::mainSingleton().dispatch([this, reply = WTF::move(reply)] {
            answer(reply);
        });
    }

private:
    struct Reply {
        BlockPtr<void(NSArray<NSString *> *, NSError *)> completion;
        RetainPtr<NSArray<NSString *>> names;
        RetainPtr<NSError> error;
    };

    // Counted after the answer, so that a test waiting on the count knows the flags have been applied.
    void answer(const Reply& reply)
    {
        reply.completion(reply.names.get(), reply.error.get());
        ++m_replyCount;
    }

    RetainPtr<NSArray<NSString *>> m_names;
    RetainPtr<NSError> m_error;
    Vector<Reply> m_heldReplies;
    unsigned m_requestCount { 0 };
    unsigned m_replyCount { 0 };
    bool m_holdReplies { false };
};

static void installMockDisabledSecurityFlagsService()
{
    static bool installed = false;
    if (installed)
        return;
    installed = true;

    Class resourcesClass = PAL::getWPResourcesClassSingleton();
    ASSERT(resourcesClass);

    IMP implementation = imp_implementationWithBlock([](WPResources *, MockDisabledSecurityFlagsService::Completion completion) {
        MockDisabledSecurityFlagsService::singleton().handleRequest(completion);
    });

    // Added where the installed WebPrivacy predates the API and there is nothing to swizzle, replaced where it does not.
    if (!class_addMethod(resourcesClass, @selector(requestDisabledSecurityFlags:), implementation, "v@:@?"))
        class_replaceMethod(resourcesClass, @selector(requestDisabledSecurityFlags:), implementation, "v@:@?");
}

static bool waitForFlagStateInUIProcess(NSString *flagName, FlagState state)
{
    return Util::waitFor([&] {
        return flagStateInUIProcess(flagName) == state;
    });
}

static bool waitForReplyCountAbove(unsigned count)
{
    return Util::waitFor([&] {
        return MockDisabledSecurityFlagsService::singleton().replyCount() > count;
    });
}

static bool waitForRequestCount(unsigned count)
{
    return Util::waitFor([&] {
        return MockDisabledSecurityFlagsService::singleton().requestCount() == count;
    });
}

// A test that cannot run here returns rather than calling GTEST_SKIP(), because TestsController prints **FAIL** for
// anything that did not pass, and a skipped test is not passed.
static bool canMockDisabledSecurityFlagsRequests()
{
    return PAL::isWebPrivacyFrameworkAvailable() && PAL::getWPResourcesClassSingleton();
}

class SecurityFlagsRemoteListTest : public testing::Test {
public:
    void SetUp() final
    {
        if (canMockDisabledSecurityFlagsRequests())
            installMockDisabledSecurityFlagsService();
    }
};

TEST_F(SecurityFlagsRemoteListTest, ListDisablesFlag)
{
    if (!canMockDisabledSecurityFlagsRequests())
        return;

    MockDisabledSecurityFlagsService::singleton().answerWith(@[testSecurityFlagName]);

    auto dataStore = launchNetworkProcess();

    EXPECT_TRUE(waitForReplyCountAbove(0));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Disabled);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsRemoteListTest, EmptyListEnforcesTheFlagAgain)
{
    if (!canMockDisabledSecurityFlagsRequests())
        return;

    auto& service = MockDisabledSecurityFlagsService::singleton();
    service.answerWith(@[testSecurityFlagName]);

    auto dataStore = launchNetworkProcess();
    EXPECT_TRUE(waitForFlagStateInUIProcess(testSecurityFlagName, FlagState::Disabled));

    service.answerWith(@[]);
    auto repliesBefore = service.replyCount();
    [WKWebsiteDataStore _requestDisabledSecurityFlagsForTesting];

    EXPECT_TRUE(waitForReplyCountAbove(repliesBefore));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Enforced);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Enforced);
}

TEST_F(SecurityFlagsRemoteListTest, FailureLeavesTheFlagsAsTheyAre)
{
    if (!canMockDisabledSecurityFlagsRequests())
        return;

    auto& service = MockDisabledSecurityFlagsService::singleton();
    service.answerWith(@[testSecurityFlagName]);

    auto dataStore = launchNetworkProcess();
    EXPECT_TRUE(waitForFlagStateInUIProcess(testSecurityFlagName, FlagState::Disabled));

    service.answerWith(nil, [NSError errorWithDomain:@"WebPrivacyErrorDomain" code:1 userInfo:nil]);
    auto repliesBefore = service.replyCount();
    [WKWebsiteDataStore _requestDisabledSecurityFlagsForTesting];

    EXPECT_TRUE(waitForReplyCountAbove(repliesBefore));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Disabled);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsRemoteListTest, ChangeNotificationAsksForTheListAgain)
{
    if (!canMockDisabledSecurityFlagsRequests() || !PAL::canLoad_WebPrivacy_WPDisabledSecurityFlagsChangedNotificationName())
        return;

    auto& service = MockDisabledSecurityFlagsService::singleton();
    service.answerWith(@[]);

    auto dataStore = launchNetworkProcess();
    EXPECT_TRUE(waitForReplyCountAbove(0));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Enforced);

    service.answerWith(@[testSecurityFlagName]);
    [[NSNotificationCenter defaultCenter] postNotificationName:PAL::get_WebPrivacy_WPDisabledSecurityFlagsChangedNotificationNameSingleton() object:nil];

    EXPECT_TRUE(waitForFlagStateInUIProcess(testSecurityFlagName, FlagState::Disabled));
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

TEST_F(SecurityFlagsRemoteListTest, ChangeAnnouncedWhileRequestingIsAskedForAgain)
{
    if (!canMockDisabledSecurityFlagsRequests())
        return;

    auto& service = MockDisabledSecurityFlagsService::singleton();
    service.answerWith(@[]);

    auto dataStore = launchNetworkProcess();
    EXPECT_TRUE(waitForReplyCountAbove(0));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Enforced);

    auto requestsBefore = service.requestCount();
    service.holdReplies();
    service.answerWith(@[testSecurityFlagName]);
    [WKWebsiteDataStore _requestDisabledSecurityFlagsForTesting];
    EXPECT_TRUE(waitForRequestCount(requestsBefore + 1));

    [WKWebsiteDataStore _requestDisabledSecurityFlagsForTesting];
    Util::spinRunLoop(10);
    EXPECT_EQ(service.requestCount(), requestsBefore + 1) << "a request while one is in flight must not issue a second one";

    service.answerWith(@[]);
    service.releaseHeldReplies();

    EXPECT_TRUE(waitForRequestCount(requestsBefore + 2));
    EXPECT_TRUE(waitForFlagStateInUIProcess(testSecurityFlagName, FlagState::Enforced));
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Enforced);
}

class SecurityFlagsRemoteListUserDefaultTest : public testing::Test {
public:
    void SetUp() final
    {
        if (canMockDisabledSecurityFlagsRequests())
            installMockDisabledSecurityFlagsService();

        [NSUserDefaults.standardUserDefaults setObject:@[testSecurityFlagName] forKey:disabledSecurityFlagsDefaultsKey];
    }

    void TearDown() final { [NSUserDefaults.standardUserDefaults removeObjectForKey:disabledSecurityFlagsDefaultsKey]; }
};

TEST_F(SecurityFlagsRemoteListUserDefaultTest, EmptyListKeepsTheUserDefaultNames)
{
    if (!canMockDisabledSecurityFlagsRequests())
        return;

    MockDisabledSecurityFlagsService::singleton().answerWith(@[]);

    auto dataStore = launchNetworkProcess();

    EXPECT_TRUE(waitForReplyCountAbove(0));
    EXPECT_EQ(flagStateInUIProcess(testSecurityFlagName), FlagState::Disabled);
    EXPECT_EQ(flagStateInNetworkProcess(dataStore.get(), testSecurityFlagName), FlagState::Disabled);
}

#endif // ENABLE(ADVANCED_PRIVACY_PROTECTIONS)

} // namespace TestWebKitAPI

#endif // ENGINEERING_BUILD
