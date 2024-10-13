/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "MediaQueryFeatures.h"

#include "Chrome.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MediaQueryEvaluator.h"
#include "Page.h"
#include "Quirks.h"
#include "RenderElementInlines.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScreenProperties.h"
#include "ScriptController.h"
#include "Settings.h"
#include "Theme.h"
#include <wtf/Function.h>

namespace WebCore::MQ::Features {

struct BooleanSchema : public FeatureSchema {
    using ValueFunction = Function<bool(const FeatureEvaluationContext&)>;

    BooleanSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Integer)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateBooleanFeature(feature, valueFunction(context));
    }

private:
    ValueFunction valueFunction;
};

struct IntegerSchema : public FeatureSchema {
    using ValueFunction = Function<int(const FeatureEvaluationContext&)>;

    IntegerSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Integer)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateIntegerFeature(feature, valueFunction(context));
    }

private:
    ValueFunction valueFunction;
};

struct NumberSchema : public FeatureSchema {
    using ValueFunction = Function<double(const FeatureEvaluationContext&)>;

    NumberSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Number)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateNumberFeature(feature, valueFunction(context));
    }

private:
    ValueFunction valueFunction;
};

struct LengthSchema : public FeatureSchema {
    using ValueFunction = Function<LayoutUnit(const FeatureEvaluationContext&)>;

    LengthSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateLengthFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct RatioSchema : public FeatureSchema {
    using ValueFunction = Function<FloatSize(const FeatureEvaluationContext&)>;

    RatioSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Ratio)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateRatioFeature(feature, valueFunction(context));
    }

private:
    ValueFunction valueFunction;
};

struct ResolutionSchema : public FeatureSchema {
    using ValueFunction = Function<float(const FeatureEvaluationContext&)>;

    ResolutionSchema(const AtomString& name, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Resolution)
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateResolutionFeature(feature, valueFunction(context));
    }

private:
    ValueFunction valueFunction;
};

using MatchingIdentifiers = Vector<CSSValueID, 1>;

struct IdentifierSchema : public FeatureSchema {
    using ValueFunction = Function<MatchingIdentifiers(const FeatureEvaluationContext&)>;

    IdentifierSchema(const AtomString& name, FixedVector<CSSValueID>&& valueIdentifiers, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Identifier, WTFMove(valueIdentifiers))
        , valueFunction(WTFMove(valueFunction))
    { }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        auto valueIDs = valueFunction(context);
        for (auto valueID : valueIDs) {
            ASSERT(valueIdentifiers.contains(valueID));
            if (evaluateIdentifierFeature(feature, valueID) == EvaluationResult::True)
                return EvaluationResult::True;
        }
        return EvaluationResult::False;
    }

private:
    ValueFunction valueFunction;
};

static float deviceScaleFactor(const FeatureEvaluationContext& context)
{
    auto& frame = *context.document->frame();
    auto mediaType = frame.view()->mediaType();
    
    if (mediaType == screenAtom())
        return frame.page() ? frame.page()->deviceScaleFactor() : 1;

    if (mediaType == printAtom()) {
        // The resolution of images while printing should not depend on the dpi
        // of the screen. Until we support proper ways of querying this info
        // we use 300px which is considered minimum for current printers.
        return 3.125; // 300dpi / 96dpi;
    }
    return 0;
}

const FeatureSchema& animation()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-animation"_s,
        [](auto&) { return true; }
    };
    return schema;
}

const FeatureSchema& anyHover()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "any-hover"_s,
        FixedVector { CSSValueNone, CSSValueHover },
        [](auto& context) {
            RefPtr page = context.document->frame()->page();
            bool isSupported = page && page->chrome().client().hoverSupportedByAnyAvailablePointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueHover : CSSValueNone };
        }
    };
    return schema;
}

