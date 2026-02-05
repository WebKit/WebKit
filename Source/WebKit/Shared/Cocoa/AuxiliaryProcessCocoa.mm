/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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
#import "AuxiliaryProcess.h"

#import "Logging.h"
#import "OSStateSPI.h"
#import "SharedBufferReference.h"
#import "WKCrashReporter.h"
#import "WKProcessExtension.h"
#import "WKWebView.h"
#import "XPCServiceEntryPoint.h"
#import <WebCore/FloatingPointEnvironment.h>
#import <algorithm>
#import <mach/task.h>
#import <objc/runtime.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <pal/spi/cocoa/NotifySPI.h>
#import <sys/resource.h>
#import <sys/sysctl.h>
#import <sys/types.h>
#import <wtf/FileSystem.h>
#import <wtf/MallocSpan.h>
#import <wtf/RetainPtr.h>
#import <wtf/RuntimeApplicationChecks.h>
#import <wtf/StdLibExtras.h>
#import <wtf/SystemMalloc.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#import <wtf/cocoa/SoftLinking.h>
#import <wtf/cocoa/SpanCocoa.h>
#import <wtf/darwin/DispatchExtras.h>
#import <wtf/text/MakeString.h>

#if ENABLE(CFPREFS_DIRECT_MODE)
#import "AccessibilitySupportSPI.h"
#import <pal/spi/cocoa/AccessibilitySupportSPI.h>
#endif

#if PLATFORM(MAC)
#import "AppKitSPI.h"
#import <pal/spi/mac/HIServicesSPI.h>
#endif

#if USE(SOURCE_APPLICATION_AUDIT_DATA)
#import <wtf/darwin/XPCObjectPtr.h>
#endif

#import <pal/cf/AudioToolboxSoftLink.h>

#if HAVE(UPDATE_WEB_ACCESSIBILITY_SETTINGS) && ENABLE(CFPREFS_DIRECT_MODE)
SOFT_LINK_OPTIONAL(libAccessibility, _AXSUpdateWebAccessibilitySettings, void, (), ());
#endif

