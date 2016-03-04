/*
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
#include <WebCore/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/StringExtras.h>

using namespace WebCore;

namespace TestWebKitAPI {

const char* FileSystemTestData = "This is a test";

// FIXME: Refactor FileSystemTest and SharedBufferTest as a single class.
class FileSystemTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();
        
        // create temp file
        PlatformFileHandle handle;
        m_tempFilePath = openTemporaryFile("tempTestFile", handle);
        writeToFile(handle, FileSystemTestData, strlen(FileSystemTestData));
        closeFile(handle); 

        m_tempEmptyFilePath = openTemporaryFile("tempEmptyTestFile", handle);
        closeFile(handle); 
    }

    void TearDown() override
    {
        deleteFile(m_tempFilePath);
        deleteFile(m_tempEmptyFilePath);
    }

    const String& tempFilePath() { return m_tempFilePath; }
    const String& tempEmptyFilePath() { return m_tempEmptyFilePath; }

private:
    String m_tempFilePath;
    String m_tempEmptyFilePath;
};

TEST_F(FileSystemTest, MappingMissingFile)
{
    bool success;
    MappedFileData mappedFileData(String("not_existing_file"), success);
    EXPECT_FALSE(success);
    EXPECT_TRUE(!mappedFileData);
}

TEST_F(FileSystemTest, MappingExistingFile)
{
    bool success;
    MappedFileData mappedFileData(tempFilePath(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(!!mappedFileData);
    EXPECT_TRUE(mappedFileData.size() == strlen(FileSystemTestData));
    EXPECT_TRUE(strnstr(FileSystemTestData, static_cast<const char*>(mappedFileData.data()), mappedFileData.size()));
}

TEST_F(FileSystemTest, MappingExistingEmptyFile)
{
    bool success;
    MappedFileData mappedFileData(tempEmptyFilePath(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(!mappedFileData);
}

}
