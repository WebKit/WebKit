/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#import "ICAppBundle.h"

#if ENABLE(INSTALL_COORDINATION_BUNDLES)

#import "AppBundleRequest.h"
#import "LaunchServicesSPI.h"
#import "PushClientConnection.h"
#import "WebPushDaemon.h"
#import <JavaScriptCore/ConsoleTypes.h>
#import <pal/crypto/CryptoDigest.h>
#import <wtf/MainThread.h>
#import <wtf/text/Base64.h>
#import <wtf/text/StringBuilder.h>

NSString *pushBundleAppManagementDomain = @"com.apple.WebKit.PushBundles";

@interface _WKAppInstallCoordinatorObserver : NSObject <IXAppInstallCoordinatorObserver>
- (instancetype)initWithICAppBundle:(WebPushD::ICAppBundle&)icAppBundle;
@property WeakPtr<WebPushD::ICAppBundle> bundle;
@end

@implementation _WKAppInstallCoordinatorObserver

- (instancetype)initWithICAppBundle:(WebPushD::ICAppBundle&)icAppBundle
{
    if (!(self = [super init]))
        return nil;

    _bundle = icAppBundle;

    return self;
}

- (void)coordinatorDidCompleteSuccessfully:(nonnull IXAppInstallCoordinator *)coordinator forApplicationRecord:(nullable LSApplicationRecord *)appRecord
{
    callOnMainRunLoop([self, protectedSelf = RetainPtr<id>(self)] {
        if (_bundle)
            _bundle->bundleCreationSucceeded();
    });
}

- (void)coordinator:(nonnull IXAppInstallCoordinator *)coordinator canceledWithReason:(nonnull NSError *)cancelReason client:(IXClientIdentifier)client
{
    callOnMainRunLoop([self, protectedSelf = RetainPtr<id>(self), protectedCancelReason = RetainPtr<NSError>(cancelReason)] {
        if (_bundle)
            _bundle->bundleCreationFailed(protectedCancelReason.get());
    });
}

@end

using PAL::CryptoDigest;

