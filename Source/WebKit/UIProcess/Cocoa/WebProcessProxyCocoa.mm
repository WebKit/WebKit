/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#import "HighPerformanceGPUManager.h"
#import "Logging.h"
#import "ObjCObjectGraph.h"
#import "SandboxUtilities.h"
#import "WKBrowsingContextControllerInternal.h"
#import "WKBrowsingContextHandleInternal.h"
#import "WKTypeRefWrapper.h"
#import "WebProcessMessages.h"
#import "WebProcessPool.h"
#import <sys/sysctl.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/spi/darwin/SandboxSPI.h>

namespace WebKit {

const HashSet<String>& WebProcessProxy::platformPathsWithAssumedReadAccess()
{
    static NeverDestroyed<HashSet<String>> platformPathsWithAssumedReadAccess(std::initializer_list<String> {
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
                if (auto* webPageProxy = m_webProcessProxy.webPage(handle.pageID)) {
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
        if (process != this)
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

Vector<String> WebProcessProxy::mediaMIMETypes()
{
    return mediaTypeCache();
}

#if PLATFORM(MAC)
void WebProcessProxy::requestHighPerformanceGPU()
{
    LOG(WebGL, "WebProcessProxy::requestHighPerformanceGPU()");
    HighPerformanceGPUManager::singleton().addProcessRequiringHighPerformance(this);
}

void WebProcessProxy::releaseHighPerformanceGPU()
{
    LOG(WebGL, "WebProcessProxy::releaseHighPerformanceGPU()");
    HighPerformanceGPUManager::singleton().removeProcessRequiringHighPerformance(this);
}
#endif

}
