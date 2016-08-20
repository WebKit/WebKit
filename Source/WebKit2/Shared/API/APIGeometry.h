/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef APIGeometry_h
#define APIGeometry_h

#include "APIObject.h"
#include "WKGeometry.h"
#include <WebCore/FloatRect.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace API {

class Size : public API::ObjectImpl<API::Object::Type::Size> {
public:
    static Ref<Size> create(const WKSize& size)
    {
        return adoptRef(*new Size(size));
    }

    const WKSize& size() const { return m_size; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<API::Object>&);

private:
    explicit Size(const WKSize& size)
        : m_size(size)
    {
    }

    WKSize m_size;
};

class Point : public API::ObjectImpl<API::Object::Type::Point> {
public:
    static Ref<Point> create(const WKPoint& point)
    {
        return adoptRef(*new Point(point));
    }

    const WKPoint& point() const { return m_point; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<API::Object>&);

private:
    explicit Point(const WKPoint& point)
        : m_point(point)
    { }

    WKPoint m_point;
};

class Rect : public API::ObjectImpl<API::Object::Type::Rect> {
public:
    static Ref<Rect> create(const WKRect& rect)
    {
        return adoptRef(*new Rect(rect));
    }

    const WKRect& rect() const { return m_rect; }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, RefPtr<API::Object>&);

private:
    explicit Rect(const WKRect& rect)
        : m_rect(rect)
    {
    }

    WKRect m_rect;
};

} // namespace API

#endif // APIGeometry_h
