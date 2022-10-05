/*
 * Copyright (C) 2014-2022 Apple Inc. All rights reserved.
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
#import "CodeSigning.h"
#import "DefaultWebBrowserChecks.h"
#import "HighPerformanceGPUManager.h"
#import "Logging.h"
#import "ObjCObjectGraph.h"
#import "SandboxUtilities.h"
#import "SharedBufferReference.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKTypeRefWrapper.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <WebCore/RuntimeApplicationChecks.h>
#import <sys/sysctl.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/Scope.h>
#import <wtf/cocoa/Entitlements.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/cocoa/VectorCocoa.h>
#import <wtf/spi/darwin/SandboxSPI.h>

#if PLATFORM(IOS_FAMILY)
#import "AccessibilitySupportSPI.h"
#endif

#if ENABLE(REMOTE_INSPECTOR)
#import "WebInspectorUtilities.h"
#import <JavaScriptCore/RemoteInspectorConstants.h>
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#import <WebCore/CaptionUserPreferencesMediaAF.h>
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
#import "WindowServerConnection.h"
#endif

#if PLATFORM(MAC)
#import "TCCSoftLink.h"
#endif

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
        if (process.ptr() != this)
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
    return CFPreferencesGetAppIntegerValue(CFSTR("ShowDevelopMenu"), bundleIdentifierForSandboxBroker(), nullptr);
#endif
}

void WebProcessProxy::enableRemoteInspectorIfNeeded()
{
    if (!shouldEnableRemoteInspector())
        return;
    send(Messages::WebProcess::EnableRemoteWebInspector(), 0);
}
#endif

#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
void WebProcessProxy::setCaptionDisplayMode(WebCore::CaptionUserPreferences::CaptionDisplayMode displayMode)
{
    WebCore::CaptionUserPreferencesMediaAF::platformSetCaptionDisplayMode(displayMode);
}

void WebProcessProxy::setCaptionLanguage(const String& language)
{
    WebCore::CaptionUserPreferencesMediaAF::platformSetPreferredLanguage(language);
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

    Vector<SandboxExtension::Handle> handleArray;
#if PLATFORM(IOS_FAMILY)
    handleArray = SandboxExtension::createHandlesForMachLookup({ "com.apple.iphone.axserver-systemwide"_s, "com.apple.frontboard.systemappservices"_s }, auditToken(), SandboxExtension::MachBootstrapOptions::EnableMachBootstrap);
    ASSERT(handleArray.size() == 2);
#endif

    send(Messages::WebProcess::UnblockServicesRequiredByAccessibility(handleArray), 0);
    m_hasSentMessageToUnblockAccessibilityServer = true;
}

#if PLATFORM(MAC)
void WebProcessProxy::isAXAuthenticated(audit_token_t auditToken, CompletionHandler<void(bool)>&& completionHandler)
{
    auto authenticated = TCCAccessCheckAuditToken(get_TCC_kTCCServiceAccessibility(), auditToken, nullptr);
    completionHandler(authenticated);
}
#endif

#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
void WebProcessProxy::hardwareConsoleStateChanged()
{
    m_isConnectedToHardwareConsole = WindowServerConnection::singleton().hardwareConsoleState() == WindowServerConnection::HardwareConsoleState::Connected;
    for (const auto& page : m_pageMap.values())
        page->activityStateDidChange(WebCore::ActivityState::IsConnectedToHardwareConsole);
}
#endif

#if HAVE(AUDIO_COMPONENT_SERVER_REGISTRATIONS)
void WebProcessProxy::sendAudioComponentRegistrations()
{
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), [weakThis = WeakPtr { *this }] () mutable {

        auto registrations = fetchAudioComponentServerRegistrations();
        if (!registrations)
            return;
        
        RunLoop::main().dispatch([weakThis = WTFMove(weakThis), registrations = WTFMove(registrations)] () mutable {
            if (!weakThis)
                return;

            weakThis->send(Messages::WebProcess::ConsumeAudioComponentRegistrations(IPC::SharedBufferReference(WTFMove(registrations))), 0);
        });
    });
}
#endif

bool WebProcessProxy::messageSourceIsValidWebContentProcess()
{
    if (!hasConnection()) {
        ASSERT_NOT_REACHED();
        return false;
    }

#if USE(APPLE_INTERNAL_SDK)
#if PLATFORM(IOS)
    // FIXME(rdar://80908833): On iOS, we can only perform the below checks for platform binaries until rdar://80908833 is fixed.
    if (!currentProcessIsPlatformBinary())
        return true;
#endif

    // WebKitTestRunner does not pass the isPlatformBinary check, we should return early in this case.
    if (isRunningTest(WebCore::applicationBundleIdentifier()))
        return true;

    // Confirm that the connection is from a WebContent process:
    auto [signingIdentifier, isPlatformBinary] = codeSigningIdentifierAndPlatformBinaryStatus(connection()->xpcConnection());

    if (!isPlatformBinary || !signingIdentifier.startsWith("com.apple.WebKit.WebContent"_s)) {
        RELEASE_LOG_ERROR(Process, "Process is not an entitled WebContent process.");
        return false;
    }
#endif

    return true;
}

std::optional<audit_token_t> WebProcessProxy::auditToken() const
{
    if (!hasConnection())
        return std::nullopt;
    
    return connection()->getAuditToken();
}

SandboxExtension::Handle WebProcessProxy::fontdMachExtensionHandle(SandboxExtension::MachBootstrapOptions machBootstrapOptions) const
{
    return SandboxExtension::createHandleForMachLookup("com.apple.fonts"_s, auditToken(), machBootstrapOptions).value_or(SandboxExtension::Handle { });
}


}

