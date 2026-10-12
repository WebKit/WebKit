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
#include "StyleZoomPrimitivesInlines.h"
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
        FixedVector { CSSValueID::None, CSSValueID::Hover },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (context.document->quirks().shouldSupportHoverMediaQueries() || frame->settings().shouldReportDesktopClassPointingDevice())
                return MatchingIdentifiers { CSSValueID::Hover };
            RefPtr page = frame->page();
            bool isSupported = page && page->chrome().client().hoverSupportedByAnyAvailablePointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueID::Hover : CSSValueID::None };
        }
    };
    return schema;
}

static const IdentifierSchema& anyPointerFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "any-pointer"_s,
        FixedVector { CSSValueID::None, CSSValueID::Fine, CSSValueID::Coarse },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (frame->settings().shouldReportDesktopClassPointingDevice())
                return MatchingIdentifiers { CSSValueID::Fine };

            RefPtr page = frame->page();
            auto pointerCharacteristics = page ? page->chrome().client().pointerCharacteristicsOfAllAvailablePointingDevices() : OptionSet<PointerCharacteristics>();

            MatchingIdentifiers identifiers;
            if (pointerCharacteristics.contains(PointerCharacteristics::Fine))
                identifiers.append(CSSValueID::Fine);
            if (pointerCharacteristics.contains(PointerCharacteristics::Coarse))
                identifiers.append(CSSValueID::Coarse);
            if (identifiers.isEmpty())
                identifiers.append(CSSValueID::None);
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
            Ref view = *context.document->view();
            return FloatSize(view->layoutWidth(), view->layoutHeight());
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
            return screenDepthPerComponent(protect(context.document->frame()->mainFrame().virtualView()).get());
        }
    };
    return schema;
}

static const IdentifierSchema& colorGamutFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "color-gamut"_s,
        FixedVector { CSSValueID::SRGB, CSSValueID::P3, CSSValueID::Rec2020 },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            // FIXME: At some point we should start detecting displays that support more colors.
            MatchingIdentifiers identifiers { CSSValueID::SRGB };
            if (screenSupportsExtendedColor(protect(protect(context.document->frame())->mainFrame().virtualView()).get()))
                identifiers.append(CSSValueID::P3);
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
            if (RefPtr frame = context.document->frame()) {
                auto screenSize = frame->screenSize();
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
            if (RefPtr frame = context.document->frame())
                return LayoutUnit { frame->screenSize().height() };
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

static const IdentifierSchema& devicePostureFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "device-posture"_s,
        FixedVector { CSSValueID::Continuous, CSSValueID::Folded },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            if (!context.document->settings().devicePostureAPIEnabled())
                return MatchingIdentifiers { };

            RefPtr page = context.document->frame()->page();
            bool continuous = !page || (page->chrome().client().devicePostureType() == DevicePostureType::Continuous);
            return MatchingIdentifiers { continuous ? CSSValueID::Continuous : CSSValueID::Folded };
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
            if (RefPtr frame = context.document->frame())
                return LayoutUnit { frame->screenSize().width() };
            return LayoutUnit { 0.0f };
        }
    };
    return schema;
}

static const IdentifierSchema& dynamicRangeFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "dynamic-range"_s,
        FixedVector { CSSValueID::Standard, CSSValueID::High },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            bool supportsHighDynamicRange = [&] {
                Ref frame = *context.document->frame();
                if (frame->settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::Off)
                    return false;
                return screenSupportsHighDynamicRange(protect(frame->mainFrame().virtualView()).get());
            }();

            MatchingIdentifiers identifiers { CSSValueID::Standard };
            if (supportsHighDynamicRange)
                identifiers.append(CSSValueID::High);
            return identifiers;
        }
    };
    return schema;
}

static const IdentifierSchema& forcedColorsFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "forced-colors"_s,
        FixedVector { CSSValueID::None, CSSValueID::Active },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) {
            return MatchingIdentifiers { CSSValueID::None };
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
                height = Style::unapplyingZoom<int>(height, *renderView);
            return height;
        }
    };
    return schema;
}

static const IdentifierSchema& hoverFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "hover"_s,
        FixedVector { CSSValueID::None, CSSValueID::Hover },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (context.document->quirks().shouldSupportHoverMediaQueries() || frame->settings().shouldReportDesktopClassPointingDevice())
                return MatchingIdentifiers { CSSValueID::Hover };
            RefPtr page = frame->page();
            bool isSupported =  page && page->chrome().client().hoverSupportedByPrimaryPointingDevice();
            return MatchingIdentifiers { isSupported ? CSSValueID::Hover : CSSValueID::None };
        }
    };
    return schema;
}

