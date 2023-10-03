/*
 * Copyright (C) 2008-2023 Apple Inc. All Rights Reserved.
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

#pragma once

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class TextMetrics : public RefCounted<TextMetrics> {
public:
    static Ref<TextMetrics> create() { return adoptRef(*new TextMetrics); }

    double width() const { return m_width; }
    void setWidth(double w) { m_width = w; }

    double actualBoundingBoxLeft() const { return m_actualBoundingBoxLeft; }
    void setActualBoundingBoxLeft(double value) { m_actualBoundingBoxLeft = value; }

    double actualBoundingBoxRight() const { return m_actualBoundingBoxRight; }
    void setActualBoundingBoxRight(double value) { m_actualBoundingBoxRight = value; }

    double fontBoundingBoxAscent() const { return m_fontBoundingBoxAscent; }
    void setFontBoundingBoxAscent(double value) { m_fontBoundingBoxAscent = value; }

    double fontBoundingBoxDescent() const { return m_fontBoundingBoxDescent; }
    void setFontBoundingBoxDescent(double value) { m_fontBoundingBoxDescent = value; }

    double actualBoundingBoxAscent() const { return m_actualBoundingBoxAscent; }
    void setActualBoundingBoxAscent(double value) { m_actualBoundingBoxAscent = value; }

    double actualBoundingBoxDescent() const { return m_actualBoundingBoxDescent; }
    void setActualBoundingBoxDescent(double value) { m_actualBoundingBoxDescent = value; }

    double emHeightAscent() const { return m_emHeightAscent; }
    void setEmHeightAscent(double value) { m_emHeightAscent = value; }

    double emHeightDescent() const { return m_emHeightDescent; }
    void setEmHeightDescent(double value) { m_emHeightDescent = value; }

    double hangingBaseline() const { return m_hangingBaseline; }
    void setHangingBaseline(double value) { m_hangingBaseline = value; }

    double alphabeticBaseline() const { return m_alphabeticBaseline; }
    void setAlphabeticBaseline(double value) { m_alphabeticBaseline = value; }

    double ideographicBaseline() const { return m_ideographicBaseline; }
    void setIdeographicBaseline(double value) { m_ideographicBaseline = value; }

private:
    double m_width { 0 };
    double m_actualBoundingBoxLeft { 0 };
    double m_actualBoundingBoxRight { 0 };
    double m_fontBoundingBoxAscent { 0 };
    double m_fontBoundingBoxDescent { 0 };
    double m_actualBoundingBoxAscent { 0 };
    double m_actualBoundingBoxDescent { 0 };
    double m_emHeightAscent { 0 };
    double m_emHeightDescent { 0 };
    double m_hangingBaseline { 0 };
    double m_alphabeticBaseline { 0 };
    double m_ideographicBaseline { 0 };
};

} // namespace WebCore
