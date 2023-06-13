/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSWordBoundaryDetectionValue.h"

#include <wtf/StdLibExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

CSSWordBoundaryDetectionValue::CSSWordBoundaryDetectionValue(WordBoundaryDetection&& value)
    : CSSValue(WordBoundaryDetectionClass)
    , m_value(value)
{
}

String CSSWordBoundaryDetectionValue::customCSSText() const
{
    TextStream ts;
    // FIXME: Explicit static_cast to work around issue on libstdc++-10. Undo when upgrading GCC from 10 to 11.
    WTF::switchOn(static_cast<WordBoundaryDetectionType>(value()), [&ts](WordBoundaryDetectionNormal) {
        ts << "normal";
    }, [&ts](WordBoundaryDetectionManual) {
        ts << "manual";
    }, [&ts](const WordBoundaryDetectionAuto& wordBoundaryDetectionAuto) {
        // FIXME: This string needs to be escaped.
        ts << "auto(\"" << wordBoundaryDetectionAuto.lang << "\")";
    });
    return ts.release();
}

bool CSSWordBoundaryDetectionValue::equals(const CSSWordBoundaryDetectionValue& other) const
{
    return value() == other.value();
}

} // namespace WebCore
