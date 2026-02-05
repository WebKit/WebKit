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

#include "CSSPrimitiveNumericCategory.h"
#include "Chrome.h"
#include "ComputedStyleDependencies.h"
#include "DocumentLoader.h"
#include "DocumentPage.h"
#include "DocumentQuirks.h"
#include "DocumentView.h"
#include "FrameDestructionObserverInlines.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "MediaQueryEvaluator.h"
#include "RenderElementStyleInlines.h"
#include "RenderLayerCompositor.h"
#include "RenderView.h"
#include "ScreenProperties.h"
#include "ScriptController.h"
#include "Settings.h"
#include "Theme.h"
#include <wtf/Function.h>

namespace WebCore::MQ {
namespace Features {

struct BooleanSchema : public FeatureSchema {
    using ValueFunction = Function<bool(const FeatureEvaluationContext&)>;

    BooleanSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Integer, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateBooleanFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct IntegerSchema : public FeatureSchema {
    using ValueFunction = Function<int(const FeatureEvaluationContext&)>;

    IntegerSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Integer, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateIntegerFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct NumberSchema : public FeatureSchema {
    using ValueFunction = Function<double(const FeatureEvaluationContext&)>;

    NumberSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Number, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateNumberFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct LengthSchema : public FeatureSchema {
    using ValueFunction = Function<LayoutUnit(const FeatureEvaluationContext&)>;

    LengthSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Length, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateLengthFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct RatioSchema : public FeatureSchema {
    using ValueFunction = Function<FloatSize(const FeatureEvaluationContext&)>;

    RatioSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Ratio, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateRatioFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

struct ResolutionSchema : public FeatureSchema {
    using ValueFunction = Function<float(const FeatureEvaluationContext&)>;

    ResolutionSchema(const AtomString& name, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Range, FeatureSchema::ValueType::Resolution, dependencies)
        , valueFunction(WTF::move(valueFunction))
    {
    }

    // FeatureSchema conformance

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        return evaluateResolutionFeature(feature, valueFunction(context), context.conversionData);
    }

private:
    ValueFunction valueFunction;
};

using MatchingIdentifiers = Vector<CSSValueID, 1>;

struct IdentifierSchema : public FeatureSchema {
    using ValueFunction = Function<MatchingIdentifiers(const FeatureEvaluationContext&)>;

    IdentifierSchema(const AtomString& name, FixedVector<CSSValueID>&& valueIdentifiers, OptionSet<MediaQueryDynamicDependency> dependencies, ValueFunction&& valueFunction)
        : FeatureSchema(name, FeatureSchema::Type::Discrete, FeatureSchema::ValueType::Identifier, dependencies, WTF::move(valueIdentifiers))
        , valueFunction(WTF::move(valueFunction))
    {
    }

    EvaluationResult evaluate(const Feature& feature, const FeatureEvaluationContext& context) const override
    {
        auto valueIDs = valueFunction(context);
        for (auto valueID : valueIDs) {
            ASSERT(valueIdentifiers.contains(valueID));
            if (evaluateIdentifierFeature(feature, valueID, context.conversionData) == EvaluationResult::True)
                return EvaluationResult::True;
        }
        return EvaluationResult::False;
    }

private:
    ValueFunction valueFunction;
};

static float deviceScaleFactor(const FeatureEvaluationContext& context)
{
    Ref frame = *context.document->frame();
    auto mediaType = protect(frame->view())->mediaType();
    
    if (mediaType == screenAtom())
        return frame->page() ? frame->page()->deviceScaleFactor() : 1;

    if (mediaType == printAtom()) {
        // The resolution of images while printing should not depend on the dpi
        // of the screen. Until we support proper ways of querying this info
        // we use 300px which is considered minimum for current printers.
        return 3.125; // 300dpi / 96dpi;
    }
    return 0;
}

// MARK: - Singleton readonly instances of FeatureSchemas

static const BooleanSchema& animationFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-animation"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) { return true; }
    };
    return schema;
}

static const IdentifierSchema& anyHoverFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "any-hover"_s,
        FixedVector { CSSValueNone, CSSValueHover },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (context.document->quirks().shouldSupportHoverMediaQueries())
                return MatchingIdentifiers { CSSValueHover };
            RefPtr page = context.document->frame()->page();
            bool isSupported = page && page->chrome().client().hoverSupportedByAnyAvailablePointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueHover : CSSValueNone };
        }
    };
    return schema;
}

