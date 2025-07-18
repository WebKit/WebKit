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
    if (!annotationIsOfType(annotation, AnnotationType::Widget))
        return { };

    RetainPtr type = [annotation valueForAnnotationKey:RetainPtr { get_PDFKit_PDFAnnotationKeyWidgetFieldType() }.get()];
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationWidgetSubtypeButton() }.get()])
        return WidgetType::Button;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationWidgetSubtypeChoice() }.get()])
        return WidgetType::Choice;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationWidgetSubtypeSignature() }.get()])
        return WidgetType::Signature;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationWidgetSubtypeText() }.get()])
        return WidgetType::Text;

    ASSERT_NOT_REACHED();
    return { };
}

static std::optional<AnnotationType> annotationType(PDFAnnotation *annotation)
{
    RetainPtr type = [annotation valueForAnnotationKey:RetainPtr { get_PDFKit_PDFAnnotationKeySubtype() }.get()];
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationSubtypeLink() }.get()])
        return AnnotationType::Link;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationSubtypePopup() }.get()])
        return AnnotationType::Popup;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationSubtypeText() }.get()])
        return AnnotationType::Text;
    if ([type.get() isEqualToString:RetainPtr { get_PDFKit_PDFAnnotationSubtypeWidget() }.get()])
        return AnnotationType::Widget;

    ASSERT_NOT_REACHED();
    return { };
}

template <typename Type>
using AnnotationToTypeConverter = std::optional<Type> (*)(PDFAnnotation *);

template <typename Type>
bool annotationCheckerInternal(PDFAnnotation *annotation, Type type, AnnotationToTypeConverter<Type> converter)
{
    return converter(annotation).transform([queryType = type](Type candidateType) {
        return candidateType == queryType;
    }).value_or(false);
}

template <typename Type>
bool annotationCheckerInternal(PDFAnnotation *annotation, std::initializer_list<Type>&& types, AnnotationToTypeConverter<Type> converter)
{
    auto checker = [annotation = RetainPtr { annotation }, converter = WTFMove(converter)](auto&& type) {
        return annotationCheckerInternal(annotation.get(), std::forward<decltype(type)>(type), WTFMove(converter));
    };
    ASSERT(std::ranges::count_if(types, checker) <= 1);
    return std::ranges::any_of(WTFMove(types), WTFMove(checker));
}

bool annotationIsOfType(PDFAnnotation *annotation, AnnotationType type)
{
    return annotationCheckerInternal(annotation, type, annotationType);
}

bool annotationIsOfType(PDFAnnotation *annotation, std::initializer_list<AnnotationType>&& types)
{
    return annotationCheckerInternal(annotation, WTFMove(types), annotationType);
}

bool annotationIsWidgetOfType(PDFAnnotation *annotation, WidgetType type)
{
    return annotationCheckerInternal(annotation, type, widgetType);
}

bool annotationIsWidgetOfType(PDFAnnotation *annotation, std::initializer_list<WidgetType>&& types)
{
    return annotationCheckerInternal(annotation, WTFMove(types), widgetType);
}

} // namespace WebKit::PDFAnnotationTypeHelpers

#endif // ENABLE(PDF_PLUGIN)
