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
#import "ProcessCapabilityGranter.h"

#if ENABLE(PROCESS_CAPABILITIES)

#import "ExtensionKitSPI.h"
#import "GPUProcessProxy.h"
#import "MediaCapability.h"
#import "ProcessCapability.h"
#import "ProcessCapabilityGrant.h"
#import "WebProcessProxy.h"
#import <wtf/CrossThreadCopier.h>
#import <wtf/NativePromise.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/UniqueRef.h>

#define GRANTER_RELEASE_LOG(envID, fmt, ...) RELEASE_LOG(ProcessCapabilities, "%{public}s[envID=%{public}s] " fmt, __FUNCTION__, envID.utf8().data(), ##__VA_ARGS__)
#define GRANTER_RELEASE_LOG_ERROR(envID, fmt, ...) RELEASE_LOG_ERROR(ProcessCapabilities, "%{public}s[envID=%{public}s] " fmt, __FUNCTION__, envID.utf8().data(), ##__VA_ARGS__)

namespace WebKit {

static WorkQueue& granterQueue()
{
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("ProcessCapabilityGranter Queue", WorkQueue::QOS::UserInitiated));
    return queue.get();
}

#if USE(EXTENSIONKIT)
static RetainPtr<_SEGrant> grantCapability(_SECapabilities *capability, _SEExtensionProcess *process)
{
    ASSERT(capability);
    if (!capability || !process)
        return nil;

    NSError *error = nil;
    _SEGrant *grant = [process grantCapabilities:capability error:&error];
    if (!grant)
        RELEASE_LOG_ERROR(ProcessCapabilities, "%{public}s (process=%{public}p) failed with error: %{public}@", __FUNCTION__, process, error);

    return grant;
}
#endif

struct PlatformProcessCapabilityGrants {
    RetainPtr<_SEGrant> gpuProcessGrant;
    RetainPtr<_SEGrant> webProcessGrant;
};

enum class ProcessCapabilityGrantError: uint8_t {
    PlatformError,
};

using ProcessCapabilityGrantsPromise = NativePromise<PlatformProcessCapabilityGrants, ProcessCapabilityGrantError>;

static Ref<ProcessCapabilityGrantsPromise> grantCapabilityInternal(const ProcessCapability& capability, const GPUProcessProxy* gpuProcess, const WebProcessProxy* webProcess)
{
    RetainPtr gpuExtension = gpuProcess ? gpuProcess->extensionProcess() : nil;
    RetainPtr webExtension = webProcess ? webProcess->extensionProcess() : nil;

#if USE(EXTENSIONKIT)
    return invokeAsync(granterQueue(), [
        capability = capability.platformCapability(),
        gpuExtension = WTFMove(gpuExtension),
        webExtension = WTFMove(webExtension)
    ] {
        PlatformProcessCapabilityGrants grants {
            grantCapability(capability.get(), gpuExtension.get()),
            grantCapability(capability.get(), webExtension.get())
        };
        return ProcessCapabilityGrantsPromise::createAndResolve(WTFMove(grants));
    });
#else
    UNUSED_PARAM(capability);
    return ProcessCapabilityGrantsPromise::createAndReject(ProcessCapabilityGrantError::PlatformError);
#endif
}

static bool prepareGrant(const String& environmentIdentifier, AuxiliaryProcessProxy& auxiliaryProcess)
{
    ProcessCapabilityGrant grant { environmentIdentifier };
    auto& existingGrants = auxiliaryProcess.processCapabilityGrants();

    auto result = existingGrants.add(environmentIdentifier, WTFMove(grant));
    if (result.isNewEntry)
        return true;

    auto& existingGrant = result.iterator->value;
    if (existingGrant.isEmpty() || existingGrant.isValid())
        return false;

    existingGrants.set(environmentIdentifier, WTFMove(grant));
    return true;
}

static bool finalizeGrant(const String& environmentIdentifier, AuxiliaryProcessProxy* auxiliaryProcess, ProcessCapabilityGrant&& grant)
{
    if (!auxiliaryProcess) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "auxiliaryProcess is null");
        return false;
    }

    auto& existingGrants = auxiliaryProcess->processCapabilityGrants();

    auto iterator = existingGrants.find(environmentIdentifier);
    if (iterator == existingGrants.end()) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "grant previously revoked");
        return false;
    }

    if (grant.isValid()) {
        iterator->value = WTFMove(grant);
        return true;
    }

    GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "grant invalid");
    existingGrants.remove(iterator);
    return false;
}

UniqueRef<ProcessCapabilityGranter> ProcessCapabilityGranter::create(Client& client)
{
    return makeUniqueRef<ProcessCapabilityGranter>(client);
}

ProcessCapabilityGranter::ProcessCapabilityGranter(Client& client)
    : m_client { client }
{
}

