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

#include "Test.h"
#include "Utilities.h"
#include "WTFStringUtilities.h"
#include <WebCore/FileMonitor.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/RunLoop.h>
#include <wtf/Scope.h>
#include <wtf/StringExtras.h>
#include <wtf/WorkQueue.h>
#include <wtf/text/StringBuffer.h>

// Note: Disabling iOS since 'system' is not available on that platform.
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WPE)

using namespace WebCore;

namespace TestWebKitAPI {
    
const String FileMonitorTestData("This is a test");
const String FileMonitorRevisedData("This is some changed text for the test");
const String FileMonitorSecondRevisedData("This is some changed text for the test");

class FileMonitorTest : public testing::Test {
public:
    void SetUp() override
    {
        RunLoop::initializeMainRunLoop();
        
        // create temp file
        FileSystem::PlatformFileHandle handle;
        m_tempFilePath = FileSystem::openTemporaryFile("tempTestFile", handle);
        ASSERT_NE(handle, FileSystem::invalidPlatformFileHandle);
        
        int rc = FileSystem::writeToFile(handle, FileMonitorTestData.utf8().data(), FileMonitorTestData.length());
        ASSERT_NE(rc, -1);
        
        FileSystem::closeFile(handle);
    }
    
    void TearDown() override
    {
        FileSystem::deleteFile(m_tempFilePath);
    }
    
