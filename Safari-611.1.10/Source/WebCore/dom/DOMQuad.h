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

#include "DOMQuadInit.h"
#include "ScriptWrappable.h"
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class DOMPoint;
class DOMRect;
struct DOMPointInit;
struct DOMRectInit;

class DOMQuad : public ScriptWrappable, public RefCounted<DOMQuad> {
    WTF_MAKE_ISO_ALLOCATED(DOMQuad);
public:
    static Ref<DOMQuad> create(const DOMPointInit& p1, const DOMPointInit& p2, const DOMPointInit& p3, const DOMPointInit& p4) { return adoptRef(*new DOMQuad(p1, p2, p3, p4)); }
    static Ref<DOMQuad> fromRect(const DOMRectInit& init) { return adoptRef(*new DOMQuad(init)); }
    static Ref<DOMQuad> fromQuad(const DOMQuadInit& init) { return create(init.p1, init.p2, init.p3, init.p4); }

    const DOMPoint& p1() const { return m_p1; }
    const DOMPoint& p2() const { return m_p2; }
    const DOMPoint& p3() const { return m_p3; }
    const DOMPoint& p4() const { return m_p4; }

    Ref<DOMRect> getBounds() const;

private:
    DOMQuad(const DOMPointInit&, const DOMPointInit&, const DOMPointInit&, const DOMPointInit&);
    explicit DOMQuad(const DOMRectInit&);
    
    Ref<DOMPoint> m_p1;
    Ref<DOMPoint> m_p2;
    Ref<DOMPoint> m_p3;
    Ref<DOMPoint> m_p4;
};

}
