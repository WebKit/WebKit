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

namespace TestWebKitAPI {

// FIXME: Get this working in the iOS simulator.
#if PLATFORM(MAC)

static RetainPtr<NSURL> testWebPushDaemonLocation()
{
    return [currentExecutableDirectory() URLByAppendingPathComponent:@"webpushd" isDirectory:NO];
}

#if HAVE(OS_LAUNCHD_JOB)

static RetainPtr<xpc_object_t> testWebPushDaemonPList(NSURL *storageLocation)
{
    auto plist = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(plist.get(), "_ManagedBy", "TestWebKitAPI");
    xpc_dictionary_set_string(plist.get(), "Label", "org.webkit.webpushtestdaemon");
    xpc_dictionary_set_bool(plist.get(), "LaunchOnlyOnce", true);
    xpc_dictionary_set_string(plist.get(), "StandardErrorPath", [storageLocation URLByAppendingPathComponent:@"daemon_stderr"].path.fileSystemRepresentation);

    {
        auto environmentVariables = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_string(environmentVariables.get(), "DYLD_FRAMEWORK_PATH", currentExecutableDirectory().get().fileSystemRepresentation);
        xpc_dictionary_set_value(plist.get(), "EnvironmentVariables", environmentVariables.get());
    }
    {
        auto machServices = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));
        xpc_dictionary_set_bool(machServices.get(), "org.webkit.webpushtestdaemon.service", true);
        xpc_dictionary_set_value(plist.get(), "MachServices", machServices.get());
    }
    {
        auto programArguments = adoptNS(xpc_array_create(nullptr, 0));
        auto executableLocation = testWebPushDaemonLocation();
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, executableLocation.get().fileSystemRepresentation);
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, "--machServiceName");
        xpc_array_set_string(programArguments.get(), XPC_ARRAY_APPEND, "org.webkit.webpushtestdaemon.service");
        xpc_dictionary_set_value(plist.get(), "ProgramArguments", programArguments.get());
    }
    return plist;
}

#else // HAVE(OS_LAUNCHD_JOB)

static RetainPtr<NSDictionary> testWebPushDaemonPList(NSURL *storageLocation)
{
    return @{
        @"Label" : @"org.webkit.pcmtestdaemon",
        @"LaunchOnlyOnce" : @YES,
        @"StandardErrorPath" : [storageLocation URLByAppendingPathComponent:@"daemon_stderr"].path,
        @"EnvironmentVariables" : @{ @"DYLD_FRAMEWORK_PATH" : currentExecutableDirectory().get().path },
        @"MachServices" : @{ @"org.webkit.webpushtestdaemon.service" : @YES },
        @"ProgramArguments" : @[
            testWebPushDaemonLocation().get().path,
            @"--machServiceName",
            @"org.webkit.webpushtestdaemon.service"
        ]
    };
}

#endif // HAVE(OS_LAUNCHD_JOB)

static NSURL *setUpTestWebPushD()
{
    NSFileManager *fileManager = [NSFileManager defaultManager];
    NSURL *tempDir = [NSURL fileURLWithPath:[NSTemporaryDirectory() stringByAppendingPathComponent:@"WebPushDaemonTest"] isDirectory:YES];
    NSError *error = nil;
    if ([fileManager fileExistsAtPath:tempDir.path])
        [fileManager removeItemAtURL:tempDir error:&error];
    EXPECT_NULL(error);

    system("killall webpushd -9 2> /dev/null");

    auto plist = testWebPushDaemonPList(tempDir);
#if HAVE(OS_LAUNCHD_JOB)
    registerPlistWithLaunchD(WTFMove(plist));
#else
    registerPlistWithLaunchD(WTFMove(plist), tempDir);
#endif

    return tempDir;
}

static void cleanUpTestWebPushD(NSURL *tempDir)
{
    system("killall webpushd -9");

    EXPECT_TRUE([[NSFileManager defaultManager] fileExistsAtPath:tempDir.path]);
    NSError *error = nil;
    [[NSFileManager defaultManager] removeItemAtURL:tempDir error:&error];
    EXPECT_NULL(error);
}

TEST(WebPushD, BasicCommunication)
{
    NSURL *tempDir = setUpTestWebPushD();

    auto connection = adoptNS(xpc_connection_create_mach_service("org.webkit.webpushtestdaemon.service", dispatch_get_main_queue(), 0));
    xpc_connection_set_event_handler(connection.get(), ^(xpc_object_t) { });
    xpc_connection_activate(connection.get());
    auto dictionary = adoptNS(xpc_dictionary_create(nullptr, nullptr, 0));

    std::array<uint8_t, 10> encodedString { 5, 0, 0, 0, 1, 'h', 'e', 'l', 'l', 'o' };
    xpc_dictionary_set_uint64(dictionary.get(), "protocol version", 1);
    xpc_dictionary_set_uint64(dictionary.get(), "message type", 1);
    xpc_dictionary_set_data(dictionary.get(), "encoded message", encodedString.data(), encodedString.size());
    
    __block bool done = false;
    xpc_connection_send_message_with_reply(connection.get(), dictionary.get(), dispatch_get_main_queue(), ^(xpc_object_t reply) {
        size_t dataSize = 0;
        const void* data = xpc_dictionary_get_data(reply, "encoded message", &dataSize);
        EXPECT_EQ(dataSize, 15u);
        std::array<uint8_t, 15> expectedReply { 10, 0, 0, 0, 1, 'h', 'e', 'l', 'l', 'o' , 'h', 'e', 'l', 'l', 'o' };
        EXPECT_FALSE(memcmp(data, expectedReply.data(), expectedReply.size()));
        done = true;
    });
    TestWebKitAPI::Util::run(&done);

    cleanUpTestWebPushD(tempDir);
}

#endif // PLATFORM(MAC)

} // namespace TestWebKitAPI
