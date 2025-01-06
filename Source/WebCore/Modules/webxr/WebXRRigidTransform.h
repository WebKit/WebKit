/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(WEBXR)

#include "ExceptionOr.h"
#include "JSValueInWrappedObject.h"
#include "TransformationMatrix.h"
#include <JavaScriptCore/Forward.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

struct DOMPointInit;
class DOMPointReadOnly;

class WebXRRigidTransform : public RefCountedAndCanMakeWeakPtr<WebXRRigidTransform> {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED_EXPORT(WebXRRigidTransform, WEBCORE_EXPORT);
public:
    static Ref<WebXRRigidTransform> create();
    static Ref<WebXRRigidTransform> create(const TransformationMatrix&);
    WEBCORE_EXPORT static ExceptionOr<Ref<WebXRRigidTransform>> create(const DOMPointInit&, const DOMPointInit&);
    WEBCORE_EXPORT ~WebXRRigidTransform();

    const DOMPointReadOnly& position() const;
    const DOMPointReadOnly& orientation() const;
    const Float32Array& matrix();
    const WebXRRigidTransform& inverse();
    const TransformationMatrix& rawTransform() const;

    JSValueInWrappedObject& cachedMatrix() { return m_cachedMatrix; }

private:
    WebXRRigidTransform(const DOMPointInit&, const DOMPointInit&);
    WebXRRigidTransform(const TransformationMatrix&);

    Ref<DOMPointReadOnly> m_position;
    Ref<DOMPointReadOnly> m_orientation;
    TransformationMatrix m_rawTransform;
    RefPtr<Float32Array> m_matrix;
    RefPtr<WebXRRigidTransform> m_inverse;
    WeakPtr<WebXRRigidTransform> m_parentInverse;
    JSValueInWrappedObject m_cachedMatrix;
};

} // namespace WebCore

#endif // ENABLE(WEBXR)
