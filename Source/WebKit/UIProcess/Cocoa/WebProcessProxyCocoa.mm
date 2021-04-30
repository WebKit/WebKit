/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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
#import "WebProcessProxy.h"

#import "AccessibilitySupportSPI.h"
#import "HighPerformanceGPUManager.h"
#import "Logging.h"
#import "ObjCObjectGraph.h"
#import "SandboxUtilities.h"
#import "SharedBufferDataReference.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKTypeRefWrapper.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <sys/sysctl.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if PLATFORM(IOS_FAMILY)
#import "AccessibilitySupportSPI.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#import <JavaScriptCore/RemoteInspectorConstants.h>
#endif

#if PLATFORM(MAC)
#import <wtf/SoftLinking.h>

SOFT_LINK_PRIVATE_FRAMEWORK(TCC)
SOFT_LINK(TCC, TCCAccessCheckAuditToken, Boolean, (CFStringRef service, audit_token_t auditToken, CFDictionaryRef options), (service, auditToken, options))
SOFT_LINK_CONSTANT(TCC, kTCCServiceAccessibility, CFStringRef)
#endif

#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebKit {

static const Seconds unexpectedActivityDuration = 10_s;

const MemoryCompactLookupOnlyRobinHoodHashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<String>> platformPathsWithAssumedReadAccess(std::initializer_list<String> {
        [NSBundle bundleWithIdentifier:@"com.apple.WebCore"].resourcePath.stringByStandardizingPath,
        [NSBundle bundleWithIdentifier:@"com.apple.WebKit"].resourcePath.stringByStandardizingPath
    });

    return platformPathsWithAssumedReadAccess;
}

