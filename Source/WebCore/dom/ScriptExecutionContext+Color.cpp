/*
 * Copyright (C) 2024 Apple Inc.  All rights reserved.
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
#include "ScriptExecutionContext+Color.h"

#include "CSSPropertyParserConsumer+Color.h"
#include "ScriptExecutionContext.h"

namespace WebCore {

static OptionSet<StyleColor::CSSColorType> allowedColorTypes(const ScriptExecutionContext* scriptExecutionContext)
{
    if (scriptExecutionContext && scriptExecutionContext->isDocument())
        return { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current, StyleColor::CSSColorType::System };

    // FIXME: All canvas types should support all color types, but currently
    //        system colors are not thread safe so are disabled for non-document
    //        based canvases.
    return { StyleColor::CSSColorType::Absolute, StyleColor::CSSColorType::Current };
}

Color parseColor(const String& colorString, const ScriptExecutionContext* scriptExecutionContext)
{
    // FIXME: Add constructor for CSSParserContext that takes a ScriptExecutionContext to allow preferences to be
    //        checked correctly.
    return CSSPropertyParserHelpers::parseColorRaw(colorString, CSSParserContext(HTMLStandardMode), [&] {
        return CSSPropertyParserHelpers::ColorParsingParameters {
            CSSPropertyParserHelpers::CSSColorParsingOptions {
                .allowedColorTypes = allowedColorTypes(scriptExecutionContext)
            },
            CSSUnresolvedColorResolutionContext {
                .resolvedCurrentColor = Color::black
            }
        };
    });
}

} // namespace WebCore
