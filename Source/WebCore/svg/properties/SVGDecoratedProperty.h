/*
 * Copyright (C) 2019 Apple Inc.  All rights reserved.
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

#include <wtf/RefCounted.h>

namespace WebCore {

template<typename DecorationType>
class SVGDecoratedProperty : public RefCounted<SVGDecoratedProperty<DecorationType>> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    SVGDecoratedProperty() = default;
    virtual ~SVGDecoratedProperty() = default;

    virtual void setValueInternal(const DecorationType&) = 0;
    virtual bool setValue(const DecorationType& value)
    {
        setValueInternal(value);
        return true;
    }

    // Used internally. It doesn't check for highestExposedEnumValue for example.
    virtual DecorationType valueInternal() const = 0;

    // Used by the DOM APIs.
    virtual DecorationType value() const { return valueInternal(); }

    virtual String valueAsString() const = 0;
    virtual Ref<SVGDecoratedProperty<DecorationType>> clone() = 0;
};

}
