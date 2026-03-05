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
#import "ExtensionCapabilityGranter.h"

#if ENABLE(EXTENSION_CAPABILITIES)

#import "BrowsingContextGroup.h"
#import "ExtensionCapability.h"
#import "ExtensionCapabilityGrant.h"
#import "ExtensionKitSPI.h"
#import "GPUProcessProxy.h"
#import "MediaCapability.h"
#import "RemotePageProxy.h"
#import "WebFrameProxy.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "WebProcessProxy.h"
#import <BrowserEngineKit/BrowserEngineKit.h>
#import <wtf/CrossThreadCopier.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMallocInlines.h>

#define GRANTER_RELEASE_LOG(envID, fmt, ...) RELEASE_LOG(ProcessCapabilities, "%{public}s[envID=%{public}s] " fmt, __FUNCTION__, envID.utf8().data(), ##__VA_ARGS__)
#define GRANTER_RELEASE_LOG_ERROR(envID, fmt, ...) RELEASE_LOG_ERROR(ProcessCapabilities, "%{public}s[envID=%{public}s] " fmt, __FUNCTION__, envID.utf8().data(), ##__VA_ARGS__)

namespace WebKit {

static WorkQueue& granterQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("ExtensionCapabilityGranter Queue"_s, WorkQueue::QOS::UserInitiated));
    return queue.get();
}

#if USE(EXTENSIONKIT)
static PlatformGrant grantCapability(const PlatformCapability& capability, const ExtensionProcess& process)
{
    if (!ExtensionCapability::platformCapabilityIsValid(capability))
        return { };

    return process.grantCapability(capability);
}
#endif

struct CapabilityGrantForAuxiliaryProcess {
    Ref<AuxiliaryProcessProxy> auxiliaryProcess;
    ExtensionProcess extensionProcess;
    ExtensionCapabilityGrant grant;

    CapabilityGrantForAuxiliaryProcess isolatedCopy() &&
    {
        return {
            crossThreadCopy(WTF::move(auxiliaryProcess)),
            crossThreadCopy(WTF::move(extensionProcess)),
            crossThreadCopy(WTF::move(grant))
        };
    }
};

using PlatformExtensionCapabilityGrants = Vector<CapabilityGrantForAuxiliaryProcess>;

enum class ExtensionCapabilityGrantError: uint8_t {
    PlatformError,
};

using ExtensionCapabilityGrantsPromise = NativePromise<PlatformExtensionCapabilityGrants, ExtensionCapabilityGrantError>;

static Ref<ExtensionCapabilityGrantsPromise> grantCapabilityInternal(const ExtensionCapability& capability, const Vector<Ref<AuxiliaryProcessProxy>>& processes)
{
    PlatformExtensionCapabilityGrants capabilityGrants = WTF::compactMap(processes, [](auto& process) -> std::optional<CapabilityGrantForAuxiliaryProcess> {
        if (std::optional extensionProcess = process->extensionProcess())
            return CapabilityGrantForAuxiliaryProcess { process, *extensionProcess, { } };
        return std::nullopt;
    });

#if USE(EXTENSIONKIT)
    return invokeAsync(protect(granterQueue()), [capability = capability.platformCapability(), capabilityGrants = crossThreadCopy(WTF::move(capabilityGrants))] mutable {
        for (auto& capabilityGrant : capabilityGrants)
            capabilityGrant.grant.setPlatformGrant(grantCapability(capability, capabilityGrant.extensionProcess));
        return ExtensionCapabilityGrantsPromise::createAndResolve(WTF::move(capabilityGrants));
    });
#else
    UNUSED_PARAM(capability);
    return ExtensionCapabilityGrantsPromise::createAndReject(ExtensionCapabilityGrantError::PlatformError);
#endif
}

static bool prepareGrant(const String& environmentIdentifier, AuxiliaryProcessProxy& auxiliaryProcess)
{
    ExtensionCapabilityGrant grant { environmentIdentifier };
    auto& existingGrants = auxiliaryProcess.extensionCapabilityGrants();

    auto result = existingGrants.add(environmentIdentifier, WTF::move(grant));
    if (result.isNewEntry)
        return true;

    auto& existingGrant = result.iterator->value;
    if (existingGrant.isEmpty() || existingGrant.isValid())
        return false;

    existingGrants.set(environmentIdentifier, WTF::move(grant));
    return true;
}

