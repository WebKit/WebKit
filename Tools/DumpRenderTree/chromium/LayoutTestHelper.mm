/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#import <AppKit/AppKit.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

// This is a simple helper app that changes the color sync profile to the
// generic profile and back when done.  This program is managed by the layout
// test script, so it can do the job for multiple DumpRenderTree while they are
// running layout tests.

static CMProfileRef userColorProfile = 0;

static void saveCurrentColorProfile()
{
    CGDirectDisplayID displayID = CGMainDisplayID();
    CMProfileRef previousProfile;
    CMError error = CMGetProfileByAVID((UInt32)displayID, &previousProfile);
    if (error) {
        NSLog(@"failed to get the current color profile, pixmaps won't match. "
              @"Error: %d", (int)error);
    } else {
        userColorProfile = previousProfile;
    }
}

static void installLayoutTestColorProfile()
{
    // To make sure we get consistent colors (not dependent on the Main display),
    // we force the generic rgb color profile.  This cases a change the user can
    // see.

    CGDirectDisplayID displayID = CGMainDisplayID();
    NSColorSpace* genericSpace = [NSColorSpace genericRGBColorSpace];
    CMProfileRef genericProfile = (CMProfileRef)[genericSpace colorSyncProfile];
    CMError error = CMSetProfileByAVID((UInt32)displayID, genericProfile);
    if (error) {
        NSLog(@"failed install the generic color profile, pixmaps won't match. "
              @"Error: %d", (int)error);
    }
}

static void restoreUserColorProfile(void)
{
    if (!userColorProfile)
        return;
    CGDirectDisplayID displayID = CGMainDisplayID();
    CMError error = CMSetProfileByAVID((UInt32)displayID, userColorProfile);
    CMCloseProfile(userColorProfile);
    if (error) {
        NSLog(@"Failed to restore color profile, use System Preferences -> "
              @"Displays -> Color to reset. Error: %d", (int)error);
    }
    userColorProfile = 0;
}

static void simpleSignalHandler(int sig)
{
    // Try to restore the color profile and try to go down cleanly
    restoreUserColorProfile();
    exit(128 + sig);
}

int main(int argc, char* argv[])
{
    NSAutoreleasePool* pool = [[NSAutoreleasePool alloc] init];

    // Hooks the ways we might get told to clean up...
    signal(SIGINT, simpleSignalHandler);
    signal(SIGHUP, simpleSignalHandler);
    signal(SIGTERM, simpleSignalHandler);

    // Save off the current profile, and then install the layout test profile.
    saveCurrentColorProfile();
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
