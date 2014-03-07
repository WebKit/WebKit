/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef VisibleContentRectUpdateInfo_h
#define VisibleContentRectUpdateInfo_h

#include <WebCore/FloatRect.h>

namespace IPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class VisibleContentRectUpdateInfo {
public:
    VisibleContentRectUpdateInfo()
        : m_scale(-1)
        , m_updateID(0)
        , m_inStableState(false)
    {
    }

    VisibleContentRectUpdateInfo(uint64_t updateID, const WebCore::FloatRect& exposedRect, const WebCore::FloatRect& unobscuredRect, const WebCore::FloatRect& customFixedPositionRect, double scale, bool inStableState)
        : m_exposedRect(exposedRect)
        , m_unobscuredRect(unobscuredRect)
        , m_customFixedPositionRect(customFixedPositionRect)
        , m_scale(scale)
        , m_updateID(updateID)
        , m_inStableState(inStableState)
    {
    }

    const WebCore::FloatRect& exposedRect() const { return m_exposedRect; }
    const WebCore::FloatRect& unobscuredRect() const { return m_unobscuredRect; }
    const WebCore::FloatRect& customFixedPositionRect() const { return m_customFixedPositionRect; }
    double scale() const { return m_scale; }
    uint64_t updateID() const { return m_updateID; }
    bool inStableState() const { return m_inStableState; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, VisibleContentRectUpdateInfo&);

private:
    WebCore::FloatRect m_exposedRect;
    WebCore::FloatRect m_unobscuredRect;
    WebCore::FloatRect m_customFixedPositionRect;
    double m_scale;
    uint64_t m_updateID;
    bool m_inStableState;
};

inline bool operator==(const VisibleContentRectUpdateInfo& a, const VisibleContentRectUpdateInfo& b)
{
    // Note: the comparison doesn't include updateID since we care about equality based on the other data.
    return a.scale() == b.scale()
        && a.exposedRect() == b.exposedRect()
        && a.unobscuredRect() == b.unobscuredRect()
        && a.customFixedPositionRect() == b.customFixedPositionRect()
        && a.inStableState() == b.inStableState();
}

} // namespace WebKit

#endif // VisibleContentRectUpdateInfo_h
