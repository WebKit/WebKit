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

#import "CoreSimulatorSPI.h"
#import "LTRelayController.h"
#import <Foundation/Foundation.h>

static LTRelayController *relayController;

void usage()
{
    NSString *helpText = @"LayoutTestRelay: run a dump tool in the simulator. Not for direct consumption.\n"
    "Usage: LayoutTestRelay [-h] [options] -- [dump tool arguments]\n"
    "Required options:\n"
    "    -app <app_path>          Path to the built iOS .app bundle directory.\n"
    "    -runtime <identifier>    iOS Simulator Runtime identifier (see `xcrun -sdk iphonesimulator simctl`)\n"
    "    -deviceType <identifier> iOS Simulator device identifier (see simctl).\n"
    "    -suffix <text>           Used to create multiple unique instances when installing to the simulator.\n"
    "    -productDir <dir>        /path/to/WebKitBuild/{configuration}-{short platform}.\n";

    fprintf(stderr, "%s\n", [helpText UTF8String]);
}

SimDevice *getTestingSimDevice(SimDeviceType *deviceType, SimRuntime *runtime, NSString *suffix)
{
    NSString *deviceName = [[[[deviceType identifier] componentsSeparatedByString:@"."] lastObject] stringByReplacingOccurrencesOfString:@"-" withString:@" "];
    deviceName = [NSString stringWithFormat:@"%@%@%@", deviceName, @" WebKit Tester", suffix];

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    for (SimDevice *device in [[SimDeviceSet defaultSet] devices]) {
        if ([[device name] isEqualToString:deviceName] && [[device deviceType] isEqualTo:deviceType] && [[device runtime] isEqualTo:runtime])
            return device;
    }

    NSError *error;
    SimDevice *device = [[SimDeviceSet defaultSet] createDeviceWithType:deviceType runtime:runtime name:deviceName error:&error];
#pragma clang diagnostic pop

    if (error) {
        NSLog(@"Couldn't create device: %@", [error description]);
        return nil;
    }

    while ([device state] == SimDeviceStateCreating)
        sleep(1);

    return device;
}


NSString *getRequiredStringArgument(NSString *parameter)
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *argument = [defaults stringForKey:parameter];

    if (!argument) {
        NSLog(@"-%@ is required.", parameter);
        usage();
        exit(EXIT_FAILURE);
    }
    return argument;
}

NSArray *getDumpToolArguments()
{
    NSMutableArray *dumpToolArguments = [[NSMutableArray alloc] init];
    NSArray *processArgs = [[NSProcessInfo processInfo] arguments];
    BOOL appending = NO;
    for (NSString *arg in processArgs) {
        if ([arg isEqualToString:@"--"]) {
            appending = YES;
            continue;
        }
        if (appending)
            [dumpToolArguments addObject:arg];
    }
    return dumpToolArguments;
}

void finish()
{
    [relayController finish];
}

void receivedSignal(int signal)
{
    exit(EXIT_SUCCESS);
}

int main(int argc, const char * argv[])
{
    @autoreleasepool {
        NSArray *helpArguments = @[@"-h", @"--help"];
        for (NSString *helpArgument in helpArguments) {
            if ([[[NSProcessInfo processInfo] arguments] indexOfObject:helpArgument] != NSNotFound) {
                usage();
                exit(EXIT_FAILURE);
            }
        }
        NSString *appPath = getRequiredStringArgument(@"app");
        NSString *runtimeIdentifier = getRequiredStringArgument(@"runtime");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SimRuntime *runtime = [SimRuntime supportedRuntimesByIdentifier][runtimeIdentifier];
#pragma clang diagnostic pop
        if (!runtime) {
            NSLog(@"There is no supported runtime \"%@\"", runtimeIdentifier);
            exit(EXIT_FAILURE);
        }

        NSString *deviceTypeIdentifier = getRequiredStringArgument(@"deviceType");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        SimDeviceType *deviceType = [SimDeviceType supportedDeviceTypesByIdentifier][deviceTypeIdentifier];
#pragma clang diagnostic pop
        if (!deviceType) {
            NSLog(@"There is no supported device type \"%@\"", deviceTypeIdentifier);
            exit(EXIT_FAILURE);
        }

        NSString *suffix = getRequiredStringArgument(@"suffix");
        NSString *productDir = getRequiredStringArgument(@"productDir");
        NSArray *dumpToolArguments = getDumpToolArguments();

        SimDevice *device = getTestingSimDevice(deviceType, runtime, suffix);

        relayController = [[LTRelayController alloc] initWithDevice:device productDir:productDir appPath:appPath identifierSuffix:suffix dumpToolArguments:dumpToolArguments];
        [relayController start];

        atexit(finish);
        signal(SIGINT, receivedSignal);
        signal(SIGTERM, receivedSignal);

        [[NSRunLoop mainRunLoop] run];
    }
    return 0;
}
