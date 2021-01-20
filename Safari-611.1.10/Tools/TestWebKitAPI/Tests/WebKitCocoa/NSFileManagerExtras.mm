/*
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

#import "config.h"

#if PLATFORM(IOS_FAMILY)

#import "PlatformUtilities.h"
#import "Test.h"
#import <WebKit/_WKNSFileManagerExtras.h>
#import <pal/spi/cocoa/NSFileManagerSPI.h>

static void expectAttributes(NSDictionary *attributes, NSNumber *expectedPermissions, NSString *expectedFileType)
{
    EXPECT_NOT_NULL(attributes);
    EXPECT_WK_STREQ(NSUserName(), attributes[NSFileOwnerAccountName]);
    EXPECT_WK_STREQ([expectedPermissions stringValue], [attributes[NSFilePosixPermissions] stringValue]);
    EXPECT_WK_STREQ(expectedFileType, attributes[NSFileType]);
}

TEST(WebKit, _WKNSFileManagerExtras)
{
    NSString *fileName = @"test.pdf";
    NSFileManager *fileManager = [NSFileManager defaultManager];

    NSString *filePath = [NSFileManager _web_createTemporaryFileForQuickLook:fileName];
    EXPECT_WK_STREQ(fileName, filePath.lastPathComponent);
    EXPECT_EQ(YES, [fileManager fileExistsAtPath:filePath]);

    NSDictionary *fileAttributes = [fileManager attributesOfItemAtPath:filePath error:nil];
    expectAttributes(fileAttributes, [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE)], NSFileTypeRegular);

    NSDictionary *directoryAttributes = [fileManager attributesOfItemAtPath:filePath.stringByDeletingLastPathComponent error:nil];
    expectAttributes(directoryAttributes, [NSNumber numberWithInteger:(WEB_UREAD | WEB_UWRITE | WEB_UEXEC)], NSFileTypeDirectory);

    [fileManager removeItemAtPath:filePath error:nil];
}

#endif // PLATFORM(IOS_FAMILY)
