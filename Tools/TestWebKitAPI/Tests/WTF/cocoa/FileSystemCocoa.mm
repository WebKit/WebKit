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
#import <wtf/FileSystem.h>

#import "Helpers/PlatformUtilities.h"
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

static NSSet<NSString *> *temporaryEntriesWithPrefix(NSString *prefix)
{
    NSArray *entries = [NSFileManager.defaultManager contentsOfDirectoryAtPath:NSTemporaryDirectory() error:nil];
    return [NSSet setWithArray:[entries filteredArrayUsingPredicate:[NSPredicate predicateWithFormat:@"SELF BEGINSWITH %@", prefix]]];
}

static RetainPtr<NSString> createFileThatIsNotAZipArchive()
{
    RetainPtr path = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString];
    [@"this is not a zip archive" writeToFile:path.get() atomically:YES encoding:NSUTF8StringEncoding error:nil];
    return path;
}

TEST(FileSystemCocoa, ExtractTemporaryZipArchiveFailureDoesNotLeaveDirectory)
{
    RetainPtr sourcePath = createFileThatIsNotAZipArchive();
    RetainPtr before = temporaryEntriesWithPrefix(@"WebKitExtractedArchive");

    auto result = FileSystem::extractTemporaryZipArchive(String { sourcePath.get() });

    RetainPtr after = temporaryEntriesWithPrefix(@"WebKitExtractedArchive");
    [NSFileManager.defaultManager removeItemAtPath:sourcePath.get() error:nil];

    EXPECT_TRUE(result.isNull());
    EXPECT_TRUE([before isEqualToSet:after.get()]);
}

TEST(FileSystemCocoa, CreateTemporaryZipArchiveFailureDoesNotLeaveFile)
{
    RetainPtr missingPath = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString];
    RetainPtr before = temporaryEntriesWithPrefix(@"WebKitGeneratedFile");

    auto result = FileSystem::createTemporaryZipArchive(String { missingPath.get() });

    RetainPtr after = temporaryEntriesWithPrefix(@"WebKitGeneratedFile");

    EXPECT_TRUE(result.isNull());
    EXPECT_TRUE([before isEqualToSet:after.get()]);
}

TEST(FileSystemCocoa, CreateAndExtractTemporaryZipArchiveRoundTrip)
{
    RetainPtr directory = [NSTemporaryDirectory() stringByAppendingPathComponent:[NSUUID UUID].UUIDString];
    [NSFileManager.defaultManager createDirectoryAtPath:directory.get() withIntermediateDirectories:YES attributes:nil error:nil];
    [@"hello" writeToFile:[directory stringByAppendingPathComponent:@"a.txt"] atomically:YES encoding:NSUTF8StringEncoding error:nil];

    auto archive = FileSystem::createTemporaryZipArchive(String { directory.get() });
    EXPECT_FALSE(archive.isNull());

    auto extracted = FileSystem::extractTemporaryZipArchive(archive);
    EXPECT_FALSE(extracted.isNull());
    EXPECT_TRUE([NSFileManager.defaultManager fileExistsAtPath:[extracted.createNSString() stringByAppendingPathComponent:@"a.txt"]]);

    FileSystem::deleteFile(archive);
    FileSystem::deleteNonEmptyDirectory(extracted);
    [NSFileManager.defaultManager removeItemAtPath:directory.get() error:nil];
}
