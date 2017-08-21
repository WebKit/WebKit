/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "WTFStringUtilities.h"
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>

#define LOG_CHANNEL_PREFIX Test

using namespace WTF;

const char* logTestingSubsystem = "com.webkit.testing";

DEFINE_LOG_CHANNEL(Channel1, logTestingSubsystem);
DEFINE_LOG_CHANNEL(Channel2, logTestingSubsystem);
DEFINE_LOG_CHANNEL(Channel3, logTestingSubsystem);
DEFINE_LOG_CHANNEL(Channel4, logTestingSubsystem);

static WTFLogChannel* testLogChannels[] = {
    &TestChannel1,
    &TestChannel2,
    &TestChannel3,
    &TestChannel4,
};
static const size_t logChannelCount = sizeof(testLogChannels) / sizeof(testLogChannels[0]);

#define TEST_OUTPUT (!PLATFORM(COCOA) || (!PLATFORM(IOS) || __MAC_OS_X_VERSION_MAX_ALLOWED >= 101200))

namespace TestWebKitAPI {

class LoggingTest : public testing::Test {
public:
    void SetUp() final
    {
        WTF::initializeMainThread();

        // Replace stderr with a nonblocking pipe that we can read from.
        pipe(m_descriptors);
        fcntl(m_descriptors[0], F_SETFL, fcntl(m_descriptors[0], F_GETFL, 0) | O_NONBLOCK);
        dup2(m_descriptors[1], STDERR_FILENO);
        close(m_descriptors[1]);

        m_stderr = fdopen(m_descriptors[0], "r");

        WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "all");
        WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelError);
        WTFSetLogChannelLevel(&TestChannel2, WTFLogLevelError);
        WTFSetLogChannelLevel(&TestChannel3, WTFLogLevelError);
        WTFSetLogChannelLevel(&TestChannel4, WTFLogLevelError);
    }

    void TearDown() override
    {
        close(m_descriptors[0]);
        fclose(m_stderr);
    }

    String output()
    {
        char buffer[1024];
        StringBuilder result;
        char* line;

        while ((line = fgets(buffer, sizeof(buffer), m_stderr)))
            result.append(line);

        return result.toString();
    }

private:
    int m_descriptors[2];
    FILE* m_stderr;
};

TEST_F(LoggingTest, Initialization)
{
    EXPECT_EQ(TestChannel1.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelOn);

    EXPECT_EQ(TestChannel1.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel2.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel3.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel4.level, WTFLogLevelError);

    TestChannel1.state = WTFLogChannelOff;
    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1");
    EXPECT_EQ(TestChannel1.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel1.state, WTFLogChannelOn);

    TestChannel1.state = WTFLogChannelOff;
    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1=foo");
    EXPECT_EQ(TestChannel1.level, WTFLogLevelError);
#if TEST_OUTPUT
    EXPECT_TRUE(output().contains("Unknown logging level: foo", false));
#endif

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1=warning");
    EXPECT_EQ(TestChannel1.level, WTFLogLevelWarning);
    EXPECT_EQ(TestChannel2.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel3.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel4.level, WTFLogLevelError);

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel4=   debug, Channel3 = info,Channel2=notice");
    EXPECT_EQ(TestChannel1.level, WTFLogLevelWarning);
    EXPECT_EQ(TestChannel2.level, WTFLogLevelNotice);
    EXPECT_EQ(TestChannel3.level, WTFLogLevelInfo);
    EXPECT_EQ(TestChannel4.level, WTFLogLevelDebug);

#if TEST_OUTPUT
    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelError);
    EXPECT_TRUE(output().contains("Channel \"Channel1\" level set to 0", false));

    WTFSetLogChannelLevel(&TestChannel2, WTFLogLevelWarning);
    EXPECT_TRUE(output().contains("Channel \"Channel2\" level set to 1", false));
#endif

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "-all");
    EXPECT_EQ(TestChannel1.state, WTFLogChannelOff);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelOff);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelOff);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelOff);

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "all");
    EXPECT_EQ(TestChannel1.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelOn);
}