static Vector<Ref<AuxiliaryProcessProxy>> auxiliaryProcessesForPage(WebPageProxy& page)
{
    Vector<Ref<AuxiliaryProcessProxy>> auxiliaryProcesses;

    // FIXME: If we've made it this far we assume at least one process is playing media, but we
    // don't have the metadata to know that the main frame process is *not* one of those processes.
    // We should fix that, perhaps by storing mediaState on a per-frame basis.
    auxiliaryProcesses.append(protect(page.legacyMainFrameProcess()));

    protect(page.browsingContextGroup())->forEachRemotePage(page, [&](auto& remotePage) {
        if (WebCore::MediaProducer::needsMediaCapability(remotePage.mediaState()))
            auxiliaryProcesses.append(remotePage.process());
    });

    if (RefPtr mainFrame = page.mainFrame()) {
        if (RefPtr gpuProcess = mainFrame->process().processPool().gpuProcess())
            auxiliaryProcesses.append(*gpuProcess);
    }

    return auxiliaryProcesses;
}

static Vector<Ref<AuxiliaryProcessProxy>> processesNeedingGrant(const String& environmentIdentifier, WebPageProxy& page)
{
    Vector<Ref<AuxiliaryProcessProxy>> processesNeedingGrant;

    for (auto& auxiliaryProcess : auxiliaryProcessesForPage(page)) {
        if (prepareGrant(environmentIdentifier, auxiliaryProcess.get()))
            processesNeedingGrant.append(WTF::move(auxiliaryProcess));
    }

    return processesNeedingGrant;
}

static bool finalizeGrant(const String& environmentIdentifier, AuxiliaryProcessProxy& auxiliaryProcess, ExtensionCapabilityGrant&& grant)
{
    auto& existingGrants = auxiliaryProcess.extensionCapabilityGrants();

    auto iterator = existingGrants.find(environmentIdentifier);
    if (iterator == existingGrants.end()) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "grant previously revoked");
        return false;
    }

    if (grant.isValid()) {
        auto& existingGrant = iterator->value;
        ASSERT(!existingGrant.isValid());
        if (existingGrant.isValid()) {
            GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "grant not expected to be valid");
            ExtensionCapabilityGranter::invalidateGrants(Vector<ExtensionCapabilityGrant>::from(WTF::move(existingGrant)));
        }
        existingGrant = WTF::move(grant);
        return true;
    }

    GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "grant invalid");
    existingGrants.remove(iterator);
    return false;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ExtensionCapabilityGranter);

RefPtr<ExtensionCapabilityGranter> ExtensionCapabilityGranter::create()
{
    return adoptRef(new ExtensionCapabilityGranter());
}

void ExtensionCapabilityGranter::grant(const ExtensionCapability& capability, WebPageProxy& page)
{
    String environmentIdentifier = capability.environmentIdentifier();
    if (environmentIdentifier.isEmpty()) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "environmentIdentifier must not be empty");
        return;
    }

    Vector processes = processesNeedingGrant(environmentIdentifier, page);
    if (processes.isEmpty())
        return;

    grantCapabilityInternal(capability, processes)->whenSettled(RunLoop::mainSingleton(), [environmentIdentifier, processes = WTF::move(processes)](auto&& result) {
        if (!result)
            return;

        Vector<ExtensionCapabilityGrant> grantsToInvalidate;

        for (auto& capabilityGrant : *result) {
            if (finalizeGrant(environmentIdentifier, capabilityGrant.auxiliaryProcess.get(), WTF::move(capabilityGrant.grant)))
                GRANTER_RELEASE_LOG(environmentIdentifier, "granted for auxiliary process %d", capabilityGrant.auxiliaryProcess->processID());
            else {
                GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "failed to grant for auxiliary process %d", capabilityGrant.auxiliaryProcess->processID());
                grantsToInvalidate.append(WTF::move(capabilityGrant.grant));
            }
        }

        invalidateGrants(WTF::move(grantsToInvalidate));
    });
}

