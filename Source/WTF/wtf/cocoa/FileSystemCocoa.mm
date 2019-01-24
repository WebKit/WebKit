/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import <wtf/FileSystem.h>

@interface WTFWebFileManagerDelegate : NSObject <NSFileManagerDelegate>
@end

@implementation WTFWebFileManagerDelegate

- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSError *)error movingItemAtURL:(NSURL *)srcURL toURL:(NSURL *)dstURL
{
    UNUSED_PARAM(fileManager);
    UNUSED_PARAM(srcURL);
    UNUSED_PARAM(dstURL);
    return error.code == NSFileWriteFileExistsError;
}

@end

namespace WTF {

namespace FileSystemImpl {

String homeDirectoryPath()
{
    return NSHomeDirectory();
}

String openTemporaryFile(const String& prefix, PlatformFileHandle& platformFileHandle)
{
    platformFileHandle = invalidPlatformFileHandle;

    Vector<char> temporaryFilePath(PATH_MAX);
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryFilePath.data(), temporaryFilePath.size()))
        return String();

    // Shrink the vector.
    temporaryFilePath.shrink(strlen(temporaryFilePath.data()));

    // FIXME: Change to a runtime assertion that the path ends with a slash once <rdar://problem/23579077> is
    // fixed in all iOS Simulator versions that we use.
    if (temporaryFilePath.last() != '/')
        temporaryFilePath.append('/');

    // Append the file name.
    CString prefixUtf8 = prefix.utf8();
    temporaryFilePath.append(prefixUtf8.data(), prefixUtf8.length());
    temporaryFilePath.append("XXXXXX", 6);
    temporaryFilePath.append('\0');

    platformFileHandle = mkstemp(temporaryFilePath.data());
    if (platformFileHandle == invalidPlatformFileHandle)
        return String();

    return String::fromUTF8(temporaryFilePath.data());
}

bool moveFile(const String& oldPath, const String& newPath)
{
    // Overwrite existing files.
    auto manager = adoptNS([[NSFileManager alloc] init]);
    auto delegate = adoptNS([[WTFWebFileManagerDelegate alloc] init]);
    [manager setDelegate:delegate.get()];

    NSError *error = nil;
    bool success = [manager moveItemAtURL:[NSURL fileURLWithPath:oldPath] toURL:[NSURL fileURLWithPath:newPath] error:&error];
    if (!success)
        NSLog(@"Error in moveFile: %@", error);
    return success;
}

bool getVolumeFreeSpace(const String& path, uint64_t& freeSpace)
{
    NSDictionary *fileSystemAttributesDictionary = [[NSFileManager defaultManager] attributesOfFileSystemForPath:(NSString *)path error:NULL];
    if (!fileSystemAttributesDictionary)
        return false;
    freeSpace = [[fileSystemAttributesDictionary objectForKey:NSFileSystemFreeSize] unsignedLongLongValue];
    return true;
}

NSString *createTemporaryDirectory(NSString *directoryPrefix)
{
    NSString *tempDirectory = NSTemporaryDirectory();
    if (!tempDirectory || ![tempDirectory length])
        return nil;

    if (!directoryPrefix || ![directoryPrefix length])
        return nil;

    NSString *tempDirectoryComponent = [directoryPrefix stringByAppendingString:@"-XXXXXXXX"];
    const char* tempDirectoryCString = [[tempDirectory stringByAppendingPathComponent:tempDirectoryComponent] fileSystemRepresentation];
    if (!tempDirectoryCString)
        return nil;

    const size_t length = strlen(tempDirectoryCString);
    ASSERT(length <= MAXPATHLEN);
    if (length > MAXPATHLEN)
        return nil;

    const size_t lengthPlusNullTerminator = length + 1;
    Vector<char, MAXPATHLEN + 1> path(lengthPlusNullTerminator);
    memcpy(path.data(), tempDirectoryCString, lengthPlusNullTerminator);

    if (!mkdtemp(path.data()))
        return nil;

    return [[NSFileManager defaultManager] stringWithFileSystemRepresentation:path.data() length:length];
}

bool deleteNonEmptyDirectory(const String& path)
{
    return [[NSFileManager defaultManager] removeItemAtPath:path error:nil];
}

} // namespace FileSystemImpl
} // namespace WTF
