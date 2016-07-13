/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "LTRelayController.h"

#import "CoreSimulatorSPI.h"
#import "LTPipeRelay.h"
#import <AppKit/AppKit.h>
#import <crt_externs.h>

@interface LTRelayController ()
@property (readonly, strong) dispatch_source_t standardInputDispatchSource;
@property (readonly, strong) NSFileHandle *standardOutput;
@property (readonly, strong) NSFileHandle *standardError;
@property (readonly, strong) NSString *ipcIdentifier;
@property (readonly, strong) NSString *appBundleIdentifier;
@property (readonly, strong) NSString *appPath;
@property (readonly, strong) NSUUID *deviceUDID;
@property (readonly, strong) NSArray *dumpToolArguments;
@property (readonly, strong) NSString *productDir;
@property (strong) SimDevice *device;
@property (strong) id<LTRelay> relay;
@property pid_t pid;
@property (strong) dispatch_source_t appDispatchSource;
@end

@implementation LTRelayController

- (id)initWithDevice:(SimDevice *)device productDir:(NSString *)productDir appPath:(NSString *)appPath deviceUDID:(NSUUID *)udid dumpToolArguments:(NSArray *)arguments
{
    if ((self = [super init])) {
        _device = device;
        _productDir = productDir;
        _appPath = appPath;
        _appBundleIdentifier = [[NSBundle bundleWithPath:appPath] bundleIdentifier];
        _deviceUDID = udid;
        _dumpToolArguments = arguments;
        _standardInputDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_READ, STDIN_FILENO, 0, dispatch_get_main_queue());
        _standardOutput = [NSFileHandle fileHandleWithStandardOutput];
        _standardError = [NSFileHandle fileHandleWithStandardError];

        dispatch_source_set_event_handler(_standardInputDispatchSource, ^{
            uint8_t buffer[1024];
            ssize_t len = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (len > 0) {
                @try {
                    [[[self relay] outputStream] write:buffer maxLength:len];
                } @catch (NSException *e) {
                    // Broken pipe - the dump tool crashed. Time to die.
                    [self didCrashWithMessage:nil];
                }
            } else {
                // EOF Received on the relay's standard input.
                [self finish];
            }
        });

        _relay = [[LTPipeRelay alloc] initWithPrefix:[@"/tmp" stringByAppendingPathComponent:self.ipcIdentifier]];
        [_relay setRelayDelegate:self];
    }
    return self;
}

- (NSString *)ipcIdentifier
{
    return [NSString stringWithFormat:@"%@-%@", self.appBundleIdentifier, self.deviceUDID.UUIDString];
}

- (NSString *)processName
{
    return [self.appBundleIdentifier componentsSeparatedByString:@"."].lastObject;
}

- (void)didReceiveStdoutData:(NSData *)data
{
    @try {
        [[self standardOutput] writeData:data];
    } @catch (NSException *exception) {
        // NSFileHandleOperationException
        // Broken pipe - the test harness stopped listening to us,
        // probably because we timed out or a run was canceled.
        exit(EXIT_FAILURE);
    }
}

- (void)didReceiveStderrData:(NSData *)data
{
    @try {
        [[self standardError] writeData:data];
    } @catch (NSException *exception) {
        // NSFileHandleOperationException
        // Broken pipe - the test harness stopped listening to us,
        // probably because we timed out or a run was canceled.
        exit(EXIT_FAILURE);
    }
}

- (void)didDisconnect
{
    dispatch_suspend([self standardInputDispatchSource]);
}

- (void)didConnect
{
    dispatch_resume([self standardInputDispatchSource]);
}

- (void)didCrashWithMessage:(NSString *)message
{
    [[self relay] disconnect];
    NSString *crashMessage = [NSString stringWithFormat:@"\n#CRASHED - %@ (pid %d)\n", [self processName], [self pid]];

    if (message)
        crashMessage = [crashMessage stringByAppendingFormat:@"%@\n", message];

    [[self standardError] writeData:[crashMessage dataUsingEncoding:NSUTF8StringEncoding]];
    [[self standardError] closeFile];
    [[self standardOutput] closeFile];
    exit(EXIT_FAILURE);
}

