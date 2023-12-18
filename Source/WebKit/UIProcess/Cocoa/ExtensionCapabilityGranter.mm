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

#import "ExtensionCapability.h"
#import "ExtensionCapabilityGrant.h"
#import "ExtensionKitSPI.h"
#import "GPUProcessProxy.h"
#import "MediaCapability.h"
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
    static NeverDestroyed<Ref<WorkQueue>> queue(WorkQueue::create("ExtensionCapabilityGranter Queue", WorkQueue::QOS::UserInitiated));
    return queue.get();
}

#if USE(EXTENSIONKIT)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
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
ALLOW_DEPRECATED_DECLARATIONS_END
#endif

struct PlatformExtensionCapabilityGrants {
    RetainPtr<_SEGrant> gpuProcessGrant;
    RetainPtr<_SEGrant> webProcessGrant;
};

enum class ExtensionCapabilityGrantError: uint8_t {
    PlatformError,
};

using ExtensionCapabilityGrantsPromise = NativePromise<PlatformExtensionCapabilityGrants, ExtensionCapabilityGrantError>;

static Ref<ExtensionCapabilityGrantsPromise> grantCapabilityInternal(const ExtensionCapability& capability, const GPUProcessProxy* gpuProcess, const WebProcessProxy* webProcess)
{
    RetainPtr gpuExtension = gpuProcess ? gpuProcess->extensionProcess() : nil;
    RetainPtr webExtension = webProcess ? webProcess->extensionProcess() : nil;

#if USE(EXTENSIONKIT)
    return invokeAsync(granterQueue(), [
        capability = capability.platformCapability(),
        gpuExtension = WTFMove(gpuExtension),
        webExtension = WTFMove(webExtension)
    ] {
        PlatformExtensionCapabilityGrants grants {
            grantCapability(capability.get(), gpuExtension.get()),
            grantCapability(capability.get(), webExtension.get())
        };
        return ExtensionCapabilityGrantsPromise::createAndResolve(WTFMove(grants));
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

    auto result = existingGrants.add(environmentIdentifier, WTFMove(grant));
    if (result.isNewEntry)
        return true;

    auto& existingGrant = result.iterator->value;
    if (existingGrant.isEmpty() || existingGrant.isValid())
        return false;

    existingGrants.set(environmentIdentifier, WTFMove(grant));
    return true;
}

static bool finalizeGrant(const String& environmentIdentifier, AuxiliaryProcessProxy* auxiliaryProcess, ExtensionCapabilityGrant&& grant)
{
    if (!auxiliaryProcess) {
        GRANTER_RELEASE_LOG_ERROR(environmentIdentifier, "auxiliaryProcess is null");
        return false;
    }

    auto& existingGrants = auxiliaryProcess->extensionCapabilityGrants();

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

UniqueRef<ExtensionCapabilityGranter> ExtensionCapabilityGranter::create(Client& client)
{
    return makeUniqueRef<ExtensionCapabilityGranter>(client);
}

ExtensionCapabilityGranter::ExtensionCapabilityGranter(Client& client)
    : m_client { client }
{
}

void ExtensionCapabilityGranter::grant(const ExtensionCapability& capability)
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
        ExtensionCapabilityGrant gpuProcessGrant { environmentIdentifier };
        gpuProcessGrant.setPlatformGrant(result ? WTFMove(result->gpuProcessGrant) : nil);

        ExtensionCapabilityGrant webProcessGrant { environmentIdentifier };
        webProcessGrant.setPlatformGrant(result ? WTFMove(result->webProcessGrant) : nil);

        if (!weakThis) {
            invalidateGrants(Vector<ExtensionCapabilityGrant>::from(WTFMove(gpuProcessGrant), WTFMove(webProcessGrant)));
            return;
        }

        Vector<ExtensionCapabilityGrant> grantsToInvalidate;
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

void ExtensionCapabilityGranter::revoke(const ExtensionCapability& capability)
{
    Vector<ExtensionCapabilityGrant> grants;
    grants.reserveInitialCapacity(2);

    String environmentIdentifier = capability.environmentIdentifier();

    if (RefPtr gpuProcess = m_client->gpuProcessForCapabilityGranter(*this))
        grants.append(gpuProcess->extensionCapabilityGrants().take(environmentIdentifier));

    if (RefPtr webProcess = m_client->webProcessForCapabilityGranter(*this, environmentIdentifier))
        grants.append(webProcess->extensionCapabilityGrants().take(environmentIdentifier));

    invalidateGrants(WTFMove(grants));
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

    invokeAsync(granterQueue(), [platformCapability = capability.platformCapability(), isActive] {
#if USE(EXTENSIONKIT)
        if ([platformCapability setActive:isActive])
            return ExtensionCapabilityActivationPromise::createAndResolve();
#endif
        return ExtensionCapabilityActivationPromise::createAndReject(ExtensionCapabilityGrantError::PlatformError);
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

void ExtensionCapabilityGranter::invalidateGrants(Vector<ExtensionCapabilityGrant>&& grants)
{
    granterQueue().dispatch([grants = crossThreadCopy(WTFMove(grants))]() mutable {
        for (auto& grant : grants)
            grant.setPlatformGrant(nil);
    });
}

} // namespace WebKit

#undef GRANTER_RELEASE_LOG
#undef GRANTER_RELEASE_LOG_ERROR

#endif // ENABLE(EXTENSION_CAPABILITIES)
