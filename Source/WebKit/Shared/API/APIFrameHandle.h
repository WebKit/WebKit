/*
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <WebCore/FrameIdentifier.h>
#include <wtf/Ref.h>

namespace API {

class FrameHandle final : public ObjectImpl<Object::Type::FrameHandle> {
public:
    static Ref<FrameHandle> create(WebCore::FrameIdentifier frameID)
    {
        return adoptRef(*new FrameHandle(frameID, false));
    }
    static Ref<FrameHandle> createAutoconverting(WebCore::FrameIdentifier frameID)
    {
        return adoptRef(*new FrameHandle(frameID, true));
    }
    static Ref<FrameHandle> create(WebCore::FrameIdentifier frameID, bool autoconverting)
    {
        return adoptRef(*new FrameHandle(frameID, autoconverting));
    }

    explicit FrameHandle(WebCore::FrameIdentifier frameID, bool isAutoconverting)
        : m_frameID(frameID)
        , m_isAutoconverting(isAutoconverting)
    {
    }

    WebCore::FrameIdentifier frameID() const { return m_frameID; }
    bool isAutoconverting() const { return m_isAutoconverting; }

private:
    const WebCore::FrameIdentifier m_frameID;
    const bool m_isAutoconverting;
};

} // namespace API
