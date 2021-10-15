/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "DaemonTestUtilities.h"

#import <mach-o/dyld.h>
#import <wtf/Vector.h>

namespace TestWebKitAPI {

static RetainPtr<NSURL> currentExecutableLocation()
{
    uint32_t size { 0 };
    _NSGetExecutablePath(nullptr, &size);
    Vector<char> buffer;
    buffer.resize(size + 1);
    _NSGetExecutablePath(buffer.data(), &size);
    buffer[size] = '\0';
    auto pathString = adoptNS([[NSString alloc] initWithUTF8String:buffer.data()]);
    return adoptNS([[NSURL alloc] initFileURLWithPath:pathString.get() isDirectory:NO]);
}

RetainPtr<NSURL> currentExecutableDirectory()
{
    return [currentExecutableLocation() URLByDeletingLastPathComponent];
}

// FIXME: Get this working in the iOS simulator.
#if PLATFORM(MAC)
#if HAVE(OS_LAUNCHD_JOB)

void registerPlistWithLaunchD(RetainPtr<xpc_object_t>&& plist)
{
    NSError *error = nil;
    auto launchDJob = adoptNS([[OSLaunchdJob alloc] initWithPlist:plist.get()]);
    [launchDJob submit:&error];
    EXPECT_FALSE(error);
}

#else // HAVE(OS_LAUNCHD_JOB)

void registerPlistWithLaunchD(RetainPtr<NSDictionary>&& plist, NSURL *tempDir)
{
    NSError *error = nil;
    NSURL *plistLocation = [tempDir URLByAppendingPathComponent:@"DaemonInfo.plist"];
    BOOL success = [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_TRUE(success);
    EXPECT_FALSE(error);
    success = [plist writeToURL:plistLocation error:&error];
    EXPECT_TRUE(success);
    system([NSString stringWithFormat:@"launchctl unload %@ 2> /dev/null", plistLocation.path].fileSystemRepresentation);
    system([NSString stringWithFormat:@"launchctl load %@", plistLocation.path].fileSystemRepresentation);
    EXPECT_FALSE(error);
}

#endif // HAVE(OS_LAUNCHD_JOB)
#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
