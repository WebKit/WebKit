/*
 * Copyright (C) 2010-2015 Apple Inc. All rights reserved.
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
#import "ActivateFonts.h"

#import <CoreFoundation/CoreFoundation.h>
#import <CoreText/CTFontManager.h>
#import <WebKit/WKStringCF.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/VectorCocoa.h>

#if USE(APPKIT)
#import <AppKit/AppKit.h>
#endif

@interface WKTRFontActivatorDummyClass : NSObject
@end

@implementation WKTRFontActivatorDummyClass
@end

namespace WTR {

static NSURL *resourcesDirectoryURL()
{
    static NeverDestroyed<RetainPtr<NSURL>> resourcesDirectory([[NSBundle bundleForClass:[WKTRFontActivatorDummyClass class]] resourceURL]);
    return resourcesDirectory.get().get();
}

#if USE(APPKIT)

// Activating system copies of these fonts overrides any others that could be preferred, such as ones
// in /Library/Fonts/Microsoft, and which don't always have the same metrics.
// FIXME: Now that <rdar://problem/19553550> has been resolved we should have a better solution.
static void activateSystemCoreWebFonts()
{
    constexpr NSString *coreWebFontNames[] = {
        @"Andale Mono",
        @"Arial",
        @"Arial Black",
        @"Comic Sans MS",
        @"Courier New",
        @"Georgia",
        @"Impact",
        @"Times New Roman",
        @"Trebuchet MS",
        @"Verdana",
        @"Webdings"
    };

    NSArray *fontFiles = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:[NSURL fileURLWithPath:@"/Library/Fonts" isDirectory:YES]
        includingPropertiesForKeys:@[NSURLFileResourceTypeKey, NSURLNameKey] options:0 error:0];

    for (NSURL *fontURL in fontFiles) {
        NSString *resourceType;
        NSString *fileName;
        if (![fontURL getResourceValue:&resourceType forKey:NSURLFileResourceTypeKey error:0]
            || ![fontURL getResourceValue:&fileName forKey:NSURLNameKey error:0])
            continue;
        if (![resourceType isEqualToString:NSURLFileResourceTypeRegular])
            continue;

        // Activate all font variations, such as Arial Bold Italic.ttf. This algorithm is not 100% precise, as it
        // also activates e.g. Arial Unicode, which is not a variation of Arial.
        for (auto& coreWebFontName : coreWebFontNames) {
            if ([fileName hasPrefix:coreWebFontName]) {
                CFErrorRef error = nullptr;
                if (!CTFontManagerRegisterFontsForURL((__bridge CFURLRef)fontURL, kCTFontManagerScopeProcess, &error)) {
                    NSLog(@"Failed to activate %@: %@", coreWebFontName, error);
                    CFRelease(error);
                }
                break;
            }
        }
    }
}

#endif // USE(APPKIT)

void activateFonts()
{
    constexpr NSString *fontFileNames[] = {
        @"AHEM____.TTF",
        @"WebKitWeightWatcher100.ttf",
        @"WebKitWeightWatcher200.ttf",
        @"WebKitWeightWatcher300.ttf",
        @"WebKitWeightWatcher400.ttf",
        @"WebKitWeightWatcher500.ttf",
        @"WebKitWeightWatcher600.ttf",
        @"WebKitWeightWatcher700.ttf",
        @"WebKitWeightWatcher800.ttf",
        @"WebKitWeightWatcher900.ttf",
        @"FontWithFeatures.otf",
        @"FontWithFeatures.ttf",
    };

    auto fontURLs = createNSArray(fontFileNames, [] (NSString *name) {
        return [resourcesDirectoryURL() URLByAppendingPathComponent:name isDirectory:NO].absoluteURL;
    });

    CFArrayRef errors = nullptr;
    if (!CTFontManagerRegisterFontsForURLs((__bridge CFArrayRef)fontURLs.get(), kCTFontManagerScopeProcess, &errors)) {
        NSLog(@"Failed to activate fonts: %@", errors);
        CFRelease(errors);
        exit(1);
    }

#if USE(APPKIT)
    activateSystemCoreWebFonts();
#endif
}

void installFakeHelvetica(WKStringRef configuration)
{
    auto configurationString = adoptCF(WKStringCopyCFString(kCFAllocatorDefault, configuration));
    NSURL *resourceURL = [resourcesDirectoryURL() URLByAppendingPathComponent:[NSString stringWithFormat:@"FakeHelvetica-%@.ttf", configurationString.get()] isDirectory:NO];
    CFErrorRef error = nullptr;
    if (!CTFontManagerRegisterFontsForURL((__bridge CFURLRef)resourceURL, kCTFontManagerScopeProcess, &error)) {
#if PLATFORM(MAC) || (PLATFORM(IOS) && !PLATFORM(IOS_FAMILY_SIMULATOR))
        // Registering shadow fonts is only supported on macOS Mojave and iOS 12 or newer, but not iOS Simulator. See Bugs 180062 & 194761.
        NSLog(@"Failed to activate fake Helvetica: %@", error);
#endif
        CFRelease(error);
    }
}

void uninstallFakeHelvetica()
{
    NSFileManager *defaultManager = [NSFileManager defaultManager];
    NSError *nsError = nil;
    NSArray *urls = [defaultManager contentsOfDirectoryAtURL:resourcesDirectoryURL() includingPropertiesForKeys:@[NSURLNameKey] options:NSDirectoryEnumerationSkipsSubdirectoryDescendants | NSDirectoryEnumerationSkipsPackageDescendants | NSDirectoryEnumerationSkipsHiddenFiles error:&nsError];
    ASSERT(urls && !nsError);
    if (!urls || nsError)
        return;
    NSMutableArray *fontsToRemove = [NSMutableArray array];
    for (NSURL *url in urls) {
        if ([[url lastPathComponent] hasPrefix:@"FakeHelvetica"])
            [fontsToRemove addObject:url];
    }
    CTFontManagerUnregisterFontsForURLs((__bridge CFArrayRef)fontsToRemove, kCTFontManagerScopeProcess, nullptr);
}

}