static const IdentifierSchema& anyPointerFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "any-pointer"_s,
        FixedVector { CSSValueNone, CSSValueFine, CSSValueCoarse },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const RatioSchema& aspectRatioFeatureSchema()
{
    static MainThreadNeverDestroyed<RatioSchema> schema {
        "aspect-ratio"_s,
        MediaQueryDynamicDependency::Viewport,
        [](auto& context) {
            auto& view = *context.document->view();
            return FloatSize(view.layoutWidth(), view.layoutHeight());
        }
    };
    return schema;
}

static const IntegerSchema& colorFeatureSchema()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "color"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            return screenDepthPerComponent(context.document->frame()->mainFrame().protectedVirtualView().get());
        }
    };
    return schema;
}

static const IdentifierSchema& colorGamutFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "color-gamut"_s,
        FixedVector { CSSValueSRGB, CSSValueP3, CSSValueRec2020 },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            // FIXME: At some point we should start detecting displays that support more colors.
            MatchingIdentifiers identifiers { CSSValueSRGB };
            if (screenSupportsExtendedColor(context.document->protectedFrame()->mainFrame().protectedVirtualView().get()))
                identifiers.append(CSSValueP3);
            return identifiers;
        }
    };
    return schema;
}

static const IntegerSchema& colorIndexFeatureSchema()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "color-index"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) { return 0; }
    };
    return schema;
}

static const RatioSchema& deviceAspectRatioFeatureSchema()
{
    static MainThreadNeverDestroyed<RatioSchema> schema {
        "device-aspect-ratio"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (RefPtr localFrame = context.document->frame()->localMainFrame()) {
                auto screenSize = localFrame->screenSize();
                return FloatSize { screenSize.width(), screenSize.height() };
            }
            return FloatSize { 0.0f, 0.0f };
        }
    };
    return schema;
}

static const LengthSchema& deviceHeightFeatureSchema()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "device-height"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (RefPtr localFrame = context.document->frame()->localMainFrame())
                return LayoutUnit { localFrame->screenSize().height() };
            return LayoutUnit { 0.0f };
        }
    };
    return schema;
}

static const NumberSchema& devicePixelRatioFeatureSchema()
{
    static MainThreadNeverDestroyed<NumberSchema> schema {
        "-webkit-device-pixel-ratio"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            return deviceScaleFactor(context);
        }
    };
    return schema;
}

static const LengthSchema& deviceWidthFeatureSchema()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "device-width"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (RefPtr localFrame = context.document->frame()->localMainFrame())
                return LayoutUnit { localFrame->screenSize().width() };
            return LayoutUnit { 0.0f };
        }
    };
    return schema;
}

static const IdentifierSchema& dynamicRangeFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "dynamic-range"_s,
        FixedVector { CSSValueStandard, CSSValueHigh },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const IdentifierSchema& forcedColorsFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "forced-colors"_s,
        FixedVector { CSSValueNone, CSSValueActive },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) {
            return MatchingIdentifiers { CSSValueNone };
        }
    };
    return schema;
}

static const BooleanSchema& gridFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "grid"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) { return false; }
    };
    return schema;
}

static const LengthSchema& heightFeatureSchema()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "height"_s,
        MediaQueryDynamicDependency::Viewport,
        [](auto& context) {
            auto height = protect(context.document->view())->layoutHeight();
            if (CheckedPtr renderView = context.document->renderView())
                height = adjustForAbsoluteZoom(height, *renderView);
            return height;
        }
    };
    return schema;
}

static const IdentifierSchema& hoverFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "hover"_s,
        FixedVector { CSSValueNone, CSSValueHover },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (context.document->quirks().shouldSupportHoverMediaQueries())
                return MatchingIdentifiers { CSSValueHover };
            RefPtr page = context.document->frame()->page();
            bool isSupported =  page && page->chrome().client().hoverSupportedByPrimaryPointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueHover : CSSValueNone };
        }
    };
    return schema;
}

static const IdentifierSchema& invertedColorsFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "inverted-colors"_s,
        FixedVector { CSSValueNone, CSSValueInverted },
        MediaQueryDynamicDependency::Accessibility,
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

