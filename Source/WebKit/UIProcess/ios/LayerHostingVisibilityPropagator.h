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

#pragma once

#if ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)

#include "EndowmentStateTracker.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

OBJC_CLASS BSServiceConnectionEndpointInjector;

namespace WebKit {

class AuxiliaryProcessProxy;
class ProcessAssertion;

class LayerHostingVisibilityPropagator final : public RefCounted<LayerHostingVisibilityPropagator>, public EndowmentStateTrackerClient {
    WTF_MAKE_TZONE_ALLOCATED(LayerHostingVisibilityPropagator);
public:
    static Ref<LayerHostingVisibilityPropagator> create();
    ~LayerHostingVisibilityPropagator();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void propagateVisibilityToProcess(AuxiliaryProcessProxy&);
    void stopPropagatingVisibilityToProcess(AuxiliaryProcessProxy&);
    void clear();

private:
    LayerHostingVisibilityPropagator();

    void visibilityEndowmentEnvironmentsChanged(const HashSet<String>&) final;
    void refreshInjectorsAtIndex(size_t, const HashSet<String>&);

    struct ProcessAndInjectors {
        WeakPtr<AuxiliaryProcessProxy> process;
        HashMap<String, RetainPtr<BSServiceConnectionEndpointInjector>> injectorsBySourceEnvironment;
    };
    Vector<ProcessAndInjectors> m_processesAndInjectors;
    RefPtr<ProcessAssertion> m_processAssertion;
};

} // namespace WebKit

#endif // ENABLE(ENDOWMENT_BASED_APPLICATION_STATE_TRACKING)
