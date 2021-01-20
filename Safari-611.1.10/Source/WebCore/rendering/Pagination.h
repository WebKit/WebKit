/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#include <wtf/EnumTraits.h>

namespace WebCore {

struct Pagination {
    enum Mode : uint8_t { Unpaginated, LeftToRightPaginated, RightToLeftPaginated, TopToBottomPaginated, BottomToTopPaginated };

    bool operator==(const Pagination& other) const
    {
        return mode == other.mode && behavesLikeColumns == other.behavesLikeColumns && pageLength == other.pageLength && gap == other.gap;
    }

    bool operator!=(const Pagination& other) const
    {
        return !(*this == other);
    }

    Mode mode { Unpaginated };
    bool behavesLikeColumns { false };
    unsigned pageLength { 0 };
    unsigned gap { 0 };
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::Pagination::Mode> {
    using values = EnumValues<
        WebCore::Pagination::Mode,
        WebCore::Pagination::Mode::Unpaginated,
        WebCore::Pagination::Mode::LeftToRightPaginated,
        WebCore::Pagination::Mode::RightToLeftPaginated,
        WebCore::Pagination::Mode::TopToBottomPaginated,
        WebCore::Pagination::Mode::BottomToTopPaginated
    >;
};

} // namespace WTF
