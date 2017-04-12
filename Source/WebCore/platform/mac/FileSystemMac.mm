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

#import "WebCoreNSURLExtras.h"
#import "WebCoreSystemInterface.h"
#import <wtf/text/WTFString.h>

namespace WebCore {

bool deleteEmptyDirectory(const String& path)
{
    auto fileManager = adoptNS([[NSFileManager alloc] init]);

    if (NSArray *directoryContents = [fileManager contentsOfDirectoryAtPath:path error:nullptr]) {
        // Explicitly look for and delete .DS_Store files.
        if (directoryContents.count == 1 && [directoryContents.firstObject isEqualToString:@".DS_Store"])
            [fileManager removeItemAtPath:[path stringByAppendingPathComponent:directoryContents.firstObject] error:nullptr];
    }

    // rmdir(...) returns 0 on successful deletion of the path and non-zero in any other case (including invalid permissions or non-existent file)
    return !rmdir(fileSystemRepresentation(path).data());
}

void setMetadataURL(const String& path, const String& metadataURLString)
{
    String urlString;
    if (NSURL *url = URLWithUserTypedString(urlString, nil))
        urlString = userVisibleString(URLByRemovingUserInfo(url));
    else
        urlString = metadataURLString;

    // Call WKSetMetadataURL on a background queue because it can take some time.
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [path = path.isolatedCopy(), urlString = urlString.isolatedCopy()] {
        wkSetMetadataURL(urlString, nil, [NSString stringWithUTF8String:[path fileSystemRepresentation]]);
    });
}

bool canExcludeFromBackup()
{
    return true;
}

bool excludeFromBackup(const String& path)
{
    // It is critical to pass FALSE for excludeByPath because excluding by path requires root privileges.
    CSBackupSetItemExcluded(pathAsURL(path).get(), TRUE, FALSE); 
    return true;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
