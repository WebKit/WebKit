/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "BAssert.h"
#include "BInline.h"
#include "Gigacage.h"

namespace bmalloc {

enum class HeapKind {
    Primary,
    PrimitiveGigacage,
    JSValueGigacage
};

static constexpr unsigned numHeaps = 3;

BINLINE bool isGigacage(HeapKind heapKind)
{
    switch (heapKind) {
    case HeapKind::Primary:
        return false;
    case HeapKind::PrimitiveGigacage:
    case HeapKind::JSValueGigacage:
        return true;
    }
    BCRASH();
    return false;
}

BINLINE Gigacage::Kind gigacageKind(HeapKind kind)
{
    switch (kind) {
    case HeapKind::Primary:
        BCRASH();
        return Gigacage::Primitive;
    case HeapKind::PrimitiveGigacage:
        return Gigacage::Primitive;
    case HeapKind::JSValueGigacage:
        return Gigacage::JSValue;
    }
    BCRASH();
    return Gigacage::Primitive;
}

BINLINE HeapKind heapKind(Gigacage::Kind kind)
{
    switch (kind) {
    case Gigacage::ReservedForFlagsAndNotABasePtr:
        RELEASE_BASSERT_NOT_REACHED();
    case Gigacage::Primitive:
        return HeapKind::PrimitiveGigacage;
    case Gigacage::JSValue:
        return HeapKind::JSValueGigacage;
    }
    BCRASH();
    return HeapKind::Primary;
}

BINLINE bool isActiveHeapKindAfterEnsuringGigacage(HeapKind kind)
{
    switch (kind) {
    case HeapKind::PrimitiveGigacage:
    case HeapKind::JSValueGigacage:
        if (Gigacage::wasEnabled())
            return true;
        return false;
    default:
        return true;
    }
}

BEXPORT bool isActiveHeapKind(HeapKind);

BINLINE HeapKind mapToActiveHeapKindAfterEnsuringGigacage(HeapKind kind)
{
    switch (kind) {
    case HeapKind::PrimitiveGigacage:
    case HeapKind::JSValueGigacage:
        if (Gigacage::wasEnabled())
            return kind;
        return HeapKind::Primary;
    default:
        return kind;
    }
}

BEXPORT HeapKind mapToActiveHeapKind(HeapKind);

} // namespace bmalloc

