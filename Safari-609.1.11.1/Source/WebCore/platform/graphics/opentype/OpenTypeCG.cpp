/*
 * Copyright (C) 2015 Frederic Wang (fred.wang@free.fr). All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "OpenTypeCG.h"

#include "OpenTypeTypes.h"

namespace WebCore {
namespace OpenType {

#if PLATFORM(WIN)
static const unsigned long kCTFontTableOS2 = 'OS/2';
#endif

bool fontHasMathTable(CTFontRef ctFont)
{
    RetainPtr<CFArrayRef> tableTags = adoptCF(CTFontCopyAvailableTables(ctFont, kCTFontTableOptionNoOptions));
    if (!tableTags)
        return false;
    CFIndex numTables = CFArrayGetCount(tableTags.get());
    for (CFIndex index = 0; index < numTables; ++index) {
        CTFontTableTag tag = (CTFontTableTag)(uintptr_t)CFArrayGetValueAtIndex(tableTags.get(), index);
        if (tag == 'MATH')
            return true;
    }
    return false;
}

static inline short readShortFromTable(const UInt8* os2Data, CFIndex offset)
{
    return *(reinterpret_cast<const OpenType::Int16*>(os2Data + offset));
}

bool tryGetTypoMetrics(CTFontRef font, short& ascent, short& descent, short& lineGap)
{
    bool result = false;
    if (auto os2Table = adoptCF(CTFontCopyTable(font, kCTFontTableOS2, kCTFontTableOptionNoOptions))) {
        // For the structure of the OS/2 table, see
        // https://developer.apple.com/fonts/TrueType-Reference-Manual/RM06/Chap6OS2.html
        const CFIndex fsSelectionOffset = 16 * 2 + 10 + 4 * 4 + 4 * 1;
        const CFIndex sTypoAscenderOffset = fsSelectionOffset + 3 * 2;
        const CFIndex sTypoDescenderOffset = sTypoAscenderOffset + 2;
        const CFIndex sTypoLineGapOffset = sTypoDescenderOffset + 2;
        if (CFDataGetLength(os2Table.get()) >= sTypoLineGapOffset + 2) {
            const UInt8* os2Data = CFDataGetBytePtr(os2Table.get());
            // We test the use typo bit on the least significant byte of fsSelection.
            const UInt8 useTypoMetricsMask = 1 << 7;
            if (*(os2Data + fsSelectionOffset + 1) & useTypoMetricsMask) {
                ascent = readShortFromTable(os2Data, sTypoAscenderOffset);
                descent = readShortFromTable(os2Data, sTypoDescenderOffset);
                lineGap = readShortFromTable(os2Data, sTypoLineGapOffset);
                result = true;
            }
        }
    }
    return result;
}

} // namespace OpenType
} // namespace WebCore
