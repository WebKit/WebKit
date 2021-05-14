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
#include "Utilities.h"
#include <wtf/FileMetadata.h>
#include <wtf/FileSystem.h>
#include <wtf/MainThread.h>
#include <wtf/StringExtras.h>

namespace TestWebKitAPI {

const char* FileSystemTestData = "This is a test";

static void createTestFile(const String& path)
{
    auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::Write);
    EXPECT_TRUE(FileSystem::isHandleValid(fileHandle));
    FileSystem::writeToFile(fileHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(fileHandle);
};

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

        m_tempFileSymlinkPath = FileSystem::openTemporaryFile("tempTestFile-symlink", handle);
        FileSystem::closeFile(handle);
        FileSystem::deleteFile(m_tempFileSymlinkPath);
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

TEST_F(FileSystemTest, GetFileMetadataFileSymlink)
{
    auto symlinkMetadata = FileSystem::fileMetadata(tempFileSymlinkPath());
    ASSERT_TRUE(symlinkMetadata.hasValue());
    EXPECT_TRUE(symlinkMetadata->type == FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(symlinkMetadata->isHidden);
    EXPECT_TRUE(static_cast<size_t>(symlinkMetadata->length) == strlen(FileSystemTestData));

    auto targetMetadata = FileSystem::fileMetadataFollowingSymlinks(tempFileSymlinkPath());
    ASSERT_TRUE(targetMetadata.hasValue());
    EXPECT_TRUE(targetMetadata->type == FileMetadata::Type::File);
    EXPECT_FALSE(targetMetadata->isHidden);
    EXPECT_EQ(strlen(FileSystemTestData), static_cast<size_t>(targetMetadata->length));

    auto actualTargetMetadata = FileSystem::fileMetadata(tempFilePath());
    ASSERT_TRUE(actualTargetMetadata.hasValue());
    EXPECT_TRUE(actualTargetMetadata->type == FileMetadata::Type::File);
    EXPECT_EQ(targetMetadata->modificationTime, actualTargetMetadata->modificationTime);
    EXPECT_EQ(strlen(FileSystemTestData), static_cast<size_t>(targetMetadata->length));
    EXPECT_FALSE(actualTargetMetadata->isHidden);
}

TEST_F(FileSystemTest, GetFileMetadataSymlinkToFileSymlink)
{
    // Create a symbolic link pointing the tempFileSymlinkPath().
    auto symlinkToSymlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlinkToSymlink");
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkToSymlinkPath));

    auto symlinkMetadata = FileSystem::fileMetadata(symlinkToSymlinkPath);
    ASSERT_TRUE(symlinkMetadata.hasValue());
    EXPECT_TRUE(symlinkMetadata->type == FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(symlinkMetadata->isHidden);

    auto targetMetadata = FileSystem::fileMetadataFollowingSymlinks(symlinkToSymlinkPath);
    ASSERT_TRUE(targetMetadata.hasValue());
    EXPECT_TRUE(targetMetadata->type == FileMetadata::Type::File);
    EXPECT_FALSE(targetMetadata->isHidden);
    EXPECT_EQ(strlen(FileSystemTestData), static_cast<size_t>(targetMetadata->length));

    EXPECT_TRUE(FileSystem::deleteFile(symlinkToSymlinkPath));
}

TEST_F(FileSystemTest, GetFileMetadataDirectorySymlink)
{
    auto symlinkMetadata = FileSystem::fileMetadata(tempEmptyFolderSymlinkPath());
    ASSERT_TRUE(symlinkMetadata.hasValue());
    EXPECT_TRUE(symlinkMetadata->type == FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(symlinkMetadata->isHidden);

    auto targetMetadata = FileSystem::fileMetadataFollowingSymlinks(tempEmptyFolderSymlinkPath());
    ASSERT_TRUE(targetMetadata.hasValue());
    EXPECT_TRUE(targetMetadata->type == FileMetadata::Type::Directory);
    EXPECT_FALSE(targetMetadata->isHidden);

    auto actualTargetMetadata = FileSystem::fileMetadata(tempEmptyFolderPath());
    ASSERT_TRUE(actualTargetMetadata.hasValue());
    EXPECT_TRUE(actualTargetMetadata->type == FileMetadata::Type::Directory);
    EXPECT_EQ(targetMetadata->modificationTime, actualTargetMetadata->modificationTime);
    EXPECT_EQ(targetMetadata->length, actualTargetMetadata->length);
    EXPECT_FALSE(actualTargetMetadata->isHidden);
}

TEST_F(FileSystemTest, GetFileMetadataFileDoesNotExist)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    auto metadata = FileSystem::fileMetadata(doesNotExistPath);
    EXPECT_TRUE(!metadata);
}

#if OS(UNIX)
TEST_F(FileSystemTest, GetFileMetadataHiddenFile)
{
    auto hiddenFilePath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), ".hiddenFile");
    auto fileHandle = FileSystem::openFile(hiddenFilePath, FileSystem::FileOpenMode::Write);
    EXPECT_TRUE(FileSystem::isHandleValid(fileHandle));
    FileSystem::writeToFile(fileHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(fileHandle);

    auto metadata = FileSystem::fileMetadata(hiddenFilePath);
    ASSERT_TRUE(metadata.hasValue());
    EXPECT_TRUE(metadata->type == FileMetadata::Type::File);
    EXPECT_TRUE(metadata->isHidden);
    EXPECT_EQ(strlen(FileSystemTestData), static_cast<size_t>(metadata->length));

    EXPECT_TRUE(FileSystem::deleteFile(hiddenFilePath));
}
#endif

