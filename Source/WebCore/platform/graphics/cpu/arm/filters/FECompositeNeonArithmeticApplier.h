/*
 * Copyright (C) 2011 University of Szeged
 * Copyright (C) 2011 Felician Marton
 * Copyright (C) 2022 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if HAVE(ARM_NEON_INTRINSICS)

#include "FilterEffectApplier.h"

namespace WebCore {

class FEComposite;

class FECompositeNeonArithmeticApplier final : public FilterEffectConcreteApplier<FEComposite> {
    WTF_MAKE_FAST_ALLOCATED;
    using Base = FilterEffectConcreteApplier<FEComposite>;

public:
    FECompositeNeonArithmeticApplier(const FEComposite&);

private:
    template <int b1, int b4>
    static inline void computePixels(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4);

    static inline void applyPlatform(const uint8_t* source, uint8_t* destination, unsigned pixelArrayLength, float k1, float k2, float k3, float k4);

    bool apply(const Filter&, const FilterImageVector& inputs, FilterImage& result) const final;
};

} // namespace WebCore

#endif // HAVE(ARM_NEON_INTRINSICS)
