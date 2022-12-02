/*
 * Copyright (C) 2007-2022 Apple Inc. All rights reserved.
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

#import <wtf/SoftLinking.h>
#import <sys/resource.h>

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

String openTemporaryFile(StringView prefix, PlatformFileHandle& platformFileHandle, StringView suffix)
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
    CString prefixUTF8 = prefix.utf8();
    temporaryFilePath.append(prefixUTF8.data(), prefixUTF8.length());
    temporaryFilePath.append("XXXXXX", 6);
    
    // Append the file name suffix.
    CString suffixUTF8 = suffix.utf8();
    temporaryFilePath.append(suffixUTF8.data(), suffixUTF8.length());
    temporaryFilePath.append('\0');

    platformFileHandle = mkstemps(temporaryFilePath.data(), suffixUTF8.length());
    if (platformFileHandle == invalidPlatformFileHandle)
        return String();

    return String::fromUTF8(temporaryFilePath.data());
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

#ifdef IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES
static int toIOPolicyScope(PolicyScope scope)
{
    switch (scope) {
    case PolicyScope::Process:
        return IOPOL_SCOPE_PROCESS;
    case PolicyScope::Thread:
        return IOPOL_SCOPE_THREAD;
    }
}
#endif

bool setAllowsMaterializingDatalessFiles(bool allow, PolicyScope scope)
{
#ifdef IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES
    if (setiopolicy_np(IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES, toIOPolicyScope(scope), allow ? IOPOL_MATERIALIZE_DATALESS_FILES_ON : IOPOL_MATERIALIZE_DATALESS_FILES_OFF) == -1) {
        LOG_ERROR("FileSystem::setAllowsMaterializingDatalessFiles(%d): setiopolicy_np call failed, errno: %d", allow, errno);
        return false;
    }
    return true;
#else
    UNUSED_PARAM(allow);
    UNUSED_PARAM(scope);
    return false;
#endif
}

std::optional<bool> allowsMaterializingDatalessFiles(PolicyScope scope)
{
#ifdef IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES
    int ret = getiopolicy_np(IOPOL_TYPE_VFS_MATERIALIZE_DATALESS_FILES, toIOPolicyScope(scope));
    if (ret == IOPOL_MATERIALIZE_DATALESS_FILES_ON)
        return true;
    if (ret == IOPOL_MATERIALIZE_DATALESS_FILES_OFF)
        return false;
    LOG_ERROR("FileSystem::allowsMaterializingDatalessFiles(): getiopolicy_np call failed, errno: %d", errno);
    return std::nullopt;
#else
    UNUSED_PARAM(scope);
    return std::nullopt;
#endif
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

bool makeSafeToUseMemoryMapForPath(const String& path)
{
    if (isSafeToUseMemoryMapForPath(path))
        return true;
    
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] setAttributes:@{ NSFileProtectionKey: NSFileProtectionCompleteUnlessOpen } ofItemAtPath:path error:&error];
    if (error || !success) {
        WTFLogAlways("makeSafeToUseMemoryMapForPath(%s) failed with error %@", path.utf8().data(), error);
        return false;
    }
    return true;
}
#endif

bool setExcludedFromBackup(const String& path, bool excluded)
{
    if (path.isEmpty())
        return false;

    NSError *error;
    if (![[NSURL fileURLWithPath:(NSString *)path isDirectory:YES] setResourceValue:[NSNumber numberWithBool:excluded] forKey:NSURLIsExcludedFromBackupKey error:&error]) {
        LOG_ERROR("Cannot exclude path '%s' from backup with error '%@'", path.utf8().data(), error.localizedDescription);
        return false;
    }

    return true;
}

NSString *systemDirectoryPath()
{
    static NeverDestroyed<RetainPtr<NSString>> path = ^{
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        char *simulatorRoot = getenv("SIMULATOR_ROOT");
        return simulatorRoot ? [NSString stringWithFormat:@"%s/System/", simulatorRoot] : @"/System/";
#else
        return @"/System/";
#endif
    }();

    return path.get().get();
}

} // namespace FileSystemImpl
} // namespace WTF
