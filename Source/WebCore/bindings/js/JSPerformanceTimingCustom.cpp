/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSPerformanceTiming.h"

#if ENABLE(WEB_TIMING)

#include "DOMWrapperWorld.h"
#include <heap/HeapInlines.h>
#include <runtime/AuxiliaryBarrierInlines.h>
#include <runtime/IdentifierInlines.h>
#include <runtime/JSObject.h>
#include <runtime/ObjectConstructor.h>

namespace WebCore {

using namespace JSC;

JSC::JSValue JSPerformanceTiming::toJSON(ExecState& state)
{
    PerformanceTiming& timing = wrapped();
    VM& vm = state.vm();

    auto object = constructEmptyObject(&state);

    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("navigationStart")), jsNumber(timing.navigationStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("unloadEventStart")), jsNumber(timing.unloadEventStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("unloadEventEnd")), jsNumber(timing.unloadEventEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("redirectStart")), jsNumber(timing.redirectStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("redirectEnd")), jsNumber(timing.redirectEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("fetchStart")), jsNumber(timing.fetchStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domainLookupStart")), jsNumber(timing.domainLookupStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domainLookupEnd")), jsNumber(timing.domainLookupEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("connectStart")), jsNumber(timing.connectStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("connectEnd")), jsNumber(timing.connectEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("secureConnectionStart")), jsNumber(timing.secureConnectionStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("requestStart")), jsNumber(timing.requestStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("responseStart")), jsNumber(timing.responseStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("responseEnd")), jsNumber(timing.responseEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domLoading")), jsNumber(timing.domLoading()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domInteractive")), jsNumber(timing.domInteractive()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domContentLoadedEventStart")), jsNumber(timing.domContentLoadedEventStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domContentLoadedEventEnd")), jsNumber(timing.domContentLoadedEventEnd()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("domComplete")), jsNumber(timing.domComplete()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("loadEventStart")), jsNumber(timing.loadEventStart()));
    object->putDirect(vm, Identifier::fromString(&vm, ASCIILiteral("loadEventEnd")), jsNumber(timing.loadEventEnd()));

    return object;
}

}

#endif // ENABLE(WEB_TIMING)
