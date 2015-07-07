/*
 * Copyright (C) 2014 Frederic Wang (fred.wang@free.fr). All rights reserved.
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

#ifndef OpenTypeMathData_h
#define OpenTypeMathData_h

#include "Glyph.h"
#include "SharedBuffer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class SimpleFontData;
class FontPlatformData;

class OpenTypeMathData : public RefCounted<OpenTypeMathData> {
public:
    static PassRefPtr<OpenTypeMathData> create(const FontPlatformData& fontData)
    {
        return adoptRef(new OpenTypeMathData(fontData));
    }

    bool hasMathData() const { return m_mathBuffer; }

    // These constants are defined in the MATH table.
    // The implementation of OpenTypeMathData::getMathConstant assumes that they correspond to the indices of the MathContant table.
    enum MathConstant {
        ScriptPercentScaleDown,
        ScriptScriptPercentScaleDown,
        DelimitedSubFormulaMinHeight,
        DisplayOperatorMinHeight,
        MathLeading,
        AxisHeight,
        AccentBaseHeight,
        FlattenedAccentBaseHeight,
        SubscriptShiftDown,
        SubscriptTopMax,
        SubscriptBaselineDropMin,
        SuperscriptShiftUp,
        SuperscriptShiftUpCramped,
        SuperscriptBottomMin,
        SuperscriptBaselineDropMax,
        SubSuperscriptGapMin,
        SuperscriptBottomMaxWithSubscript,
        SpaceAfterScript,
        UpperLimitGapMin,
        UpperLimitBaselineRiseMin,
        LowerLimitGapMin,
        LowerLimitBaselineDropMin,
        StackTopShiftUp,
        StackTopDisplayStyleShiftUp,
        StackBottomShiftDown,
        StackBottomDisplayStyleShiftDown,
        StackGapMin,
        StackDisplayStyleGapMin,
        StretchStackTopShiftUp,
        StretchStackBottomShiftDown,
        StretchStackGapAboveMin,
        StretchStackGapBelowMin,
        FractionNumeratorShiftUp,
        FractionNumeratorDisplayStyleShiftUp,
        FractionDenominatorShiftDown,
        FractionDenominatorDisplayStyleShiftDown,
        FractionNumeratorGapMin,
        FractionNumDisplayStyleGapMin,
        FractionRuleThickness,
        FractionDenominatorGapMin,
        FractionDenomDisplayStyleGapMin,
        SkewedFractionHorizontalGap,
        SkewedFractionVerticalGap,
        OverbarVerticalGap,
        OverbarRuleThickness,
        OverbarExtraAscender,
        UnderbarVerticalGap,
        UnderbarRuleThickness,
        UnderbarExtraDescender,
        RadicalVerticalGap,
        RadicalDisplayStyleVerticalGap,
        RadicalRuleThickness,
        RadicalExtraAscender,
        RadicalKernBeforeDegree,
        RadicalKernAfterDegree,
        RadicalDegreeBottomRaisePercent
    };

    struct AssemblyPart {
        Glyph glyph;
        bool isExtender;
    };

    float getMathConstant(const SimpleFontData*, MathConstant) const;
    float getItalicCorrection(const SimpleFontData*, Glyph) const;
    void getMathVariants(Glyph, bool isVertical, Vector<Glyph>& sizeVariants, Vector<AssemblyPart>& assemblyParts) const;

private:
    explicit OpenTypeMathData(const FontPlatformData&);
    RefPtr<SharedBuffer> m_mathBuffer;
};

} // namespace WebCore

#endif // OpenTypeMathData_h