TEST_F(FileSystemTest, UnicodeDirectoryName)
{
    String path = String::fromUTF8("/test/a\u0308lo/test.txt");
    String directoryName = FileSystem::parentPath(path);
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
    FileSystem::PlatformFileHandle temporaryFile;
    auto temporaryTestFolder = FileSystem::openTemporaryFile("deleteNonEmptyDirectoryTest", temporaryFile);
    FileSystem::closeFile(temporaryFile);

    EXPECT_TRUE(FileSystem::deleteFile(temporaryTestFolder));
    EXPECT_TRUE(FileSystem::makeAllDirectories(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder" })));
    createTestFile(FileSystem::pathByAppendingComponent(temporaryTestFolder, "file1.txt"));
    createTestFile(FileSystem::pathByAppendingComponent(temporaryTestFolder, "file2.txt"));
    createTestFile(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder", "file3.txt" }));
    createTestFile(FileSystem::pathByAppendingComponents(temporaryTestFolder, { "subfolder", "file4.txt" }));
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

TEST_F(FileSystemTest, fileExistsBrokenSymlink)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist-symlink"_s);
    EXPECT_TRUE(FileSystem::createSymbolicLink(doesNotExistPath, symlinkPath));
    EXPECT_FALSE(FileSystem::fileExists(doesNotExistPath));
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath)); // fileExists() follows symlinks.
    auto symlinkMetadata = FileSystem::fileMetadata(symlinkPath);
    ASSERT_TRUE(!!symlinkMetadata);
    EXPECT_EQ(symlinkMetadata->type, FileMetadata::Type::SymbolicLink);
    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
}

TEST_F(FileSystemTest, fileExistsSymlinkToSymlink)
{
    // Create a valid symlink to a symlink to a regular file.
    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlink"_s);
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(symlinkPath));

    auto metadata = FileSystem::fileMetadata(symlinkPath);
    ASSERT_TRUE(!!metadata);
    EXPECT_EQ(metadata->type, FileMetadata::Type::SymbolicLink);

    auto targetMetadata = FileSystem::fileMetadataFollowingSymlinks(symlinkPath);
    ASSERT_TRUE(!!targetMetadata);
    EXPECT_EQ(targetMetadata->type, FileMetadata::Type::File);

    // Break the symlink by deleting the target.
    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));

    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFileSymlinkPath())); // fileExists() follows symlinks.
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath)); // fileExists() follows symlinks.

    metadata = FileSystem::fileMetadata(symlinkPath);
    ASSERT_TRUE(!!metadata);
    EXPECT_EQ(metadata->type, FileMetadata::Type::SymbolicLink);

    metadata = FileSystem::fileMetadata(tempFileSymlinkPath());
    ASSERT_TRUE(!!metadata);
    EXPECT_EQ(metadata->type, FileMetadata::Type::SymbolicLink);

    targetMetadata = FileSystem::fileMetadataFollowingSymlinks(tempFileSymlinkPath());
    EXPECT_TRUE(!targetMetadata);

    targetMetadata = FileSystem::fileMetadataFollowingSymlinks(symlinkPath);
    EXPECT_TRUE(!targetMetadata);

    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
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

