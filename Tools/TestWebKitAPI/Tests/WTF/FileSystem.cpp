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

// FIXME: Refactor FileSystemTest and FragmentedSharedBufferTest as a single class.
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
        FileSystem::deleteNonEmptyDirectory(m_tempEmptyFolderPath);
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

TEST_F(FileSystemTest, fileType)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    EXPECT_FALSE(FileSystem::fileType(doesNotExistPath));

    EXPECT_EQ(FileSystem::fileType(tempFilePath()), FileSystem::FileType::Regular);
    EXPECT_EQ(FileSystem::fileType(tempFileSymlinkPath()), FileSystem::FileType::SymbolicLink);
    EXPECT_EQ(FileSystem::fileType(tempEmptyFolderSymlinkPath()), FileSystem::FileType::SymbolicLink);
    EXPECT_EQ(FileSystem::fileType(tempEmptyFolderPath()), FileSystem::FileType::Directory);

    // Symlink to file symlink case.
    auto symlinkToFileSymlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlinkToSymlink");
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkToFileSymlinkPath));
    EXPECT_EQ(FileSystem::fileType(symlinkToFileSymlinkPath), FileSystem::FileType::SymbolicLink);

    // Symlink to directory symlink case.
    FileSystem::PlatformFileHandle handle;
    auto symlinkToDirectorySymlinkPath = FileSystem::openTemporaryFile("tempTestFile-symlink", handle);
    FileSystem::closeFile(handle);
    FileSystem::deleteFile(symlinkToDirectorySymlinkPath);
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempEmptyFolderSymlinkPath(), symlinkToDirectorySymlinkPath));
    EXPECT_EQ(FileSystem::fileType(symlinkToDirectorySymlinkPath), FileSystem::FileType::SymbolicLink);

    // Broken file symlink case.
    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_EQ(FileSystem::fileType(tempFileSymlinkPath()), FileSystem::FileType::SymbolicLink);

    // Broken directory symlink case.
    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_EQ(FileSystem::fileType(tempEmptyFolderSymlinkPath()), FileSystem::FileType::SymbolicLink);
}

TEST_F(FileSystemTest, fileTypeFollowingSymlinks)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    EXPECT_FALSE(FileSystem::fileTypeFollowingSymlinks(doesNotExistPath));

    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(tempFilePath()), FileSystem::FileType::Regular);
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(tempFileSymlinkPath()), FileSystem::FileType::Regular);
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(tempEmptyFolderSymlinkPath()), FileSystem::FileType::Directory);
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(tempEmptyFolderPath()), FileSystem::FileType::Directory);

    // Symlink to file symlink case.
    auto symlinkToFileSymlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlinkToSymlink");
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkToFileSymlinkPath));
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(symlinkToFileSymlinkPath), FileSystem::FileType::Regular);

    // Symlink to directory symlink case.
    FileSystem::PlatformFileHandle handle;
    auto symlinkToDirectorySymlinkPath = FileSystem::openTemporaryFile("tempTestFile-symlink", handle);
    FileSystem::closeFile(handle);
    FileSystem::deleteFile(symlinkToDirectorySymlinkPath);
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempEmptyFolderSymlinkPath(), symlinkToDirectorySymlinkPath));
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(symlinkToDirectorySymlinkPath), FileSystem::FileType::Directory);

    // Broken file symlink case.
    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileTypeFollowingSymlinks(tempFileSymlinkPath()));

    // Broken directory symlink case.
    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileTypeFollowingSymlinks(tempEmptyFolderSymlinkPath()));
}

#if OS(UNIX)
TEST_F(FileSystemTest, isHiddenFile)
{
    auto hiddenFilePath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), ".hiddenFile");
    EXPECT_TRUE(FileSystem::isHiddenFile(hiddenFilePath));

    EXPECT_FALSE(FileSystem::isHiddenFile(tempFilePath()));
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
    EXPECT_EQ(FileSystem::fileType(symlinkPath), FileSystem::FileType::SymbolicLink);
    EXPECT_TRUE(FileSystem::deleteFile(symlinkPath));
}