static const IdentifierSchema& invertedColorsFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "inverted-colors"_s,
        FixedVector { CSSValueID::None, CSSValueID::Inverted },
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

            return MatchingIdentifiers { isInverted ? CSSValueID::Inverted : CSSValueID::None };
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
            RefPtr mainFrameView = frame->mainFrame().virtualView();
            bool isMonochrome = [&] {
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::On)
                    return true;
                if (frame->settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::Off)
                    return false;
                return screenIsMonochrome(mainFrameView.get());
            }();

            return isMonochrome ? screenDepthPerComponent(mainFrameView.get()) : 0;
        }
    };
    return schema;
}

static const IdentifierSchema& orientationFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "orientation"_s,
        FixedVector { CSSValueID::Landscape, CSSValueID::Portrait },
        MediaQueryDynamicDependency::Viewport,
        [](auto& context) {
            if (context.document->quirks().shouldPreventOrientationMediaQueryFromEvaluatingToLandscape())
                return MatchingIdentifiers { CSSValueID::Portrait };

            Ref view = *context.document->view();
            // Square viewport is portrait.
            bool isPortrait = view->layoutHeight() >= view->layoutWidth();
            return MatchingIdentifiers { isPortrait ? CSSValueID::Portrait : CSSValueID::Landscape };
        }
    };
    return schema;
}

static const IdentifierSchema& pointerFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "pointer"_s,
        FixedVector { CSSValueID::None, CSSValueID::Fine, CSSValueID::Coarse },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (frame->settings().shouldReportDesktopClassPointingDevice())
                return MatchingIdentifiers { CSSValueID::Fine };

            RefPtr page = frame->page();
            auto pointerCharacteristics = page ? page->chrome().client().pointerCharacteristicsOfPrimaryPointingDevice() : OptionSet<PointerCharacteristics>();
            MatchingIdentifiers identifiers;
            if (pointerCharacteristics.contains(PointerCharacteristics::Fine))
                identifiers.append(CSSValueID::Fine);
            if (pointerCharacteristics.contains(PointerCharacteristics::Coarse) && !context.document->quirks().shouldHideCoarsePointerCharacteristics())
                identifiers.append(CSSValueID::Coarse);
            if (identifiers.isEmpty())
                identifiers.append(CSSValueID::None);
            return identifiers;
        }

    };
    return schema;
}

static const IdentifierSchema& prefersContrastFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-contrast"_s,
        FixedVector { CSSValueID::NoPreference, CSSValueID::More, CSSValueID::Less, CSSValueID::Custom },
        MediaQueryDynamicDependency::Accessibility,
        [](auto& context) {
            InterfaceContrastPreference userPreferredContrast = [&] {
                Ref frame = *context.document->frame();
                switch (frame->settings().forcedPrefersContrastAccessibilityValue()) {
                case ForcedAccessibilityValue::On:
                    return InterfaceContrastPreference::MoreContrast;
                case ForcedAccessibilityValue::Off:
                    return InterfaceContrastPreference::NoPreference;
                case ForcedAccessibilityValue::System:
                    return Theme::singleton().userPreferredContrast();
                }
                return InterfaceContrastPreference::NoPreference;
            }();

            switch (userPreferredContrast) {
            case InterfaceContrastPreference::NoPreference:
                return MatchingIdentifiers { CSSValueID::NoPreference };
            case InterfaceContrastPreference::MoreContrast:
                return MatchingIdentifiers { CSSValueID::More };
            case InterfaceContrastPreference::LessContrast:
                return MatchingIdentifiers { CSSValueID::Less };
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
    };
    return schema;
}

static const IdentifierSchema& prefersDarkInterfaceFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-dark-interface"_s,
        FixedVector { CSSValueID::NoPreference, CSSValueID::Prefers },
        MediaQueryDynamicDependency::Appearance,
        [](auto& context) {
            Ref page = *context.document->frame()->page();
            bool prefersDarkInterface = page->settings().useSystemAppearance() && page->useDarkAppearance();

            return MatchingIdentifiers { prefersDarkInterface ? CSSValueID::Prefers : CSSValueID::NoPreference };
        }
    };
    return schema;
}

