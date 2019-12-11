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
#include <wtf/LoggerHelper.h>
#include <wtf/MainThread.h>

#define LOG_CHANNEL_PREFIX Test

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

// Define the following to enable all tests. Disabled by default because replacing stderr with a
// non-blocking pipe fails on some of the bots.
#define TEST_OUTPUT 0

namespace TestWebKitAPI {

class LoggingTest : public testing::Test, public LoggerHelper {
public:
    LoggingTest()
        : m_logger { Logger::create(this) }
    {
    }

    void SetUp() final
    {
        WTF::initializeMainThread();

        // Replace stderr with a non-blocking pipe that we can read from.
        pipe(m_descriptors);
        fcntl(m_descriptors[0], F_SETFL, fcntl(m_descriptors[0], F_GETFL, 0) | O_NONBLOCK);
        dup2(m_descriptors[1], STDERR_FILENO);
        close(m_descriptors[1]);

        m_stderr = fdopen(m_descriptors[0], "r");

        WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "all");
        WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Error);
        WTFSetLogChannelLevel(&TestChannel2, WTFLogLevel::Error);
        WTFSetLogChannelLevel(&TestChannel3, WTFLogLevel::Error);
        WTFSetLogChannelLevel(&TestChannel4, WTFLogLevel::Error);
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

    const Logger& logger() const final { return m_logger.get(); }
    const char* logClassName() const final { return "LoggingTest"; }
    WTFLogChannel& logChannel() const final { return TestChannel1; }
    const void* logIdentifier() const final { return reinterpret_cast<const void*>(123456789); }

private:

    Ref<Logger> m_logger;
    int m_descriptors[2];
    FILE* m_stderr;
};

TEST_F(LoggingTest, Initialization)
{
    EXPECT_EQ(TestChannel1.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelState::On);

    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel2.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel3.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel4.level, WTFLogLevel::Error);

    TestChannel1.state = WTFLogChannelState::Off;
    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1");
    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel1.state, WTFLogChannelState::On);

    TestChannel1.state = WTFLogChannelState::Off;
    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1=foo");
    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Error);
#if TEST_OUTPUT
    EXPECT_TRUE(output().contains("Unknown logging level: foo", false));
#endif

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel1=warning");
    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Warning);
    EXPECT_EQ(TestChannel2.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel3.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel4.level, WTFLogLevel::Error);

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "Channel4=   debug, Channel3 = info,Channel2=error");
    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Warning);
    EXPECT_EQ(TestChannel2.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel3.level, WTFLogLevel::Info);
    EXPECT_EQ(TestChannel4.level, WTFLogLevel::Debug);

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "-all");
    EXPECT_EQ(TestChannel1.state, WTFLogChannelState::Off);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelState::Off);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelState::Off);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelState::Off);

    WTFInitializeLogChannelStatesFromString(testLogChannels, logChannelCount, "all");
    EXPECT_EQ(TestChannel1.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelState::On);
}

TEST_F(LoggingTest, WTFWillLogWithLevel)
{
    EXPECT_EQ(TestChannel1.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel2.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel3.state, WTFLogChannelState::On);
    EXPECT_EQ(TestChannel4.state, WTFLogChannelState::On);

    EXPECT_EQ(TestChannel1.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel2.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel3.level, WTFLogLevel::Error);
    EXPECT_EQ(TestChannel4.level, WTFLogLevel::Error);

    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Error));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel2, WTFLogLevel::Error));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel3, WTFLogLevel::Error));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel4, WTFLogLevel::Error));

    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Info));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel2, WTFLogLevel::Info));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel3, WTFLogLevel::Info));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel4, WTFLogLevel::Info));

    TestChannel1.state = WTFLogChannelState::Off;
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Error));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Info));

    TestChannel1.state = WTFLogChannelState::On;
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Error));
    EXPECT_FALSE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Info));

    TestChannel1.level = WTFLogLevel::Info;
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Error));
    EXPECT_TRUE(WTFWillLogWithLevel(&TestChannel1, WTFLogLevel::Info));
}

#if TEST_OUTPUT
TEST_F(LoggingTest, LOG)
{
    LOG(Channel1, "Log message.");
    EXPECT_TRUE(output().contains("Log Message.", false));
}