namespace WebPushD {

static void broadcastDebugMessage(const String& message)
{
    Daemon::singleton().broadcastDebugMessage(makeString("ICAppBundle: ", message));
}

ICAppBundle::ICAppBundle(ClientConnection& clientConnection, const String& originString, PushAppBundleClient& client)
    : PushAppBundle(client)
    , m_originString(originString)
    , m_clientConnection(&clientConnection)
{
}

static String encodeOriginStringForBundleIdentifier(const String& input)
{
    // Bundle identifiers have exactly 64 valid characters.
    // a-z, A-Z, 0-9, '-', and '.'
    // Base64 URL encoding uses '-' and '_' for the final 2 characters
    // For our bundle identifier, we'll Base64 URL encode then replace '_' with '.'
    // We also get rid of the typical '=' padding for base64 encoding.

    auto base64 = base64URLEncodeToVector(input.utf8());
    for (size_t i = 0; i < base64.size(); ++i) {
        if (base64[i] == '_')
            base64[i] = '.';
    }

    size_t numberEqualSigns = 0;
    if (base64.size() >= 1 && base64.last() == '=')
        ++numberEqualSigns;
    if (base64.size() >= 2 && base64[base64.size() - 2] == '=')
        ++numberEqualSigns;

    return String::fromUTF8(base64.data(), base64.size() - numberEqualSigns);
}

static String originStringFromEncodedBundleIdentifier(const String& input)
{
    auto utf8 = input.utf8();
    auto buffer = utf8.mutableData();
    for (size_t i = 0; i < utf8.length(); ++i) {
        if (buffer[i] == '.')
            buffer[i] = '_';
    }

    auto decoded = base64URLDecode(buffer, utf8.length());
    if (!decoded)
        return { };

    return String::fromUTF8(decoded->data(), decoded->size());
}

static String bundlePrefixForHostAppIdentifier(const String& hostAppIdentifier)
{
    auto appDigest = PAL::CryptoDigest::create(CryptoDigest::Algorithm::SHA_256);
    auto utf8Identifier = hostAppIdentifier.utf8();
    appDigest->addBytes(utf8Identifier.data(), utf8Identifier.length());

    return makeString("com.apple.WebKit.PushBundle.", appDigest->toHexString(), ".");
}

static String bundleNameForOriginStringAndHostAppIdentifier(const String& originString, const String& hostAppIdentifier)
{
    // FIXME: Once we can store the origin string within the app bundle, hash the origin string as well.
    // For now we base64 encode it to make a valid bundle identifier that carries the information.
    return makeString(bundlePrefixForHostAppIdentifier(hostAppIdentifier), encodeOriginStringForBundleIdentifier(originString));
}

void ICAppBundle::getOriginsWithRegistrations(ClientConnection& connection, CompletionHandler<void(const Vector<String>&)>&& completionHandler)
{
    Vector<String> results;

    RetainPtr<NSString> bundleIdentifierPrefix = (NSString *)bundlePrefixForHostAppIdentifier(connection.hostAppCodeSigningIdentifier());

    LSEnumerator<LSApplicationRecord *> *enumerator = [LSApplicationRecord enumeratorWithOptions:LSApplicationEnumerationOptionsEnumeratePlaceholders];
    for (LSApplicationRecord *record = [enumerator nextObject]; record; record = [enumerator nextObject]) {
        if (![record.managementDomain isEqualToString:pushBundleAppManagementDomain])
            continue;

        if (![record.bundleIdentifier hasPrefix:bundleIdentifierPrefix.get()])
            continue;

        // FIXME: For now we store the origin string in the bundle identifier base64 encoded
        // Once we store it in the bundle itself, we'll hash the origin string in the bundle identifier and extract it from the bundle

        NSArray *components = [record.bundleIdentifier componentsSeparatedByString:bundleIdentifierPrefix.get()];
        ASSERT(components.count >= 2);

        auto originString = originStringFromEncodedBundleIdentifier(components[1]);
        if (originString.isEmpty())
            continue;

        results.append(originString);
    }
    
    completionHandler(results);
}

const String& ICAppBundle::getBundleIdentifier()
{
    if (m_bundleIdentifier.isEmpty() && m_clientConnection)
        m_bundleIdentifier = bundleNameForOriginStringAndHostAppIdentifier(m_originString, m_clientConnection->hostAppCodeSigningIdentifier());

    return m_bundleIdentifier;
}

void ICAppBundle::checkForExistingBundle()
{
    NSError *error = nil;
    RetainPtr<LSApplicationRecord> appRecord = adoptNS([[LSApplicationRecord alloc] initWithBundleIdentifier:(NSString *)getBundleIdentifier() allowPlaceholder:YES error:&error]);

    if (m_client)
        m_client->didCheckForExistingBundle(*this, appRecord ? PushAppBundleExists::Yes : PushAppBundleExists::No);
}

void ICAppBundle::deleteExistingBundle()
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [IXAppInstallCoordinator uninstallAppWithBundleID:(NSString *)getBundleIdentifier() requestUserConfirmation:NO completion:[this, protectedThis = Ref { *this }] (NSError *error) {
ALLOW_DEPRECATED_DECLARATIONS_END
        callOnMainRunLoop([this, protectedThis = Ref { *this }, error = RetainPtr<NSError>(error)] {
            didDeleteExistingBundleWithError(error.get());
        });
    }];
}

void ICAppBundle::didDeleteExistingBundleWithError(NSError *error)
{
    if (error)
        broadcastDebugMessage(makeString("Error deleting existing bundle: ", String([error description])));

    if (m_client)
        m_client->didDeleteExistingBundleWithError(*this, error);
}

void ICAppBundle::stop()
{
    m_appInstallCoordinator.get().observer = nil;
    m_appInstallCoordinator = nil;
    m_appInstallObserver = nil;
}