void ExtensionCapabilityGranter::revoke(const ExtensionCapability& capability, WebPageProxy& page)
{
    String environmentIdentifier = capability.environmentIdentifier();
    Vector<ExtensionCapabilityGrant> grants;

    grants.append(page.legacyMainFrameProcess().extensionCapabilityGrants().take(environmentIdentifier));

    protect(page.browsingContextGroup())->forEachRemotePage(page, [&](auto& remotePage) {
        grants.append(remotePage.process().extensionCapabilityGrants().take(environmentIdentifier));
    });

    if (RefPtr mainFrame = page.mainFrame()) {
        if (RefPtr gpuProcess = mainFrame->process().processPool().gpuProcess())
            grants.append(gpuProcess->extensionCapabilityGrants().take(environmentIdentifier));
    }

    invalidateGrants(WTF::move(grants));
}

using ExtensionCapabilityActivationPromise = NativePromise<void, ExtensionCapabilityGrantError>;

void ExtensionCapabilityGranter::setMediaCapabilityActive(MediaCapability& capability, bool isActive)
{
    switch (capability.state()) {
    case MediaCapability::State::Inactive:
    case MediaCapability::State::Deactivating:
        if (!isActive)
            return;
        capability.setState(MediaCapability::State::Activating);
        break;
    case MediaCapability::State::Activating:
    case MediaCapability::State::Active:
        if (isActive)
            return;
        capability.setState(MediaCapability::State::Deactivating);
        break;
    }

    GRANTER_RELEASE_LOG(capability.environmentIdentifier(), "%{public}s", isActive ? "activating" : "deactivating");

    invokeAsync(protect(granterQueue()), [platformCapability = capability.platformCapability(), platformMediaEnvironment = RetainPtr { capability.platformMediaEnvironment() }, isActive] {
#if USE(EXTENSIONKIT)
        NSError *error = nil;
        if (isActive)
            [platformMediaEnvironment activateWithError:&error];
        else
            [platformMediaEnvironment suspendWithError:&error];
        if (error)
            RELEASE_LOG_ERROR(ProcessCapabilities, "%{public}s failed with error: %{public}@", __FUNCTION__, error);
        else
            return ExtensionCapabilityActivationPromise::createAndResolve();
#endif
        return ExtensionCapabilityActivationPromise::createAndReject(ExtensionCapabilityGrantError::PlatformError);
    })->whenSettled(RunLoop::mainSingleton(), [weakCapability = WeakPtr { capability }, isActive](auto&& result) {
        RefPtr capability = weakCapability.get();
        if (!capability)
            return;

        if (!result) {
            GRANTER_RELEASE_LOG_ERROR(capability->environmentIdentifier(), "failed to %{public}s", isActive ? "activate" : "deactivate");
            capability->setState(MediaCapability::State::Inactive);
            return;
        }

        switch (capability->state()) {
        case MediaCapability::State::Deactivating:
            if (!isActive)
                capability->setState(MediaCapability::State::Inactive);
            break;
        case MediaCapability::State::Activating:
            if (isActive)
                capability->setState(MediaCapability::State::Active);
            break;
        case MediaCapability::State::Inactive:
        case MediaCapability::State::Active:
            ASSERT_NOT_REACHED();
            return;
        }

        GRANTER_RELEASE_LOG(capability->environmentIdentifier(), "%{public}s", isActive ? "activated" : "deactivated");
    });
}

void ExtensionCapabilityGranter::invalidateGrants(Vector<ExtensionCapabilityGrant>&& grants)
{
    protect(granterQueue())->dispatch([grants = crossThreadCopy(WTF::move(grants))]() mutable {
        for (auto& grant : grants)
            grant.setPlatformGrant({ });
    });
}

} // namespace WebKit

#undef GRANTER_RELEASE_LOG
#undef GRANTER_RELEASE_LOG_ERROR

#endif // ENABLE(EXTENSION_CAPABILITIES)
