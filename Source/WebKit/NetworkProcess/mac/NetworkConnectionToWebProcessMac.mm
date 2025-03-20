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
#import "NetworkConnectionToWebProcess.h"

#import "CoreIPCAuditToken.h"
#import "Logging.h"
#import <pal/spi/cocoa/LaunchServicesSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/cocoa/VectorCocoa.h>

#if PLATFORM(MAC)

#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
SOFT_LINK_FRAMEWORK(CoreServices)
SOFT_LINK_OPTIONAL(CoreServices, _LSApplicationCheckInProxy, Boolean, __cdecl, (LSSessionID, const audit_token_t, CFDictionaryRef applicationInfoRef, void(^block)(CFDictionaryRef, CFErrorRef)))
#endif

namespace WebKit {

void NetworkConnectionToWebProcess::updateActivePages(String&& overrideDisplayName, const Vector<String>& activePagesOrigins, CoreIPCAuditToken&& auditToken)
{
    // Setting and getting the display name of another process requires a private entitlement.
    RELEASE_LOG(Process, "NetworkConnectionToWebProcess::updateActivePages");
#if USE(APPLE_INTERNAL_SDK)
    auto asn = adoptCF(_LSCopyLSASNForAuditToken(kLSDefaultSessionID, auditToken.auditToken()));
    if (!asn) {
#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
        // In this case, the WebContent process has not been checked in with Launch Services yet.
        // We store the display name, and set it after checkin has completed.
        m_pendingDisplayName = WTFMove(overrideDisplayName);
#endif
        return;
    }
    if (!overrideDisplayName)
        _LSSetApplicationInformationItem(kLSDefaultSessionID, asn.get(), CFSTR("LSActivePageUserVisibleOriginsKey"), (__bridge CFArrayRef)createNSArray(activePagesOrigins).get(), nullptr);
    else
        _LSSetApplicationInformationItem(kLSDefaultSessionID, asn.get(), _kLSDisplayNameKey, overrideDisplayName.createCFString().get(), nullptr);
#else
    UNUSED_PARAM(overrideDisplayName);
    UNUSED_PARAM(activePagesOrigins);
    UNUSED_PARAM(auditToken);
#endif
}

void NetworkConnectionToWebProcess::getProcessDisplayName(CoreIPCAuditToken&& auditToken, CompletionHandler<void(const String&)>&& completionHandler)
{
#if USE(APPLE_INTERNAL_SDK)
    auto asn = adoptCF(_LSCopyLSASNForAuditToken(kLSDefaultSessionID, auditToken.auditToken()));
    return completionHandler(adoptCF((CFStringRef)_LSCopyApplicationInformationItem(kLSDefaultSessionID, asn.get(), _kLSDisplayNameKey)).get());
#else
    completionHandler({ });
#endif
}

#if ENABLE(LAUNCHSERVICES_SANDBOX_EXTENSION_BLOCKING)
void NetworkConnectionToWebProcess::checkInWebProcess(const CoreIPCAuditToken& auditToken)
{
    RELEASE_LOG(Process, "NetworkConnectionToWebProcess::checkInWebProcess");

    int dyldPlatform = dyld_get_active_platform();
    RetainPtr dyldPlatformValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &dyldPlatform));
    RetainPtr sdkExecutableVersion = adoptCF(_LSVersionNumberCopyStringRepresentation(_LSVersionNumberGetCurrentSystemVersion()));
    RetainPtr bundleURL = adoptCF(CFURLCreateWithString(kCFAllocatorDefault, CFSTR("file:///System/Library/Frameworks/WebKit.framework/XPCServices/com.apple.WebKit.WebContent.xpc"), nil));
    RetainPtr bundle = adoptCF(CFBundleCreate(kCFAllocatorDefault, bundleURL.get()));
    RetainPtr infoDictionary  = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, CFBundleGetInfoDictionary(bundle.get())));
    RetainPtr executableURL = adoptCF(CFBundleCopyExecutableURL(bundle.get()));
    RetainPtr executablePath = adoptCF(CFURLCopyFileSystemPath(executableURL.get(), kCFURLPOSIXPathStyle));

    if (!infoDictionary) {
        RELEASE_LOG_ERROR(Process, "Failed to create dictionary for Launch Services checkin");
        return;
    }

    CFDictionarySetValue(infoDictionary.get(), _kLSDisplayNameKey, CFSTR("Web Content Process"));
#if CPU(ARM64E)
    CFDictionarySetValue(infoDictionary.get(), _kLSArchitectureKey, _kLSArchitectureARM64Value);
#else
    CFDictionarySetValue(infoDictionary.get(), _kLSArchitectureKey, _kLSArchitecturex86_64Value);
#endif
    CFDictionarySetValue(infoDictionary.get(), _kLSExecutablePathKey, executablePath.get());
    CFDictionarySetValue(infoDictionary.get(), _kLSDYLDPlatformKey, dyldPlatformValue.get());
    CFDictionarySetValue(infoDictionary.get(), _kLSExecutableSDKVersionKey, sdkExecutableVersion.get());
    CFDictionarySetValue(infoDictionary.get(), _kLSParentASNKey, _LSGetCurrentApplicationASN());

    if (!_LSApplicationCheckInProxyPtr()) {
        RELEASE_LOG_ERROR(Process, "Unable to soft link the function for Launch Services checkin");
        return;
    }

    RELEASE_LOG(Process, "Launch Services checkin starting, infoDictionary = %{public}@", (__bridge NSDictionary *)infoDictionary.get());

    auto block = makeBlockPtr([weakThis = WeakPtr { *this }, auditToken](CFDictionaryRef result, CFErrorRef error) mutable {
        NSDictionary *dictionary = (__bridge NSDictionary *)result;
        RELEASE_LOG(Process, "Launch Services checkin completed, result = %{public}@, error = %{public}@", dictionary, (__bridge NSError *)error);

        callOnMainRunLoop([weakThis = WTFMove(weakThis), auditToken = WTFMove(auditToken)] mutable {
            RefPtr protectedThis = weakThis.get();
            if (!protectedThis)
                return;
            if (protectedThis->m_pendingDisplayName.isNull())
                return;
            protectedThis->updateActivePages(std::exchange(protectedThis->m_pendingDisplayName, String()), { }, WTFMove(auditToken));
        });
    });

    Boolean ok = _LSApplicationCheckInProxyPtr()(kLSDefaultSessionID, auditToken.auditToken(), infoDictionary.get(), block.get());

    if (!ok)
        RELEASE_LOG(Process, "Launch Services checkin did not succeed");
}
#endif

} // namespace WebKit

#endif // PLATFORM(MAC)
