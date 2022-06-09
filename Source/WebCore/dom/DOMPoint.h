/*
 * Copyright (C) 2014 Adobe Systems Incorporated. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "DOMPointReadOnly.h"

namespace WebCore {

class DOMPoint final : public DOMPointReadOnly {
public:
    static Ref<DOMPoint> create(double x, double y, double z = 0, double w = 1) { return adoptRef(*new DOMPoint(x, y, z, w)); }
    static Ref<DOMPoint> create(const DOMPointInit& init) { return create(init.x, init.y, init.z, init.w); }
    static Ref<DOMPoint> fromPoint(const DOMPointInit& init) { return create(init.x, init.y, init.z, init.w); }

    void setX(double x) { m_x = x; }
    void setY(double y) { m_y = y; }
    void setZ(double z) { m_z = z; }
    void setW(double w) { m_w = w; }

private:
    DOMPoint(double x, double y, double z, double w)
        : DOMPointReadOnly(x, y, z, w)
    {
    }
};
static_assert(sizeof(DOMPoint) == sizeof(DOMPointReadOnly), "");

} // namespace WebCore