    const String& tempFilePath() { return m_tempFilePath; }
    
private:
    String m_tempFilePath;
};
    
static bool observedFileModification = false;
static bool observedFileDeletion = false;
static bool didFinish = false;
    
static void handleFileModification()
{
    observedFileModification = true;
    didFinish = true;
}
    
static void handleFileDeletion()
{
    observedFileDeletion = true;
    didFinish = true;
}
    
static void resetTestState()
{
    observedFileModification = false;
    observedFileDeletion = false;
    didFinish = false;
}

static String createCommand(const String& path, const String& payload)
{
    StringBuilder command;
    command.appendLiteral("echo \"");
    command.append(payload);
    command.appendLiteral("\" > ");
    command.append(path);

    return command.toString();
}

static String readContentsOfFile(const String& path)
{
    constexpr int bufferSize = 1024;

    auto source = FileSystem::openFile(path, FileSystem::FileOpenMode::Read);
    if (!FileSystem::isHandleValid(source))
        return emptyString();

    StringBuffer<LChar> buffer(bufferSize);

    auto fileCloser = WTF::makeScopeExit([source]() {
        FileSystem::PlatformFileHandle handle = source;
        FileSystem::closeFile(handle);
    });

    // Since we control the test files, we know we only need one read
    int readBytes = FileSystem::readFromFile(source, reinterpret_cast<char*>(buffer.characters()), bufferSize);
    if (readBytes < 0)
        return emptyString();

    // Strip the trailing carriage return from the file:
    if (readBytes > 1) {
        int lastByte = readBytes - 1;
        if (buffer[lastByte] == '\n')
            buffer.shrink(lastByte);
    }
    ASSERT(readBytes < bufferSize);
    return String::adopt(WTFMove(buffer));
}

TEST_F(FileMonitorTest, DetectChange)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));

    RunLoop::initializeMainRunLoop();

    auto testQueue = WorkQueue::create("Test Work Queue");

    auto monitor = makeUnique<FileMonitor>(tempFilePath(), testQueue.copyRef(), [] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
        case FileMonitor::FileChangeType::Modification:
            handleFileModification();
            break;
        case FileMonitor::FileChangeType::Removal:
            handleFileDeletion();
            break;
        }
    });

    testQueue->dispatch([this] () mutable {
        String fileContents = readContentsOfFile(tempFilePath());
        EXPECT_STREQ(FileMonitorTestData.utf8().data(), fileContents.utf8().data());

        auto command = createCommand(tempFilePath(), FileMonitorRevisedData);
        auto rc = system(command.utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_TRUE(observedFileModification);
    EXPECT_FALSE(observedFileDeletion);

    String revisedFileContents = readContentsOfFile(tempFilePath());
    EXPECT_STREQ(FileMonitorRevisedData.utf8().data(), revisedFileContents.utf8().data());

    resetTestState();
}

TEST_F(FileMonitorTest, DetectMultipleChanges)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));

    RunLoop::initializeMainRunLoop();

    auto testQueue = WorkQueue::create("Test Work Queue");

    auto monitor = makeUnique<FileMonitor>(tempFilePath(), testQueue.copyRef(), [] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
        case FileMonitor::FileChangeType::Modification:
            handleFileModification();
            break;
        case FileMonitor::FileChangeType::Removal:
            handleFileDeletion();
            break;
        }
    });
    
    testQueue->dispatch([this] () mutable {
        String fileContents = readContentsOfFile(tempFilePath());
        EXPECT_STREQ(FileMonitorTestData.utf8().data(), fileContents.utf8().data());

        auto firstCommand = createCommand(tempFilePath(), FileMonitorRevisedData);
        auto rc = system(firstCommand.utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_TRUE(observedFileModification);
    EXPECT_FALSE(observedFileDeletion);

    String revisedFileContents = readContentsOfFile(tempFilePath());
    EXPECT_STREQ(FileMonitorRevisedData.utf8().data(), revisedFileContents.utf8().data());

    resetTestState();

    testQueue->dispatch([this] () mutable {
        auto secondCommand = createCommand(tempFilePath(), FileMonitorSecondRevisedData);
        auto rc = system(secondCommand.utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_TRUE(observedFileModification);
    EXPECT_FALSE(observedFileDeletion);

    String secondRevisedfileContents = readContentsOfFile(tempFilePath());
    EXPECT_STREQ(FileMonitorSecondRevisedData.utf8().data(), secondRevisedfileContents.utf8().data());

    resetTestState();
}

TEST_F(FileMonitorTest, DetectDeletion)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));

    RunLoop::initializeMainRunLoop();

    auto testQueue = WorkQueue::create("Test Work Queue");

    auto monitor = makeUnique<FileMonitor>(tempFilePath(), testQueue.copyRef(), [] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
        case FileMonitor::FileChangeType::Modification:
            handleFileModification();
            break;
        case FileMonitor::FileChangeType::Removal:
            handleFileDeletion();
            break;
        }
    });

    testQueue->dispatch([this] () mutable {
        StringBuilder command;
        command.appendLiteral("rm -f ");
        command.append(tempFilePath());

        auto rc = system(command.toString().utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_FALSE(observedFileModification);
    EXPECT_TRUE(observedFileDeletion);

    resetTestState();
}

TEST_F(FileMonitorTest, DetectChangeAndThenDelete)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));

    RunLoop::initializeMainRunLoop();

    auto testQueue = WorkQueue::create("Test Work Queue");

    auto monitor = makeUnique<FileMonitor>(tempFilePath(), testQueue.copyRef(), [] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
            case FileMonitor::FileChangeType::Modification:
                handleFileModification();
                break;
            case FileMonitor::FileChangeType::Removal:
                handleFileDeletion();
                break;
        }
    });

    testQueue->dispatch([this] () mutable {
        String fileContents = readContentsOfFile(tempFilePath());
        EXPECT_STREQ(FileMonitorTestData.utf8().data(), fileContents.utf8().data());

        auto firstCommand = createCommand(tempFilePath(), FileMonitorRevisedData);
        auto rc = system(firstCommand.utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_TRUE(observedFileModification);
    EXPECT_FALSE(observedFileDeletion);

    resetTestState();

    testQueue->dispatch([this] () mutable {
        StringBuilder command;
        command.appendLiteral("rm -f ");
        command.append(tempFilePath());

        auto rc = system(command.toString().utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_FALSE(observedFileModification);
    EXPECT_TRUE(observedFileDeletion);

    resetTestState();
}

TEST_F(FileMonitorTest, DetectDeleteButNotSubsequentChange)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));

    RunLoop::initializeMainRunLoop();

    auto testQueue = WorkQueue::create("Test Work Queue");

    auto monitor = makeUnique<FileMonitor>(tempFilePath(), testQueue.copyRef(), [] (FileMonitor::FileChangeType type) {
        ASSERT(!RunLoop::isMain());
        switch (type) {
            case FileMonitor::FileChangeType::Modification:
                handleFileModification();
                break;
            case FileMonitor::FileChangeType::Removal:
                handleFileDeletion();
                break;
        }
    });

    testQueue->dispatch([this] () mutable {
        StringBuilder command;
        command.appendLiteral("rm -f ");
        command.append(tempFilePath());

        auto rc = system(command.toString().utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_FALSE(observedFileModification);
    EXPECT_TRUE(observedFileDeletion);

    resetTestState();

    testQueue->dispatch([this] () mutable {
        EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));

        auto handle = FileSystem::openFile(tempFilePath(), FileSystem::FileOpenMode::Write);
        ASSERT_NE(handle, FileSystem::invalidPlatformFileHandle);

        int rc = FileSystem::writeToFile(handle, FileMonitorTestData.utf8().data(), FileMonitorTestData.length());
        ASSERT_NE(rc, -1);

        auto firstCommand = createCommand(tempFilePath(), FileMonitorRevisedData);
        rc = system(firstCommand.utf8().data());
        ASSERT_NE(rc, -1);
        if (rc == -1)
            didFinish = true;
    });

    // Set a timer to end the test, since we do not expect the file modification
    // to be observed.
    testQueue->dispatchAfter(500_ms, [&](void) {
        EXPECT_FALSE(observedFileModification);
        EXPECT_FALSE(observedFileDeletion);
        didFinish = true;
    });

    Util::run(&didFinish);

    EXPECT_FALSE(observedFileModification);
    EXPECT_FALSE(observedFileDeletion);

    resetTestState();
}

}

#endif

