/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_SOURCE)

#include <WebCore/FourCC.h>
#include <WebCore/PlatformMediaError.h>
#include <WebCore/SourceBufferParser.h>
#include <optional>
#include <span>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/Ref.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class SharedBuffer;

// Lightweight ISO-BMFF pre-parser implementing the MSE Segment Parser Loop
// state machine. Walks box headers (via BitReader) to detect initialization
// and media segment boundaries without parsing box contents.
//
// We intentionally do not reuse ISOBox::peekBox here because it requires
// JSC::DataView, which routes through Gigacage — accessing non-Gigacaged
// memory (e.g. from SharedBuffer) causes EXC_BAD_ACCESS. See rdar://171573373.
class ISOBMFFPreParser {
    WTF_MAKE_TZONE_ALLOCATED(ISOBMFFPreParser);
    WTF_MAKE_NONCOPYABLE(ISOBMFFPreParser);
public:
    using AppendFlags = SourceBufferParser::AppendFlags;
    using ForwardDataCallback = Function<void(Ref<const SharedBuffer>&&, AppendFlags)>;

    explicit ISOBMFFPreParser(ForwardDataCallback&&);

    Expected<void, PlatformMediaError> appendData(Ref<const SharedBuffer>&&, AppendFlags = AppendFlags::None);

    // Resets the segment parser loop state. Does NOT clear
    // m_firstInitializationSegmentReceived per the MSE spec
    // (abort() resets the parser but remembers that init was received).
    void reset();

    void setPendingInitializationSegmentForChangeType();

private:
    enum class State : uint8_t {
        WaitingForSegment,
        ParsingInitSegment,
        ParsingMediaSegment,
    };

    struct BoxHeader {
        FourCC type;
        uint64_t size;
        uint8_t headerSize; // 8 or 16
    };

    static std::optional<BoxHeader> parseBoxHeader(std::span<const uint8_t>);
    static bool isInitSegmentStartBox(FourCC);
    static bool isMediaSegmentStartBox(FourCC);

    ForwardDataCallback m_forwardDataCallback;
    State m_state { State::WaitingForSegment };
    bool m_firstInitializationSegmentReceived { false };
    bool m_pendingInitializationSegmentForChangeType { false };
    Vector<uint8_t, 16> m_pendingHeaderBytes;
    uint64_t m_remainingBytesInCurrentBox { 0 };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_SOURCE)
