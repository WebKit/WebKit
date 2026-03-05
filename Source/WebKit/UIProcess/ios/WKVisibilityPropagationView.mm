/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if HAVE(VISIBILITY_PROPAGATION_VIEW) && USE(EXTENSIONKIT)

#import "AuxiliaryProcessProxy.h"
#import "ExtensionKitSPI.h"
#import "Logging.h"
#import <wtf/RetainPtr.h>
#import <wtf/WeakPtr.h>

namespace WebKit {
using ProcessAndInteractionPair = std::pair<WeakPtr<AuxiliaryProcessProxy>, RetainPtr<id<UIInteraction>>>;
}

@implementation WKVisibilityPropagationView {
    Vector<WebKit::ProcessAndInteractionPair> _processesAndInteractions;
}

- (void)propagateVisibilityToProcess:(WebKit::AuxiliaryProcessProxy&)process
{
    if ([self _containsInteractionForProcess:process])
        return;

    auto extensionProcess = process.extensionProcess();
    if (!extensionProcess)
        return;

    auto visibilityPropagationInteraction = extensionProcess->createVisibilityPropagationInteraction();
    if (!visibilityPropagationInteraction)
        return;

    [self addInteraction:visibilityPropagationInteraction.get()];

    RELEASE_LOG(Process, "Created visibility propagation interaction %@ for process with PID=%d", visibilityPropagationInteraction.get(), process.processID());

    auto processAndInteraction = std::make_pair(WeakPtr(process), visibilityPropagationInteraction);
    _processesAndInteractions.append(WTF::move(processAndInteraction));
}

- (void)stopPropagatingVisibilityToProcess:(WebKit::AuxiliaryProcessProxy&)process
{
    _processesAndInteractions.removeAllMatching([&](auto& processAndInteraction) {
        auto existingProcess = processAndInteraction.first.get();
        if (existingProcess && existingProcess != &process)
            return false;

        RELEASE_LOG(Process, "Removing visibility propagation interaction %p", processAndInteraction.second.get());

        [self removeInteraction:processAndInteraction.second.get()];
        return true;
    });
}

- (BOOL)_containsInteractionForProcess:(WebKit::AuxiliaryProcessProxy&)process
{
    return _processesAndInteractions.containsIf([&](auto& processAndInteraction) {
        return processAndInteraction.first.get() == &process;
    });
}

- (void)clear
{
    _processesAndInteractions.clear();
}

@end

#endif // HAVE(VISIBILITY_PROPAGATION_VIEW) && USE(EXTENSIONKIT)
