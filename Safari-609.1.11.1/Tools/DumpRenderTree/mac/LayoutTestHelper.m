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

#import "config.h"

#if !PLATFORM(IOS_FAMILY)

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#import <getopt.h>
#import <signal.h>
#import <stdio.h>
#import <stdlib.h>

#if USE(APPLE_INTERNAL_SDK)

#import <ColorSync/ColorSyncPriv.h>

#else

CFUUIDRef CGDisplayCreateUUIDFromDisplayID(uint32_t displayID);

#endif

// This is a simple helper app that changes the color profile of the main display
// to GenericRGB and back when done. This program is managed by the layout
// test script, so it can do the job for multiple DumpRenderTree while they are
// running layout tests.

static int installColorProfile = false;
static uint32_t assertionIDForDisplaySleep = 0;
static uint32_t assertionIDForSystemSleep = 0;

static NSMutableDictionary *originalColorProfileURLs()
{
    static NSMutableDictionary *sharedInstance;
    if (!sharedInstance)
        sharedInstance = [[NSMutableDictionary alloc] init];
    return sharedInstance;
}

static NSURL *colorProfileURLForDisplay(NSString *displayUUIDString)
{
    CFUUIDRef uuid = CFUUIDCreateFromString(kCFAllocatorDefault, (CFStringRef)displayUUIDString);
    CFDictionaryRef deviceInfo = ColorSyncDeviceCopyDeviceInfo(kColorSyncDisplayDeviceClass, uuid);
    CFRelease(uuid);
    if (!deviceInfo) {
        NSLog(@"No display attached to system; not setting main display's color profile.");
        return nil;
    }

    CFURLRef profileURL = nil;
    CFDictionaryRef profileInfo = (CFDictionaryRef)CFDictionaryGetValue(deviceInfo, kColorSyncCustomProfiles);
    if (profileInfo)
        profileURL = (CFURLRef)CFDictionaryGetValue(profileInfo, CFSTR("1"));
    else {
        profileInfo = (CFDictionaryRef)CFDictionaryGetValue(deviceInfo, kColorSyncFactoryProfiles);
        CFDictionaryRef factoryProfile = (CFDictionaryRef)CFDictionaryGetValue(profileInfo, CFSTR("1"));
        if (factoryProfile)
            profileURL = (CFURLRef)CFDictionaryGetValue(factoryProfile, kColorSyncDeviceProfileURL);
    }
    
    if (!profileURL) {
        NSLog(@"Could not determine current color profile, so it will not be reset after running the tests.");
        CFRelease(deviceInfo);
        return nil;
    }

    NSURL *url = CFBridgingRelease(CFRetain(profileURL));
    CFRelease(deviceInfo);
    return url;
}

static NSArray *displayUUIDStrings()
{
    NSMutableArray *result = [NSMutableArray array];

    static const uint32_t maxDisplayCount = 10;
    CGDirectDisplayID displayIDs[maxDisplayCount] = { 0 };
    uint32_t displayCount = 0;
    
    CGError err = CGGetActiveDisplayList(maxDisplayCount, displayIDs, &displayCount);
    if (err != kCGErrorSuccess) {
        NSLog(@"Error %d getting active display list; not setting display color profile.", err);
        return nil;
    }

    if (!displayCount) {
        NSLog(@"No display attached to system; not setting display color profile.");
        return nil;
    }

    for (uint32_t i = 0; i < displayCount; ++i) {
        CFUUIDRef displayUUIDRef = CGDisplayCreateUUIDFromDisplayID(displayIDs[i]);
        [result addObject:(NSString *)CFBridgingRelease(CFUUIDCreateString(kCFAllocatorDefault, displayUUIDRef))];
        CFRelease(displayUUIDRef);
    }
    
    return result;
}

static void saveDisplayColorProfiles(NSArray *displayUUIDStrings)
{
    NSMutableDictionary *userColorProfiles = originalColorProfileURLs();

    for (NSString *UUIDString in displayUUIDStrings) {
        if ([userColorProfiles objectForKey:UUIDString])
            continue;
        
        NSURL *colorProfileURL = colorProfileURLForDisplay(UUIDString);
        if (!colorProfileURL)
            continue;

        [userColorProfiles setObject:colorProfileURL forKey:UUIDString];
    }
}