const FeatureSchema& anyPointer()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "any-pointer"_s,
        FixedVector { CSSValueNone, CSSValueFine, CSSValueCoarse },
        [](auto& context) {
            RefPtr page = context.document->frame()->page();
            auto pointerCharacteristics = page ? page->chrome().client().pointerCharacteristicsOfAllAvailablePointingDevices() : OptionSet<PointerCharacteristics>();

            MatchingIdentifiers identifiers;
            if (pointerCharacteristics.contains(PointerCharacteristics::Fine))
                identifiers.append(CSSValueFine);
            if (pointerCharacteristics.contains(PointerCharacteristics::Coarse))
                identifiers.append(CSSValueCoarse);
            if (identifiers.isEmpty())
                identifiers.append(CSSValueNone);
            return identifiers;
        }

    };
    return schema;
}

const FeatureSchema& aspectRatio()
{
    static MainThreadNeverDestroyed<RatioSchema> schema {
        "aspect-ratio"_s,
        [](auto& context) {
            auto& view = *context.document->view();
            return FloatSize(view.layoutWidth(), view.layoutHeight());
        }
    };
    return schema;
}

const FeatureSchema& color()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "color"_s,
        [](auto& context) {
            return screenDepthPerComponent(context.document->frame()->mainFrame().protectedVirtualView().get());
        }
    };
    return schema;
}

const FeatureSchema& colorGamut()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "color-gamut"_s,
        FixedVector { CSSValueSRGB, CSSValueP3, CSSValueRec2020 },
        [](auto& context) {
            // FIXME: At some point we should start detecting displays that support more colors.
            MatchingIdentifiers identifiers { CSSValueSRGB };
#if HAVE(IOSURFACE_RGB10)
            if (screenContentsFormat(context.document->protectedFrame()->mainFrame().protectedVirtualView().get()) == ContentsFormat::RGBA10)
                identifiers.append(CSSValueP3);
#else
            UNUSED_PARAM(context);
#endif
            return identifiers;
        }
    };
    return schema;
}

const FeatureSchema& colorIndex()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "color-index"_s,
        [](auto&) { return 0; }
    };
    return schema;
}

const FeatureSchema& deviceAspectRatio()
{
    static MainThreadNeverDestroyed<RatioSchema> schema {
        "device-aspect-ratio"_s,
        [](auto& context) {
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(context.document->frame()->mainFrame())) {
                auto screenSize = localFrame->screenSize();
                return FloatSize { screenSize.width(), screenSize.height() };
            }
            return FloatSize { 0.0f, 0.0f };
        }
    };
    return schema;
}

const FeatureSchema& deviceHeight()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "device-height"_s,
        [](auto& context) {
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(context.document->frame()->mainFrame()))
                return LayoutUnit { localFrame->screenSize().height() };
            return LayoutUnit { 0.0f };
        }
    };
    return schema;
}

const FeatureSchema& devicePixelRatio()
{
    static MainThreadNeverDestroyed<NumberSchema> schema {
        "-webkit-device-pixel-ratio"_s,
        [](auto& context) {
            return deviceScaleFactor(context);
        }
    };
    return schema;
}

const FeatureSchema& deviceWidth()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "device-width"_s,
        [](auto& context) {
            if (RefPtr localFrame = dynamicDowncast<LocalFrame>(context.document->frame()->mainFrame()))
                return LayoutUnit { localFrame->screenSize().width() };
            return LayoutUnit { 0.0f };
        }
    };
    return schema;
}

const FeatureSchema& dynamicRange()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "dynamic-range"_s,
        FixedVector { CSSValueStandard, CSSValueHigh },
        [](auto& context) {
            bool supportsHighDynamicRange = [&] {
                Ref frame = *context.document->frame();
                if (frame->settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::Off)
                    return false;
                return screenSupportsHighDynamicRange(frame->mainFrame().protectedVirtualView().get());
            }();

            MatchingIdentifiers identifiers { CSSValueStandard };
            if (supportsHighDynamicRange)
                identifiers.append(CSSValueHigh);
            return identifiers;
        }
    };
    return schema;
}