static const IdentifierSchema& prefersReducedMotionFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-reduced-motion"_s,
        FixedVector { CSSValueID::NoPreference, CSSValueID::Reduce },
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

            return MatchingIdentifiers { userPrefersReducedMotion ? CSSValueID::Reduce : CSSValueID::NoPreference };
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
        FixedVector { CSSValueID::Interlace, CSSValueID::Progressive },
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
        FixedVector { CSSValueID::None, CSSValueID::InitialOnly, CSSValueID::Enabled },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            Ref frame = *context.document->frame();
            if (!protect(frame->script())->canExecuteScripts(ReasonForCallingCanExecuteScripts::NotAboutToExecuteScript))
                return MatchingIdentifiers { CSSValueID::None };
            return MatchingIdentifiers { CSSValueID::Enabled };
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
        FixedVector { CSSValueID::None, CSSValueID::Slow, CSSValueID::Fast },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            RefPtr frameView = context.document->frame()->view();
            if (frameView && frameView->mediaType() == printAtom())
                return MatchingIdentifiers { CSSValueID::None };

            // FIXME: Potentially add a hook for ports to change this value.
            return MatchingIdentifiers { CSSValueID::Fast };
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
                width = Style::unapplyingZoom<int>(width, *renderView);
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
        FixedVector { CSSValueID::Fullscreen, CSSValueID::Standalone, CSSValueID::MinimalUi, CSSValueID::Browser },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            auto identifier = [&] {
                Ref frame = *context.document->frame();
                auto manifest = frame->page() ? frame->page()->applicationManifest() : std::nullopt;
                if (!manifest)
                    return CSSValueID::Browser;

                switch (manifest->display) {
                case ApplicationManifest::Display::Fullscreen:
                    return CSSValueID::Fullscreen;
                case ApplicationManifest::Display::Standalone:
                    return CSSValueID::Standalone;
                case ApplicationManifest::Display::MinimalUI:
                    return CSSValueID::MinimalUi;
                case ApplicationManifest::Display::Browser:
                    return CSSValueID::Browser;
                }
                ASSERT_NOT_REACHED();
                return CSSValueID::Browser;
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
        FixedVector { CSSValueID::None, CSSValueID::Scroll, CSSValueID::Paged },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto& context) {
            // FIXME: Match none when scrollEnabled is set to false by UIKit.
            bool matchesPaged = [&] {
                RefPtr frameView = context.document->frame()->view();
                if (!frameView)
                    return false;
                return frameView->mediaType() == printAtom() || frameView->pagination().mode != PaginationMode::Unpaginated;
            }();
            return MatchingIdentifiers { matchesPaged ? CSSValueID::Paged : CSSValueID::Scroll };
        }
    };
    return schema;
}

static const IdentifierSchema& overflowInlineFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "overflow-inline"_s,
        FixedVector { CSSValueID::None, CSSValueID::Scroll },
        OptionSet<MediaQueryDynamicDependency>(),
        [](auto&) {
            // FIXME: Match none when scrollEnabled is set to false by UIKit.
            return MatchingIdentifiers { CSSValueID::Scroll };
        }
    };
    return schema;
}

#if ENABLE(DARK_MODE_CSS)
static bool frameOwnerElementAncestorsUseDarkAppearance(const Frame& frame)
{
    {
        RefPtr<const Frame> child = &frame;
        RefPtr<const Frame> parent = child->parent();

        // From CSS Media Queries Level 5: if the frame is a subframe, its preferred color scheme
        // is the color scheme of its owner element:
        // > the preferred color scheme must reflect the value of the used color scheme on the
        // > embedding node in the embedding document.

        // Iterate up the chain of owner elements to find the first one with explicitly set color scheme.
        while (parent) {
            ASSERT(child);

            auto ownerElementAppearance = protect(parent->virtualView())->appearanceOfOwnerElementOfChildFrame(*child);

            if (ownerElementAppearance.contains(FrameOwnerElementAppearance::ExplicitlySet))
                return ownerElementAppearance.contains(FrameOwnerElementAppearance::IsDark);

            child = parent;
            parent = child->parent();
        }
    }

    // If none of the ancestor owner elements specify color scheme, fallback to the system appearance.
    return protect(frame.page())->useDarkAppearance();
}

static const IdentifierSchema& prefersColorSchemeFeatureSchema()
{
    static MainThreadNeverDestroyed<IdentifierSchema> schema {
        "prefers-color-scheme"_s,
        FixedVector { CSSValueID::Light, CSSValueID::Dark },
        MediaQueryDynamicDependency::Appearance,
        [](auto& context) {
            bool useDarkAppearance = frameOwnerElementAncestorsUseDarkAppearance(*context.document->frame());

            return MatchingIdentifiers { useDarkAppearance ? CSSValueID::Dark : CSSValueID::Light };
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

const FeatureSchema& devicePosture()
{
    return devicePostureFeatureSchema();
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
        &devicePosture(),
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
