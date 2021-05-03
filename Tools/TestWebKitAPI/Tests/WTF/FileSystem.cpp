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

        // create temp file.
        FileSystem::PlatformFileHandle handle;
        m_tempFilePath = FileSystem::openTemporaryFile("tempTestFile", handle);
        FileSystem::writeToFile(handle, FileSystemTestData, strlen(FileSystemTestData));
        FileSystem::closeFile(handle);

        m_tempFileSymlinkPath = "tempTestFile-symlink";
        FileSystem::createSymbolicLink(m_tempFilePath, m_tempFileSymlinkPath);

        // Create temp directory.
        FileSystem::PlatformFileHandle temporaryFile;
        m_tempEmptyFolderPath = FileSystem::openTemporaryFile("tempEmptyFolder", temporaryFile);
        FileSystem::closeFile(temporaryFile);
        FileSystem::deleteFile(m_tempEmptyFolderPath);
        FileSystem::makeAllDirectories(m_tempEmptyFolderPath);

        m_tempEmptyFolderSymlinkPath = FileSystem::openTemporaryFile("tempEmptyFolder-symlink", temporaryFile);
        FileSystem::closeFile(temporaryFile);
        FileSystem::deleteFile(m_tempEmptyFolderSymlinkPath);
        FileSystem::createSymbolicLink(m_tempEmptyFolderPath, m_tempEmptyFolderSymlinkPath);

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
        FileSystem::deleteEmptyDirectory(m_tempEmptyFolderPath);
        FileSystem::deleteFile(m_tempEmptyFolderSymlinkPath);
        FileSystem::deleteFile(m_tempEmptyFilePath);
        FileSystem::deleteFile(m_spaceContainingFilePath);
        FileSystem::deleteFile(m_bangContainingFilePath);
        FileSystem::deleteFile(m_quoteContainingFilePath);
    }

    const String& tempFilePath() const { return m_tempFilePath; }
    const String& tempFileSymlinkPath() const { return m_tempFileSymlinkPath; }
    const String& tempEmptyFolderPath() const { return m_tempEmptyFolderPath; }
    const String& tempEmptyFolderSymlinkPath() const { return m_tempEmptyFolderSymlinkPath; }
    const String& tempEmptyFilePath() const { return m_tempEmptyFilePath; }
    const String& spaceContainingFilePath() const { return m_spaceContainingFilePath; }
    const String& bangContainingFilePath() const { return m_bangContainingFilePath; }
    const String& quoteContainingFilePath() const { return m_quoteContainingFilePath; }

private:
    String m_tempFilePath;
    String m_tempFileSymlinkPath;
    String m_tempEmptyFolderPath;
    String m_tempEmptyFolderSymlinkPath;
    String m_tempEmptyFilePath;
    String m_spaceContainingFilePath;
    String m_bangContainingFilePath;
    String m_quoteContainingFilePath;
};

TEST_F(FileSystemTest, MappingMissingFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(String("not_existing_file"), FileSystem::MappedFileMode::Shared, success);
    EXPECT_FALSE(success);
    EXPECT_TRUE(!mappedFileData);
}

TEST_F(FileSystemTest, MappingExistingFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(tempFilePath(), FileSystem::MappedFileMode::Shared, success);
    EXPECT_TRUE(success);
    EXPECT_TRUE(!!mappedFileData);
    EXPECT_TRUE(mappedFileData.size() == strlen(FileSystemTestData));
    EXPECT_TRUE(strnstr(FileSystemTestData, static_cast<const char*>(mappedFileData.data()), mappedFileData.size()));
}