TEST_F(FileSystemTest, moveDirectory)
{
    FileSystem::PlatformFileHandle temporaryFile;
    auto temporaryTestFolder = FileSystem::openTemporaryFile("moveDirectoryTest", temporaryFile);
    FileSystem::closeFile(temporaryFile);

    EXPECT_TRUE(FileSystem::deleteFile(temporaryTestFolder));
    EXPECT_TRUE(FileSystem::makeAllDirectories(temporaryTestFolder));
    auto testFilePath = FileSystem::pathByAppendingComponent(temporaryTestFolder, "testFile");
    auto fileHandle = FileSystem::openFile(testFilePath, FileSystem::FileOpenMode::Write);
    FileSystem::writeToFile(fileHandle, FileSystemTestData, strlen(FileSystemTestData));
    FileSystem::closeFile(fileHandle);

    EXPECT_TRUE(FileSystem::fileExists(testFilePath));

    auto destinationPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "moveDirectoryTest");
    EXPECT_TRUE(FileSystem::moveFile(temporaryTestFolder, destinationPath));
    EXPECT_FALSE(FileSystem::fileExists(temporaryTestFolder));
    EXPECT_FALSE(FileSystem::fileExists(testFilePath));
    EXPECT_TRUE(FileSystem::fileExists(destinationPath));
    EXPECT_TRUE(FileSystem::fileExists(FileSystem::pathByAppendingComponent(destinationPath, "testFile")));

    EXPECT_FALSE(FileSystem::deleteEmptyDirectory(destinationPath));
    EXPECT_TRUE(FileSystem::fileExists(destinationPath));
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

TEST_F(FileSystemTest, isDirectory)
{
    EXPECT_TRUE(FileSystem::isDirectory(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::isDirectoryFollowingSymlinks(tempEmptyFolderPath()));

    auto folderSymlinkMetadata = FileSystem::fileMetadata(tempEmptyFolderSymlinkPath());
    EXPECT_TRUE(!!folderSymlinkMetadata);
    EXPECT_EQ(folderSymlinkMetadata->type, FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(FileSystem::isDirectory(tempEmptyFolderSymlinkPath()));
    EXPECT_TRUE(FileSystem::isDirectoryFollowingSymlinks(tempEmptyFolderSymlinkPath()));

    EXPECT_FALSE(FileSystem::isDirectory(tempFilePath()));
    EXPECT_FALSE(FileSystem::isDirectoryFollowingSymlinks(tempFilePath()));

    auto fileSymlinkMetadata = FileSystem::fileMetadata(tempFileSymlinkPath());
    EXPECT_TRUE(!!fileSymlinkMetadata);
    EXPECT_EQ(fileSymlinkMetadata->type, FileMetadata::Type::SymbolicLink);
    EXPECT_FALSE(FileSystem::isDirectory(tempFileSymlinkPath()));
    EXPECT_FALSE(FileSystem::isDirectoryFollowingSymlinks(tempFileSymlinkPath()));

    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::isDirectory(fileThatDoesNotExist));
    EXPECT_FALSE(FileSystem::isDirectoryFollowingSymlinks(fileThatDoesNotExist));
}

TEST_F(FileSystemTest, makeAllDirectories)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::isDirectory(tempEmptyFolderPath()));
    EXPECT_TRUE(FileSystem::makeAllDirectories(tempEmptyFolderPath()));
    String subFolderPath = FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subFolder1", "subFolder2", "subFolder3" });
    EXPECT_FALSE(FileSystem::fileExists(subFolderPath));
    EXPECT_TRUE(FileSystem::makeAllDirectories(subFolderPath));
    EXPECT_TRUE(FileSystem::fileExists(subFolderPath));
    EXPECT_TRUE(FileSystem::isDirectory(subFolderPath));
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

    EXPECT_FALSE(FileSystem::isDirectory(symlinkPath));
    EXPECT_TRUE(FileSystem::isDirectoryFollowingSymlinks(symlinkPath));

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

TEST_F(FileSystemTest, hardLinkCount)
{
    auto linkCount = FileSystem::hardLinkCount(tempFilePath());
    ASSERT_TRUE(!!linkCount);
    EXPECT_EQ(*linkCount, 1U);

    auto hardlink1Path = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-hardlink1");
    EXPECT_TRUE(FileSystem::hardLink(tempFilePath(), hardlink1Path));
    linkCount = FileSystem::hardLinkCount(tempFilePath());
    ASSERT_TRUE(!!linkCount);
    EXPECT_EQ(*linkCount, 2U);

    auto hardlink2Path = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-hardlink2");
    EXPECT_TRUE(FileSystem::hardLink(tempFilePath(), hardlink2Path));
    linkCount = FileSystem::hardLinkCount(tempFilePath());
    ASSERT_TRUE(!!linkCount);
    EXPECT_EQ(*linkCount, 3U);

    EXPECT_TRUE(FileSystem::deleteFile(hardlink1Path));
    linkCount = FileSystem::hardLinkCount(tempFilePath());
    ASSERT_TRUE(!!linkCount);
    EXPECT_EQ(*linkCount, 2U);

    EXPECT_TRUE(FileSystem::deleteFile(hardlink2Path));
    linkCount = FileSystem::hardLinkCount(tempFilePath());
    ASSERT_TRUE(!!linkCount);
    EXPECT_EQ(*linkCount, 1U);

    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    linkCount = FileSystem::hardLinkCount(tempFilePath());
    EXPECT_TRUE(!linkCount);
}

