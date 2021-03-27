/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ContextDestructionObserver.h"
#include "EventTarget.h"
#include "JSDOMPromiseDeferred.h"
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class AbortAlgorithm;
class ScriptExecutionContext;

class AbortSignal final : public RefCounted<AbortSignal>, public EventTargetWithInlineData, private ContextDestructionObserver {
    WTF_MAKE_ISO_ALLOCATED_EXPORT(AbortSignal, WEBCORE_EXPORT);
public:
    static Ref<AbortSignal> create(ScriptExecutionContext&);

    static Ref<AbortSignal> abort(ScriptExecutionContext&);

    static bool whenSignalAborted(AbortSignal&, Ref<AbortAlgorithm>&&);

    void signalAbort();
    void signalFollow(AbortSignal&);

    bool aborted() const { return m_aborted; }

    using RefCounted::ref;
    using RefCounted::deref;

    using Algorithm = Function<void()>;
    void addAlgorithm(Algorithm&& algorithm) { m_algorithms.append(WTFMove(algorithm)); }

    bool isFollowingSignal() const { return !!m_followingSignal; }

private:
    enum class Aborted : bool { No, Yes };
    explicit AbortSignal(ScriptExecutionContext&, Aborted = Aborted::No);

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final { return AbortSignalEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ContextDestructionObserver::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }
    
    bool m_aborted { false };
    Vector<Algorithm> m_algorithms;
    WeakPtr<AbortSignal> m_followingSignal;
};

}