static void setDisplayColorProfile(NSString *displayUUIDString, NSURL *colorProfileURL)
{
    NSDictionary *profileInfo = @{
        (__bridge NSString *)kColorSyncDeviceDefaultProfileID : colorProfileURL
    };

    CFUUIDRef uuid = CFUUIDCreateFromString(kCFAllocatorDefault, (CFStringRef)displayUUIDString);
    BOOL success = ColorSyncDeviceSetCustomProfiles(kColorSyncDisplayDeviceClass, uuid, (CFDictionaryRef)profileInfo);
    if (!success)
        NSLog(@"Failed to set color profile for display %@! Many pixel tests may fail as a result.", displayUUIDString);
    CFRelease(uuid);
}

static void restoreDisplayColorProfiles(NSArray *displayUUIDStrings)
{
    NSMutableDictionary* userColorProfiles = originalColorProfileURLs();

    for (NSString *UUIDString in displayUUIDStrings) {
        NSURL *profileURL = [userColorProfiles objectForKey:UUIDString];
        if (!profileURL)
            continue;
        
        setDisplayColorProfile(UUIDString, profileURL);
    }
}

static void installLayoutTestColorProfile()
{
    if (!installColorProfile)
        return;

    // To make sure we get consistent colors (not dependent on the chosen color
    // space of the display), we force the generic sRGB color profile on all displays.
    // This causes a change the user can see.

    NSArray *displays = displayUUIDStrings();
    saveDisplayColorProfiles(displays);

    // Profile path needs to be hardcoded because of <rdar://problem/28392768>.
    NSURL *sRGBProfileURL = [NSURL fileURLWithPath:@"/System/Library/ColorSync/Profiles/sRGB Profile.icc"];
    
    for (NSString *displayUUIDString in displays)
        setDisplayColorProfile(displayUUIDString, sRGBProfileURL);
}

static void restoreUserColorProfile(void)
{
    if (!installColorProfile)
        return;

    // This is used as a signal handler, and thus the calls into ColorSync are unsafe.
    // But we might as well try to restore the user's color profile, we're going down anyway...
    
    NSArray *displays = displayUUIDStrings();
    restoreDisplayColorProfiles(displays);
}

static void releaseSleepAssertions()
{
    IOPMAssertionRelease(assertionIDForDisplaySleep);
    IOPMAssertionRelease(assertionIDForSystemSleep);
}

static void simpleSignalHandler(int sig)
{
    // Try to restore the color profile and try to go down cleanly
    restoreUserColorProfile();
    releaseSleepAssertions();
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
    if (kernResult != KERN_SUCCESS) {
        NSLog(@"IOServiceOpen() failed in %s with kernResult = %d", __FUNCTION__, kernResult);
        return;
    }

    kernResult = IOObjectRelease(serviceObject);
    if (kernResult != KERN_SUCCESS)
        NSLog(@"IOObjectRelease() failed in %s with kernResult = %d", __FUNCTION__, kernResult);
}

void addSleepAssertions()
{
    CFStringRef assertionName = CFSTR("WebKit LayoutTestHelper");
    CFStringRef assertionDetails = CFSTR("WebKit layout-test helper tool is preventing sleep.");
    IOPMAssertionCreateWithDescription(kIOPMAssertionTypePreventUserIdleDisplaySleep,
        assertionName, assertionDetails, assertionDetails, NULL, 0, NULL, &assertionIDForDisplaySleep);
    IOPMAssertionCreateWithDescription(kIOPMAssertionTypePreventUserIdleSystemSleep,
        assertionName, assertionDetails, assertionDetails, NULL, 0, NULL, &assertionIDForSystemSleep);
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

    // Hooks the ways we might get told to clean up...
    signal(SIGINT, simpleSignalHandler);
    signal(SIGHUP, simpleSignalHandler);
    signal(SIGTERM, simpleSignalHandler);

    addSleepAssertions();
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
    releaseSleepAssertions();

    return 0;
}

#endif // !PLATFORM(IOS_FAMILY)

#if PLATFORM(IOS_FAMILY)
int main(int argc, char* argv[])
{
    return 0;
}
#endif
