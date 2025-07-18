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
#import <wtf/WeakObjCPtr.h>

@interface WKUserDefaults : NSUserDefaults {
@private
    RetainPtr<NSString> m_suiteName;
@public
    WeakObjCPtr<WKPreferenceObserver> m_observer;
}
- (void)findPreferenceChangesAndNotifyForKeys:(NSDictionary<NSString *, id> *)oldValues toValuesForKeys:(NSDictionary<NSString *, id> *)newValues;
@end

@interface WKPreferenceObserver () {
@private
    Vector<RetainPtr<WKUserDefaults>> m_userDefaults;
}
@end

@implementation WKUserDefaults

- (void)findPreferenceChangesAndNotifyForKeys:(NSDictionary<NSString *, id> *)oldValues toValuesForKeys:(NSDictionary<NSString *, id> *)newValues
{
    if (!m_observer)
        return;

    for (NSString *key in oldValues) {
        RetainPtr<id> oldValue = oldValues[key];
        RetainPtr<id> newValue = newValues[key];

        if ([oldValue isEqual:newValue.get()])
            continue;

        if (newValue && ![[newValue class] supportsSecureCoding])
            continue;

        RetainPtr<NSString> encodedString;

        if (newValue) {
            NSError *e = nil;
            auto data = retainPtr([NSKeyedArchiver archivedDataWithRootObject:newValue.get() requiringSecureCoding:YES error:&e]);
            ASSERT(!e);
            encodedString = [data base64EncodedStringWithOptions:0];
        }

        auto systemValue = adoptCF(CFPreferencesCopyValue((__bridge CFStringRef)key, kCFPreferencesAnyApplication, kCFPreferencesAnyUser, kCFPreferencesAnyHost));
        auto globalValue = adoptCF(CFPreferencesCopyValue((__bridge CFStringRef)key, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost));
        auto domainValue = adoptCF(CFPreferencesCopyValue((__bridge CFStringRef)key, (__bridge CFStringRef)m_suiteName.get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost));

        auto preferenceValuesAreEqual = [] (id a, id b) {
            return a == b || [a isEqual:b];
        };

        if (preferenceValuesAreEqual((__bridge id)systemValue.get(), newValue.get()) || preferenceValuesAreEqual((__bridge id)globalValue.get(), newValue.get()))
            [m_observer preferenceDidChange:nil key:key encodedValue:encodedString.get()];

        if (preferenceValuesAreEqual((__bridge id)domainValue.get(), newValue.get()))
            [m_observer preferenceDidChange:m_suiteName.get() key:key encodedValue:encodedString.get()];
    }
}

- (void)_notifyObserversOfChangeFromValuesForKeys:(NSDictionary<NSString *, id> *)oldValues toValuesForKeys:(NSDictionary<NSString *, id> *)newValues
{
    [super _notifyObserversOfChangeFromValuesForKeys:oldValues toValuesForKeys:newValues];

    if (!isMainRunLoop()) {
        [self findPreferenceChangesAndNotifyForKeys:oldValues toValuesForKeys:newValues];
        return;
    }

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [self, protectedSelf = retainPtr(self), oldValues = retainPtr(oldValues), newValues = retainPtr(newValues)] {
        [self findPreferenceChangesAndNotifyForKeys:oldValues.get() toValuesForKeys:newValues.get()];
    });
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
    static NeverDestroyed<RetainPtr<WKPreferenceObserver>> instance = adoptNS([[[self class] alloc] init]);
    return instance.get().get();
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    std::initializer_list<NSString*> domains = {
        @"com.apple.Accessibility",
        @"com.apple.mediaaccessibility",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.AdLib",
        @"com.apple.SpeakSelection",
        @"com.apple.UIKit",
        @"com.apple.WebUI",
        @"com.apple.avfaudio",
        @"com.apple.itunesstored",
        @"com.apple.mediaremote",
        @"com.apple.preferences.sounds",
        @"com.apple.voiceservices",
#else
        @"com.apple.CFNetwork",
        @"com.apple.CoreGraphics",
        @"com.apple.HIToolbox",
        @"com.apple.ServicesMenu.Services",
        @"com.apple.ViewBridge",
        @"com.apple.avfoundation",
        @"com.apple.avfoundation.videoperformancehud",
        @"com.apple.driver.AppleBluetoothMultitouch.mouse",
        @"com.apple.driver.AppleBluetoothMultitouch.trackpad",
        @"com.apple.speech.voice.prefs",
        @"com.apple.universalaccess",
#endif
    };

    for (RetainPtr domain : domains) {
        auto userDefaults = adoptNS([[WKUserDefaults alloc] initWithSuiteName:domain.get()]);
        if (!userDefaults) {
            WTFLogAlways("Could not init user defaults instance for domain %s", String(domain.get()).utf8().data());
            continue;
        }
        userDefaults->m_observer = self;
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
    RunLoop::mainSingleton().dispatch([domain = retainPtr(domain), key = retainPtr(key), encodedValue = retainPtr(encodedValue)] {
        std::optional<String> encodedValueString;
        if (encodedValue)
            encodedValueString = String(encodedValue.get());
        String domainString = domain.get();
        String keyString = key.get();

#if ENABLE(GPU_PROCESS)
        if (RefPtr gpuProcess = WebKit::GPUProcessProxy::singletonIfCreated())
            gpuProcess->notifyPreferencesChanged(domainString, keyString, encodedValueString);
#endif

        if (RefPtr networkProcess = WebKit::NetworkProcessProxy::defaultNetworkProcess().get())
            networkProcess->notifyPreferencesChanged(domainString, keyString, encodedValueString);

        for (auto& processPool : WebKit::WebProcessPool::allProcessPools())
            processPool->notifyPreferencesChanged(domainString, keyString, encodedValueString);
    });
#endif
}
@end