const FeatureSchema& forcedColors()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "forced-colors"_s,
        FixedVector { CSSValueNone, CSSValueActive },
        [](auto&) {
            return MatchingIdentifiers { CSSValueNone };
        }
    };
    return schema;
}

const FeatureSchema& grid()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "grid"_s,
        [](auto&) { return false; }
    };
    return schema;
}

const FeatureSchema& height()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "height"_s,
        [](auto& context) {
            auto height = context.document->protectedView()->layoutHeight();
            if (CheckedPtr renderView = context.document->renderView())
                height = adjustForAbsoluteZoom(height, *renderView);
            return height;
        }
    };
    return schema;
}

const FeatureSchema& hover()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "hover"_s,
        FixedVector { CSSValueNone, CSSValueHover },
        [](auto& context) {
            RefPtr page = context.document->frame()->page();
            bool isSupported =  page && page->chrome().client().hoverSupportedByPrimaryPointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueHover : CSSValueNone };
        }
    };
    return schema;
}

const FeatureSchema& invertedColors()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "inverted-colors"_s,
        FixedVector { CSSValueNone, CSSValueInverted },
        [](auto& context) {
            bool isInverted = [&] {
                Ref frame = *context.document->frame();
                if (frame->settings().forcedColorsAreInvertedAccessibilityValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedColorsAreInvertedAccessibilityValue() == ForcedAccessibilityValue::Off)
                    return false;
                return screenHasInvertedColors();
            }();

            return MatchingIdentifiers { isInverted ? CSSValueInverted : CSSValueNone };
        }
    };
    return schema;
}

const FeatureSchema& monochrome()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "monochrome"_s,
        [](auto& context) {
            Ref frame = *context.document->frame();
            RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
            bool isMonochrome = [&] {
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::Off)
                    return false;
                if (localFrame)
                    return screenIsMonochrome(localFrame->protectedView().get());
                return false;
            }();

            return isMonochrome && localFrame ? screenDepthPerComponent(localFrame->protectedView().get()) : 0;
        }
    };
    return schema;
}

const FeatureSchema& orientation()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "orientation"_s,
        FixedVector { CSSValueLandscape, CSSValuePortrait },
        [](auto& context) {
            if (context.document->quirks().shouldPreventOrientationMediaQueryFromEvaluatingToLandscape())
                return MatchingIdentifiers { CSSValuePortrait };

            Ref view = *context.document->view();
            // Square viewport is portrait.
            bool isPortrait = view->layoutHeight() >= view->layoutWidth();
            return MatchingIdentifiers { isPortrait ? CSSValuePortrait : CSSValueLandscape };
        }
    };
    return schema;
}

const FeatureSchema& pointer()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "pointer"_s,
        FixedVector { CSSValueNone, CSSValueFine, CSSValueCoarse },
        [](auto& context) {
            RefPtr page = context.document->frame()->page();
            auto pointerCharacteristics = page ? page->chrome().client().pointerCharacteristicsOfPrimaryPointingDevice() : OptionSet<PointerCharacteristics>();
            MatchingIdentifiers identifiers;
            if (pointerCharacteristics.contains(PointerCharacteristics::Fine))
                identifiers.append(CSSValueFine);
            if (pointerCharacteristics.contains(PointerCharacteristics::Coarse) && !context.document->quirks().shouldHideCoarsePointerCharacteristics())
                identifiers.append(CSSValueCoarse);
            if (identifiers.isEmpty())
                identifiers.append(CSSValueNone);
            return identifiers;
        }

    };
    return schema;
}

const FeatureSchema& prefersContrast()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-contrast"_s,
        FixedVector { CSSValueNoPreference, CSSValueMore, CSSValueLess, CSSValueCustom },
        [](auto& context) {
            bool userPrefersContrast = [&] {
                Ref frame = *context.document->frame();
                switch (frame->settings().forcedPrefersContrastAccessibilityValue()) {
                case ForcedAccessibilityValue::On:
                    return true;
                case ForcedAccessibilityValue::Off:
                    return false;
                case ForcedAccessibilityValue::System:
                    return Theme::singleton().userPrefersContrast();
                }
                return false;
            }();

            return MatchingIdentifiers { userPrefersContrast ? CSSValueMore : CSSValueNoPreference };
        }
    };
    return schema;
}