TEST_F(LoggingTest, LOG_WITH_LEVEL)
{
    LOG_WITH_LEVEL(Channel1, WTFLogLevel::Error, "Go and boil your bottoms, you sons of a silly person.");
    EXPECT_TRUE(output().contains("sons of a silly person.", false));

    LOG_WITH_LEVEL(Channel1, WTFLogLevel::Warning, "You don't frighten us, English pig dogs.");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Info);
    LOG_WITH_LEVEL(Channel1, WTFLogLevel::Warning, "I'm French. Why do you think I have this outrageous accent, you silly king?");
    EXPECT_TRUE(output().contains("outrageous accent", false));

    LOG_WITH_LEVEL(Channel1, WTFLogLevel::Debug, "You don't frighten us with your silly knees-bent running around advancing behavior!");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Debug);
    LOG_WITH_LEVEL(Channel1, WTFLogLevel::Debug, "Go and tell your master that we have been charged by God with a sacred quest.");
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
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevel::Error, "You don't frighten us, English pig dogs.");
    EXPECT_TRUE(output().contains("pig dogs.", false));

    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevel::Warning, "Go and boil your bottoms, you sons of a silly person.");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Info);
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevel::Warning, "I'm French. Why do you think I have this outrageous accent, you silly king?");
    EXPECT_TRUE(output().contains("outrageous accent", false));

    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevel::Debug, "You don't frighten us with your silly knees-bent running around advancing behavior!");
    EXPECT_EQ(0u, output().length());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Debug);
    RELEASE_LOG_WITH_LEVEL(Channel1, WTFLogLevel::Debug, "Go and tell your master that we have been charged by God with a sacred quest.");
    EXPECT_TRUE(output().contains("sacred quest", false));
}

TEST_F(LoggingTest, RELEASE_LOG_WITH_LEVEL_IF)
{
    bool enabled = true;
    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevel::Error, "Is there someone else up there that we can talk to?");
    EXPECT_TRUE(output().contains("someone else", false));

    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevel::Debug, "No, now go away");
    EXPECT_EQ(0u, output().length());

    enabled = false;
    RELEASE_LOG_WITH_LEVEL_IF(enabled, Channel1, WTFLogLevel::Warning, "or I shall taunt you a second time! %i", 12);
    EXPECT_EQ(0u, output().length());
}

TEST_F(LoggingTest, Logger)
{
    Ref<Logger> logger = Logger::create(this);
    EXPECT_TRUE(logger->enabled());

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Error);
    EXPECT_TRUE(logger->willLog(TestChannel1, WTFLogLevel::Error));
    logger->error(TestChannel1, "You're using coconuts!");
    EXPECT_TRUE(output().contains("You're using coconuts!", false));
    logger->warning(TestChannel1, "You're using coconuts!");
    EXPECT_EQ(0u, output().length());
    logger->info(TestChannel1, "You're using coconuts!");
    EXPECT_EQ(0u, output().length());
    logger->debug(TestChannel1, "You're using coconuts!");
    EXPECT_EQ(0u, output().length());

    logger->error(TestChannel1, Logger::LogSiteIdentifier("LoggingTest::Logger", this) , ": test output");
    EXPECT_TRUE(output().contains("LoggingTest::Logger(", false));

    logger->error(TestChannel1, "What is ", 1, " + " , 12.5F, "?");
    EXPECT_TRUE(output().contains("What is 1 + 12.5?", false));

    logger->error(TestChannel1, "What, ", "ridden on a horse?");
    EXPECT_TRUE(output().contains("What, ridden on a horse?", false));

    logger->setEnabled(this, false);
    EXPECT_FALSE(logger->enabled());
    logger->error(TestChannel1, "You've got two empty halves of coconuts");
    EXPECT_EQ(0u, output().length());

    logger->setEnabled(this, true);
    EXPECT_TRUE(logger->enabled());
    logger->error(TestChannel1, "You've got ", 2, " empty halves of ", "coconuts!");
    EXPECT_TRUE(output().contains("You've got 2 empty halves of coconuts!", false));

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Error);
    logger->logAlways(TestChannel1, "I shall taunt you a second time!");
    EXPECT_TRUE(output().contains("I shall taunt you a second time!", false));

    logger->setEnabled(this, false);
    EXPECT_FALSE(logger->willLog(TestChannel1, WTFLogLevel::Error));
    EXPECT_FALSE(logger->willLog(TestChannel1, WTFLogLevel::Warning));
    EXPECT_FALSE(logger->willLog(TestChannel1, WTFLogLevel::Info));
    EXPECT_FALSE(logger->willLog(TestChannel1, WTFLogLevel::Debug));
    EXPECT_FALSE(logger->enabled());
    logger->logAlways(TestChannel1, "You've got two empty halves of coconuts");
    EXPECT_EQ(0u, output().length());

    logger->setEnabled(this, true);
    AtomString string1("AtomString", AtomString::ConstructFromLiteral);
    const AtomString string2("const AtomString", AtomString::ConstructFromLiteral);
    logger->logAlways(TestChannel1, string1, " and ", string2);
    EXPECT_TRUE(output().contains("AtomString and const AtomString", false));

    String string3("String");
    const String string4("const String");
    logger->logAlways(TestChannel1, string3, " and ", string4);
    EXPECT_TRUE(output().contains("String and const String", false));
}

