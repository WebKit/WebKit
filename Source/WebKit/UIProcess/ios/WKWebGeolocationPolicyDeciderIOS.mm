/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "WKWebGeolocationPolicyDecider.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKWebViewPrivateForTesting.h"
#import <CoreLocation/CoreLocation.h>
#import <WebCore/LocalizedStrings.h>
#import <WebCore/SecurityOrigin.h>
#import <pal/spi/cocoa/NSFileManagerSPI.h>
#import <wtf/Deque.h>
#import <wtf/SoftLinking.h>
#import <wtf/spi/cf/CFBundleSPI.h>

SOFT_LINK_FRAMEWORK(CoreLocation)

SOFT_LINK_CLASS(CoreLocation, CLLocationManager)

static const NSInteger kGeolocationChallengeThreshold = 2;
static constexpr Seconds kGeolocationChallengeTimeout = 24_h;
static NSString * const kGeolocationChallengeCount = @"ChallengeCount";
static NSString * const kGeolocationChallengeDate = @"ChallengeDate";
static CFStringRef CLAppResetChangedNotification = CFSTR("com.apple.locationd.appreset");

static void clearGeolocationCache(CFNotificationCenterRef center, void *observer, CFStringRef name, const void *object, CFDictionaryRef userInfo)
{
    [(WKWebGeolocationPolicyDecider *)observer clearCache];
}

static bool appHasPreciseLocationPermission()
{
    auto locationManager = adoptNS([allocCLLocationManagerInstance() init]);

    CLAuthorizationStatus authStatus = [locationManager authorizationStatus];
    return (authStatus == kCLAuthorizationStatusAuthorizedAlways || authStatus == kCLAuthorizationStatusAuthorizedWhenInUse)
        && [locationManager accuracyAuthorization] == CLAccuracyAuthorizationFullAccuracy;
}

static NSString *appDisplayName()
{
    auto *bundle = [NSBundle mainBundle];
    NSString *displayName = [bundle objectForInfoDictionaryKey:(__bridge NSString *)_kCFBundleDisplayNameKey];
    if (!displayName)
        displayName = [bundle objectForInfoDictionaryKey:(__bridge NSString *)kCFBundleNameKey];
    if (!displayName)
        displayName = [bundle objectForInfoDictionaryKey:(__bridge NSString *)kCFBundleExecutableKey];
    if (!displayName)
        displayName = [bundle bundleIdentifier];
    return displayName;
}

static NSString *getToken(const WebCore::SecurityOriginData& securityOrigin, NSURL *requestingURL)
{
    if ([requestingURL isFileURL])
        return [requestingURL path];
    return securityOrigin.host;
}

struct PermissionRequest {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    static std::unique_ptr<PermissionRequest> create(const WebCore::SecurityOriginData& origin, NSURL *requestingURL, WKWebView *view, id<WKWebAllowDenyPolicyListener> listener)
    {
        auto request = std::unique_ptr<PermissionRequest>(new PermissionRequest);
        request->token = getToken(origin, requestingURL);
        request->requestingURL = requestingURL;
        request->view = view;
        request->listener = listener;
        return request;
    }

    RetainPtr<NSString> token;
    RetainPtr<NSString> domain;
    RetainPtr<NSURL> requestingURL;
    RetainPtr<WKWebView> view;
    RetainPtr<id<WKWebAllowDenyPolicyListener>> listener;
};

@implementation WKWebGeolocationPolicyDecider {
@private
    RetainPtr<dispatch_queue_t> _diskDispatchQueue;
    RetainPtr<NSMutableDictionary> _sites;
    Deque<std::unique_ptr<PermissionRequest>> _challenges;
    std::unique_ptr<PermissionRequest> _activeChallenge;
}

+ (instancetype)sharedPolicyDecider
{
    static WKWebGeolocationPolicyDecider *policyDecider = nil;
    if (!policyDecider)
        policyDecider = [[WKWebGeolocationPolicyDecider alloc] init];
    return policyDecider;
}

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

    _diskDispatchQueue = adoptNS(dispatch_queue_create("com.apple.WebKit.WKWebGeolocationPolicyDecider", DISPATCH_QUEUE_SERIAL));

    CFNotificationCenterAddObserver(CFNotificationCenterGetDarwinNotifyCenter(), self, clearGeolocationCache, CLAppResetChangedNotification, NULL, CFNotificationSuspensionBehaviorCoalesce);

    return self;
}