TEST_F(LoggingTest, WTFWillLogWithLevel)
{
    EXPECT_EQ(TestChannel1.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelOn);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelOn);

    EXPECT_EQ(TestChannel1.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel2.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel3.level, WTFLogLevelError);
    EXPECT_EQ(TestChannel4.level, WTFLogLevelError);

    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelError));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel2, WTFLogLevelError));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel3, WTFLogLevelError));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel4, WTFLogLevelError));

    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelInfo));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel2, WTFLogLevelInfo));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel3, WTFLogLevelInfo));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel4, WTFLogLevelInfo));

    TestChannel1.state = WTFLogChannelOff;
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelError));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelInfo));

    TestChannel1.state = WTFLogChannelOn;
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelError));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelInfo));

    TestChannel1.level = WTFLogLevelInfo;
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelError));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevelInfo));
}

#if TEST_OUTPUT
TEST_F(LoggingTest, LOG)
{
    LOG(Channel1, "Log message.");
    EXPECT_TRUE(output().contains("Log Message.", false));
}

TEST_F(LoggingTest, LOG_WITH_LEVEL)
{
    LOG_WITH_LEVEL(Channel1, WTFLogLevelError, "Go and boil your bottoms, you sons of a silly person.");
    EXPECT_TRUE(output().contains("sons of a silly person.", false));

    LOG_WITH_LEVEL(Channel1, WTFLogLevelWarning, "You don't frighten us, English pig dogs.");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelInfo);
    LOG_WITH_LEVEL(Channel1, WTFLogLevelWarning, "I'm French. Why do you think I have this outrageous accent, you silly king?");
    EXPECT_TRUE(output().contains("outrageous accent", false));

    LOG_WITH_LEVEL(Channel1, WTFLogLevelDebug, "You don't frighten us with your silly knees-bent running around advancing behavior!");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelDebug);
    LOG_WITH_LEVEL(Channel1, WTFLogLevelDebug, "Go and tell your master that we have been charged by God with a sacred quest.");
    EXPECT_TRUE(output().contains("sacred quest", false));
}

TEST_F(LoggingTest, RELEASE_LOG)
{
    RELEASE_LOG(Channel1, "Log message.");
    EXPECT_TRUE(output().contains("Log Message.", false));
}

TEST_F(LoggingTest, RELEASE_LOG_IF)
{
    bool enabled = true;
    RELEASE_LOG_IF(enabled, Channel1, "Your mother was a hamster,");
    EXPECT_TRUE(output().contains("hamster,", false));

    enabled = false;
    RELEASE_LOG_IF(enabled, Channel1, "and your father smelt of elderberries ...");
    EXPECT_EQ(0u, output().length());
}

TEST_F(LoggingTest, RELEASE_LOG_WITH_LEVEL)
{
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevelError, "You don't frighten us, English pig dogs.");
    EXPECT_TRUE(output().contains("pig dogs.", false));

    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevelWarning, "Go and boil your bottoms, you sons of a silly person.");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelInfo);
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevelWarning, "I'm French. Why do you think I have this outrageous accent, you silly king?");
    EXPECT_TRUE(output().contains("outrageous accent", false));

    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevelDebug, "You don't frighten us with your silly knees-bent running around advancing behavior!");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevelDebug);
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevelDebug, "Go and tell your master that we have been charged by God with a sacred quest.");
    EXPECT_TRUE(output().contains("sacred quest", false));
}

TEST_F(LoggingTest, RELEASE_LOG_WITH_LEVEL_IF)
{
    bool enabled = true;
    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevelError, "Is there someone else up there that we can talk to?");
    EXPECT_TRUE(output().contains("someone else", false));

    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevelDebug, "No, now go away");
    EXPECT_EQ(0u, output().length());

    enabled = false;
    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevelWarning, "or I shall taunt you a second time!");
    EXPECT_EQ(0u, output().length());
}
#endif

} // namespace TestWebKitAPI