TEST_F(FileSystemTest, MappingExistingEmptyFile)
{
    bool success;
    FileSystem::MappedFileData mappedFileData(tempEmptyFilePath(), FileSystem::MappedFileMode::Shared, success);
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

TEST_F(FileSystemTest, openExistingFileAndFailIfFileExists)
{
    auto handle = FileSystem::openFile(tempFilePath(), FileSystem::FileOpenMode::ReadWrite, FileSystem::FileAccessPermission::All, true);
    EXPECT_FALSE(FileSystem::isHandleValid(handle));
}

TEST_F(FileSystemTest, deleteNonEmptyDirectory)
{
    auto createTestTile = [](const String& path) {
        auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::Write);
        EXPECT_TRUE(FileSystem::isHandleValid(fileHandle));
        FileSystem::writeToFile(fileHandle, FileSystemTestData, strlen(FileSystemTestData));
        FileSystem::closeFile(fileHandle);
    };

    FileSystem::PlatformFileHandle temporaryFile;
    auto temporaryTestFolder = FileSystem::openTemporaryFile("deleteNonEmptyDirectoryTest", temporaryFile);
    FileSystem::closeFile(temporaryFile);

    EXPECT_TRUE(FileSystem::deleteFile(temporaryTestFolder));
    EXPECT_TRUE(FileSystem::makeAllDirectories(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder" })));
    createTestTile(FileSystem::pathByAppendingComponent(temporaryTestFolder, "file1.txt"));
    createTestTile(FileSystem::pathByAppendingComponent(temporaryTestFolder, "file2.txt"));
    createTestTile(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder", "file3.txt" }));
    createTestTile(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder", "file4.txt" }));
    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(temporaryTestFolder));
    EXPECT_TRUE(FileSystem::fileExists(temporaryTestFolder));
    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(temporaryTestFolder));
    EXPECT_FALSE(FileSystem::fileExists(temporaryTestFolder));
}

TEST_F(FileSystemTest, fileExists)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempFileSymlinkPath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s)));
}

TEST_F(FileSystemTest, deleteSymlink)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempFileSymlinkPath()));

    EXPECT_TRUE(FileSystem::deleteFile(tempFileSymlinkPath()));

    // Should have deleted the symlink but not the target file.
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFileSymlinkPath()));
}

TEST_F(FileSystemTest, deleteFile)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::deleteFile(tempFilePath()));
}

TEST_F(FileSystemTest, deleteFileOnEmptyDirectory)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::deleteFile(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
}

TEST_F(FileSystemTest, deleteEmptyDirectory)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::deleteEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(tempEmptyFolderPath()));
}

#if PLATFORM(MAC)
TEST_F(FileSystemTest, deleteEmptyDirectoryContainingDSStoreFile)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));

    // Create .DSStore file.
    auto dsStorePath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), ".DS_Store");
    auto dsStoreHandle = FileSystem::openFile(dsStorePath, FileSystem::FileOpenMode::Write);
    FileSystem::writeToFile(dsStoreHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(dsStoreHandle);
    EXPECT_TRUE(FileSystem::fileExists(dsStorePath));

    EXPECT_TRUE(FileSystem::deleteEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(tempEmptyFolderPath()));
}
#endif

TEST_F(FileSystemTest, deleteEmptyDirectoryOnNonEmptyDirectory)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));

    // Create .DSStore file.
    auto dsStorePath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), ".DS_Store");
    auto dsStoreHandle = FileSystem::openFile(dsStorePath, FileSystem::FileOpenMode::Write);
    FileSystem::writeToFile(dsStoreHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(dsStoreHandle);
    EXPECT_TRUE(FileSystem::fileExists(dsStorePath));

    // Create a dummy file.
    auto dummyFilePath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "dummyFile");
    auto dummyFileHandle = FileSystem::openFile(dummyFilePath, FileSystem::FileOpenMode::Write);
    FileSystem::writeToFile(dummyFileHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(dummyFileHandle);
    EXPECT_TRUE(FileSystem::fileExists(dummyFilePath));

    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::fileExists(dsStorePath));
    EXPECT_TRUE(FileSystem::fileExists(dummyFilePath));

    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(tempEmptyFolderPath()));
}