static void runGetFileModificationTimeTest(const String& path, Function<Optional<WallTime>(const String&)>&& getFileModificationTime)
{
    auto modificationTime = getFileModificationTime(path);
    EXPECT_TRUE(!!modificationTime);
    if (!modificationTime)
        return;

    unsigned timeout = 0;
    while (*modificationTime >= WallTime::now() && ++timeout < 20)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_LT(modificationTime->secondsSinceEpoch().value(), WallTime::now().secondsSinceEpoch().value());

    auto timeBeforeModification = WallTime::now();

    TestWebKitAPI::Util::sleep(2);

    // Modify the file.
    auto fileHandle = FileSystem::openFile(path, FileSystem::FileOpenMode::ReadWrite);
    EXPECT_TRUE(FileSystem::isHandleValid(fileHandle));
    FileSystem::writeToFile(fileHandle, "foo", strlen("foo"));
    FileSystem::closeFile(fileHandle);

    auto newModificationTime = getFileModificationTime(path);
    EXPECT_TRUE(!!newModificationTime);
    if (!newModificationTime)
        return;

    EXPECT_GT(newModificationTime->secondsSinceEpoch().value(), modificationTime->secondsSinceEpoch().value());
    EXPECT_GT(newModificationTime->secondsSinceEpoch().value(), timeBeforeModification.secondsSinceEpoch().value());
}

TEST_F(FileSystemTest, getFileModificationTime)
{
    runGetFileModificationTimeTest(tempFilePath(), [](const String& path) {
        return FileSystem::getFileModificationTime(path);
    });
}

TEST_F(FileSystemTest, getFileModificationTimeViaFileMetadata)
{
    runGetFileModificationTimeTest(tempFilePath(), [](const String& path) -> Optional<WallTime> {
        auto metadata = FileSystem::fileMetadata(path);
        if (!metadata)
            return WTF::nullopt;
        return metadata->modificationTime;
    });
}

TEST_F(FileSystemTest, pathFileName)
{
    auto testPath = FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subfolder", "filename.txt" });
    EXPECT_STREQ("filename.txt", FileSystem::pathFileName(testPath).utf8().data());

#if OS(UNIX)
    EXPECT_STREQ(".", FileSystem::pathFileName(".").utf8().data());
    EXPECT_STREQ("..", FileSystem::pathFileName("..").utf8().data());
    EXPECT_STREQ("", FileSystem::pathFileName("/").utf8().data());
    EXPECT_STREQ(".", FileSystem::pathFileName("/foo/.").utf8().data());
    EXPECT_STREQ("..", FileSystem::pathFileName("/foo/..").utf8().data());
    EXPECT_STREQ("", FileSystem::pathFileName("/foo/").utf8().data());
    EXPECT_STREQ("host", FileSystem::pathFileName("//host").utf8().data());
#endif
#if OS(WINDOWS)
    EXPECT_STREQ("", FileSystem::pathFileName("C:\\").utf8().data());
    EXPECT_STREQ("foo", FileSystem::pathFileName("C:\\foo").utf8().data());
    EXPECT_STREQ("", FileSystem::pathFileName("C:\\foo\\").utf8().data());
    EXPECT_STREQ("bar.txt", FileSystem::pathFileName("C:\\foo\\bar.txt").utf8().data());
#endif
}

TEST_F(FileSystemTest, parentPath)
{
    auto testPath = FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subfolder", "filename.txt" });
    EXPECT_STREQ(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "subfolder").utf8().data(), FileSystem::parentPath(testPath).utf8().data());
#if OS(UNIX)
    EXPECT_STREQ("/var/tmp", FileSystem::parentPath("/var/tmp/example.txt").utf8().data());
    EXPECT_STREQ("/var/tmp", FileSystem::parentPath("/var/tmp/").utf8().data());
    EXPECT_STREQ("/var/tmp", FileSystem::parentPath("/var/tmp/.").utf8().data());
    EXPECT_STREQ("/", FileSystem::parentPath("/").utf8().data());
