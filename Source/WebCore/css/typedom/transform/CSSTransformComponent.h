/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "DOMMatrix.h"
#include <wtf/IsoMalloc.h>
#include <wtf/RefCounted.h>

namespace WebCore {

enum class CSSTransformType : uint8_t {
    MatrixComponent,
    Perspective,
    Rotate,
    Scale,
    Skew,
    SkewX,
    SkewY,
    Translate
};

class DOMMatrix;
template<typename> class ExceptionOr;

class CSSTransformComponent : public RefCounted<CSSTransformComponent> {
protected:
    enum class Is2D : bool { No, Yes };
    CSSTransformComponent(Is2D is2D)
        : m_is2D(is2D) { }
public:
    String toString() const;
    virtual void serialize(StringBuilder&) const = 0;
    bool is2D() const { return m_is2D == Is2D::Yes; }
    virtual void setIs2D(bool is2D) { m_is2D = is2D ? Is2D::Yes : Is2D::No; }
    virtual ExceptionOr<Ref<DOMMatrix>> toMatrix() = 0;
    virtual ~CSSTransformComponent() = default;
    virtual CSSTransformType getType() const = 0;

private:
    Is2D m_is2D;
};

} // namespace WebCore