void ProcessCapabilityGranter::grant(const ProcessCapability& capability)
{
    String environmentIdentifier = capability.environmentIdentifier();
    if (environmentIdentifier.isEmpty()) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "environmentIdentifier must not be empty");
        return;
    }

    RefPtr gpuProcess = m_client->gpuProcessForCapabilityGranter(*this);
    bool needsGPUProcessGrant = gpuProcess && prepareGrant(environmentIdentifier, *gpuProcess);

    RefPtr webProcess = m_client->webProcessForCapabilityGranter(*this, environmentIdentifier);
    bool needsWebProcessGrant = webProcess && prepareGrant(environmentIdentifier, *webProcess);

    if (!needsGPUProcessGrant && !needsWebProcessGrant)
        return;

    grantCapabilityInternal(
        capability,
        needsGPUProcessGrant ? gpuProcess.get() : nullptr,
        needsWebProcessGrant ? webProcess.get() : nullptr
    )->whenSettled(RunLoop::main(), [
        this,
        weakThis = WeakPtr { *this },
        environmentIdentifier,
        needsGPUProcessGrant,
        needsWebProcessGrant
    ](auto&& result) {
        ProcessCapabilityGrant gpuProcessGrant { environmentIdentifier };
        gpuProcessGrant.setPlatformGrant(result ? WTFMove(result->gpuProcessGrant) : nil);

        ProcessCapabilityGrant webProcessGrant { environmentIdentifier };
        webProcessGrant.setPlatformGrant(result ? WTFMove(result->webProcessGrant) : nil);

        if (!weakThis) {
            invalidateGrants(Vector<ProcessCapabilityGrant>::from(WTFMove(gpuProcessGrant), WTFMove(webProcessGrant)));
            return;
        }

        Vector<ProcessCapabilityGrant> grantsToInvalidate;
        grantsToInvalidate.reserveInitialCapacity(2);

        if (needsGPUProcessGrant) {
            if (finalizeGrant(environmentIdentifier, m_client->gpuProcessForCapabilityGranter(*this).get(), WTFMove(gpuProcessGrant)))
                GRANTER_RELEASE_LOG(environmentIdentifier, "granted for GPU process");
            else {
                GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "failed to grant for GPU process");
                grantsToInvalidate.append(WTFMove(gpuProcessGrant));
            }
        } else
            ASSERT(gpuProcessGrant.isEmpty());

        if (needsWebProcessGrant) {
            if (finalizeGrant(environmentIdentifier, m_client->webProcessForCapabilityGranter(*this, environmentIdentifier).get(), WTFMove(webProcessGrant)))
                GRANTER_RELEASE_LOG(environmentIdentifier, "granted for WebContent process");
            else {
                GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "failed to grant for WebContent process");
                grantsToInvalidate.append(WTFMove(webProcessGrant));
            }
        } else
            ASSERT(webProcessGrant.isEmpty());

        invalidateGrants(WTFMove(grantsToInvalidate));
    });
}

void ProcessCapabilityGranter::revoke(const ProcessCapability& capability)
{
    Vector<ProcessCapabilityGrant> grants;
    grants.reserveInitialCapacity(2);

    String environmentIdentifier = capability.environmentIdentifier();

    if (RefPtr gpuProcess = m_client->gpuProcessForCapabilityGranter(*this))
        grants.append(gpuProcess->processCapabilityGrants().take(environmentIdentifier));

    if (RefPtr webProcess = m_client->webProcessForCapabilityGranter(*this, environmentIdentifier))
        grants.append(webProcess->processCapabilityGrants().take(environmentIdentifier));

    invalidateGrants(WTFMove(grants));
}

using ProcessCapabilityActivationPromise = NativePromise<void, ProcessCapabilityGrantError>;

void ProcessCapabilityGranter::setMediaCapabilityActive(MediaCapability& capability, bool isActive)
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

    invokeAsync(granterQueue(), [platformCapability = capability.platformCapability(), isActive] {
#if USE(EXTENSIONKIT)
        if ([platformCapability setActive:isActive])
            return ProcessCapabilityActivationPromise::createAndResolve();
#endif
        return ProcessCapabilityActivationPromise::createAndReject(ProcessCapabilityGrantError::PlatformError);
    })->whenSettled(RunLoop::main(), [weakCapability = WeakPtr { capability }, isActive](auto&& result) {
        auto capability = weakCapability.get();
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

void ProcessCapabilityGranter::invalidateGrants(Vector<ProcessCapabilityGrant>&& grants)
{
    granterQueue().dispatch([grants = crossThreadCopy(WTFMove(grants))]() mutable {
        for (auto& grant : grants)
            grant.setPlatformGrant(nil);
    });
}

} // namespace WebKit

#undef GRANTER_RELEASE_LOG
#undef GRANTER_RELEASE_LOG_ERROR

#endif // ENABLE(PROCESS_CAPABILITIES)
