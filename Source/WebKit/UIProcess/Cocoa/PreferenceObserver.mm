/*
* Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "PreferenceObserver.h"

#import "WebProcessPool.h"
#import <pal/spi/cocoa/NSUserDefaultsSPI.h>

@interface WKUserDefaults : NSUserDefaults {
@private
    NSString *m_suiteName;
@public
    WKPreferenceObserver *m_observer;
}
@end

@interface WKPreferenceObserver () {
@private
    Vector<RetainPtr<WKUserDefaults>> m_userDefaults;
}
@end

@implementation WKUserDefaults

- (void)_notifyObserversOfChangeFromValuesForKeys:(NSDictionary<NSString *, id> *)oldValues toValuesForKeys:(NSDictionary<NSString *, id> *)newValues
{
    [super _notifyObserversOfChangeFromValuesForKeys:oldValues toValuesForKeys:newValues];

    if (!m_observer)
        return;

    for (NSString *key in oldValues) {
        id oldValue = oldValues[key];
        id newValue = newValues[key];

        if ([oldValue isEqual:newValue])
            continue;

        if (newValue && ![[newValue class] supportsSecureCoding])
            continue;

        NSString *encodedString = nil;

        if (newValue) {
            NSError *e = nil;
            auto data = retainPtr([NSKeyedArchiver archivedDataWithRootObject:newValue requiringSecureCoding:YES error:&e]);
            ASSERT(!e);
            encodedString = [data base64EncodedStringWithOptions:0];
        }

        auto globalValue = adoptCF(CFPreferencesCopyValue((__bridge CFStringRef)key, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
        auto domainValue = adoptCF(CFPreferencesCopyValue((__bridge CFStringRef)key, (__bridge CFStringRef)m_suiteName, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));

        auto preferenceValuesAreEqual = [] (id a, id b) {
            return a == b || [a isEqual:b];
        };

        if (preferenceValuesAreEqual((__bridge id)globalValue.get(), newValue))
            [m_observer preferenceDidChange:nil key:key encodedValue:encodedString];

        if (preferenceValuesAreEqual((__bridge id)domainValue.get(), newValue))
            [m_observer preferenceDidChange:m_suiteName key:key encodedValue:encodedString];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
}

- (instancetype)initWithSuiteName:(NSString *)suiteName
{
    if (!(self = [super initWithSuiteName:suiteName]))
        return nil;

    m_suiteName = suiteName;
    m_observer = nil;
    return self;
}
@end

@implementation WKPreferenceObserver

+ (id)sharedInstance
{
    static WKPreferenceObserver *instance = nil;

    if (!instance)
        instance = [[[self class] alloc] init];

    return instance;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    std::initializer_list<NSString*> domains = {
#if PLATFORM(IOS_FAMILY)
        @"com.apple.Accessibility",
        @"com.apple.AdLib",
        @"com.apple.Preferences",
        @"com.apple.SpeakSelection",
        @"com.apple.UIKit",
        @"com.apple.WebUI",
        @"com.apple.avfaudio",
        @"com.apple.itunesstored",
        @"com.apple.mediaremote",
        @"com.apple.preferences.sounds",
        @"com.apple.voiceservices",
#else
        @"com.apple.CoreGraphics",
        @"com.apple.HIToolbox",
        @"com.apple.ServicesMenu.Services",
        @"com.apple.ViewBridge",
        @"com.apple.avfoundation",
        @"com.apple.avfoundation.videoperformancehud",
        @"com.apple.driver.AppleBluetoothMultitouch.mouse",
        @"com.apple.driver.AppleBluetoothMultitouch.trackpad",
        @"com.apple.mediaaccessibility",
        @"com.apple.speech.voice.prefs",
        @"com.apple.universalaccess",
#endif
    };

    for (auto domain : domains) {
        auto userDefaults = adoptNS([[WKUserDefaults alloc] initWithSuiteName:domain]);
        if (!userDefaults) {
            WTFLogAlways("Could not init user defaults instance for domain %s", String(domain).utf8().data());
            continue;
        }
        userDefaults.get()->m_observer = self;
        // Start observing a dummy key in order to make the preference daemon become aware of our NSUserDefaults instance.
        // This is to make sure we receive KVO notifications. We cannot use normal KVO techniques here, since we are looking
        // for _any_ changes in a preference domain. For normal KVO techniques to work, we need to provide the specific
        // key(s) we want to observe, but that set of keys is unknown to us.
        [userDefaults.get() addObserver:userDefaults.get() forKeyPath:@"testkey" options:NSKeyValueObservingOptionNew context:nil];
        m_userDefaults.append(userDefaults);
    }
    return self;
}

- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
#if ENABLE(CFPREFS_DIRECT_MODE)
    dispatch_async(dispatch_get_main_queue(), ^{
        Optional<String> encodedString;
        if (encodedValue)
            encodedString = String(encodedValue);

        for (auto* processPool : WebKit::WebProcessPool::allProcessPools())
            processPool->notifyPreferencesChanged(domain, key, encodedString);
    });
#endif
}
@end
