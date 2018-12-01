/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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
#import "FileSystem.h"

#if PLATFORM(MAC)

#import <pal/spi/mac/MetadataSPI.h>
#import <wtf/cocoa/NSURLExtras.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

bool FileSystem::deleteEmptyDirectory(const String& path)
{
    auto fileManager = adoptNS([[NSFileManager alloc] init]);

    if (NSArray *directoryContents = [fileManager contentsOfDirectoryAtPath:path error:nullptr]) {
        // Explicitly look for and delete .DS_Store files.
        if (directoryContents.count == 1 && [directoryContents.firstObject isEqualToString:@".DS_Store"])
            [fileManager removeItemAtPath:[path stringByAppendingPathComponent:directoryContents.firstObject] error:nullptr];
    }

    // rmdir(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    return !rmdir(FileSystem::fileSystemRepresentation(path).data());
}

void FileSystem::setMetadataURL(const String& path, const String& metadataURLString, const String& referrer)
{
    String urlString;
    if (NSURL *url = URLWithUserTypedString(metadataURLString, nil))
        urlString = WTF::userVisibleString(WTF::URLByRemovingUserInfo(url));
    else
        urlString = metadataURLString;

    // Call Metadata API on a background queue because it can take some time.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [path = path.isolatedCopy(), urlString = urlString.isolatedCopy(), referrer = referrer.isolatedCopy()] {
        auto item = adoptCF(MDItemCreate(kCFAllocatorDefault, path.createCFString().get()));
        if (!item)
            return;

        auto whereFromAttribute = adoptNS([[NSMutableArray alloc] initWithObjects:urlString, nil]);
        if (!referrer.isNull())
            [whereFromAttribute addObject:referrer];

        MDItemSetAttribute(item.get(), kMDItemWhereFroms, (__bridge CFArrayRef)whereFromAttribute.get());
        MDItemSetAttribute(item.get(), kMDItemDownloadedDate, (__bridge CFArrayRef)@[ [NSDate date] ]);
    });
}

bool FileSystem::canExcludeFromBackup()
{
    return true;
}

bool FileSystem::excludeFromBackup(const String& path)
{
    // It is critical to pass FALSE for excludeByPath because excluding by path requires root privileges.
    CSBackupSetItemExcluded(FileSystem::pathAsURL(path).get(), TRUE, FALSE);
    return true;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