TEST_F(FileSystemTest, deleteEmptyDirectoryOnARegularFile)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
}

TEST_F(FileSystemTest, deleteEmptyDirectoryDoesNotExist)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    EXPECT_FALSE(FileSystem::fileExists(doesNotExistPath));
    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(doesNotExistPath));
}

TEST_F(FileSystemTest, moveFile)
{
    auto destination = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-moved");
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(destination));
    EXPECT_TRUE(FileSystem::moveFile(tempFilePath(), destination));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(destination));
    EXPECT_FALSE(FileSystem::moveFile(tempFilePath(), destination));
}

TEST_F(FileSystemTest, moveFileOverwritesDestination)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));

    long long fileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(tempFilePath(), fileSize));
    EXPECT_GT(fileSize, 0);

    EXPECT_TRUE(FileSystem::getFileSize(tempEmptyFilePath(), fileSize));
    EXPECT_TRUE(!fileSize);

    EXPECT_TRUE(FileSystem::moveFile(tempFilePath(), tempEmptyFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));

    fileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(tempEmptyFilePath(), fileSize));
    EXPECT_GT(fileSize, 0);
}

TEST_F(FileSystemTest, getFileSize)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));

    long long fileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(tempFilePath(), fileSize));
    EXPECT_GT(fileSize, 0);

    EXPECT_TRUE(FileSystem::getFileSize(tempEmptyFilePath(), fileSize));
    EXPECT_TRUE(!fileSize);

    fileSize = 0;
    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::getFileSize(fileThatDoesNotExist, fileSize));
    EXPECT_TRUE(!fileSize);
}