- (void)installApp
{
    NSDictionary *installOptions = @{
        (NSString *)kCFBundleIdentifierKey: self.appBundleIdentifier,
    };

    NSError *error = nil;
    [self.device installApplication:[NSURL fileURLWithPath:self.appPath] withOptions:installOptions error:&error];
    if (error) {
        NSLog(@"Couldn't install %@: %@", self.appPath, error.description);
        exit(EXIT_FAILURE);
    }
}

- (void)killApp
{
    dispatch_source_t dispatch = [self appDispatchSource];

    if (dispatch)
        dispatch_source_cancel(dispatch);

    if ([self pid] > 1) {
        kill(self.pid, SIGKILL);
        [self setPid:-1];
    }
    [self setAppDispatchSource:nil];
}

- (NSDictionary *)_environmentVariables
{
    static NSDictionary *environmentVariables;
    static dispatch_once_t once;

    dispatch_once(&once, ^{
        NSString *productDirectory = [self productDir];

        NSMutableDictionary *dictionary = [@{
            @"IPC_IDENTIFIER": self.ipcIdentifier,
            @"DYLD_FRAMEWORK_PATH": productDirectory,
            @"__XPC_DYLD_FRAMEWORK_PATH": productDirectory,
            @"DYLD_LIBRARY_PATH": productDirectory,
            @"__XPC_DYLD_LIBRARY_PATH": productDirectory,
        } mutableCopy];

        for (NSString *keyName in @[@"DYLD_INSERT_LIBRARIES", @"MallocStackLogging", @"LOCAL_RESOURCE_ROOT", @"DUMPRENDERTREE_TEMP"]) {
            const char* value = getenv([keyName UTF8String]);
            if (value && strlen(value)) {
                NSString *nsValue = [NSString stringWithUTF8String:value];
                [dictionary setObject:nsValue forKey:keyName];
                [dictionary setObject:nsValue forKey:[@"__XPC_" stringByAppendingString:keyName]];
            }
        }

        for (char** envp = *_NSGetEnviron(); *envp; envp++) {
            const char* env = *envp;
            if (!strncmp("JSC_", env, 4) || !strncmp("__XPC_JSC_", env, 10)) {
                const char* equal = strchr(env, '=');
                if (!equal) {
                    NSLog(@"Missing '=' in env var '%s'", env);
                    continue;
                }

                static const size_t maxKeyLength = 256;
                size_t keyLength = equal - env;
                if (keyLength >= maxKeyLength) {
                    NSLog(@"Env var '%s' is too long", env);
                    continue;
                }

                char key[maxKeyLength];
                strncpy(key, env, keyLength);
                key[keyLength] = '\0';
                const char* value = equal + 1;

                NSString *nsKey = [NSString stringWithUTF8String:key];
                NSString *nsValue = [NSString stringWithUTF8String:value];
                [dictionary setObject:nsValue forKey:nsKey];
            }
        }

        environmentVariables = [dictionary copy];
    });

    return environmentVariables;
}

- (void)launchApp
{
    NSDictionary *launchOptions = @{
        kSimDeviceLaunchApplicationEnvironment: [self _environmentVariables],
        kSimDeviceLaunchApplicationArguments: [self dumpToolArguments],
    };

    NSError *error;
    pid_t pid = [self.device launchApplicationWithID:self.appBundleIdentifier options:launchOptions error:&error];

    if (pid < 0) {
        NSLog(@"Couldn't launch %@: %@", self.appBundleIdentifier, error.description);
        exit(EXIT_FAILURE);
    }

    [self setPid:pid];

    dispatch_queue_t queue = dispatch_get_main_queue();
    dispatch_source_t simulatorAppExitDispatchSource = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, [self pid], DISPATCH_PROC_EXIT, queue);
    [self setAppDispatchSource:simulatorAppExitDispatchSource];
    dispatch_source_set_event_handler(simulatorAppExitDispatchSource, ^{
        dispatch_source_cancel(simulatorAppExitDispatchSource);
        [self didCrashWithMessage:nil];
    });
    dispatch_resume(simulatorAppExitDispatchSource);
}

- (void)start
{
    [self installApp];
    [[self relay] setup];
    [self launchApp];
    [[self relay] connect];
}

- (void)finish
{
    [[self relay] disconnect];
    [self killApp];
    exit(EXIT_SUCCESS);
}

@end