- (void)dealloc
{
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(), self, CLAppResetChangedNotification, NULL);
    [super dealloc];
}

- (void)decidePolicyForGeolocationRequestFromOrigin:(const WebCore::SecurityOriginData&)securityOrigin requestingURL:(NSURL *)requestingURL view:(WKWebView *)view listener:(id<WKWebAllowDenyPolicyListener>)listener
{
    auto permissionRequest = PermissionRequest::create(securityOrigin, requestingURL, view, listener);
    _challenges.append(WTFMove(permissionRequest));
    [self _executeNextChallenge];
}

- (void)_executeNextChallenge
{
    if (_challenges.isEmpty())
        return;
    if (_activeChallenge)
        return;

    _activeChallenge = _challenges.takeFirst();
    [self _loadWithCompletionHandler:^{
        if ([_activeChallenge->view _shouldBypassGeolocationPromptForTesting]) {
            [self _finishActiveChallenge:YES];
            return;
        }
        NSInteger challengeCount = [self _getChallengeCountFromHistoryForToken:_activeChallenge->token.get() requestingURL:_activeChallenge->requestingURL.get()];
        if (challengeCount >= kGeolocationChallengeThreshold) {
            [self _finishActiveChallenge:YES];
            return;
        }
        if (challengeCount <= -kGeolocationChallengeThreshold) {
            [self _finishActiveChallenge:NO];
            return;
        }

        NSString *applicationName = appDisplayName();
        NSString *message;

    IGNORE_WARNINGS_BEGIN("format-nonliteral")
        NSString *title = [NSString stringWithFormat:WEB_UI_STRING("“%@” would like to use your current location.", "Prompt for a webpage to request location access. The parameter is the domain for the webpage."), _activeChallenge->token.get()];
        if (appHasPreciseLocationPermission())
            message = [NSString stringWithFormat:WEB_UI_STRING("This website will use your precise location because “%@” currently has access to your precise location.", "Message informing the user that the website will have precise location data"), applicationName];
        else
            message = [NSString stringWithFormat:WEB_UI_STRING("This website will use your approximate location because “%@” currently has access to your approximate location.", "Message informing the user that the website will have approximate location data"), applicationName];
    IGNORE_WARNINGS_END

        NSString *allowActionTitle = WEB_UI_STRING("Allow", "Action authorizing a webpage to access the user’s location.");
        NSString *denyActionTitle = WEB_UI_STRING_KEY("Don’t Allow", "Don’t Allow (website location dialog)", "Action denying a webpage access to the user’s location.");

        UIAlertController *alert = [UIAlertController alertControllerWithTitle:title message:message preferredStyle:UIAlertControllerStyleAlert];
        [alert _setTitleMaximumLineCount:0]; // No limit, we need to make sure the title doesn't get truncated.
        UIAlertAction *denyAction = [UIAlertAction actionWithTitle:denyActionTitle style:UIAlertActionStyleDefault handler:^(UIAlertAction *) {
            [self _addChallengeCount:-1 forToken:_activeChallenge->token.get() requestingURL:_activeChallenge->requestingURL.get()];
            [self _finishActiveChallenge:NO];
        }];
        UIAlertAction *allowAction = [UIAlertAction actionWithTitle:allowActionTitle style:UIAlertActionStyleDefault handler:^(UIAlertAction *) {
            [self _addChallengeCount:1 forToken:_activeChallenge->token.get() requestingURL:_activeChallenge->requestingURL.get()];
            [self _finishActiveChallenge:YES];
        }];

        [alert addAction:denyAction];
        [alert addAction:allowAction];

        [[_activeChallenge->view window].rootViewController presentViewController:alert animated:YES completion:nil];
    }];
}

- (void)_finishActiveChallenge:(BOOL)allow
{
    ASSERT(_activeChallenge);
    if (allow)
        [_activeChallenge->listener allow];
    else
        [_activeChallenge->listener deny];
    _activeChallenge = nullptr;
    [self _executeNextChallenge];
}

