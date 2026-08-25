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

#import "Test.h"
#import "TestWKWebView.h"
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebsiteDataStorePrivate.h>
#import <wtf/RetainPtr.h>

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

} // namespace TestWebKitAPI

#endif // ENGINEERING_BUILD