const FeatureSchema& prefersDarkInterface()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-dark-interface"_s,
        FixedVector { CSSValueNoPreference, CSSValuePrefers },
        [](auto& context) {
            Ref page = *context.document->frame()->page();
            bool prefersDarkInterface = page->useSystemAppearance() && page->useDarkAppearance();

            return MatchingIdentifiers { prefersDarkInterface ? CSSValuePrefers : CSSValueNoPreference };
        }
    };
    return schema;
}

const FeatureSchema& prefersReducedMotion()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-reduced-motion"_s,
        FixedVector { CSSValueNoPreference, CSSValueReduce },
        [](auto& context) {
            bool userPrefersReducedMotion = [&] {
                Ref frame = *context.document->frame();
                switch (frame->settings().forcedPrefersReducedMotionAccessibilityValue()) {
                case ForcedAccessibilityValue::On:
                    return true;
                case ForcedAccessibilityValue::Off:
                    return false;
                case ForcedAccessibilityValue::System:
                    return Theme::singleton().userPrefersReducedMotion();
                }
                return false;
            }();

            return MatchingIdentifiers { userPrefersReducedMotion ? CSSValueReduce : CSSValueNoPreference };
        }
    };
    return schema;
}

const FeatureSchema& resolution()
{
    static MainThreadNeverDestroyed<ResolutionSchema> schema {
        "resolution"_s,
        [](auto& context) {
            return deviceScaleFactor(context);
        }
    };
    return schema;
}

const FeatureSchema& scan()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "scan"_s,
        FixedVector { CSSValueInterlace, CSSValueProgressive },
        [](auto&) {
            return MatchingIdentifiers { };
        }
    };
    return schema;
}

const FeatureSchema& scripting()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "scripting"_s,
        FixedVector { CSSValueNone, CSSValueInitialOnly, CSSValueEnabled },
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (!frame->checkedScript()->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
                return MatchingIdentifiers { CSSValueNone };
            return MatchingIdentifiers { CSSValueEnabled };
        }
    };
    return schema;
}

const FeatureSchema& transform2d()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transform-2d"_s,
        [](auto&) { return true; }
    };
    return schema;
}

const FeatureSchema& transform3d()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transform-3d"_s,
        [](auto& context) {
            CheckedPtr view = context.document->renderView();
            return view && view->compositor().canRender3DTransforms();
        }
    };
    return schema;
}

const FeatureSchema& transition()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transition"_s,
        [](auto&) { return true; }
    };
    return schema;
}

const FeatureSchema& update()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "update"_s,
        FixedVector { CSSValueNone, CSSValueSlow, CSSValueFast },
        [](auto& context) {
            RefPtr frameView = context.document->frame()->view();
            if (frameView && frameView->mediaType() == printAtom())
                return MatchingIdentifiers { CSSValueNone };

            // FIXME: Potentially add a hook for ports to change this value.
            return MatchingIdentifiers { CSSValueFast };
        }
    };
    return schema;
}

const FeatureSchema& videoPlayableInline()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-video-playable-inline"_s,
        [](auto& context) {
            return context.document->frame()->settings().allowsInlineMediaPlayback();
        }
    };
    return schema;
}

const FeatureSchema& width()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "width"_s,
        [](auto& context) {
            auto width = context.document->protectedView()->layoutWidth();
            if (CheckedPtr renderView = context.document->renderView())
                width = adjustForAbsoluteZoom(width, *renderView);
            return width;
        }
    };
    return schema;
}

