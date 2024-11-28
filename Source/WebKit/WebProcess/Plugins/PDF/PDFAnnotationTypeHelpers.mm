/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "config.h"
#include "PDFAnnotationTypeHelpers.h"

#if ENABLE(PDF_PLUGIN)

#include "PDFKitSPI.h"
#include <optional>

#include "PDFKitSoftLink.h"

namespace WebKit::PDFAnnotationTypeHelpers {

static std::optional<WidgetType> widgetType(PDFAnnotation *annotation)
{
    if (!annotationIsWidget(annotation))
        return { };

    NSString *type = [annotation valueForAnnotationKey:get_PDFKit_PDFAnnotationKeyWidgetFieldType()];
    if ([type isEqualToString:get_PDFKit_PDFAnnotationWidgetSubtypeButton()])
        return WidgetType::Button;
    if ([type isEqualToString:get_PDFKit_PDFAnnotationWidgetSubtypeChoice()])
        return WidgetType::Choice;
    if ([type isEqualToString:get_PDFKit_PDFAnnotationWidgetSubtypeSignature()])
        return WidgetType::Signature;
    if ([type isEqualToString:get_PDFKit_PDFAnnotationWidgetSubtypeText()])
        return WidgetType::Text;

    ASSERT_NOT_REACHED();
    return { };
}

bool annotationIsWidget(PDFAnnotation *annotation)
{
    return static_cast<bool>([[annotation valueForAnnotationKey:get_PDFKit_PDFAnnotationKeySubtype()] isEqualToString:get_PDFKit_PDFAnnotationSubtypeWidget()]);
}

bool annotationIsWidgetOfType(PDFAnnotation *annotation, WidgetType type)
{
    return widgetType(annotation).transform([queryType = type](auto candidateType) {
        return candidateType == queryType;
    }).value_or(false);
}

bool annotationIsWidgetOfType(PDFAnnotation *annotation, std::initializer_list<WidgetType>&& types)
{
    auto widgetTypeChecker = [annotation](auto&& type) {
        return annotationIsWidgetOfType(annotation, std::forward<decltype(type)>(type));
    };
    ASSERT(std::ranges::count_if(types, widgetTypeChecker) <= 1);
    return std::ranges::any_of(WTFMove(types), WTFMove(widgetTypeChecker));
}

} // namespace WebKit::PDFAnnotationTypeHelpers

#endif // ENABLE(PDF_PLUGIN)
