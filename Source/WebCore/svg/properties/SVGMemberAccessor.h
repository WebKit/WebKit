/*
 * Copyright (C) 2018-2019 Apple Inc.  All rights reserved.
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

#include "QualifiedName.h"
#include "SVGAnimatedProperty.h"
#include "SVGAttributeAnimator.h"

namespace WebCore {

class SVGProperty;

template<typename OwnerType>
class SVGMemberAccessor {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~SVGMemberAccessor() = default;

    virtual void detach(const OwnerType&) const { }
    virtual bool isAnimatedProperty() const { return false; }
    virtual bool isAnimatedLength() const { return false; }

    virtual bool matches(const OwnerType&, const SVGProperty&) const { return false; }
    virtual bool matches(const OwnerType&, const SVGAnimatedProperty&) const { return false; }
    virtual void setDirty(const OwnerType&, SVGAnimatedProperty& animatedProperty) const { animatedProperty.setDirty(); }
    virtual Optional<String> synchronize(const OwnerType&) const { return WTF::nullopt; }

    virtual RefPtr<SVGAttributeAnimator> createAnimator(OwnerType&, const QualifiedName&, AnimationMode, CalcMode, bool, bool) const { return nullptr; }
    virtual void appendAnimatedInstance(OwnerType&, SVGAttributeAnimator&) const { }

protected:
    SVGMemberAccessor() = default;
};

} // namespace WebCore
