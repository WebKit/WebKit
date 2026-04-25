/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#pragma once

#include <CoreText/CoreText.h>

namespace WebCore {

struct FontInterrogation {
    FontInterrogation(CTFontRef font)
    {
        bool foundStat = false;
        bool foundTrak = false;
        auto tables = adoptCF(CTFontCopyAvailableTables(font, kCTFontTableOptionNoOptions));
        if (!tables)
            return;
        auto size = CFArrayGetCount(tables.get());
        for (CFIndex i = 0; i < size; ++i) {
            auto tableTag = static_cast<CTFontTableTag>(reinterpret_cast<uintptr_t>(CFArrayGetValueAtIndex(tables.get(), i)));
            switch (tableTag) {
            case kCTFontTableFvar:
                if (variationType == VariationType::NotVariable)
                    variationType = VariationType::TrueTypeGX;
                break;
            case kCTFontTableSTAT:
                foundStat = true;
                variationType = VariationType::OpenType18;
                break;
            case kCTFontTableMorx:
            case kCTFontTableMort:
                aatShaping = true;
                break;
            case kCTFontTableGPOS:
            case kCTFontTableGSUB:
                openTypeShaping = true;
                break;
            case kCTFontTableTrak:
                foundTrak = true;
                break;
            }
        }
        if (foundStat && foundTrak)
            trackingType = TrackingType::Automatic;
        else if (foundTrak)
            trackingType = TrackingType::Manual;
    }

    enum class VariationType : uint8_t { NotVariable, TrueTypeGX, OpenType18, };
    VariationType variationType { VariationType::NotVariable };
    enum class TrackingType : uint8_t { None, Automatic, Manual, };
    TrackingType trackingType { TrackingType::None };
    bool openTypeShaping { false };
    bool aatShaping { false };
};

} // namespace WebCore
