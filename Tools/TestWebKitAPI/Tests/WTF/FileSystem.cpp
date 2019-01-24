/*
 * Copyright (C) 2015 Canon Inc. All rights reserved.
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
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/StringExtras.h>

namespace TestWebKitAPI {

const char* FileSystemTestData = "This is a test";

// FIXME: Refactor FileSystemTest and SharedBufferTest as a single class.
class FileSystemTest : public testing::Test {
public:
    void SetUp() override
    {
        WTF::initializeMainThread();

        // create temp file
        FileSystem::PlatformFileHandle handle;
        m_tempFilePath = FileSystem::openTemporaryFile("tempTestFile", handle);
        FileSystem::writeToFile(handle, FileSystemTestData, strlen(FileSystemTestData));
        FileSystem::closeFile(handle);

        m_tempFileSymlinkPath = m_tempFilePath + "-symlink";
        FileSystem::createSymbolicLink(m_tempFilePath, m_tempFileSymlinkPath);

        m_tempEmptyFilePath = FileSystem::openTemporaryFile("tempEmptyTestFile", handle);
        FileSystem::closeFile(handle);

        m_spaceContainingFilePath = FileSystem::openTemporaryFile("temp Empty Test File", handle);
        FileSystem::closeFile(handle);

        m_bangContainingFilePath = FileSystem::openTemporaryFile("temp!Empty!Test!File", handle);
        FileSystem::closeFile(handle);

        m_quoteContainingFilePath = FileSystem::openTemporaryFile("temp\"Empty\"TestFile", handle);
        FileSystem::closeFile(handle);
    }

    void TearDown() override
    {
        FileSystem::deleteFile(m_tempFilePath);
        FileSystem::deleteFile(m_tempFileSymlinkPath);
        FileSystem::deleteFile(m_tempEmptyFilePath);
        FileSystem::deleteFile(m_spaceContainingFilePath);
        FileSystem::deleteFile(m_bangContainingFilePath);
        FileSystem::deleteFile(m_quoteContainingFilePath);
    }

    const String& tempFilePath() { return m_tempFilePath; }
    const String& tempFileSymlinkPath() { return m_tempFileSymlinkPath; }
    const String& tempEmptyFilePath() { return m_tempEmptyFilePath; }
    const String& spaceContainingFilePath() { return m_spaceContainingFilePath; }
    const String& bangContainingFilePath() { return m_bangContainingFilePath; }
    const String& quoteContainingFilePath() { return m_quoteContainingFilePath; }

private:
    String m_tempFilePath;
    String m_tempFileSymlinkPath;
    String m_tempEmptyFilePath;
    String m_spaceContainingFilePath;
    String m_bangContainingFilePath;
    String m_quoteContainingFilePath;
};

TEST_F(FileSystemTest, MappingMissingFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(String("not_existing_file"), success);
    EXPECT_FALSE(success);
    EXPECT_TRUE(!mappedFileData);
}

TEST_F(FileSystemTest, MappingExistingFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(tempFilePath(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(!!mappedFileData);
    EXPECT_TRUE(mappedFileData.size() == strlen(FileSystemTestData));
    EXPECT_TRUE(strnstr(FileSystemTestData, static_cast<const char*>(mappedFileData.data()), mappedFileData.size()));
}

TEST_F(FileSystemTest, MappingExistingEmptyFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(tempEmptyFilePath(), success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(!mappedFileData);
}

TEST_F(FileSystemTest, FilesHaveSameVolume)
{
    EXPECT_TRUE(FileSystem::filesHaveSameVolume(tempFilePath(), spaceContainingFilePath()));
    EXPECT_TRUE(FileSystem::filesHaveSameVolume(spaceContainingFilePath(), bangContainingFilePath()));
    EXPECT_TRUE(FileSystem::filesHaveSameVolume(bangContainingFilePath(), quoteContainingFilePath()));
}

TEST_F(FileSystemTest, GetFileMetadataSymlink)
{
    auto symlinkMetadata = FileSystem::fileMetadata(tempFileSymlinkPath());
    ASSERT_TRUE(symlinkMetadata.hasValue());
    EXPECT_TRUE(symlinkMetadata.value().type == FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(static_cast<size_t>(symlinkMetadata.value().length) == strlen(FileSystemTestData));

    auto targetMetadata = FileSystem::fileMetadataFollowingSymlinks(tempFileSymlinkPath());
    ASSERT_TRUE(targetMetadata.hasValue());
    EXPECT_TRUE(targetMetadata.value().type == FileMetadata::Type::File);
    EXPECT_EQ(strlen(FileSystemTestData), static_cast<size_t>(targetMetadata.value().length));
}

TEST_F(FileSystemTest, UnicodeDirectoryName)
{
    String path = String::fromUTF8("/test/a\u0308lo/test.txt");
    String directoryName = FileSystem::directoryName(path);
    String expectedDirectoryName = String::fromUTF8("/test/a\u0308lo");
    EXPECT_TRUE(expectedDirectoryName == directoryName);
}

}
