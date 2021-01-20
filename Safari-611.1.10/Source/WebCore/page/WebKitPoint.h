/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

class WebKitPoint : public RefCounted<WebKitPoint> {
public:
    static Ref<WebKitPoint> create()
    {
        return adoptRef(*new WebKitPoint);
    }
    static Ref<WebKitPoint> create(float x, float y)
    {
        return adoptRef(*new WebKitPoint(x, y));
    }

    float x() const { return m_x; }
    float y() const { return m_y; }
    
    void setX(float x) { m_x = x; }
    void setY(float y) { m_y = y; }

private:
    WebKitPoint(float x, float y)
        : m_x(std::isnan(x) ? 0 : x)
        , m_y(std::isnan(y) ? 0 : y)
    {
    }

    WebKitPoint()
    {
    }

    float m_x { 0 };
    float m_y { 0 };
};

} // namespace WebCore