TEST_F(LoggingTest, LoggerHelper)
{
    EXPECT_TRUE(logger().enabled());

    StringBuilder builder;
    builder.appendLiteral("LoggingTest::TestBody(");
    appendUnsigned64AsHex(reinterpret_cast<uintptr_t>(logIdentifier()), builder);
    builder.appendLiteral(")");
    String signature = builder.toString();

    ALWAYS_LOG(LOGIDENTIFIER);
    EXPECT_TRUE(this->output().contains(signature, false));

    ALWAYS_LOG(LOGIDENTIFIER, "Welcome back", " my friends", " to the show", " that never ends");
    String result = this->output();
    EXPECT_TRUE(result.contains(signature, false));
    EXPECT_TRUE(result.contains("to the show that never", false));

    WTFSetLogChannelLevel(&TestChannel1, WTFLogLevel::Warning);

    ERROR_LOG(LOGIDENTIFIER, "We're so glad you could attend");
    EXPECT_TRUE(output().contains("We're so glad you could attend", false));

    WARNING_LOG(LOGIDENTIFIER, "Come inside! ", "Come inside!");
    EXPECT_TRUE(output().contains("Come inside! Come inside!", false));

    INFO_LOG(LOGIDENTIFIER, "be careful as you pass.");
    EXPECT_EQ(0u, output().length());

    DEBUG_LOG(LOGIDENTIFIER, "Move along! Move along!");
    EXPECT_EQ(0u, output().length());
}

class LogObserver : public Logger::Observer {
public:
    LogObserver() = default;

    String log()
    {
        String log = m_logBuffer.toString();
        m_logBuffer.clear();

        return log;
    }

    WTFLogChannel channel() const { return m_lastChannel; }
    WTFLogLevel level() const { return m_lastLevel; }

private:
    void didLogMessage(const WTFLogChannel& channel, WTFLogLevel level, const String& logMessage) final
    {
        m_logBuffer.append(logMessage);
        m_lastChannel = channel;
        m_lastLevel = level;
    }

    StringBuilder m_logBuffer;
    WTFLogChannel m_lastChannel;
    WTFLogLevel m_lastLevel { WTFLogLevel::Error };
};

#if !RELEASE_LOG_DISABLED
TEST_F(LoggingTest, LogObserver)
{
    LogObserver observer;

    EXPECT_TRUE(logger().enabled());

    logger().addObserver(observer);
    ALWAYS_LOG(LOGIDENTIFIER, "testing 1, 2, 3");
    EXPECT_TRUE(this->output().contains("testing 1, 2, 3", false));
    EXPECT_TRUE(observer.log().contains("testing 1, 2, 3", false));
    EXPECT_STREQ(observer.channel().name, logChannel().name);
    EXPECT_EQ(static_cast<int>(WTFLogLevel::Always), static_cast<int>(observer.level()));

    logger().removeObserver(observer);
    ALWAYS_LOG("testing ", 1, ", ", 2, ", 3");
    EXPECT_TRUE(this->output().contains("testing 1, 2, 3", false));
    EXPECT_EQ(0u, observer.log().length());
}
#endif

#endif

} // namespace TestWebKitAPI