- (void)clearCache
{
    _sites = nil;

    dispatch_async(_diskDispatchQueue.get(), ^{
        [[NSFileManager defaultManager] _web_removeFileOnlyAtPath:[self _siteFile]];
    });
}

- (NSString *)_siteFileInContainerDirectory:(NSString *)containerDirectory creatingIntermediateDirectoriesIfNecessary:(BOOL)createIntermediateDirectories
{
    NSString *webKitDirectory = [containerDirectory stringByAppendingPathComponent:@"Library/WebKit"];
    if (createIntermediateDirectories)
        [[NSFileManager defaultManager] _web_createDirectoryAtPathWithIntermediateDirectories:webKitDirectory attributes:nil];
    return [webKitDirectory stringByAppendingPathComponent:@"GeolocationSitesV2.plist"];
}

- (NSString *)_siteFile
{
    static NSString *sSiteFile = nil;
    if (!sSiteFile)
        sSiteFile = [[self _siteFileInContainerDirectory:NSHomeDirectory() creatingIntermediateDirectoriesIfNecessary:YES] retain];
    return sSiteFile;
}

static RetainPtr<NSMutableDictionary> createChallengeDictionary(NSData *data)
{
    NSError *error = nil;
    NSPropertyListFormat dbFormat = NSPropertyListBinaryFormat_v1_0;
    return [NSPropertyListSerialization propertyListWithData:data options:NSPropertyListMutableContainersAndLeaves format:&dbFormat error:&error];
}

- (void)_loadWithCompletionHandler:(void (^)())completionHandler
{
    ASSERT(isMainRunLoop());
    if (_sites) {
        completionHandler();
        return;
    }

    dispatch_async(_diskDispatchQueue.get(), ^{
        RetainPtr<NSMutableDictionary> sites;
        RetainPtr<NSData> data = [NSData dataWithContentsOfFile:[self _siteFile] options:NSDataReadingMappedIfSafe error:NULL];
        if (data)
            sites = createChallengeDictionary(data.get());
        else
            sites = adoptNS([[NSMutableDictionary alloc] init]);

        dispatch_async(dispatch_get_main_queue(), ^{
            if (!_sites)
                _sites = sites;
            completionHandler();
        });
    });
}

- (void)_save
{
    if (![_sites count])
        return;

    NSError *error = nil;
    NSData *data = [NSPropertyListSerialization dataWithPropertyList:_sites.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];
    if (!data)
        return;

    NSString *siteFilePath = [self _siteFile];
    dispatch_async(_diskDispatchQueue.get(), ^{
        [data writeToFile:siteFilePath atomically:YES];
    });
}

- (NSInteger)_getChallengeCountFromHistoryForToken:(NSString *)token requestingURL:(NSURL *)requestingURL
{
    NSDictionary *challengeHistory = (NSDictionary *)[_sites objectForKey:token];
    if (challengeHistory && ![[requestingURL scheme] isEqualToString:@"data"]) {
        NSDate *lastChallengeDate = [challengeHistory objectForKey:kGeolocationChallengeDate];
        NSDate *expirationDate = [lastChallengeDate dateByAddingTimeInterval:kGeolocationChallengeTimeout.seconds()];
        if ([expirationDate compare:[NSDate date]] != NSOrderedAscending)
            return [(NSNumber *)[challengeHistory objectForKey:kGeolocationChallengeCount] integerValue];
    }
    return 0;
}

- (void)_addChallengeCount:(NSInteger)count forToken:(NSString *)token requestingURL:(NSURL *)requestingURL
{
    NSInteger challengeCount = [self _getChallengeCountFromHistoryForToken:token requestingURL:requestingURL];
    challengeCount += count;

    NSDictionary *savedChallenge = @{
        kGeolocationChallengeCount : @(challengeCount),
        kGeolocationChallengeDate : [NSDate date]
    };
    [_sites setObject:savedChallenge forKey:token];
    [self _save];
}

@end

#endif // PLATFORM(IOS_FAMILY)