namespace WebKit {

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
static void initializeTimerCoalescingPolicy()
{
    // Set task_latency and task_throughput QOS tiers as appropriate for a visible application.
    struct task_qos_policy qosinfo = { LATENCY_QOS_TIER_0, THROUGHPUT_QOS_TIER_0 };
    kern_return_t kr = task_policy_set(mach_task_self(), TASK_BASE_QOS_POLICY, (task_policy_t)&qosinfo, TASK_QOS_POLICY_COUNT);
    ASSERT_UNUSED(kr, kr == KERN_SUCCESS);
}
#endif

#if PLATFORM(MAC)
static void disableDowngradeToLayoutManager()
{
    RetainPtr existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
    RetainPtr newArguments = adoptNS([existingArguments mutableCopy]);
    [newArguments setValue:@NO forKey:@"NSTextViewAllowsDowngradeToLayoutManager"];
    [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments.get() forName:NSArgumentDomain];
}
#endif

void AuxiliaryProcess::platformInitialize(const AuxiliaryProcessInitializationParameters& parameters)
{
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    initializeTimerCoalescingPolicy();
#endif

    FloatingPointEnvironment& floatingPointEnvironment = FloatingPointEnvironment::singleton();
#if PLATFORM(IOS_FAMILY) && !PLATFORM(MACCATALYST)
    floatingPointEnvironment.enableDenormalSupport();
#endif
    floatingPointEnvironment.saveMainThreadEnvironment();

    [[NSFileManager defaultManager] changeCurrentDirectoryPath:retainPtr([[NSBundle mainBundle] bundlePath]).get()];

    setApplicationBundleIdentifier(parameters.clientBundleIdentifier);

#if USE(SOURCE_APPLICATION_AUDIT_DATA)
    if (OSObjectPtr<xpc_connection_t> connection = parameters.connectionIdentifier.xpcConnection) {
        audit_token_t auditToken { };
        xpc_connection_get_audit_token(connection.get(), &auditToken);
        setApplicationAuditToken(auditToken);
    }
#endif

#if PLATFORM(MAC)
    disableDowngradeToLayoutManager();
#endif

#if USE(EXTENSIONKIT)
    setProcessIsExtension(!!WKProcessExtension.sharedInstance);
#endif
}

void AuxiliaryProcess::didReceiveInvalidMessage(IPC::Connection&, IPC::MessageName messageName, const Vector<uint32_t>& indicesOfObjectsFailingDecoding)
{
    auto errorMessage = makeString("Received invalid message: '"_s, description(messageName), "' ("_s, messageName, ')');
    logAndSetCrashLogMessage(errorMessage.utf8().data());

    ASSERT(indicesOfObjectsFailingDecoding.size() <= 6);
    auto index = [&](size_t i) -> int32_t {
        return i < indicesOfObjectsFailingDecoding.size() ? indicesOfObjectsFailingDecoding[i] : -1;
    };
    CRASH_WITH_INFO(WTF::enumToUnderlyingType(messageName), index(5), index(4), index(3), index(2), index(1), index(0));
}

bool AuxiliaryProcess::parentProcessHasEntitlement(ASCIILiteral entitlement)
{
    return WTF::hasEntitlement(protect(protect(parentProcessConnection())->xpcConnection()).get(), entitlement);
}

void AuxiliaryProcess::platformStopRunLoop()
{
    XPCServiceExit();
}

#if USE(OS_STATE)

void AuxiliaryProcess::registerWithStateDumper(ASCIILiteral title)
{
    os_state_add_handler(mainDispatchQueueSingleton(), [weakThis = WeakPtr { *this }, title] (os_state_hints_t hints) -> os_state_data_s* {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return nullptr;

        @autoreleasepool {
            os_state_data_t os_state = nullptr;

            // Only gather state on faults and sysdiagnose. It's overkill for
            // general error messages.
            if (hints->osh_api == OS_STATE_API_ERROR)
                return os_state;

            auto stateDictionary = protectedThis->additionalStateForDiagnosticReport();

            // Submitting an empty process state object may provide an
            // indication of the existance of private sessions, which we'd like
            // to hide, so don't return empty dictionaries.
            if (![stateDictionary count])
                return os_state;

            // Serialize the accumulated process state so that we can put the
            // result in an os_state_data_t structure.
            NSError *error = nil;
            RetainPtr data = [NSPropertyListSerialization dataWithPropertyList:stateDictionary.get() format:NSPropertyListBinaryFormat_v1_0 options:0 error:&error];

            if (!data) {
                ASSERT_NOT_REACHED_WITH_MESSAGE("Failed to serialize OS state info with error: %@", error);
                return os_state;
            }

            auto neededSize = OS_STATE_DATA_SIZE_NEEDED(data.get().length);
            auto osStateSpan = MallocSpan<uint8_t, SystemMalloc>::malloc(neededSize);
            zeroSpan(osStateSpan.mutableSpan());
            os_state = (os_state_data_t)osStateSpan.leakSpan().data();
            os_state->osd_type = OS_STATE_DATA_SERIALIZED_NSCF_OBJECT;
            os_state->osd_data_size = data.get().length;
            strlcpy(os_state->osd_title, title.characters(), sizeof(os_state->osd_title));
            memcpySpan(unsafeMakeSpan(os_state->osd_data, os_state->osd_data_size), span(data.get()));

            return os_state;
        }
    });
}

#endif // USE(OS_STATE)

#if ENABLE(CFPREFS_DIRECT_MODE)
RetainPtr<id> AuxiliaryProcess::decodePreferenceValue(const std::optional<String>& encodedValue)
{
    if (!encodedValue)
        return nil;
    
    RetainPtr encodedData = adoptNS([[NSData alloc] initWithBase64EncodedString:encodedValue->createNSString().get() options:0]);
    RetainPtr classes = [NSSet setWithObjects:
        NSString.class,
        NSMutableString.class,
        NSNumber.class,
        NSDate.class,
        NSDictionary.class,
        NSMutableDictionary.class,
        NSArray.class,
        NSMutableArray.class,
        NSData.class,
        NSMutableData.class,
    nil];
    SUPPRESS_UNRETAINED_LOCAL NSError *error { nil };
    RetainPtr<id> result = [NSKeyedUnarchiver _strictlyUnarchivedObjectOfClasses:classes.get() fromData:encodedData.get() error:&error];
    ASSERT(!error);
    return result;
}

void AuxiliaryProcess::setPreferenceValue(const String& domain, const String& key, id value)
{
    if (domain.isEmpty()) {
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, kCFPreferencesAnyApplication, kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
#if ASSERT_ENABLED
        RetainPtr valueAfterSetting = [[NSUserDefaults standardUserDefaults] objectForKey:key.createNSString().get()];
        ASSERT(valueAfterSetting.get() == value || [valueAfterSetting.get() isEqual:value] || key == "AppleLanguages"_s || key == "PayloadUUID"_s);
#endif
    } else
        CFPreferencesSetValue(key.createCFString().get(), (__bridge CFPropertyListRef)value, domain.createCFString().get(), kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
}

void AuxiliaryProcess::preferenceDidUpdate(const String& domain, const String& key, const std::optional<String>& encodedValue)
{
    RetainPtr<id> value;
    if (encodedValue) {
        value = decodePreferenceValue(encodedValue);
        if (!value)
            return;
    }
    setPreferenceValue(domain, key, value.get());
    handlePreferenceChange(domain, key, value.get());
}

const WTF::String& AuxiliaryProcess::increaseContrastPreferenceKey()
{
#if PLATFORM(MAC)
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("increaseContrast"));
#else
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("DarkenSystemColors"));
#endif
    return key;
}

