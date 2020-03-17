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

#import <wtf/ObjCRuntimeExtras.h>

static IMP registerDefaultsOriginal = nil;
static bool registeringDefaults = false;

static void registerDefaultsOverride(id self, SEL selector, NSDictionary<NSString *, id> *dictionary)
{
    registeringDefaults = true;
    if (registerDefaultsOriginal)
        wtfCallIMP<void>(registerDefaultsOriginal, self, selector, dictionary);
    registeringDefaults = false;
}

@implementation WKUserDefaults

- (void)_notifyObserversOfChangeFromValuesForKeys:(NSDictionary<NSString *, id> *)oldValues toValuesForKeys:(NSDictionary<NSString *, id> *)newValues
{
    if (registeringDefaults)
        return;

    [super _notifyObserversOfChangeFromValuesForKeys:oldValues toValuesForKeys:newValues];

    if ([oldValues isEqualToDictionary:newValues])
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

        if (m_observer)
            [m_observer preferenceDidChange:m_suiteName key:key encodedValue:encodedString];
    }
}

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary<NSKeyValueChangeKey, id> *)change context:(void *)context
{
}

- (instancetype)initWithSuiteName:(NSString *)suitename
{
    m_suiteName = suitename;
    m_observer = nil;
    return [super initWithSuiteName:suitename];
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

+ (void)swizzleRegisterDefaults
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        Method registerDefaultsMethod = class_getInstanceMethod(objc_getClass("NSUserDefaults"), @selector(registerDefaults:));
        registerDefaultsOriginal = method_setImplementation(registerDefaultsMethod, (IMP)registerDefaultsOverride);
    });
}

- (instancetype)init
{
    std::initializer_list<NSString*> domains = {
        @"com.apple.Accessibility",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.AdLib",
        @"com.apple.CFNetwork",
        @"com.apple.EmojiPreferences",
        @"com.apple.FontParser",
        @"com.apple.ImageIO",
        @"com.apple.InputModePreferences",
#else
        @"com.apple.ATS",
        @"com.apple.CoreGraphics",
        @"com.apple.DownloadAssessment",
        @"com.apple.HIToolbox",
#endif
        @"com.apple.LaunchServices",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.Metal",
        @"com.apple.MobileAsset",
        @"com.apple.Preferences",
        @"com.apple.SpeakSelection",
        @"com.apple.UIKit",
        @"com.apple.VoiceOverTouch",
#else
        @"com.apple.MultitouchSupport"
        @"com.apple.ServicesMenu.Services"
        @"com.apple.ViewBridge"
#endif
        @"com.apple.WebFoundation",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.WebKit.WebContent",
        @"com.apple.WebUI",
        @"com.apple.airplay",
        @"com.apple.applejpeg",
        @"com.apple.audio.virtualaudio",
        @"com.apple.avfaudio",
#endif
        @"com.apple.avfoundation",
        @"com.apple.avfoundation.frecents",
        @"com.apple.avfoundation.videoperformancehud",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.avkit",
        @"com.apple.coreanimation",
        @"com.apple.coreaudio",
#endif
        @"com.apple.coremedia",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.corevideo",
        @"com.apple.da",
        @"com.apple.hangtracer",
        @"com.apple.indigo",
        @"com.apple.iokit.IOMobileGraphicsFamily",
        @"com.apple.itunesstored",
        @"com.apple.keyboard",
#else
        @"com.apple.crypto",
        @"com.apple.driver.AppleBluetoothMultitouch.mouse",
        @"com.apple.driver.AppleBluetoothMultitouch.trackpad",
        @"com.apple.driver.AppleHIDMouse"
#endif
        @"com.apple.lookup.shared",
        @"com.apple.mediaaccessibility",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.mediaaccessibility.public",
        @"com.apple.mediaremote",
        @"com.apple.mobileipod",
        @"com.apple.mt",
#else
        @"com.apple.networkConnect"
#endif
        @"com.apple.opengl",
#if PLATFORM(IOS_FAMILY)
        @"com.apple.preferences.sounds",
        @"com.apple.security",
        @"com.apple.voiceservices",
        @"com.apple.voiceservices.logging",
#else
        @"com.apple.speech.voice.prefs",
        @"com.apple.systemsound",
        @"com.apple.universalaccess",
        @"com.nvidia.OpenGL",
        @"edu.mit.Kerberos",
#endif
        @"kCFPreferencesAnyApplication",
#if !PLATFORM(IOS_FAMILY)
        @"pbs",
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
    return [super init];
}

- (void)preferenceDidChange:(NSString *)domain key:(NSString *)key encodedValue:(NSString *)encodedValue
{
#if ENABLE(CFPREFS_DIRECT_MODE)
    Optional<String> encodedString;
    if (encodedValue)
        encodedString = String(encodedValue);
    for (auto* processPool : WebKit::WebProcessPool::allProcessPools())
        processPool->notifyPreferencesChanged(domain, key, encodedString);
#endif
}
@end
