/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "LayerHostingVisibilityPropagator.h"

#if ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)

#import "AuxiliaryProcessProxy.h"
#import "BaseBoardSPI.h"
#import "Logging.h"
#import "ProcessAssertion.h"
#import "RunningBoardServicesSPI.h"
#import <wtf/ProcessID.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringBuilder.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(LayerHostingVisibilityPropagator);

static NSString * const visibilityEndowmentNamespace = @"com.apple.frontboard.visibility";
static constexpr ASCIILiteral environmentSuffix = "-layerHosting"_s;

static void invalidateInjectors(HashMap<String, RetainPtr<BSServiceConnectionEndpointInjector>>& injectorsBySourceEnvironment)
{
    for (auto& injector : injectorsBySourceEnvironment.values())
        [injector invalidate];
}

Ref<LayerHostingVisibilityPropagator> LayerHostingVisibilityPropagator::create()
{
    return adoptRef(*new LayerHostingVisibilityPropagator());
}

LayerHostingVisibilityPropagator::LayerHostingVisibilityPropagator()
{
    EndowmentStateTracker::singleton().addClient(*this);
}

LayerHostingVisibilityPropagator::~LayerHostingVisibilityPropagator()
{
    for (auto& entry : std::exchange(m_processesAndInjectors, { }))
        invalidateInjectors(entry.injectorsBySourceEnvironment);
    EndowmentStateTracker::singleton().removeClient(*this);
}

void LayerHostingVisibilityPropagator::propagateVisibilityToProcess(AuxiliaryProcessProxy& process)
{
    RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: propagateVisibilityToProcess pid=%d", this, process.processID());

    auto matchIndex = m_processesAndInjectors.findIf([&](auto& entry) {
        return entry.process.get() == &process;
    });

    if (matchIndex == notFound) {
        ProcessAndInjectors entry;
        entry.process = WeakPtr(process);
        m_processesAndInjectors.append(WTF::move(entry));
        matchIndex = m_processesAndInjectors.size() - 1;
    }

    if (!m_processAssertion) {
        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: taking MediaPlayback process assertion on self", this);
        m_processAssertion = ProcessAssertion::create(getCurrentProcessID(), "WebKit Media Layer Hosting"_s, ProcessAssertionType::MediaPlayback);
    }

    refreshInjectorsAtIndex(matchIndex, EndowmentStateTracker::singleton().visibilityEndowmentEnvironments());
}

void LayerHostingVisibilityPropagator::stopPropagatingVisibilityToProcess(AuxiliaryProcessProxy& process)
{
    RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: stopPropagatingVisibilityToProcess pid=%d", this, process.processID());

    m_processesAndInjectors.removeAllMatching([&](auto& entry) {
        auto existingProcess = entry.process.get();
        if (existingProcess && existingProcess != &process)
            return false;

        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: removing %u visibility propagation injectors for process with PID=%d", this, entry.injectorsBySourceEnvironment.size(), process.processID());
        invalidateInjectors(entry.injectorsBySourceEnvironment);
        return true;
    });

    if (m_processesAndInjectors.isEmpty() && m_processAssertion) {
        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: releasing MediaPlayback process assertion on self", this);
        m_processAssertion = nullptr;
    }
}

void LayerHostingVisibilityPropagator::clear()
{
    RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: clearing all visibility propagation injectors", this);
    for (auto& entry : std::exchange(m_processesAndInjectors, { }))
        invalidateInjectors(entry.injectorsBySourceEnvironment);
    if (m_processAssertion) {
        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: releasing MediaPlayback process assertion on self", this);
        m_processAssertion = nullptr;
    }
}

void LayerHostingVisibilityPropagator::visibilityEndowmentEnvironmentsChanged(const HashSet<String>& environments)
{
    RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: visibilityEndowmentEnvironmentsChanged: %u environments now active", this, environments.size());
    for (size_t i = 0; i < m_processesAndInjectors.size(); ++i)
        refreshInjectorsAtIndex(i, environments);
}

void LayerHostingVisibilityPropagator::refreshInjectorsAtIndex(size_t index, const HashSet<String>& environments)
{
    auto& entry = m_processesAndInjectors[index];
    RefPtr process = entry.process.get();
    if (!process)
        return;

    auto processID = process->processID();
    if (!processID)
        return;

    auto environmentIdentifier = makeString(process->environmentIdentifier(), environmentSuffix);

    entry.injectorsBySourceEnvironment.removeIf([&](auto& pair) {
        if (environments.contains(pair.key))
            return false;
        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: releasing visibility endowment for pid=%d (source=%{public}s) — source environment no longer active", this, processID, pair.key.utf8().data());
        [pair.value invalidate];
        return true;
    });

    for (auto& sourceEnvironment : environments) {
        if (entry.injectorsBySourceEnvironment.contains(sourceEnvironment))
            continue;

        if (sourceEnvironment.isEmpty())
            continue;

        RetainPtr targetEnvironmentString = environmentIdentifier.createNSString();
        RetainPtr sourceEnvironmentString = sourceEnvironment.createNSString();
        RetainPtr target = [RBSTarget targetWithPid:processID environmentIdentifier:targetEnvironmentString.get()];

        RetainPtr injector = [BSServiceConnectionEndpointInjector injectorWithConfigurator:^(id<BSServiceConnectionEndpointInjectorConfiguring> config) {
            [config setTarget:target.get()];
            [config setInheritingEnvironment:sourceEnvironmentString.get()];
            [config setAdditionalAttributes:@[[RBSHereditaryGrant grantWithNamespace:visibilityEndowmentNamespace sourceEnvironment:sourceEnvironmentString.get() attributes:nil]]];
        }];

        if (!injector) {
            RELEASE_LOG_ERROR(Process, "LayerHostingVisibilityPropagator %p: failed to create endpoint injector for pid=%d", this, processID);
            continue;
        }

        RELEASE_LOG(Process, "LayerHostingVisibilityPropagator %p: acquired visibility endowment for pid=%d (source=%{public}s)", this, processID, sourceEnvironment.utf8().data());
        entry.injectorsBySourceEnvironment.set(sourceEnvironment, WTF::move(injector));
    }
}

} // namespace WebKit

#endif // ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)