#if USE(APPKIT)
static const WTF::String& invertColorsPreferenceKey()
{
    static NeverDestroyed<WTF::String> key(MAKE_STATIC_STRING_IMPL("whiteOnBlack"));
    return key;
}
#endif

void AuxiliaryProcess::handleAXPreferenceChange(const String& domain, const String& key, id value)
{
#if HAVE(UPDATE_WEB_ACCESSIBILITY_SETTINGS)
    if (!libAccessibilityLibrary())
        return;
#endif

    if (domain == String(kAXSAccessibilityPreferenceDomain)) {
#if HAVE(UPDATE_WEB_ACCESSIBILITY_SETTINGS)
        if (_AXSUpdateWebAccessibilitySettingsPtr())
            _AXSUpdateWebAccessibilitySettingsPtr()();
#elif PLATFORM(IOS_FAMILY)
        // If the update method is not available, to update the cache inside AccessibilitySupport,
        // these methods need to be called directly.
        if (CFEqual(key.createCFString().get(), kAXSReduceMotionPreference) && [value isKindOfClass:[NSNumber class]])
            _AXSSetReduceMotionEnabled([(NSNumber *)value boolValue]);
        else if (key == increaseContrastPreferenceKey() && [value isKindOfClass:[NSNumber class]])
            _AXSSetDarkenSystemColors([(NSNumber *)value boolValue]);
#endif
    }

#if USE(APPKIT)
    auto cfKey = key.createCFString();
    if (CFEqual(cfKey.get(), kAXInterfaceReduceMotionKey) || CFEqual(cfKey.get(), kAXInterfaceIncreaseContrastKey) || CFEqual(cfKey.get(), kAXInterfaceDifferentiateWithoutColorKey) || key == invertColorsPreferenceKey()) {
        [NSWorkspace _invalidateAccessibilityDisplayValues];
        accessibilitySettingsDidChange();
    }
#endif
}

void AuxiliaryProcess::handlePreferenceChange(const String& domain, const String& key, id value)
{
    handleAXPreferenceChange(domain, key, value);
    dispatchSimulatedNotificationsForPreferenceChange(key);
}

