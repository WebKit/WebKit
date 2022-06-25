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

#include "config.h"
#include "DOMQuad.h"

#include "DOMPoint.h"
#include "DOMRect.h"
#include <wtf/IsoMallocInlines.h>
#include <wtf/MathExtras.h>


namespace WebCore {
using namespace WTF;

WTF_MAKE_ISO_ALLOCATED_IMPL(DOMQuad);

DOMQuad::DOMQuad(const DOMPointInit& p1, const DOMPointInit& p2, const DOMPointInit& p3, const DOMPointInit& p4)
    : m_p1(DOMPoint::create(p1))
    , m_p2(DOMPoint::create(p2))
    , m_p3(DOMPoint::create(p3))
    , m_p4(DOMPoint::create(p4))
{
}

//  p1------p2
//  |       |
//  |       |
//  p4------p3
DOMQuad::DOMQuad(const DOMRectInit& r)
    : m_p1(DOMPoint::create(r.x, r.y))
    , m_p2(DOMPoint::create(r.x + r.width, r.y))
    , m_p3(DOMPoint::create(r.x + r.width, r.y + r.height))
    , m_p4(DOMPoint::create(r.x, r.y + r.height))
{
}

Ref<DOMRect> DOMQuad::getBounds() const
{
    double left = nanPropagatingMin(nanPropagatingMin(nanPropagatingMin(m_p1->x(), m_p2->x()), m_p3->x()), m_p4->x());
    double top = nanPropagatingMin(nanPropagatingMin(nanPropagatingMin(m_p1->y(), m_p2->y()), m_p3->y()), m_p4->y());
    double right = nanPropagatingMax(nanPropagatingMax(nanPropagatingMax(m_p1->x(), m_p2->x()), m_p3->x()), m_p4->x());
    double bottom = nanPropagatingMax(nanPropagatingMax(nanPropagatingMax(m_p1->y(), m_p2->y()), m_p3->y()), m_p4->y());

    return DOMRect::create(left, top, right - left, bottom - top);
}

}