TEST_F(FileSystemTest, fileIsDirectory)
{
    EXPECT_TRUE(FileSystem::fileIsDirectory(tempEmptyFolderPath(), FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_TRUE(FileSystem::fileIsDirectory(tempEmptyFolderPath(), FileSystem::ShouldFollowSymbolicLinks::Yes));

    auto folderSymlinkMetadata = FileSystem::fileMetadata(tempEmptyFolderSymlinkPath());
    EXPECT_TRUE(!!folderSymlinkMetadata);
    EXPECT_EQ(folderSymlinkMetadata->type, FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(FileSystem::fileIsDirectory(tempEmptyFolderSymlinkPath(), FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_TRUE(FileSystem::fileIsDirectory(tempEmptyFolderSymlinkPath(), FileSystem::ShouldFollowSymbolicLinks::Yes));

    EXPECT_FALSE(FileSystem::fileIsDirectory(tempFilePath(), FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_FALSE(FileSystem::fileIsDirectory(tempFilePath(), FileSystem::ShouldFollowSymbolicLinks::Yes));

    auto fileSymlinkMetadata = FileSystem::fileMetadata(tempFileSymlinkPath());
    EXPECT_TRUE(!!fileSymlinkMetadata);
    EXPECT_EQ(fileSymlinkMetadata->type, FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(FileSystem::fileIsDirectory(tempFileSymlinkPath(), FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_FALSE(FileSystem::fileIsDirectory(tempFileSymlinkPath(), FileSystem::ShouldFollowSymbolicLinks::Yes));

    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::fileIsDirectory(fileThatDoesNotExist, FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_FALSE(FileSystem::fileIsDirectory(fileThatDoesNotExist, FileSystem::ShouldFollowSymbolicLinks::Yes));
}

TEST_F(FileSystemTest, makeAllDirectories)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::fileIsDirectory(tempEmptyFolderPath(), FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_TRUE(FileSystem::makeAllDirectories(tempEmptyFolderPath()));
    String subFolderPath = FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subFolder1", "subFolder2", "subFolder3" });
    EXPECT_FALSE(FileSystem::fileExists(subFolderPath));
    EXPECT_TRUE(FileSystem::makeAllDirectories(subFolderPath));
    EXPECT_TRUE(FileSystem::fileExists(subFolderPath));
    EXPECT_TRUE(FileSystem::fileIsDirectory(subFolderPath, FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(subFolderPath));
}

TEST_F(FileSystemTest, getVolumeFreeSpace)
{
    uint64_t freeSpace = 0;
    EXPECT_TRUE(FileSystem::getVolumeFreeSpace(tempFilePath(), freeSpace));
    EXPECT_GT(freeSpace, 0U);

    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::getVolumeFreeSpace(fileThatDoesNotExist, freeSpace));
}

TEST_F(FileSystemTest, createSymbolicLink)
{
    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-symlink");
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFilePath(), symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(symlinkPath));

    auto symlinkMetadata = FileSystem::fileMetadata(symlinkPath);
    EXPECT_TRUE(!!symlinkMetadata);
    EXPECT_EQ(symlinkMetadata->type, FileMetadata::Type::SymbolicLink);

    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
}

TEST_F(FileSystemTest, createSymbolicLinkFolder)
{
    auto symlinkPath = tempEmptyFolderSymlinkPath();
    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempEmptyFolderPath(), symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(symlinkPath));

    auto symlinkMetadata = FileSystem::fileMetadata(symlinkPath);
    EXPECT_TRUE(!!symlinkMetadata);
    EXPECT_EQ(symlinkMetadata->type, FileMetadata::Type::SymbolicLink);

    EXPECT_FALSE(FileSystem::fileIsDirectory(symlinkPath, FileSystem::ShouldFollowSymbolicLinks::No));
    EXPECT_TRUE(FileSystem::fileIsDirectory(symlinkPath, FileSystem::ShouldFollowSymbolicLinks::Yes));

    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
}

TEST_F(FileSystemTest, createSymbolicLinkFileDoesNotExist)
{
    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::fileExists(fileThatDoesNotExist));

    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist-symlink"_s);
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::createSymbolicLink(fileThatDoesNotExist, symlinkPath));
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
}

TEST_F(FileSystemTest, createHardLink)
{
    auto hardlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-hardlink");
    EXPECT_FALSE(FileSystem::fileExists(hardlinkPath));

    long long fileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(tempFilePath(), fileSize));
    EXPECT_GT(fileSize, 0);

    EXPECT_TRUE(FileSystem::hardLink(tempFilePath(), hardlinkPath));

    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    long long linkFileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(hardlinkPath, linkFileSize));
    EXPECT_EQ(linkFileSize, fileSize);

    auto hardlinkMetadata = FileSystem::fileMetadata(hardlinkPath);
    EXPECT_TRUE(!!hardlinkMetadata);
    EXPECT_EQ(hardlinkMetadata->type, FileMetadata::Type::File);

    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    linkFileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(hardlinkPath, linkFileSize));
    EXPECT_EQ(linkFileSize, fileSize);
}

TEST_F(FileSystemTest, createHardLinkOrCopyFile)
{
    auto hardlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-hardlink");
    EXPECT_FALSE(FileSystem::fileExists(hardlinkPath));

    long long fileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(tempFilePath(), fileSize));
    EXPECT_GT(fileSize, 0);

    EXPECT_TRUE(FileSystem::hardLinkOrCopyFile(tempFilePath(), hardlinkPath));

    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    long long linkFileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(hardlinkPath, linkFileSize));
    EXPECT_EQ(linkFileSize, fileSize);

    auto hardlinkMetadata = FileSystem::fileMetadata(hardlinkPath);
    EXPECT_TRUE(!!hardlinkMetadata);
    EXPECT_EQ(hardlinkMetadata->type, FileMetadata::Type::File);

    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    linkFileSize = 0;
    EXPECT_TRUE(FileSystem::getFileSize(hardlinkPath, linkFileSize));
    EXPECT_EQ(linkFileSize, fileSize);
}

} // namespace TestWebKitAPI