static const IntegerSchema& monochromeFeatureSchema()
{
    static MainThreadNeverDestroyed<IntegerSchema> schema {
        "monochrome"_s,
        MediaQueryDynamicDependency::Accessibility,
        [](auto& context) {
            Ref frame = *context.document->frame();
            RefPtr localFrame = context.document->localMainFrame();
            bool isMonochrome = [&] {
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::Off)
                    return false;
                if (localFrame)
                    return screenIsMonochrome(protect(localFrame->view()).get());
                return false;
            }();

            return isMonochrome && localFrame ? screenDepthPerComponent(protect(localFrame->view()).get()) : 0;
        }
    };
    return schema;
}

static const IdentifierSchema& orientationFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "orientation"_s,
        FixedVector { CSSValueLandscape, CSSValuePortrait },
        MediaQueryDynamicDependency::Viewport,
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

static const IdentifierSchema& pointerFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "pointer"_s,
        FixedVector { CSSValueNone, CSSValueFine, CSSValueCoarse },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const IdentifierSchema& prefersContrastFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-contrast"_s,
        FixedVector { CSSValueNoPreference, CSSValueMore, CSSValueLess, CSSValueCustom },
        MediaQueryDynamicDependency::Accessibility,
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

static const IdentifierSchema& prefersDarkInterfaceFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-dark-interface"_s,
        FixedVector { CSSValueNoPreference, CSSValuePrefers },
        MediaQueryDynamicDependency::Appearance,
        [](auto& context) {
            Ref page = *context.document->frame()->page();
            bool prefersDarkInterface = page->settings().useSystemAppearance() && page->useDarkAppearance();

            return MatchingIdentifiers { prefersDarkInterface ? CSSValuePrefers : CSSValueNoPreference };
        }
    };
    return schema;
}

static const IdentifierSchema& prefersReducedMotionFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-reduced-motion"_s,
        FixedVector { CSSValueNoPreference, CSSValueReduce },
        MediaQueryDynamicDependency::Accessibility,
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

static const ResolutionSchema& resolutionFeatureSchema()
{
    static MainThreadNeverDestroyed<ResolutionSchema> schema {
        "resolution"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            return deviceScaleFactor(context);
        }
    };
    return schema;
}

static const IdentifierSchema& scanFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "scan"_s,
        FixedVector { CSSValueInterlace, CSSValueProgressive },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) {
            return MatchingIdentifiers { };
        }
    };
    return schema;
}

static const IdentifierSchema& scriptingFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "scripting"_s,
        FixedVector { CSSValueNone, CSSValueInitialOnly, CSSValueEnabled },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (!frame->checkedScript()->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
                return MatchingIdentifiers { CSSValueNone };
            return MatchingIdentifiers { CSSValueEnabled };
        }
    };
    return schema;
}

static const BooleanSchema& transform2dFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transform-2d"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) { return true; }
    };
    return schema;
}

static const BooleanSchema& transform3dFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transform-3d"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            CheckedPtr view = context.document->renderView();
            return view && view->compositor().canRender3DTransforms();
        }
    };
    return schema;
}

static const BooleanSchema& transitionFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-transition"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) { return true; }
    };
    return schema;
}

static const IdentifierSchema& updateFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "update"_s,
        FixedVector { CSSValueNone, CSSValueSlow, CSSValueFast },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const BooleanSchema& videoPlayableInlineFeatureSchema()
{
    static MainThreadNeverDestroyed<BooleanSchema> schema {
        "-webkit-video-playable-inline"_s,
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            return context.document->frame()->settings().allowsInlineMediaPlayback();
        }
    };
    return schema;
}

static const LengthSchema& widthFeatureSchema()
{
    static MainThreadNeverDestroyed<LengthSchema> schema {
        "width"_s,
        MediaQueryDynamicDependency::Viewport,
        [](auto& context) {
            auto width = protect(context.document->view())->layoutWidth();
            if (CheckedPtr renderView = context.document->renderView())
                width = adjustForAbsoluteZoom(width, *renderView);
            return width;
        }
    };
    return schema;
}

#if ENABLE(APPLICATION_MANIFEST)
static const IdentifierSchema& displayModeFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "display-mode"_s,
        FixedVector { CSSValueFullscreen, CSSValueStandalone, CSSValueMinimalUi, CSSValueBrowser },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const IdentifierSchema& overflowBlockFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "overflow-block"_s,
        FixedVector { CSSValueNone, CSSValueScroll, CSSValuePaged },
        OptionSet<MediaQueryDynamicDependency>(),
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