RefPtr<ObjCObjectGraph> WebProcessProxy::transformHandlesToObjects(ObjCObjectGraph& objectGraph)
{
    struct Transformer final : ObjCObjectGraph::Transformer {
        Transformer(WebProcessProxy& webProcessProxy)
            : m_webProcessProxy(webProcessProxy)
        {
        }

        bool shouldTransformObject(id object) const override
        {
            if (dynamic_objc_cast<WKBrowsingContextHandle>(object))
                return true;

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
            if (auto* handle = dynamic_objc_cast<WKBrowsingContextHandle>(object)) {
                if (auto* webPageProxy = m_webProcessProxy.webPage(handle.pageProxyID)) {
                    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
                    return [WKBrowsingContextController _browsingContextControllerForPageRef:toAPI(webPageProxy)];
                    ALLOW_DEPRECATED_DECLARATIONS_END
                }

                return [NSNull null];
            }

            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(m_webProcessProxy.transformHandlesToObjects(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
            return object;
        }

        WebProcessProxy& m_webProcessProxy;
    };

    return ObjCObjectGraph::create(ObjCObjectGraph::transform(objectGraph.rootObject(), Transformer(*this)).get());
}

RefPtr<ObjCObjectGraph> WebProcessProxy::transformObjectsToHandles(ObjCObjectGraph& objectGraph)
{
    struct Transformer final : ObjCObjectGraph::Transformer {
        bool shouldTransformObject(id object) const override
        {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (dynamic_objc_cast<WKBrowsingContextController>(object))
                return true;
            if (dynamic_objc_cast<WKTypeRefWrapper>(object))
                return true;
            ALLOW_DEPRECATED_DECLARATIONS_END
            return false;
        }

        RetainPtr<id> transformObject(id object) const override
        {
            ALLOW_DEPRECATED_DECLARATIONS_BEGIN
            if (auto* controller = dynamic_objc_cast<WKBrowsingContextController>(object))
                return controller.handle;
            if (auto* wrapper = dynamic_objc_cast<WKTypeRefWrapper>(object))
                return adoptNS([[WKTypeRefWrapper alloc] initWithObject:toAPI(transformObjectsToHandles(toImpl(wrapper.object)).get())]);
            ALLOW_DEPRECATED_DECLARATIONS_END
            return object;
        }
    };

    return ObjCObjectGraph::create(ObjCObjectGraph::transform(objectGraph.rootObject(), Transformer()).get());
}

bool WebProcessProxy::platformIsBeingDebugged() const
{
    // If the UI process is sandboxed and lacks 'process-info-pidinfo', it cannot find out whether other processes are being debugged.
    if (currentProcessIsSandboxed() && !!sandbox_check(getpid(), "process-info-pidinfo", SANDBOX_CHECK_NO_REPORT))
        return false;

    struct kinfo_proc info;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, processIdentifier() };
    size_t size = sizeof(info);
    if (sysctl(mib, WTF_ARRAY_LENGTH(mib), &info, &size, nullptr, 0) == -1)
        return false;

    return info.kp_proc.p_flag & P_TRACED;
}

static Vector<String>& mediaTypeCache()
{
    ASSERT(RunLoop::isMain());
    static NeverDestroyed<Vector<String>> typeCache;
    return typeCache;
}

void WebProcessProxy::cacheMediaMIMETypes(const Vector<String>& types)
{
    if (!mediaTypeCache().isEmpty())
        return;

    mediaTypeCache() = types;
    for (auto& process : processPool().processes()) {
        if (process != *this)
            cacheMediaMIMETypesInternal(types);
    }
}

void WebProcessProxy::cacheMediaMIMETypesInternal(const Vector<String>& types)
{
    if (!mediaTypeCache().isEmpty())
        return;

    mediaTypeCache() = types;
    send(Messages::WebProcess::SetMediaMIMETypes(types), 0);
}

Vector<String> WebProcessProxy::mediaMIMETypes() const
{
    return mediaTypeCache();
}

#if PLATFORM(MAC)
void WebProcessProxy::requestHighPerformanceGPU()
{
    LOG(WebGL, "WebProcessProxy::requestHighPerformanceGPU()");
    HighPerformanceGPUManager::singleton().addProcessRequiringHighPerformance(*this);
}

void WebProcessProxy::releaseHighPerformanceGPU()
{
    LOG(WebGL, "WebProcessProxy::releaseHighPerformanceGPU()");
    HighPerformanceGPUManager::singleton().removeProcessRequiringHighPerformance(*this);
}
#endif

#if ENABLE(REMOTE_INSPECTOR)
bool WebProcessProxy::shouldEnableRemoteInspector()
{
#if PLATFORM(IOS_FAMILY)
    return CFPreferencesGetAppIntegerValue(WIRRemoteInspectorEnabledKey, WIRRemoteInspectorDomainName, nullptr);
#else
    return CFPreferencesGetAppIntegerValue(CFSTR("ShowDevelopMenu"), CFSTR("com.apple.Safari.SandboxBroker"), nullptr);
#endif
}

void WebProcessProxy::enableRemoteInspectorIfNeeded()
{
    if (!shouldEnableRemoteInspector())
        return;
    send(Messages::WebProcess::EnableRemoteWebInspector(), 0);
}
#endif

void WebProcessProxy::unblockAccessibilityServerIfNeeded()
{
    if (m_hasSentMessageToUnblockAccessibilityServer)
        return;
#if PLATFORM(IOS_FAMILY)
    if (!_AXSApplicationAccessibilityEnabled())
        return;
#endif
    if (!processIdentifier())
        return;
    if (!canSendMessage())
        return;

    SandboxExtension::HandleArray handleArray;
#if PLATFORM(IOS_FAMILY)
    handleArray = SandboxExtension::createHandlesForMachLookup({ "com.apple.iphone.axserver-systemwide"_s, "com.apple.frontboard.systemappservices"_s }, connection() ? connection()->getAuditToken() : WTF::nullopt);
    ASSERT(handleArray.size() == 2);
#endif

    send(Messages::WebProcess::UnblockServicesRequiredByAccessibility(handleArray), 0);
    m_hasSentMessageToUnblockAccessibilityServer = true;
}

#if ENABLE(CFPREFS_DIRECT_MODE)
void WebProcessProxy::unblockPreferenceServiceIfNeeded()
{
    if (m_hasSentMessageToUnblockPreferenceService)
        return;
    if (!processIdentifier())
        return;
    if (!canSendMessage())
        return;

    auto handleArray = SandboxExtension::createHandlesForMachLookup({ "com.apple.cfprefsd.agent"_s, "com.apple.cfprefsd.daemon"_s }, connection() ? connection()->getAuditToken() : WTF::nullopt);
    ASSERT(handleArray.size() == 2);
    
    send(Messages::WebProcess::UnblockPreferenceService(WTFMove(handleArray)), 0);
    m_hasSentMessageToUnblockPreferenceService = true;
}
#endif

Vector<String> WebProcessProxy::platformOverrideLanguages() const
{
    static const NeverDestroyed<Vector<String>> overrideLanguages = makeVector<String>([[NSUserDefaults standardUserDefaults] valueForKey:@"AppleLanguages"]);
    return overrideLanguages;
}

#if PLATFORM(MAC)
void WebProcessProxy::isAXAuthenticated(audit_token_t auditToken, CompletionHandler<void(bool)>&& completionHandler)
{
    auto authenticated = TCCAccessCheckAuditToken(getkTCCServiceAccessibility(), auditToken, nullptr);
    completionHandler(authenticated);
}
#endif

void WebProcessProxy::sendAudioComponentRegistrations()
{
    using namespace PAL;

    if (!isAudioToolboxCoreFrameworkAvailable() || !canLoad_AudioToolboxCore_AudioComponentFetchServerRegistrations())
        return;

    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [protectedThis = makeRef(*this)] () mutable {
        CFDataRef registrations { nullptr };
        if (noErr != AudioComponentFetchServerRegistrations(&registrations) || !registrations)
            return;

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), registrations = adoptCF(registrations)] () mutable {
            auto registrationData = SharedBuffer::create(registrations.get());
            protectedThis->send(Messages::WebProcess::ConsumeAudioComponentRegistrations({ registrationData }), 0);
        });
    });
}

}