#endif // ENABLE(CFPREFS_DIRECT_MODE)

void AuxiliaryProcess::setApplicationIsDaemon()
{
#if PLATFORM(MAC)
    // Having a window server connection in this process would result in spin logs (<rdar://problem/13239119>).
    OSStatus error = SetApplicationIsDaemon(true);
    ASSERT_UNUSED(error, error == noErr);
#elif PLATFORM(MACCATALYST)
    CGSSetDenyWindowServerConnections(true);
#endif
}

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
void AuxiliaryProcess::consumeAudioComponentRegistrations(const IPC::SharedBufferReference& data)
{
    using namespace PAL;

    if (!PAL::isAudioToolboxCoreFrameworkAvailable() || !PAL::canLoad_AudioToolboxCore_AudioComponentApplyServerRegistrations())
        return;

    if (data.isNull())
        return;
    auto registrations = data.unsafeBuffer()->createCFData();

    auto err = AudioComponentApplyServerRegistrations(registrations.get());
    if (noErr != err)
        RELEASE_LOG_ERROR(Process, "Could not apply AudioComponent registrations, err(%ld)", static_cast<long>(err));
}
#endif

bool AuxiliaryProcess::isSystemWebKit()
{
    static bool isSystemWebKit = []() -> bool {
        auto imagePath = class_getImageName([WKWebView class]);
        if (!imagePath)
            return false;

        RetainPtr path = adoptNS([[NSString alloc] initWithUTF8String:imagePath]);
#if HAVE(READ_ONLY_SYSTEM_VOLUME)
        if ([path hasPrefix:@"/Library/Apple/System/"])
            return true;
#endif
        return [path hasPrefix:RetainPtr { FileSystem::systemDirectoryPath() }.get()];
    }();

    return isSystemWebKit;
}

void AuxiliaryProcess::setNotifyOptions()
{
#if ENABLE(NOTIFY_BLOCKING)
    notify_set_options(NOTIFY_OPT_DISPATCH);
#elif ENABLE(NOTIFY_FILTERING)
    notify_set_options(NOTIFY_OPT_DISPATCH | NOTIFY_OPT_REGEN | NOTIFY_OPT_FILTERED);
#endif
}

void AuxiliaryProcess::increaseFileDescriptorLimit()
{
    struct rlimit currentLimits = { };
    if (int returnCode = getrlimit(RLIMIT_NOFILE, &currentLimits)) {
        RELEASE_LOG_ERROR(Process, "Could not getrlimit(RLIMIT_NOFILE): %d", returnCode);
        return;
    }

    int mib[] = { CTL_KERN, KERN_MAXFILESPERPROC };
    int maxFilesPerProc = 0;
    size_t len = sizeof(maxFilesPerProc);
    if (int returnCode = sysctl(mib, 2, &maxFilesPerProc, &len, NULL, 0)) {
        RELEASE_LOG_ERROR(Process, "Could not get KERN_MAXFILESPERPROC: %d", returnCode);
        return;
    }

    // Set the fd limit to 2560, which is the magic number used by several other frameworks. The
    // default on macOS is 256 (see `launchctl limit`).
    struct rlimit newLimits = currentLimits;
    newLimits.rlim_cur = std::min({ rlim_t { 2560 }, currentLimits.rlim_max, static_cast<rlim_t>(maxFilesPerProc) });

    if (newLimits.rlim_cur < currentLimits.rlim_cur) {
        RELEASE_LOG_ERROR(Process, "Could not increase fd limit: proposed limit %llu is less than current limit %llu", newLimits.rlim_cur, currentLimits.rlim_cur);
        return;
    }

    if (int returnCode = setrlimit(RLIMIT_NOFILE, &newLimits))
        RELEASE_LOG_ERROR(Process, "Could not setrlimit(RLIMIT_NOFILE): %d", returnCode);
}

} // namespace WebKit