#endif
#if OS(WINDOWS)
    EXPECT_STREQ("C:\\foo", FileSystem::parentPath("C:\\foo\\example.txt").utf8().data());
    EXPECT_STREQ("C:\\", FileSystem::parentPath("C:\\").utf8().data());
#endif
}

TEST_F(FileSystemTest, pathByAppendingComponent)
{
#if OS(UNIX)
    EXPECT_STREQ("/var", FileSystem::pathByAppendingComponent("/", "var").utf8().data());
    EXPECT_STREQ("/var/tmp", FileSystem::pathByAppendingComponent("/var/", "tmp").utf8().data());
    EXPECT_STREQ("/var/tmp", FileSystem::pathByAppendingComponent("/var", "tmp").utf8().data());
    EXPECT_STREQ("/var/tmp/file.txt", FileSystem::pathByAppendingComponent("/var/tmp", "file.txt").utf8().data());
    EXPECT_STREQ("/var/", FileSystem::pathByAppendingComponent("/var", "").utf8().data());
    EXPECT_STREQ("/var/", FileSystem::pathByAppendingComponent("/var/", "").utf8().data());
#endif
#if OS(WINDOWS)
    EXPECT_STREQ("C:\\Foo", FileSystem::pathByAppendingComponent("C:\\", "Foo").utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar", FileSystem::pathByAppendingComponent("C:\\Foo", "Bar").utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponent("C:\\Foo\\Bar", "File.txt").utf8().data());
#endif
}

TEST_F(FileSystemTest, pathByAppendingComponents)
{
    EXPECT_STREQ(tempEmptyFolderPath().utf8().data(), FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { }).utf8().data());
    EXPECT_STREQ(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "file.txt").utf8().data(), FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "file.txt" }).utf8().data());
#if OS(UNIX)
    EXPECT_STREQ("/var/tmp/file.txt", FileSystem::pathByAppendingComponents("/", { "var", "tmp", "file.txt" }).utf8().data());
    EXPECT_STREQ("/var/tmp/file.txt", FileSystem::pathByAppendingComponents("/var", { "tmp", "file.txt" }).utf8().data());
    EXPECT_STREQ("/var/tmp/file.txt", FileSystem::pathByAppendingComponents("/var/", { "tmp", "file.txt" }).utf8().data());
    EXPECT_STREQ("/var/tmp/file.txt", FileSystem::pathByAppendingComponents("/var/tmp", { "file.txt" }).utf8().data());
#endif
#if OS(WINDOWS)
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponents("C:\\", { "Foo", "Bar", "File.txt" }).utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponents("C:\\Foo", { "Bar", "File.txt" }).utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponents("C:\\Foo\\", { "Bar", "File.txt" }).utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponents("C:\\Foo\\Bar", { "File.txt" }).utf8().data());
    EXPECT_STREQ("C:\\Foo\\Bar\\File.txt", FileSystem::pathByAppendingComponents("C:\\Foo\\Bar\\", { "File.txt" }).utf8().data());
#endif
}

TEST_F(FileSystemTest, listDirectory)
{
    createTestFile(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "a.txt"));
    createTestFile(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "b.txt"));
    createTestFile(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "bar.png"));
    createTestFile(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "foo.png"));
    FileSystem::makeAllDirectories(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "subfolder"));
    createTestFile(FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subfolder", "c.txt" }));
    createTestFile(FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subfolder", "d.txt" }));

    auto matches = FileSystem::listDirectory(tempEmptyFolderPath());
    ASSERT_EQ(matches.size(), 5U);
    std::sort(matches.begin(), matches.end(), WTF::codePointCompareLessThan);
    EXPECT_STREQ(matches[0].utf8().data(), "a.txt");
    EXPECT_STREQ(matches[1].utf8().data(), "b.txt");
    EXPECT_STREQ(matches[2].utf8().data(), "bar.png");
    EXPECT_STREQ(matches[3].utf8().data(), "foo.png");
    EXPECT_STREQ(matches[4].utf8().data(), "subfolder");

    matches = FileSystem::listDirectory(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "subfolder"));
    ASSERT_EQ(matches.size(), 2U);
    std::sort(matches.begin(), matches.end(), WTF::codePointCompareLessThan);
    EXPECT_STREQ(matches[0].utf8().data(), "c.txt");
    EXPECT_STREQ(matches[1].utf8().data(), "d.txt");

    matches = FileSystem::listDirectory(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"));
    ASSERT_EQ(matches.size(), 0U);

    matches = FileSystem::listDirectory(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "a.txt"));
    ASSERT_EQ(matches.size(), 0U);

    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
}

} // namespace TestWebKitAPI
