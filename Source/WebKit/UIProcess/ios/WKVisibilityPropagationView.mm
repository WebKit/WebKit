/*
 * Copyright (C) 2024-2026 Apple Inc. All rights reserved.
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
#import "WKVisibilityPropagationView.h"

#if HAVE(VISIBILITY_PROPAGATION_VIEW)

#import "AuxiliaryProcessProxy.h"
#import "ExtensionKitSPI.h"
#import "Logging.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>

#if PLATFORM(IOS_FAMILY)
#import "UIKitSPI.h"
#endif

namespace WebKit {
#if USE(EXTENSIONKIT)
using ProcessAndVisibilityPropagatorPair = std::pair<WeakPtr<AuxiliaryProcessProxy>, RetainPtr<id<UIInteraction>>>;
#else
using ProcessAndVisibilityPropagatorPair = std::pair<WeakPtr<AuxiliaryProcessProxy>, RetainPtr<UIView>>;
#endif
}

@implementation WKVisibilityPropagationView {
    Vector<WebKit::ProcessAndVisibilityPropagatorPair> _processesAndVisibilityPropagators;
}

- (void)propagateVisibilityToProcess:(WebKit::AuxiliaryProcessProxy&)process contextID:(WebKit::LayerHostingContextID)contextID
{
    if ([self _containsVisibilityPropagatorForProcess:process])
        return;

#if USE(EXTENSIONKIT)
    auto extensionProcess = process.extensionProcess();
    if (!extensionProcess)
        return;

    auto visibilityPropagationInteraction = extensionProcess->createVisibilityPropagationInteraction();
    if (!visibilityPropagationInteraction)
        return;

    RELEASE_LOG(Process, "Created visibility propagation interaction %p for process with PID=%d", visibilityPropagationInteraction.get(), process.processID());

    [self addInteraction:visibilityPropagationInteraction.get()];
    auto processAndVisibilityPropagator = std::make_pair(WeakPtr(process), WTF::move(visibilityPropagationInteraction));
#else
    auto processID = process.processID();
    if (!processID)
        return;

#if HAVE(NON_HOSTING_VISIBILITY_PROPAGATION_VIEW)
    RetainPtr visibilityPropagationView = adoptNS([[_UINonHostingVisibilityPropagationView alloc] initWithFrame:CGRectZero pid:processID environmentIdentifier:process.environmentIdentifier().createNSString().get()]);
#else
    if (!contextID)
        return;

    RetainPtr visibilityPropagationView = adoptNS([[_UILayerHostView alloc] initWithFrame:CGRectZero pid:processID contextID:contextID]);
#endif

    RELEASE_LOG(Process, "Created visibility propagation view %p (contextID=%u) for process with PID=%d", visibilityPropagationView.get(), contextID, processID);

    [self addSubview:visibilityPropagationView.get()];
    auto processAndVisibilityPropagator = std::make_pair(WeakPtr(process), WTF::move(visibilityPropagationView));
#endif

    _processesAndVisibilityPropagators.append(WTF::move(processAndVisibilityPropagator));
}

- (void)_removeVisibilityPropagator:(const WebKit::ProcessAndVisibilityPropagatorPair::second_type&)visibilityPropagator
{
#if USE(EXTENSIONKIT)
    [self removeInteraction:visibilityPropagator.get()];
#else
    [visibilityPropagator removeFromSuperview];
#endif
}

- (void)stopPropagatingVisibilityToProcess:(WebKit::AuxiliaryProcessProxy&)process
{
    _processesAndVisibilityPropagators.removeAllMatching([&](auto& processAndVisibilityPropagator) {
        auto existingProcess = processAndVisibilityPropagator.first.get();
        if (existingProcess && existingProcess != &process)
            return false;

        RELEASE_LOG(Process, "Removing visibility propagation %p", processAndVisibilityPropagator.second.get());

        [self _removeVisibilityPropagator:processAndVisibilityPropagator.second];
        return true;
    });
}

- (BOOL)_containsVisibilityPropagatorForProcess:(WebKit::AuxiliaryProcessProxy&)process
{
    return _processesAndVisibilityPropagators.containsIf([&](auto& processesAndVisibilityPropagator) {
        return processesAndVisibilityPropagator.first.get() == &process;
    });
}

- (void)clear
{
    _processesAndVisibilityPropagators.removeAllMatching([&](auto& processAndVisibilityPropagator) {
        [self _removeVisibilityPropagator:processAndVisibilityPropagator.second];
        return true;
    });
}

@end

#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)
