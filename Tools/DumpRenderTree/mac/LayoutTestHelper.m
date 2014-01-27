/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2012 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if !PLATFORM(IOS)

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <getopt.h>
#import <signal.h>
#import <stdio.h>
#import <stdlib.h>

// This is a simple helper app that changes the color profile of the main display
// to GenericRGB and back when done. This program is managed by the layout
// test script, so it can do the job for multiple DumpRenderTree while they are
// running layout tests.

static int installColorProfile = false;

static CFURLRef sUserColorProfileURL;

static void installLayoutTestColorProfile()
{
    if (!installColorProfile)
        return;

    // To make sure we get consistent colors (not dependent on the chosen color
    // space of the main display), we force the generic RGB color profile.
    // This causes a change the user can see.
    
    CFUUIDRef mainDisplayID = CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
    
    if (!sUserColorProfileURL) {
        CFDictionaryRef deviceInfo = ColorSyncDeviceCopyDeviceInfo(kColorSyncDisplayDeviceClass, mainDisplayID);

        if (!deviceInfo) {
            NSLog(@"No display attached to system; not setting main display's color profile.");
            CFRelease(mainDisplayID);
            return;
        }

        CFDictionaryRef profileInfo = (CFDictionaryRef)CFDictionaryGetValue(deviceInfo, kColorSyncCustomProfiles);
        if (profileInfo) {
            sUserColorProfileURL = (CFURLRef)CFDictionaryGetValue(profileInfo, CFSTR("1"));
            CFRetain(sUserColorProfileURL);
        } else {
            profileInfo = (CFDictionaryRef)CFDictionaryGetValue(deviceInfo, kColorSyncFactoryProfiles);
            CFDictionaryRef factoryProfile = (CFDictionaryRef)CFDictionaryGetValue(profileInfo, CFSTR("1"));
            sUserColorProfileURL = (CFURLRef)CFDictionaryGetValue(factoryProfile, kColorSyncDeviceProfileURL);
            CFRetain(sUserColorProfileURL);
        }
        
        CFRelease(deviceInfo);
    }

    ColorSyncProfileRef genericRGBProfile = ColorSyncProfileCreateWithName(kColorSyncGenericRGBProfile);
    CFErrorRef error;
    CFURLRef profileURL = ColorSyncProfileGetURL(genericRGBProfile, &error);
    if (!profileURL) {
        NSLog(@"Failed to get URL of Generic RGB color profile! Many pixel tests may fail as a result. Error: %@", error);
        
        if (sUserColorProfileURL) {
            CFRelease(sUserColorProfileURL);
            sUserColorProfileURL = 0;
        }
        
        CFRelease(genericRGBProfile);
        CFRelease(mainDisplayID);
        return;
    }
        
    CFMutableDictionaryRef profileInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(profileInfo, kColorSyncDeviceDefaultProfileID, profileURL);
    
    if (!ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, mainDisplayID, profileInfo)) {
        NSLog(@"Failed to set color profile for main display! Many pixel tests may fail as a result.");
        
        if (sUserColorProfileURL) {
            CFRelease(sUserColorProfileURL);
            sUserColorProfileURL = 0;
        }
    }
    
    CFRelease(profileInfo);
    CFRelease(genericRGBProfile);
    CFRelease(mainDisplayID);
}

static void restoreUserColorProfile(void)
{
    if (!installColorProfile)
        return;

    // This is used as a signal handler, and thus the calls into ColorSync are unsafe.
    // But we might as well try to restore the user's color profile, we're going down anyway...
    
    if (!sUserColorProfileURL)
        return;
    
    CFUUIDRef mainDisplayID = CGDisplayCreateUUIDFromDisplayID(CGMainDisplayID());
    CFMutableDictionaryRef profileInfo = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(profileInfo, kColorSyncDeviceDefaultProfileID, sUserColorProfileURL);
    ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, mainDisplayID, profileInfo);
    CFRelease(mainDisplayID);
    CFRelease(profileInfo);
}

static void simpleSignalHandler(int sig)
{
    // Try to restore the color profile and try to go down cleanly
    restoreUserColorProfile();
    exit(128 + sig);
}

void lockDownDiscreteGraphics()
{
    mach_port_t masterPort;
    kern_return_t kernResult = IOMasterPort(bootstrap_port, &masterPort);
    if (kernResult != KERN_SUCCESS)
        return;
    CFDictionaryRef classToMatch = IOServiceMatching("AppleGraphicsControl");
    if (!classToMatch)
        return;

    io_service_t serviceObject = IOServiceGetMatchingService(masterPort, classToMatch);
    if (!serviceObject) {
        // The machine does not allow control over the choice of graphics device.
        return;
    }

    // We're intentionally leaking this io_connect in order for the process to stay locked to discrete graphics
    // for the lifetime of the service connection.
    static io_connect_t permanentLockDownService = 0;

    // This call stalls until the graphics device lock is granted.
    kernResult = IOServiceOpen(serviceObject, mach_task_self(), 1, &permanentLockDownService);
    IOObjectRelease(serviceObject);
}

int main(int argc, char* argv[])
{
    struct option options[] = {
        { "install-color-profile", no_argument, &installColorProfile, true },
    };

    int option;
    while ((option = getopt_long(argc, (char* const*)argv, "", options, NULL)) != -1) {
        switch (option) {
        case '?':   // unknown or ambiguous option
        case ':':   // missing argument
            exit(1);
            break;
        }
    }

    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // Hooks the ways we might get told to clean up...
    signal(SIGINT, simpleSignalHandler);
    signal(SIGHUP, simpleSignalHandler);
    signal(SIGTERM, simpleSignalHandler);

    lockDownDiscreteGraphics();

    // Save off the current profile, and then install the layout test profile.
    installLayoutTestColorProfile();

    // Let the script know we're ready
    printf("ready\n");
    fflush(stdout);

    // Wait for any key (or signal)
    getchar();

    // Restore the profile
    restoreUserColorProfile();

    [pool release];
    return 0;
}

#endif // !PLATFORM(IOS)

#if PLATFORM(IOS)
int main(int argc, char* argv[])
{
    return 0;
}
#endif
