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

#import <sys/resource.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

#if HAVE(APFS_CACHEDELETE_PURGEABLE)
#import <apfs/apfs_fsctl.h>
#endif

typedef struct _BOMCopier* BOMCopier;

SOFT_LINK_PRIVATE_FRAMEWORK(Bom)
SOFT_LINK(Bom, BOMCopierNew, BOMCopier, (), ())
SOFT_LINK(Bom, BOMCopierFree, void, (BOMCopier copier), (copier))
SOFT_LINK(Bom, BOMCopierCopyWithOptions, int, (BOMCopier copier, const char* fromObj, const char* toObj, CFDictionaryRef options), (copier, fromObj, toObj, options))

#define kBOMCopierOptionCreatePKZipKey CFSTR("createPKZip")
#define kBOMCopierOptionExtractPKZipKey CFSTR("extractPKZip")
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

    RetainPtr coordinator = adoptNS([[NSFileCoordinator alloc] initWithFilePresenter:nil]);
    [coordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:path] options:NSFileCoordinatorReadingWithoutChanges error:nullptr byAccessor:[&](NSURL *newURL) mutable {
        CString archivePath([NSTemporaryDirectory() stringByAppendingPathComponent:@"WebKitGeneratedFileXXXXXX"].fileSystemRepresentation);
        int fd = mkostemp(archivePath.mutableData(), O_CLOEXEC);
        if (fd == -1)
            return;
        close(fd);

        auto *options = @{
            bridge_id_cast(kBOMCopierOptionCreatePKZipKey): @YES,
            bridge_id_cast(kBOMCopierOptionSequesterResourcesKey): @YES,
            bridge_id_cast(kBOMCopierOptionKeepParentKey): @YES,
            bridge_id_cast(kBOMCopierOptionCopyResourcesKey): @YES,
        };

        auto copier = BOMCopierNew();
        if (!BOMCopierCopyWithOptions(copier, newURL.path.fileSystemRepresentation, archivePath.data(), bridge_cast(options)))
            temporaryFile = String::fromUTF8(archivePath.span());
        BOMCopierFree(copier);
    }];

    return temporaryFile;
}

String extractTemporaryZipArchive(const String& path)
{
    String temporaryDirectory = createTemporaryDirectory(@"WebKitExtractedArchive");
    if (!temporaryDirectory)
        return nullString();

    RetainPtr coordinator = adoptNS([[NSFileCoordinator alloc] initWithFilePresenter:nil]);
    [coordinator coordinateReadingItemAtURL:[NSURL fileURLWithPath:path] options:NSFileCoordinatorReadingWithoutChanges error:nullptr byAccessor:[&](NSURL *newURL) mutable {
        auto *options = @{
            bridge_id_cast(kBOMCopierOptionExtractPKZipKey): @YES,
            bridge_id_cast(kBOMCopierOptionSequesterResourcesKey): @YES,
            bridge_id_cast(kBOMCopierOptionCopyResourcesKey): @YES,
        };

        auto copier = BOMCopierNew();
        if (BOMCopierCopyWithOptions(copier, newURL.path.fileSystemRepresentation, fileSystemRepresentation(temporaryDirectory).data(), bridge_cast(options)))
            temporaryDirectory = nullString();
        BOMCopierFree(copier);
    }];

    return temporaryDirectory;
}

std::pair<String, PlatformFileHandle> openTemporaryFile(StringView prefix, StringView suffix)
{
    PlatformFileHandle platformFileHandle = invalidPlatformFileHandle;

    Vector<char> temporaryFilePath(PATH_MAX);
    if (!confstr(_CS_DARWIN_USER_TEMP_DIR, temporaryFilePath.data(), temporaryFilePath.size()))
        return { String(), invalidPlatformFileHandle };

    // Shrink the vector.
    temporaryFilePath.shrink(strlen(temporaryFilePath.data()));

    ASSERT(temporaryFilePath.last() == '/');

    // Append the file name.
    temporaryFilePath.append(prefix.utf8().span());
    temporaryFilePath.append("XXXXXX"_span);
    
    // Append the file name suffix.
    CString suffixUTF8 = suffix.utf8();
    temporaryFilePath.append(suffixUTF8.spanIncludingNullTerminator());

    platformFileHandle = mkostemps(temporaryFilePath.data(), suffixUTF8.length(), O_CLOEXEC);
    if (platformFileHandle == invalidPlatformFileHandle)
        return { nullString(), invalidPlatformFileHandle };

    return { String::fromUTF8(temporaryFilePath.data()), platformFileHandle };
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

bool markPurgeable(const String& path)
{
    CString fileSystemPath = fileSystemRepresentation(path);
    if (fileSystemPath.isNull())
        return false;

#if HAVE(APFS_CACHEDELETE_PURGEABLE)
    uint64_t flags = APFS_MARK_PURGEABLE | APFS_PURGEABLE_DATA_TYPE | APFS_PURGEABLE_MARK_CHILDREN;
    return !fsctl(fileSystemPath.data(), APFSIOC_MARK_PURGEABLE, &flags, 0);
#else
    return false;
#endif
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