static const IdentifierSchema& overflowInlineFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "overflow-inline"_s,
        FixedVector { CSSValueNone, CSSValueScroll },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) {
            // FIXME: Match none when scrollEnabled is set to false by UIKit.
            return MatchingIdentifiers { CSSValueScroll };
        }
    };
    return schema;
}

#if ENABLE(DARK_MODE_CSS)
static const IdentifierSchema& prefersColorSchemeFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-color-scheme"_s,
        FixedVector { CSSValueLight, CSSValueDark },
        MediaQueryDynamicDependency::Appearance,
        [](auto& context) {
            Ref page = *context.document->frame()->page();
            bool useDarkAppearance = page->useDarkAppearance();

            return MatchingIdentifiers { useDarkAppearance ? CSSValueDark : CSSValueLight };
        }
    };
    return schema;
}
#endif

// MARK: - Type erased exposed schemas

const FeatureSchema& animation()
{
    return animationFeatureSchema();
}

const FeatureSchema& anyHover()
{
    return anyHoverFeatureSchema();
}

const FeatureSchema& anyPointer()
{
    return anyPointerFeatureSchema();
}

const FeatureSchema& aspectRatio()
{
    return aspectRatioFeatureSchema();
}

const FeatureSchema& color()
{
    return colorFeatureSchema();
}

const FeatureSchema& colorGamut()
{
    return colorGamutFeatureSchema();
}

const FeatureSchema& colorIndex()
{
    return colorIndexFeatureSchema();
}

const FeatureSchema& deviceAspectRatio()
{
    return deviceAspectRatioFeatureSchema();
}

const FeatureSchema& deviceHeight()
{
    return deviceHeightFeatureSchema();
}

const FeatureSchema& devicePixelRatio()
{
    return devicePixelRatioFeatureSchema();
}

const FeatureSchema& deviceWidth()
{
    return deviceWidthFeatureSchema();
}

const FeatureSchema& dynamicRange()
{
    return dynamicRangeFeatureSchema();
}

const FeatureSchema& forcedColors()
{
    return forcedColorsFeatureSchema();
}

const FeatureSchema& grid()
{
    return gridFeatureSchema();
}

const FeatureSchema& height()
{
    return heightFeatureSchema();
}

const FeatureSchema& hover()
{
    return hoverFeatureSchema();
}

const FeatureSchema& invertedColors()
{
    return invertedColorsFeatureSchema();
}

const FeatureSchema& monochrome()
{
    return monochromeFeatureSchema();
}

const FeatureSchema& orientation()
{
    return orientationFeatureSchema();
}

const FeatureSchema& pointer()
{
    return pointerFeatureSchema();
}

const FeatureSchema& prefersContrast()
{
    return prefersContrastFeatureSchema();
}

const FeatureSchema& prefersDarkInterface()
{
    return prefersDarkInterfaceFeatureSchema();
}

const FeatureSchema& prefersReducedMotion()
{
    return prefersReducedMotionFeatureSchema();
}

const FeatureSchema& resolution()
{
    return resolutionFeatureSchema();
}

const FeatureSchema& scan()
{
    return scanFeatureSchema();
}

const FeatureSchema& scripting()
{
    return scriptingFeatureSchema();
}

const FeatureSchema& transform2d()
{
    return transform2dFeatureSchema();
}

const FeatureSchema& transform3d()
{
    return transform3dFeatureSchema();
}

const FeatureSchema& transition()
{
    return transitionFeatureSchema();
}

const FeatureSchema& update()
{
    return updateFeatureSchema();
}

const FeatureSchema& videoPlayableInline()
{
    return videoPlayableInlineFeatureSchema();
}

const FeatureSchema& width()
{
    return widthFeatureSchema();
}

#if ENABLE(APPLICATION_MANIFEST)
const FeatureSchema& displayMode()
{
    return displayModeFeatureSchema();
}
#endif

const FeatureSchema& overflowBlock()
{
    return overflowBlockFeatureSchema();
}

const FeatureSchema& overflowInline()
{
    return overflowInlineFeatureSchema();
}

#if ENABLE(DARK_MODE_CSS)
const FeatureSchema& prefersColorScheme()
{
    return prefersColorSchemeFeatureSchema();
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

} // namespace Features
} // namespace WebCore::MQ
