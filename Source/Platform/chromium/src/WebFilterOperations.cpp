/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include <cmath>
#include <public/WebFilterOperation.h>
#include <public/WebFilterOperations.h>
#include <wtf/Vector.h>

namespace WebKit {

class WebFilterOperationsPrivate {
public:
    Vector<WebFilterOperation> operations;
};


void WebFilterOperations::initialize()
{
    m_private.reset(new WebFilterOperationsPrivate);
}

void WebFilterOperations::destroy()
{
    m_private.reset(0);
}

void WebFilterOperations::assign(const WebFilterOperations& other)
{
    m_private->operations = other.m_private->operations;
}

bool WebFilterOperations::equals(const WebFilterOperations& other) const
{
    if (other.size() != size())
        return false;
    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        if (other.at(i) != at(i))
            return false;
    }
    return true;
}

void WebFilterOperations::append(const WebFilterOperation& filter)
{
    m_private->operations.append(filter);
}

void WebFilterOperations::clear()
{
    m_private->operations.clear();
}

bool WebFilterOperations::isEmpty() const
{
    return m_private->operations.isEmpty();
}

static int spreadForStdDeviation(float stdDeviation)
{
    // https://dvcs.w3.org/hg/FXTF/raw-file/tip/filters/index.html#feGaussianBlurElement provides this approximation for
    // evaluating a gaussian blur by a triple box filter.
    float d = floorf(stdDeviation * 3 * sqrt(8.f * atan(1.f)) / 4 + 0.5);
    return static_cast<int>(ceilf(d * 3 / 2));
}

void WebFilterOperations::getOutsets(int& top, int& right, int& bottom, int& left) const
{
    top = right = bottom = left = 0;
    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        const WebFilterOperation op = m_private->operations[i];
        if (op.type() == WebFilterOperation::FilterTypeBlur || op.type() == WebFilterOperation::FilterTypeDropShadow) {
            int spread = spreadForStdDeviation(op.amount());
            if (op.type() == WebFilterOperation::FilterTypeBlur) {
                top += spread;
                right += spread;
                bottom += spread;
                left += spread;
            } else {
                top += spread - op.dropShadowOffset().y;
                right += spread + op.dropShadowOffset().x;
                bottom += spread + op.dropShadowOffset().y;
                left += spread - op.dropShadowOffset().x;
            }
        }
    }
}

bool WebFilterOperations::hasFilterThatMovesPixels() const
{
    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        const WebFilterOperation op = m_private->operations[i];
        switch (op.type()) {
        case WebFilterOperation::FilterTypeBlur:
        case WebFilterOperation::FilterTypeDropShadow:
        case WebFilterOperation::FilterTypeZoom:
            return true;
        default:
            break;
        }
    }
    return false;
}

bool WebFilterOperations::hasFilterThatAffectsOpacity() const
{
    for (size_t i = 0; i < m_private->operations.size(); ++i) {
        const WebFilterOperation op = m_private->operations[i];
        switch (op.type()) {
        case WebFilterOperation::FilterTypeOpacity:
        case WebFilterOperation::FilterTypeBlur:
        case WebFilterOperation::FilterTypeDropShadow:
        case WebFilterOperation::FilterTypeZoom:
            return true;
        case WebFilterOperation::FilterTypeColorMatrix: {
            const SkScalar* matrix = op.matrix();
            return matrix[15]
                || matrix[16]
                || matrix[17]
                || matrix[18] != 1
                || matrix[19];
        }
        default:
            break;
        }
    }
    return false;
}

size_t WebFilterOperations::size() const
{
    return m_private->operations.size();
}

WebFilterOperation WebFilterOperations::at(size_t i) const
{
    return m_private->operations.at(i);
}

} // namespace WebKit
