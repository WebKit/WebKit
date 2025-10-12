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

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)

#import <mach-o/dyld.h>
#import <wtf/Vector.h>

#if USE(APPLE_INTERNAL_SDK)
// AppServerSupport cannot be safely imported within modules, so avoid putting
// this SPI declaration in headers.
#import <AppServerSupport/OSLaunchdJob.h>
#else
#import <Foundation/NSError.h>
NS_ASSUME_NONNULL_BEGIN
@interface OSLaunchdJob : NSObject
- (instancetype)initWithPlist:(xpc_object_t)plist;
- (BOOL)submit:(NSError * _Nullable __autoreleasing * _Nullable)errorOut;
@end
NS_ASSUME_NONNULL_END
#endif

NS_ASSUME_NONNULL_BEGIN

#if PLATFORM(IOS) || PLATFORM(VISION)
@interface NSTask : NSObject
- (instancetype)init;
- (void)launch;
- (void)waitUntilExit;

@property (nullable, copy) NSString *launchPath;
@property (nullable, copy) NSArray<NSString *> *arguments;
@property (nullable, retain) id standardOutput;
@property (readonly, getter=isRunning) BOOL running;
@end
#endif

namespace TestWebKitAPI {

static RetainPtr<NSURL> currentExecutableLocation()
{
    uint32_t size { 0 };
    _NSGetExecutablePath(nullptr, &size);
    Vector<char> buffer;
    buffer.resize(size + 1);
    _NSGetExecutablePath(buffer.mutableSpan().data(), &size);
    buffer[size] = '\0';
    auto pathString = adoptNS([[NSString alloc] initWithUTF8String:buffer.span().data()]);
    return adoptNS([[NSURL alloc] initFileURLWithPath:pathString.get() isDirectory:NO]);
}

RetainPtr<NSURL> currentExecutableDirectory()
{
    return [currentExecutableLocation() URLByDeletingLastPathComponent];
}

#if PLATFORM(IOS) || PLATFORM(VISION)
static RetainPtr<xpc_object_t> convertArrayToXPC(NSArray *array)
{
    auto xpc = adoptNS(xpc_array_create(nullptr, 0));
    for (id value in array) {
        if ([value isKindOfClass:NSString.class])
            xpc_array_set_string(xpc.get(), XPC_ARRAY_APPEND, [value UTF8String]);
        else
            ASSERT_NOT_REACHED();
    }
    return xpc;
}

static RetainPtr<xpc_object_t> convertDictionaryToXPC(NSDictionary<NSString *, id> *dictionary)
{
    auto xpc = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    for (NSString *key in dictionary) {
        ASSERT([key isKindOfClass:NSString.class]);
        const char* keyUTF8 = key.UTF8String;
        id value = dictionary[key];
        if ([value isKindOfClass:NSString.class])
            xpc_dictionary_set_string(xpc.get(), keyUTF8, [value UTF8String]);
        else if ([value isKindOfClass:NSNumber.class]) {
            uint64_t uint64Value = [value unsignedLongLongValue];
            if (uint64Value == 1)
                xpc_dictionary_set_bool(xpc.get(), keyUTF8, uint64Value);
            else {
                ASSERT([value doubleValue] == uint64Value);
                xpc_dictionary_set_uint64(xpc.get(), keyUTF8, uint64Value);
            }
        } else if ([value isKindOfClass:NSArray.class])
            xpc_dictionary_set_value(xpc.get(), keyUTF8, convertArrayToXPC(value).get());
        else if ([value isKindOfClass:NSDictionary.class])
            xpc_dictionary_set_value(xpc.get(), keyUTF8, convertDictionaryToXPC(value).get());
        else
            ASSERT_NOT_REACHED();
    }
    return xpc;
}
#endif

void registerPlistWithLaunchD(NSDictionary<NSString *, id> *plist, NSURL *tempDir)
{
    NSError *error = nil;
#if PLATFORM(IOS) || PLATFORM(VISION)
    auto xpcPlist = convertDictionaryToXPC(plist);
    xpc_dictionary_set_string(xpcPlist.get(), "_ManagedBy", "TestWebKitAPI");
    xpc_dictionary_set_bool(xpcPlist.get(), "RootedSimulatorPath", true);
    auto launchDJob = adoptNS([[OSLaunchdJob alloc] initWithPlist:xpcPlist.get()]);
    [launchDJob submit:&error];
#else
    NSURL *plistLocation = [tempDir URLByAppendingPathComponent:@"DaemonInfo.plist"];
    BOOL success = [[NSFileManager defaultManager] createDirectoryAtURL:tempDir withIntermediateDirectories:YES attributes:nil error:&error];
    EXPECT_TRUE(success);
    EXPECT_FALSE(error);
    success = [plist writeToURL:plistLocation error:&error];
    EXPECT_TRUE(success);
    system([NSString stringWithFormat:@"launchctl unload %@ 2> /dev/null", plistLocation.path].fileSystemRepresentation);
    system([NSString stringWithFormat:@"launchctl load %@", plistLocation.path].fileSystemRepresentation);
    EXPECT_FALSE(error);
#endif
}

static int pidOfFirstDaemonInstance(NSString *daemonExecutableName)
{
    auto task = adoptNS([[NSTask alloc] init]);
    task.get().launchPath = @"/bin/ps";
    task.get().arguments = @[
        @"-ax",
        @"-o",
        @"pid,comm"
    ];

    auto taskPipe = adoptNS([[NSPipe alloc] init]);
    [task setStandardOutput:taskPipe.get()];
    [task launch];

    auto data = adoptNS([[NSMutableData alloc] init]);
    while ([task isRunning])
        [data appendData:[[taskPipe fileHandleForReading] readDataToEndOfFile]];
    [data appendData:[[taskPipe fileHandleForReading] readDataToEndOfFile]];

    auto psString = adoptNS([[NSString alloc] initWithData:data.get() encoding:NSUTF8StringEncoding]);
    NSArray<NSString *> *psEntries = [psString componentsSeparatedByString:@"\n"];

    for (NSString* entry in psEntries) {
        if (![entry hasSuffix:daemonExecutableName])
            continue;
        NSArray<NSString *> *components = [entry componentsSeparatedByString:@" "];
        EXPECT_GE([components count], 2u);
        return [[components firstObject] integerValue];
    }

    return 0;
}

void killFirstInstanceOfDaemon(NSString *daemonExecutableName)
{
    auto pid = pidOfFirstDaemonInstance(daemonExecutableName);
    if (!pid)
        return;

    auto task = adoptNS([[NSTask alloc] init]);
    task.get().launchPath = @"/bin/kill";
    task.get().arguments = @[
        @"-9",
        [@(pid) stringValue]
    ];

    [task launch];
    [task waitUntilExit];
}

#if PLATFORM(IOS) || PLATFORM(VISION)

BOOL restartService(NSString *, NSString *daemonExecutableName)
{
    killFirstInstanceOfDaemon(daemonExecutableName);
    sleep(1);
    return YES;
}

#else

BOOL restartService(NSString *serviceName, NSString *)
{
    auto task = adoptNS([[NSTask alloc] init]);
    [task setLaunchPath:@"/bin/launchctl"];
    [task setArguments:@[@"kickstart", @"-k", @"-p", [NSString stringWithFormat:@"gui/%u/%@", geteuid(), serviceName]]];
    [task launch];
    [task waitUntilExit];
    return [task terminationStatus] == EXIT_SUCCESS;
}

#endif

} // namespace TestWebKitAPI

NS_ASSUME_NONNULL_END

#endif // PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(VISION)
