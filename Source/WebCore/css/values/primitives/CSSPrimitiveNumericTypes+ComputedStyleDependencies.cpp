/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CSSPrimitiveNumericTypes+ComputedStyleDependencies.h"

#include "CSSPropertyNames.h"
#include "ComputedStyleDependencies.h"

namespace WebCore {
namespace CSS {

// MARK: - Computed Style Dependencies

void ComputedStyleDependenciesCollector<LengthUnit>::operator()(ComputedStyleDependencies& dependencies, LengthUnit lengthUnit)
{
    using enum LengthUnit;

    switch (lengthUnit) {
    case Rcap:
    case Rch:
    case Rex:
    case Ric:
    case Rem:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case Rlh:
        dependencies.rootProperties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.rootProperties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case Em:
    case QuirkyEm:
    case Ex:
    case Cap:
    case Ch:
    case Ic:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        break;
    case Lh:
        dependencies.properties.appendIfNotContains(CSSPropertyFontSize);
        dependencies.properties.appendIfNotContains(CSSPropertyLineHeight);
        break;
    case Cqw:
    case Cqh:
    case Cqi:
    case Cqb:
    case Cqmin:
    case Cqmax:
        dependencies.containerDimensions = true;
        break;
    case Vw:
    case Vh:
    case Vmin:
    case Vmax:
    case Vb:
    case Vi:
    case Svw:
    case Svh:
    case Svmin:
    case Svmax:
    case Svb:
    case Svi:
    case Lvw:
    case Lvh:
    case Lvmin:
    case Lvmax:
    case Lvb:
    case Lvi:
    case Dvw:
    case Dvh:
    case Dvmin:
    case Dvmax:
    case Dvb:
    case Dvi:
        dependencies.viewportDimensions = true;
        break;
    case Px:
    case Cm:
    case Mm:
    case In:
    case Pt:
    case Pc:
    case Q:
        break;
    }
}

} // namespace CSS
} // namespace WebCore