TEST_F(FileSystemTest, fileExistsSymlinkToSymlink)
{
    // Create a valid symlink to a symlink to a regular file.
    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlink"_s);
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(symlinkPath));
    EXPECT_EQ(FileSystem::fileType(symlinkPath), FileSystem::FileType::SymbolicLink);
    EXPECT_EQ(FileSystem::fileTypeFollowingSymlinks(symlinkPath), FileSystem::FileType::Regular);

    // Break the symlink by deleting the target.
    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));

    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFileSymlinkPath())); // fileExists() follows symlinks.
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath)); // fileExists() follows symlinks.

    EXPECT_EQ(FileSystem::fileType(symlinkPath), FileSystem::FileType::SymbolicLink);
    EXPECT_EQ(FileSystem::fileType(tempFileSymlinkPath()), FileSystem::FileType::SymbolicLink);

    EXPECT_FALSE(FileSystem::fileTypeFollowingSymlinks(tempFileSymlinkPath()));
    EXPECT_FALSE(FileSystem::fileTypeFollowingSymlinks(symlinkPath));

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

    auto fileSize = FileSystem::fileSize(tempFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_GT(*fileSize, 0U);

    fileSize = FileSystem::fileSize(tempEmptyFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_EQ(*fileSize, 0U);

    EXPECT_TRUE(FileSystem::moveFile(tempFilePath(), tempEmptyFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));

    fileSize = FileSystem::fileSize(tempEmptyFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_GT(*fileSize, 0U);
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

TEST_F(FileSystemTest, fileSize)
{
    EXPECT_TRUE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFilePath()));

    auto fileSize = FileSystem::fileSize(tempFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_GT(*fileSize, 0U);

    fileSize = FileSystem::fileSize(tempEmptyFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_EQ(*fileSize, 0U);

    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    fileSize = FileSystem::fileSize(fileThatDoesNotExist);
    EXPECT_TRUE(!fileSize);
}

TEST_F(FileSystemTest, makeAllDirectories)
{
    EXPECT_TRUE(FileSystem::fileExists(tempEmptyFolderPath()));
    EXPECT_EQ(FileSystem::fileType(tempEmptyFolderPath()), FileSystem::FileType::Directory);
    EXPECT_TRUE(FileSystem::makeAllDirectories(tempEmptyFolderPath()));
    String subFolderPath = FileSystem::pathByAppendingComponents(tempEmptyFolderPath(), { "subFolder1", "subFolder2", "subFolder3" });
    EXPECT_FALSE(FileSystem::fileExists(subFolderPath));
    EXPECT_TRUE(FileSystem::makeAllDirectories(subFolderPath));
    EXPECT_TRUE(FileSystem::fileExists(subFolderPath));
    EXPECT_EQ(FileSystem::fileType(subFolderPath), FileSystem::FileType::Directory);
    EXPECT_TRUE(FileSystem::deleteNonEmptyDirectory(tempEmptyFolderPath()));
    EXPECT_FALSE(FileSystem::fileExists(subFolderPath));
}

TEST_F(FileSystemTest, volumeFreeSpace)
{
    auto freeSpace = FileSystem::volumeFreeSpace(tempFilePath());
    ASSERT_TRUE(freeSpace);
    EXPECT_GT(*freeSpace, 0U);

    String fileThatDoesNotExist = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist"_s);
    EXPECT_FALSE(FileSystem::volumeFreeSpace(fileThatDoesNotExist));
}

TEST_F(FileSystemTest, createSymbolicLink)
{
    auto symlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-symlink");
    EXPECT_FALSE(FileSystem::fileExists(symlinkPath));
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFilePath(), symlinkPath));
    EXPECT_TRUE(FileSystem::fileExists(symlinkPath));

    EXPECT_EQ(FileSystem::fileType(symlinkPath), FileSystem::FileType::SymbolicLink);

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

    EXPECT_EQ(FileSystem::fileType(symlinkPath), FileSystem::FileType::SymbolicLink);

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

    auto fileSize = FileSystem::fileSize(tempFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_GT(*fileSize, 0U);

    EXPECT_TRUE(FileSystem::hardLink(tempFilePath(), hardlinkPath));

    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    auto linkFileSize = FileSystem::fileSize(hardlinkPath);
    ASSERT_TRUE(linkFileSize);
    EXPECT_EQ(*linkFileSize, *fileSize);

    EXPECT_EQ(FileSystem::fileType(hardlinkPath), FileSystem::FileType::Regular);

    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    linkFileSize = FileSystem::fileSize(hardlinkPath);
    ASSERT_TRUE(linkFileSize);
    EXPECT_EQ(*linkFileSize, *fileSize);
}

TEST_F(FileSystemTest, createHardLinkOrCopyFile)
{
    auto hardlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "tempFile-hardlink");
    EXPECT_FALSE(FileSystem::fileExists(hardlinkPath));

    auto fileSize = FileSystem::fileSize(tempFilePath());
    ASSERT_TRUE(fileSize);
    EXPECT_GT(*fileSize, 0U);

    EXPECT_TRUE(FileSystem::hardLinkOrCopyFile(tempFilePath(), hardlinkPath));

    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    auto linkFileSize = FileSystem::fileSize(hardlinkPath);
    ASSERT_TRUE(linkFileSize);
    EXPECT_EQ(*linkFileSize, *fileSize);

    EXPECT_EQ(FileSystem::fileType(hardlinkPath), FileSystem::FileType::Regular);

    EXPECT_TRUE(FileSystem::deleteFile(tempFilePath()));
    EXPECT_FALSE(FileSystem::fileExists(tempFilePath()));
    EXPECT_TRUE(FileSystem::fileExists(hardlinkPath));

    linkFileSize = FileSystem::fileSize(hardlinkPath);
    ASSERT_TRUE(linkFileSize);
    EXPECT_EQ(*linkFileSize, *fileSize);
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

static void runGetFileModificationTimeTest(const String& path, Function<std::optional<WallTime>(const String&)>&& fileModificationTime)
{
    auto modificationTime = fileModificationTime(path);
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

    auto newModificationTime = fileModificationTime(path);
    EXPECT_TRUE(!!newModificationTime);
    if (!newModificationTime)
        return;

    EXPECT_GT(newModificationTime->secondsSinceEpoch().value(), modificationTime->secondsSinceEpoch().value());
    EXPECT_GT(newModificationTime->secondsSinceEpoch().value(), timeBeforeModification.secondsSinceEpoch().value());
}

TEST_F(FileSystemTest, fileModificationTime)
{
    runGetFileModificationTimeTest(tempFilePath(), [](const String& path) {
        return FileSystem::fileModificationTime(path);
    });
}

TEST_F(FileSystemTest, updateFileModificationTime)
{
    auto modificationTime = FileSystem::fileModificationTime(tempFilePath());
    ASSERT_TRUE(!!modificationTime);

    unsigned timeout = 0;
    while (*modificationTime >= WallTime::now() && ++timeout < 20)
        TestWebKitAPI::Util::sleep(0.1);
    EXPECT_LT(modificationTime->secondsSinceEpoch().value(), WallTime::now().secondsSinceEpoch().value());

    TestWebKitAPI::Util::sleep(1);

    EXPECT_TRUE(FileSystem::updateFileModificationTime(tempFilePath()));
    auto newModificationTime = FileSystem::fileModificationTime(tempFilePath());
    ASSERT_TRUE(!!newModificationTime);
    EXPECT_GT(newModificationTime->secondsSinceEpoch().value(), modificationTime->secondsSinceEpoch().value());

    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    EXPECT_FALSE(FileSystem::updateFileModificationTime(doesNotExistPath));
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

TEST_F(FileSystemTest, realPath)
{
    auto doesNotExistPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist");
    EXPECT_STREQ(FileSystem::realPath(doesNotExistPath).utf8().data(), doesNotExistPath.utf8().data());

    auto resolvedTempFilePath = FileSystem::realPath(tempFilePath());
    EXPECT_STREQ(FileSystem::realPath(resolvedTempFilePath).utf8().data(), resolvedTempFilePath.utf8().data());
    EXPECT_STREQ(FileSystem::realPath(tempFileSymlinkPath()).utf8().data(), resolvedTempFilePath.utf8().data()); // Should resolve file symlink.

    auto resolvedTempEmptyFolderPath = FileSystem::realPath(tempEmptyFolderPath());
    EXPECT_STREQ(FileSystem::realPath(resolvedTempEmptyFolderPath).utf8().data(), resolvedTempEmptyFolderPath.utf8().data());
    EXPECT_STREQ(FileSystem::realPath(tempEmptyFolderSymlinkPath()).utf8().data(), resolvedTempEmptyFolderPath.utf8().data()); // Should resolve directory symlink.

    // Symlink to symlink case.
    auto symlinkToSymlinkPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "symlinkToSymlink");
    EXPECT_TRUE(FileSystem::createSymbolicLink(tempFileSymlinkPath(), symlinkToSymlinkPath));
    EXPECT_STREQ(FileSystem::realPath(symlinkToSymlinkPath).utf8().data(), resolvedTempFilePath.utf8().data()); // Should resolve all symlinks.

    auto subFolderPath = FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "subfolder");
    FileSystem::makeAllDirectories(subFolderPath);
    auto resolvedSubFolderPath = FileSystem::realPath(subFolderPath);
    EXPECT_STREQ(FileSystem::realPath(FileSystem::pathByAppendingComponent(subFolderPath, "..")).utf8().data(), resolvedTempEmptyFolderPath.utf8().data()); // Should resolve "..".
    EXPECT_STREQ(FileSystem::realPath(FileSystem::pathByAppendingComponents(subFolderPath, { "..", "subfolder" })).utf8().data(), resolvedSubFolderPath.utf8().data()); // Should resolve "..".
    EXPECT_STREQ(FileSystem::realPath(FileSystem::pathByAppendingComponents(subFolderPath, { "..", ".", ".", "subfolder" })).utf8().data(), resolvedSubFolderPath.utf8().data()); // Should resolve ".." and "."
}

TEST_F(FileSystemTest, readEntireFile)
{
    EXPECT_FALSE(FileSystem::readEntireFile(FileSystem::invalidPlatformFileHandle));
    EXPECT_FALSE(FileSystem::readEntireFile(emptyString()));
    EXPECT_FALSE(FileSystem::readEntireFile(FileSystem::pathByAppendingComponent(tempEmptyFolderPath(), "does-not-exist")));
    EXPECT_FALSE(FileSystem::readEntireFile(tempEmptyFilePath()));

    auto buffer = FileSystem::readEntireFile(tempFilePath());
    EXPECT_TRUE(buffer);
    auto contents = String::adopt(WTFMove(buffer.value()));
    EXPECT_STREQ(contents.utf8().data(), FileSystemTestData);
}

} // namespace TestWebKitAPI
