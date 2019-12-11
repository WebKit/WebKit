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

#include <wtf/SoftLinking.h>

typedef struct _BOMCopier* BOMCopier;

SOFT_LINK_PRIVATE_FRAMEWORK(Bom)
SOFT_LINK(Bom, BOMCopierNew, BOMCopier, (), ())
SOFT_LINK(Bom, BOMCopierFree, void, (BOMCopier copier), (copier))
SOFT_LINK(Bom, BOMCopierCopyWithOptions, int, (BOMCopier copier, const char* fromObj, const char* toObj, CFDictionaryRef options), (copier, fromObj, toObj, options))

#define kBOMCopierOptionCreatePKZipKey CFSTR("createPKZip")
#define kBOMCopierOptionSequesterResourcesKey CFSTR("sequesterResources")
#define kBOMCopierOptionKeepParentKey CFSTR("keepParent")
#define kBOMCopierOptionCopyResourcesKey CFSTR("copyResources")

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

String createTemporaryZipArchive(const String& path)
{
    String temporaryFile;
    
    RetainPtr<NSFileCoordinator> coordinator = adoptNS([[NSFileCoordinator alloc] initWithFilePresenter:nil]);
    [coordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:path] options:NSFileCoordinatorReadingWithoutChanges error:nullptr byAccessor:[&](NSURL *newURL) mutable {
        CString archivePath([NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitGeneratedFileXXXXXX"].fileSystemRepresentation);
        if (mkstemp(archivePath.mutableData()) == -1)
            return;
        
        NSDictionary *options = @{
            (__bridge id)kBOMCopierOptionCreatePKZipKey : @YES,
            (__bridge id)kBOMCopierOptionSequesterResourcesKey : @YES,
            (__bridge id)kBOMCopierOptionKeepParentKey : @YES,
            (__bridge id)kBOMCopierOptionCopyResourcesKey : @YES,
        };
        
        BOMCopier copier = BOMCopierNew();
        if (!BOMCopierCopyWithOptions(copier, newURL.path.fileSystemRepresentation, archivePath.data(), (__bridge CFDictionaryRef)options))
            temporaryFile = String::fromUTF8(archivePath);
        BOMCopierFree(copier);
    }];
    
    return temporaryFile;
}

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

#if PLATFORM(IOS_FAMILY)
bool isSafeToUseMemoryMapForPath(const String& path)
{
    NSError *error = nil;
    NSDictionary<NSFileAttributeKey, id> *attributes = [[NSFileManager defaultManager] attributesOfItemAtPath:path error:&error];
    if (error) {
        LOG_ERROR("Unable to get path protection class");
        return false;
    }
    if ([[attributes objectForKey:NSFileProtectionKey] isEqualToString:NSFileProtectionComplete]) {
        LOG_ERROR("Path protection class is NSFileProtectionComplete, so it is not safe to use memory map");
        return false;
    }
    return true;
}

void makeSafeToUseMemoryMapForPath(const String& path)
{
    if (isSafeToUseMemoryMapForPath(path))
        return;
    
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] setAttributes:@{ NSFileProtectionKey: NSFileProtectionCompleteUnlessOpen } ofItemAtPath:path error:&error];
    ASSERT(!error);
    ASSERT_UNUSED(success, success);
}
#endif

} // namespace FileSystemImpl
} // namespace WTF
