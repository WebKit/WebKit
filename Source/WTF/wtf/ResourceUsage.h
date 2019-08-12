/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#include <array>

namespace WTF {

#if PLATFORM(COCOA)

struct TagInfo {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    size_t dirty { 0 };
    size_t reclaimable { 0 };
    size_t reserved { 0 };
};

WTF_EXPORT_PRIVATE const char* displayNameForVMTag(unsigned);
WTF_EXPORT_PRIVATE size_t vmPageSize();
WTF_EXPORT_PRIVATE std::array<TagInfo, 256> pagesPerVMTag();
WTF_EXPORT_PRIVATE void logFootprintComparison(const std::array<TagInfo, 256>&, const std::array<TagInfo, 256>&);

#endif

}

#if PLATFORM(COCOA)
using WTF::TagInfo;
using WTF::displayNameForVMTag;
using WTF::vmPageSize;
using WTF::pagesPerVMTag;
#endif