void ICAppBundle::createBundle()
{
    RetainPtr<NSString> bundleIdentifier = (NSString *)getBundleIdentifier();

    // Cancel any previous install coordinator that might've been left hanging in a partially finished state
    NSError *error = nil;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [IXAppInstallCoordinator cancelCoordinatorForAppWithBundleID:bundleIdentifier.get() withReason:[NSError errorWithDomain:@"WKErrorDomain" code:1 userInfo:nil] client:IXClientIdentifierAppliedFor error:&error];
ALLOW_DEPRECATED_DECLARATIONS_END

    BOOL created = NO;
    error = nil;
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    m_appInstallCoordinator = [IXRestoringDemotedAppInstallCoordinator coordinatorForAppWithBundleID:bundleIdentifier.get() withClientID:IXClientIdentifierAppliedFor createIfNotExisting:YES created:&created error:&error];
ALLOW_DEPRECATED_DECLARATIONS_END

    if (!m_appInstallCoordinator || error) {
        broadcastDebugMessage(makeString("Unable to create install coordinatior for app bundle identifier ", String(bundleIdentifier.get())));
        bundleCreationFailed(error);
        return;
    }

    if (!created) {
        broadcastDebugMessage(makeString("Existing install coordinatior for app bundle identifier ", String(bundleIdentifier.get()), " was found. This is unexpected."));
        bundleCreationFailed(error);
        return;
    }

    m_appInstallObserver = adoptNS([[_WKAppInstallCoordinatorObserver alloc] initWithICAppBundle:*this]);
    m_appInstallCoordinator.get().observer = m_appInstallObserver.get();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<IXPlaceholder> placeholder = adoptNS([[IXPlaceholder alloc] initAppPlaceholderWithBundleName:(NSString *)m_originString bundleID:bundleIdentifier.get() installType:LSInstallTypeIntentionalDowngrade client:IXClientIdentifierAppliedFor]);
ALLOW_DEPRECATED_DECLARATIONS_END

    RetainPtr<IXPlaceholderAttributes> placeholderAttributes = adoptNS([IXPlaceholderAttributes new]);
    placeholderAttributes.get().launchProhibited = YES;

    [placeholder setPlaceholderAttributes:placeholderAttributes.get() error:&error];
    if (error) {
        broadcastDebugMessage(makeString("Unable to set placeholder attributes for bundle ", String(bundleIdentifier.get()), " Error: ", String([error description])));
        bundleCreationFailed(error);
        return;
    }

    NSDictionary *entitlementsDictionary = @{
        @"com.apple.private.usernotifications.bundle-identifiers": bundleIdentifier.get(),
        @"com.apple.private.usernotifications.notification-simulator": @YES,
        @"com.apple.developer.app-management-domain": pushBundleAppManagementDomain
    };
    RetainPtr<IXPromisedInMemoryDictionary> entitlementsPromise = adoptNS([[IXPromisedInMemoryDictionary alloc] initWithName:[NSString stringWithFormat:@"Entitlements Promise for %@", bundleIdentifier.get()] client:IXClientIdentifierAppliedFor dictionary:entitlementsDictionary]);

    if (!entitlementsPromise) {
        broadcastDebugMessage(makeString("Unable to create placeholder entitlements promise for bundle ", String(bundleIdentifier.get())));
        bundleCreationFailed(error);
        return;
    }

    error = nil;
    if (![placeholder setEntitlementsPromise:entitlementsPromise.get() error:&error] || error) {
        broadcastDebugMessage(makeString("Failed to set placeholder entitlements promise for bundle ", String(bundleIdentifier.get()), " Error: ", String([error description])));
        bundleCreationFailed(error);
        return;
    }

    BOOL result = [placeholder setConfigurationCompleteWithError:&error];
    if (!result || error) {
        broadcastDebugMessage(makeString("Failed to set placeholder entitlements promise complete for bundle ", String(bundleIdentifier.get()), " Error: ", String([error description])));
        bundleCreationFailed(error);
        return;
    }

    [m_appInstallCoordinator setPlaceholderPromise:placeholder.get() error:&error];
    if (error) {
        broadcastDebugMessage(makeString("Failed to set placeholder promise for bundle ", String(bundleIdentifier.get()), " Error: ", String([error description])));
        bundleCreationFailed(error);
        return;
    }

    RetainPtr<IXPromisedOutOfBandTransfer> dataPromise = adoptNS([[IXPromisedOutOfBandTransfer alloc] initWithName:@"Empty user data promise" client:IXClientIdentifierAppliedFor diskSpaceNeeded:0]);
    dataPromise.get().percentComplete = 1.0;
    dataPromise.get().complete = YES;

    [m_appInstallCoordinator setUserDataPromise:dataPromise.get() error:&error];
    if (error) {
        broadcastDebugMessage(makeString("Failed to set user data promise for bundle ", String(bundleIdentifier.get()), " Error: ", String([error description])));
        bundleCreationFailed(error);
        return;
    }
}

void ICAppBundle::bundleCreationSucceeded()
{
    if (m_client)
        m_client->didCreateAppBundle(*this, PushAppBundleCreationResult::Success);

}

void ICAppBundle::bundleCreationFailed(NSError *error)
{
    if (m_client)
        m_client->didCreateAppBundle(*this, PushAppBundleCreationResult::Failure);
}

} // namespace WebPushD

#endif // ENABLE(INSTALL_COORDINATION_BUNDLES)
