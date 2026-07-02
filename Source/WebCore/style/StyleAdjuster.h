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

#include "AutosizeStatus.h"
#include "RenderStyleConstants.h"
#include <wtf/CheckedRef.h>
#include <wtf/OptionSet.h>

namespace WebCore {

class Document;
class Element;
class EventTarget;
class SVGElement;
class Settings;

enum class AnimationImpact : uint8_t;
enum class PaginationMode : uint8_t;

namespace Style {

class ComputedStyle;
class Update;

class Adjuster {
public:
    Adjuster(const Document&, const Style::ComputedStyle& parentStyle, const Style::ComputedStyle* parentBoxStyle, Element*);

    static void adjustFromBuilder(Style::ComputedStyle&);
    void adjust(Style::ComputedStyle&) const;
    void adjustAnimatedStyle(Style::ComputedStyle&, OptionSet<AnimationImpact>) const;

    static void adjustVisibilityForPseudoElement(Style::ComputedStyle&, const Element& host);
    static void NODELETE adjustFirstLetterStyle(Style::ComputedStyle&);
    static void NODELETE adjustFirstLineStyle(Style::ComputedStyle&);
    static void adjustSVGElementStyle(Style::ComputedStyle&, const SVGElement&);
    static bool adjustEventListenerRegionTypesForRootStyle(Style::ComputedStyle&, const Document&);
    static void adjustColumnStylesForPaginationMode(Style::ComputedStyle&, PaginationMode);
    static void propagateToDocumentElementAndInitialContainingBlock(Update&, const Document&);
    static std::unique_ptr<Style::ComputedStyle> restoreUsedDocumentElementStyleToComputed(const Style::ComputedStyle&);

#if ENABLE(TEXT_AUTOSIZING)
    struct AdjustmentForTextAutosizing {
        std::optional<float> newFontSize;
        std::optional<float> newLineHeight;
        std::optional<AutosizeStatus> newStatus;
        explicit operator bool() const { return newFontSize || newLineHeight || newStatus; }
    };
    static AdjustmentForTextAutosizing adjustmentForTextAutosizing(const Style::ComputedStyle&, const Element&);
    static bool adjustForTextAutosizing(Style::ComputedStyle&, AdjustmentForTextAutosizing);
    static bool adjustForTextAutosizing(Style::ComputedStyle&, const Element&);
#endif

private:
    void NODELETE adjustDisplayContentsStyle(Style::ComputedStyle&) const;
    void adjustForSiteSpecificQuirks(Style::ComputedStyle&) const;

    void adjustThemeStyle(Style::ComputedStyle&, const Style::ComputedStyle& parentStyle) const;

    static void adjustAnimations(Style::ComputedStyle&);
    static void adjustTransitions(Style::ComputedStyle&);
    static void adjustBackgroundLayers(Style::ComputedStyle&);
    static void adjustMaskLayers(Style::ComputedStyle&);
    static void adjustScrollTimelines(Style::ComputedStyle&);
    static void adjustViewTimelines(Style::ComputedStyle&);

    static OptionSet<EventListenerRegionType> computeEventListenerRegionTypes(const Document&, const Style::ComputedStyle&, const EventTarget&, OptionSet<EventListenerRegionType>);

    CheckedRef<const Document> m_document;
    const Style::ComputedStyle& m_parentStyle;
    const Style::ComputedStyle& m_parentBoxStyle;
    RefPtr<Element> m_element;
};

}
}
