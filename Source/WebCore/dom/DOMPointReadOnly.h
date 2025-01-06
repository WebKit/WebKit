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

#include "DOMPointInit.h"
#include "ExceptionOr.h"
#include "FloatPoint3D.h"
#include "ScriptWrappable.h"
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

struct DOMMatrixInit;
class DOMPoint;
class WebCoreOpaqueRoot;

class DOMPointReadOnly : public ScriptWrappable, public RefCounted<DOMPointReadOnly> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(DOMPointReadOnly, WEBCORE_EXPORT);
public:
    static Ref<DOMPointReadOnly> create(double x, double y, double z, double w) { return adoptRef(*new DOMPointReadOnly(x, y, z, w)); }
    static Ref<DOMPointReadOnly> create(const DOMPointInit& init) { return create(init.x, init.y, init.z, init.w); }
    static Ref<DOMPointReadOnly> fromPoint(const DOMPointInit& init) { return create(init.x, init.y, init.z, init.w); }
    static Ref<DOMPointReadOnly> fromFloatPoint(const FloatPoint3D& p) { return create(p.x(), p.y(), p.z(), 1); }

    double x() const { return m_x; }
    double y() const { return m_y; }
    double z() const { return m_z; }
    double w() const { return m_w; }

    ExceptionOr<Ref<DOMPoint>> matrixTransform(DOMMatrixInit&&) const;

protected:
    DOMPointReadOnly(double x, double y, double z, double w)
        : m_x(x)
        , m_y(y)
        , m_z(z)
        , m_w(w)
    {
    }

    // Any of these can be NaN or Inf.
    double m_x;
    double m_y;
    double m_z;
    double m_w;
};

WebCoreOpaqueRoot root(DOMPointReadOnly*);

} // namespace WebCore