#if ENABLE(APPLICATION_MANIFEST)
const FeatureSchema& displayMode()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "display-mode"_s,
        FixedVector { CSSValueFullscreen, CSSValueStandalone, CSSValueMinimalUi, CSSValueBrowser },
        [](auto& context) {
            auto identifier = [&] {
                Ref frame = *context.document->frame();
                auto manifest = frame->page() ? frame->page()->applicationManifest() : std::nullopt;
                if (!manifest)
                    return CSSValueBrowser;

                switch (manifest->display) {
                case ApplicationManifest::Display::Fullscreen:
                    return CSSValueFullscreen;
                case ApplicationManifest::Display::Standalone:
                    return CSSValueStandalone;
                case ApplicationManifest::Display::MinimalUI:
                    return CSSValueMinimalUi;
                case ApplicationManifest::Display::Browser:
                    return CSSValueBrowser;
                }
                ASSERT_NOT_REACHED();
                return CSSValueBrowser;
            }();

            return MatchingIdentifiers { identifier };
        }
    };
    return schema;
}
#endif

const FeatureSchema& overflowBlock()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "overflow-block"_s,
        FixedVector { CSSValueNone, CSSValueScroll, CSSValuePaged },
        [](auto& context) {
            // FIXME: Match none when scrollEnabled is set to false by UIKit.
            bool matchesPaged = [&] {
                RefPtr frameView = context.document->frame()->view();
                if (!frameView)
                    return false;
                return frameView->mediaType() == printAtom() || frameView->pagination().mode != PaginationMode::Unpaginated;
            }();
            return MatchingIdentifiers { matchesPaged ? CSSValuePaged : CSSValueScroll };
        }
    };
    return schema;
}

const FeatureSchema& overflowInline()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "overflow-inline"_s,
        FixedVector { CSSValueNone, CSSValueScroll },
        [](auto&) {
            // FIXME: Match none when scrollEnabled is set to false by UIKit.
            return MatchingIdentifiers { CSSValueScroll };
        }
    };
    return schema;
}

#if ENABLE(DARK_MODE_CSS)
const FeatureSchema& prefersColorScheme()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-color-scheme"_s,
        FixedVector { CSSValueLight, CSSValueDark },
        [](auto& context) {
            Ref page = *context.document->frame()->page();
            bool useDarkAppearance = page->useDarkAppearance();

            return MatchingIdentifiers { useDarkAppearance ? CSSValueDark : CSSValueLight };
        }
    };
    return schema;
}
#endif

Vector<const FeatureSchema*> allSchemas()
{
    return {
        &animation(),
        &anyHover(),
        &anyPointer(),
        &aspectRatio(),
        &color(),
        &colorGamut(),
        &colorIndex(),
        &deviceAspectRatio(),
        &deviceHeight(),
        &devicePixelRatio(),
        &deviceWidth(),
        &dynamicRange(),
        &forcedColors(),
        &grid(),
        &height(),
        &hover(),
        &invertedColors(),
        &monochrome(),
        &overflowBlock(),
        &overflowInline(),
        &orientation(),
        &pointer(),
        &prefersContrast(),
        &prefersDarkInterface(),
        &prefersReducedMotion(),
        &resolution(),
        &scan(),
        &scripting(),
        &transform2d(),
        &transform3d(),
        &transition(),
        &update(),
        &videoPlayableInline(),
        &width(),
#if ENABLE(APPLICATION_MANIFEST)
        &displayMode(),
#endif
#if ENABLE(DARK_MODE_CSS)
        &prefersColorScheme(),
#endif
    };
}

// FIXME: This could be part of the schema.
std::optional<MediaQueryDynamicDependency> dynamicDependency(const FeatureSchema& schema)
{
    if (&schema == &width() || &schema == &height() || &schema == &orientation() || &schema == &aspectRatio())
        return MediaQueryDynamicDependency::Viewport;

    if (&schema == &prefersDarkInterface())
        return MediaQueryDynamicDependency::Appearance;
#if ENABLE(DARK_MODE_CSS)
    if (&schema == &prefersColorScheme())
        return MediaQueryDynamicDependency::Appearance;
#endif

    if (&schema == &invertedColors() || &schema == &monochrome() || &schema == &prefersReducedMotion() || &schema == &prefersContrast())
        return MediaQueryDynamicDependency::Accessibility;

    return { };
}

}
