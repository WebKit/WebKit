/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY
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
#include "LegacyMediaQueryEvaluator.h"

#include "CSSAspectRatioValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentLoader.h"
#include "Frame.h"
#include "FrameView.h"
#include "LegacyMediaQuery.h"
#include "Logging.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQueryParserContext.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "Quirks.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleFontSizeFunctions.h"
#include "Theme.h"
#include <wtf/HashMap.h>
#include <wtf/text/StringConcatenateNumbers.h>
#include <wtf/text/TextStream.h>

#if ENABLE(3D_TRANSFORMS)
#include "RenderLayerCompositor.h"
#endif

namespace WebCore {

enum MediaFeaturePrefix { MinPrefix, MaxPrefix, NoPrefix };

#ifndef LOG_DISABLED
static TextStream& operator<<(TextStream& ts, MediaFeaturePrefix op)
{
    switch (op) {
    case MinPrefix: ts << "min"; break;
    case MaxPrefix: ts << "max"; break;
    case NoPrefix: ts << ""; break;
    }
    return ts;
}
#endif

typedef bool (*MediaQueryFunction)(CSSValue*, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix);
typedef HashMap<AtomStringImpl*, MediaQueryFunction> MediaQueryFunctionMap;

static bool isAccessibilitySettingsDependent(const AtomString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::invertedColors
        || mediaFeature == MediaFeatureNames::maxMonochrome
        || mediaFeature == MediaFeatureNames::minMonochrome
        || mediaFeature == MediaFeatureNames::monochrome
        || mediaFeature == MediaFeatureNames::prefersReducedMotion
        || mediaFeature == MediaFeatureNames::prefersContrast;
}

static bool isAppearanceDependent(const AtomString& mediaFeature)
{
#if ENABLE(DARK_MODE_CSS)
    if (mediaFeature == MediaFeatureNames::prefersColorScheme)
        return true;
#endif
    return mediaFeature == MediaFeatureNames::prefersDarkInterface;
}

MediaQueryViewportState mediaQueryViewportStateForDocument(const Document& document)
{
    // These things affect evaluation of viewport dependent media queries.
    return { document.view()->layoutSize(), document.frame()->pageZoomFactor(), document.printing() };
}

LegacyMediaQueryEvaluator::LegacyMediaQueryEvaluator(bool mediaFeatureResult)
    : m_fallbackResult(mediaFeatureResult)
{
}

LegacyMediaQueryEvaluator::LegacyMediaQueryEvaluator(const String& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_fallbackResult(mediaFeatureResult)
{
}

LegacyMediaQueryEvaluator::LegacyMediaQueryEvaluator(const String& acceptedMediaType, const Document& document, const RenderStyle* style)
    : m_mediaType(acceptedMediaType)
    , m_document(document)
    , m_style(style)
{
}

bool LegacyMediaQueryEvaluator::mediaTypeMatch(const String& mediaTypeToMatch) const
{
    return mediaTypeToMatch.isEmpty()
        || equalLettersIgnoringASCIICase(mediaTypeToMatch, "all"_s)
        || equalIgnoringASCIICase(mediaTypeToMatch, m_mediaType);
}

bool LegacyMediaQueryEvaluator::mediaTypeMatchSpecific(ASCIILiteral mediaTypeToMatch) const
{
    // Like mediaTypeMatch, but without the special cases for "" and "all".
    ASSERT(!mediaTypeToMatch.isNull());
    ASSERT(mediaTypeToMatch.characterAt(0) != '\0');
    ASSERT(!equalLettersIgnoringASCIICase(mediaTypeToMatch, "all"_s));
    return equalIgnoringASCIICase(m_mediaType, mediaTypeToMatch);
}

static bool applyRestrictor(LegacyMediaQuery::Restrictor r, bool value)
{
    return r == LegacyMediaQuery::Not ? !value : value;
}

bool LegacyMediaQueryEvaluator::evaluate(const MediaQuerySet& querySet) const
{
    LOG_WITH_STREAM(MediaQueries, stream << "LegacyMediaQueryEvaluator::evaluate on " << (m_document ? m_document->url().string() : emptyString()));

    auto& queries = querySet.queryVector();
    if (!queries.size()) {
        LOG_WITH_STREAM(MediaQueries, stream << "LegacyMediaQueryEvaluator::evaluate " << querySet << " returning true");
        return true; // Empty query list evaluates to true.
    }

    // Iterate over queries, stop if any of them eval to true (OR semantics).
    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        auto& query = queries[i];

        if (query.ignored() || (!query.expressions().size() && query.mediaType().isEmpty()))
            continue;

        if (mediaTypeMatch(query.mediaType())) {
            auto& expressions = query.expressions();

            // Iterate through expressions, stop if any of them eval to false (AND semantics).
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool expressionResult = evaluate(expressions[j]);
                if (!expressionResult)
                    break;
            }
            // Assume true if we are at the end of the list, otherwise assume false.
            result = applyRestrictor(query.restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query.restrictor(), false);
    }

    LOG_WITH_STREAM(MediaQueries, stream << "LegacyMediaQueryEvaluator::evaluate " << querySet << " returning " << result);
    return result;
}

template<typename T, typename U> bool compareValue(T a, U b, MediaFeaturePrefix op)
{
    switch (op) {
    case MinPrefix:
        return a >= b;
    case MaxPrefix:
        return a <= b;
    case NoPrefix:
        return a == b;
    }
    return false;
}

#if !LOG_DISABLED

static String aspectRatioValueAsString(CSSValue* value)
{
    if (!is<CSSAspectRatioValue>(value))
        return emptyString();

    auto& aspectRatio = downcast<CSSAspectRatioValue>(*value);
    return makeString(aspectRatio.numeratorValue(), '/', aspectRatio.denominatorValue());
}

#endif

static bool compareAspectRatioValue(CSSValue* value, int width, int height, MediaFeaturePrefix op)
{
    if (!is<CSSAspectRatioValue>(value))
        return false;
    auto& aspectRatio = downcast<CSSAspectRatioValue>(*value);
    return compareValue(width * aspectRatio.denominatorValue(), height * aspectRatio.numeratorValue(), op);
}

static std::optional<double> doubleValue(CSSValue* value)
{
    if (!is<CSSPrimitiveValue>(value) || !downcast<CSSPrimitiveValue>(*value).isNumberOrInteger())
        return std::nullopt;
    return downcast<CSSPrimitiveValue>(*value).doubleValue(CSSUnitType::CSS_NUMBER);
}

static bool zeroEvaluate(CSSValue* value, MediaFeaturePrefix op)
{
    auto numericValue = doubleValue(value);
    return numericValue && compareValue(0, numericValue.value(), op);
}

static bool oneEvaluate(CSSValue* value, MediaFeaturePrefix op)
{
    if (!value)
        return true;
    auto numericValue = doubleValue(value);
    return numericValue && compareValue(1, numericValue.value(), op);
}

static bool colorEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    int bitsPerComponent = screenDepthPerComponent(frame.mainFrame().view());
    auto numericValue = doubleValue(value);
    if (!numericValue)
        return bitsPerComponent;
    return compareValue(bitsPerComponent, numericValue.value(), op);
}

static bool colorIndexEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    // Always return false for indexed display.
    return zeroEvaluate(value, op);
}

static bool colorGamutEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    if (!value)
        return true;

    switch (downcast<CSSPrimitiveValue>(*value).valueID()) {
    case CSSValueSRGB:
        return true;
    case CSSValueP3:
        // FIXME: For the moment we just assume any "extended color" display is at least as good as P3.
        return screenSupportsExtendedColor(frame.mainFrame().view());
    case CSSValueRec2020:
        // FIXME: At some point we should start detecting displays that support more colors.
        return false;
    default:
        return false; // Any unknown value should not be considered a match.
    }
}

static bool monochromeEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    bool isMonochrome;

    if (frame.settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::On)
        isMonochrome = true;
    else if (frame.settings().forcedDisplayIsMonochromeAccessibilityValue() == ForcedAccessibilityValue::Off)
        isMonochrome = false;
    else
        isMonochrome = screenIsMonochrome(frame.mainFrame().view());

    if (!isMonochrome)
        return zeroEvaluate(value, op);
    return colorEvaluate(value, conversionData, frame, op);
}

static bool invertedColorsEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    bool isInverted;

    if (frame.settings().forcedColorsAreInvertedAccessibilityValue() == ForcedAccessibilityValue::On)
        isInverted = true;
    else if (frame.settings().forcedColorsAreInvertedAccessibilityValue() == ForcedAccessibilityValue::Off)
        isInverted = false;
    else
        isInverted = screenHasInvertedColors();

    if (!value)
        return isInverted;

    return downcast<CSSPrimitiveValue>(*value).valueID() == (isInverted ? CSSValueInverted : CSSValueNone);
}

static bool orientationEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    FrameView* view = frame.view();
    if (!view)
        return false;

    auto width = view->layoutWidth();
    auto height = view->layoutHeight();

    if (!is<CSSPrimitiveValue>(value)) {
        // Expression (orientation) evaluates to true if width and height >= 0.
        return height >= 0 && width >= 0;
    }

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    bool result;
    if (width > height) // Square viewport is portrait.
        result = keyword == CSSValueLandscape;
    else
        result = keyword == CSSValuePortrait;

    LOG_WITH_STREAM(MediaQueries, stream << "  orientationEvaluate: view size " << width << "x" << height << " is " << value->cssText() << ": " << result);
    return result;
}

static bool aspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    if (!value)
        return true;
    FrameView* view = frame.view();
    if (!view)
        return true;
    bool result = compareAspectRatioValue(value, view->layoutWidth(), view->layoutHeight(), op);
    LOG_WITH_STREAM(MediaQueries, stream << "  aspectRatioEvaluate: " << op << " " << aspectRatioValueAsString(value) << " actual view size " << view->layoutWidth() << "x" << view->layoutHeight() << " : " << result);
    return result;
}

static bool deviceAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    if (!value)
        return true;

    auto size = frame.mainFrame().screenSize();
    bool result = compareAspectRatioValue(value, size.width(), size.height(), op);
    LOG_WITH_STREAM(MediaQueries, stream << "  deviceAspectRatioEvaluate: " << op << " " << aspectRatioValueAsString(value) << " actual screen size " << size << ": " << result);
    return result;
}

static bool evaluateResolution(CSSValue* value, Frame& frame, MediaFeaturePrefix op)
{
    // FIXME: Possible handle other media types than 'screen' and 'print'.
    FrameView* view = frame.view();
    if (!view)
        return false;

    float deviceScaleFactor = 0;

    // This checks the actual media type applied to the document, and we know
    // this method only got called if this media type matches the one defined
    // in the query. Thus, if if the document's media type is "print", the
    // media type of the query will either be "print" or "all".
    String mediaType = view->mediaType();
    if (equalLettersIgnoringASCIICase(mediaType, "screen"_s))
        deviceScaleFactor = frame.page() ? frame.page()->deviceScaleFactor() : 1;
    else if (equalLettersIgnoringASCIICase(mediaType, "print"_s)) {
        // The resolution of images while printing should not depend on the dpi
        // of the screen. Until we support proper ways of querying this info
        // we use 300px which is considered minimum for current printers.
        deviceScaleFactor = 3.125; // 300dpi / 96dpi;
    }

    if (!value)
        return !!deviceScaleFactor;

    if (!is<CSSPrimitiveValue>(value))
        return false;

    auto& resolution = downcast<CSSPrimitiveValue>(*value);
    float resolutionValue = resolution.isNumberOrInteger() ? resolution.floatValue() : resolution.floatValue(CSSUnitType::CSS_DPPX);
    bool result = compareValue(deviceScaleFactor, resolutionValue, op);
    LOG_WITH_STREAM(MediaQueries, stream << "  evaluateResolution: " << op << " " << resolutionValue << " device scale factor " << deviceScaleFactor << ": " << result);
    return result;
}

static bool devicePixelRatioEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    return (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isNumberOrInteger())) && evaluateResolution(value, frame, op);
}

static bool resolutionEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    if (!frame.settings().resolutionMediaFeatureEnabled())
        return false;

    return (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isResolution())) && evaluateResolution(value, frame, op);
}

static bool dynamicRangeEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    if (!value)
        return false;

    bool supportsHighDynamicRange;

    if (frame.settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::On)
        supportsHighDynamicRange = true;
    else if (frame.settings().forcedSupportsHighDynamicRangeValue() == ForcedAccessibilityValue::Off)
        supportsHighDynamicRange = false;
    else
        supportsHighDynamicRange = screenSupportsHighDynamicRange(frame.mainFrame().view());

    switch (downcast<CSSPrimitiveValue>(*value).valueID()) {
    case CSSValueHigh:
        return supportsHighDynamicRange;
    case CSSValueStandard:
        return true;
    default:
        return false; // Any unknown value should not be considered a match.
    }
}

static bool scanEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    RefPtr view = frame.view();
    if (!view)
        return false;

    /* With Media Queries Level 4, the "tv" media type is deprecated
     * and the "scan" feature applies to all media types.
     * We are currently supporting Media Queries Level 3,
     * thus the check against "tv".
     */
    if (!equalLettersIgnoringASCIICase(view->mediaType(), "tv"_s))
        return false;

    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return false;

    // All known implementations (Blink, Gecko) assume and match "progressive".
    return primitiveValue->valueID() == CSSValueProgressive;
}

static bool forcedColorsEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix)
{
    auto* primitiveValue = dynamicDowncast<CSSPrimitiveValue>(value);
    if (!primitiveValue)
        return false;

    // On Cocoa platforms, there is no concept of "forced colors".
    return primitiveValue->valueID() == CSSValueNone;
}

static bool gridEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    return zeroEvaluate(value, op);
}

static std::optional<double> computeLength(CSSValue* value, bool strict, const CSSToLengthConversionData& conversionData)
{
    if (!is<CSSPrimitiveValue>(value))
        return std::nullopt;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);
    if (primitiveValue.isNumberOrInteger()) {
        double value = primitiveValue.doubleValue();
        // The only unitless number value allowed in strict mode is zero.
        if (strict && value)
            return std::nullopt;
        return value;
    }

    if (primitiveValue.isLength())
        return primitiveValue.computeLength<double>(conversionData);

    return std::nullopt;
}

static bool deviceHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-height)
    // assume if we have a device, assume non-zero
    if (!value)
        return true;
    auto length = computeLength(value, !frame.document()->inQuirksMode(), conversionData);
    if (!length)
        return false;

    auto height = frame.mainFrame().screenSize().height();
    LOG_WITH_STREAM(MediaQueries, stream << "  deviceHeightEvaluate: query " << op << " height " << *length << ", actual height " << height << " result: " << compareValue(height, *length, op));
    return compareValue(height, *length, op);
}

static bool deviceWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-width)
    // assume if we have a device, assume non-zero
    if (!value)
        return true;
    auto length = computeLength(value, !frame.document()->inQuirksMode(), conversionData);
    if (!length)
        return false;

    auto width = frame.mainFrame().screenSize().width();
    LOG_WITH_STREAM(MediaQueries, stream << "  deviceWidthEvaluate: query " << op << " width " << *length << ", actual width " << width << " result: " << compareValue(width, *length, op));
    return compareValue(width, *length, op);
}

static bool heightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    FrameView* view = frame.view();
    if (!view)
        return false;
    int height = view->layoutHeight();
    if (!value)
        return height;

    auto length = computeLength(value, !frame.document()->inQuirksMode(), conversionData);
    if (!length)
        return false;

    if (auto* renderView = frame.document()->renderView())
        height = adjustForAbsoluteZoom(height, *renderView);

    LOG_WITH_STREAM(MediaQueries, stream << "  heightEvaluate: query " << op << " height " << *length << ", actual height " << height << " result: " << compareValue(height, *length, op));
    return compareValue(height, *length, op);
}

static bool widthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    FrameView* view = frame.view();
    if (!view)
        return false;
    int width = view->layoutWidth();
    if (!value)
        return width;

    auto length = computeLength(value, !frame.document()->inQuirksMode(), conversionData);
    if (!length)
        return false;

    if (auto* renderView = frame.document()->renderView())
        width = adjustForAbsoluteZoom(width, *renderView);

    LOG_WITH_STREAM(MediaQueries, stream << "  widthEvaluate: query " << op << " width " << *length << ", actual width " << width << " result: " << compareValue(width, *length, op));
    return compareValue(width, *length, op);
}

static bool minColorEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return colorEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxColorEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return colorEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minColorIndexEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return colorIndexEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxColorIndexEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return colorIndexEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minMonochromeEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return monochromeEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxMonochromeEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return monochromeEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return aspectRatioEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return aspectRatioEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minDeviceAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceAspectRatioEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxDeviceAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceAspectRatioEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minDevicePixelRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return devicePixelRatioEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxDevicePixelRatioEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return devicePixelRatioEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return heightEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return heightEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return widthEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return widthEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minDeviceHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceHeightEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxDeviceHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceHeightEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minDeviceWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceWidthEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxDeviceWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return deviceWidthEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool minResolutionEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return resolutionEvaluate(value, conversionData, frame, MinPrefix);
}

static bool maxResolutionEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix)
{
    return resolutionEvaluate(value, conversionData, frame, MaxPrefix);
}

static bool animationEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    return oneEvaluate(value, op);
}

static bool transitionEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    return oneEvaluate(value, op);
}

static bool transform2dEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    return oneEvaluate(value, op);
}

static bool transform3dEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
#if ENABLE(3D_TRANSFORMS)
    auto* view = frame.contentRenderer();
    return view && view->compositor().canRender3DTransforms() ? oneEvaluate(value, op) : zeroEvaluate(value, op);
#else
    UNUSED_PARAM(frame);
    return zeroEvaluate(value, op);
#endif
}

static bool videoPlayableInlineEvaluate(CSSValue*, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    return frame.settings().allowsInlineMediaPlayback();
}

static bool hoverEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    auto* page = frame.page();
    bool hoverSupportedByPrimaryPointingDevice = page && page->chrome().client().hoverSupportedByPrimaryPointingDevice();

    if (!is<CSSPrimitiveValue>(value))
        return hoverSupportedByPrimaryPointingDevice;

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    return hoverSupportedByPrimaryPointingDevice ? (keyword == CSSValueHover) : (keyword == CSSValueNone);
}

static bool anyHoverEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    auto* page = frame.page();
    bool hoverSupportedByAnyAvailablePointingDevice = page && page->chrome().client().hoverSupportedByAnyAvailablePointingDevice();

    if (!is<CSSPrimitiveValue>(value))
        return hoverSupportedByAnyAvailablePointingDevice;

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    return hoverSupportedByAnyAvailablePointingDevice ? (keyword == CSSValueHover) : (keyword == CSSValueNone);
}

static bool pointerEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    auto* page = frame.page();
    auto pointerCharacteristicsOfPrimaryPointingDevice = page ? page->chrome().client().pointerCharacteristicsOfPrimaryPointingDevice() : std::optional<PointerCharacteristics>();

#if ENABLE(TOUCH_EVENTS)
    if (pointerCharacteristicsOfPrimaryPointingDevice == PointerCharacteristics::Coarse) {
        auto* document = frame.document();
        if (document && document->quirks().shouldPreventPointerMediaQueryFromEvaluatingToCoarse())
            pointerCharacteristicsOfPrimaryPointingDevice = PointerCharacteristics::Fine;
    }
#endif

    if (!is<CSSPrimitiveValue>(value))
        return !!pointerCharacteristicsOfPrimaryPointingDevice;

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    if (keyword == CSSValueFine)
        return pointerCharacteristicsOfPrimaryPointingDevice == PointerCharacteristics::Fine;
    if (keyword == CSSValueCoarse)
        return pointerCharacteristicsOfPrimaryPointingDevice == PointerCharacteristics::Coarse;
    if (keyword == CSSValueNone)
        return !pointerCharacteristicsOfPrimaryPointingDevice;
    return false;
}

static bool anyPointerEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    auto* page = frame.page();
    auto pointerCharacteristicsOfAllAvailablePointingDevices = page ? page->chrome().client().pointerCharacteristicsOfAllAvailablePointingDevices() : OptionSet<PointerCharacteristics>();

    if (!is<CSSPrimitiveValue>(value))
        return !pointerCharacteristicsOfAllAvailablePointingDevices.isEmpty();

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    if (keyword == CSSValueFine)
        return pointerCharacteristicsOfAllAvailablePointingDevices.contains(PointerCharacteristics::Fine);
    if (keyword == CSSValueCoarse)
        return pointerCharacteristicsOfAllAvailablePointingDevices.contains(PointerCharacteristics::Coarse);
    if (keyword == CSSValueNone)
        return pointerCharacteristicsOfAllAvailablePointingDevices.isEmpty();
    return false;
}

static bool prefersDarkInterfaceEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    bool prefersDarkInterface = false;

    if (frame.page()->useSystemAppearance() && frame.page()->useDarkAppearance())
        prefersDarkInterface = true;

    if (!value)
        return prefersDarkInterface;

    return downcast<CSSPrimitiveValue>(*value).valueID() == (prefersDarkInterface ? CSSValuePrefers : CSSValueNoPreference);
}

#if ENABLE(DARK_MODE_CSS)
static bool prefersColorSchemeEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    if (!value)
        return true;

    if (!is<CSSPrimitiveValue>(value))
        return false;

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();
    bool useDarkAppearance = [&] () -> auto {
        if (frame.document()->loader()) {
            auto colorSchemePreference = frame.document()->loader()->colorSchemePreference();
            if (colorSchemePreference != ColorSchemePreference::NoPreference)
                return colorSchemePreference == ColorSchemePreference::Dark;
        }

        return frame.page()->useDarkAppearance();
    }();

    switch (keyword) {
    case CSSValueDark:
        return useDarkAppearance;
    case CSSValueLight:
        return !useDarkAppearance;
    default:
        return false;
    }
}
#endif

static bool prefersContrastEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    bool userPrefersContrast = false;

    switch (frame.settings().forcedPrefersContrastAccessibilityValue()) {
    case ForcedAccessibilityValue::On:
        userPrefersContrast = true;
        break;
    case ForcedAccessibilityValue::Off:
        break;
    case ForcedAccessibilityValue::System:
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
        userPrefersContrast = Theme::singleton().userPrefersContrast();
#endif
        break;
    }

    if (!value)
        return userPrefersContrast;

    // Apple platforms: "less" is ignored and only "more" is mapped to the user's preference.
    return downcast<CSSPrimitiveValue>(*value).valueID() == (userPrefersContrast ? CSSValueMore : CSSValueNoPreference);
}

static bool prefersReducedMotionEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    bool userPrefersReducedMotion = false;

    switch (frame.settings().forcedPrefersReducedMotionAccessibilityValue()) {
    case ForcedAccessibilityValue::On:
        userPrefersReducedMotion = true;
        break;
    case ForcedAccessibilityValue::Off:
        break;
    case ForcedAccessibilityValue::System:
#if USE(NEW_THEME) || PLATFORM(IOS_FAMILY)
        userPrefersReducedMotion = Theme::singleton().userPrefersReducedMotion();
#endif
        break;
    }

    if (!value)
        return userPrefersReducedMotion;

    return downcast<CSSPrimitiveValue>(*value).valueID() == (userPrefersReducedMotion ? CSSValueReduce : CSSValueNoPreference);
}

#if ENABLE(APPLICATION_MANIFEST)
static bool displayModeEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    if (!value)
        return true;

    auto keyword = downcast<CSSPrimitiveValue>(*value).valueID();

    auto manifest = frame.page() ? frame.page()->applicationManifest() : std::nullopt;
    if (!manifest)
        return keyword == CSSValueBrowser;

    switch (manifest->display) {
    case ApplicationManifest::Display::Fullscreen:
        return keyword == CSSValueFullscreen;
    case ApplicationManifest::Display::Standalone:
        return keyword == CSSValueStandalone;
    case ApplicationManifest::Display::MinimalUI:
        return keyword == CSSValueMinimalUi;
    case ApplicationManifest::Display::Browser:
        return keyword == CSSValueBrowser;
    }

    return false;
}
#endif // ENABLE(APPLICATION_MANIFEST)

// Use this function instead of calling add directly to avoid inlining.
static void add(MediaQueryFunctionMap& map, AtomStringImpl* key, MediaQueryFunction value)
{
    map.add(key, value);
}

bool LegacyMediaQueryEvaluator::evaluate(const MediaQueryExpression& expression) const
{
    if (!m_document)
        return m_fallbackResult;

    auto& document = *m_document;
    auto* frame = document.frame();
    if (!frame || !frame->view() || !m_style)
        return m_fallbackResult;

    if (!expression.isValid())
        return false;

    static NeverDestroyed<MediaQueryFunctionMap> map = [] {
        MediaQueryFunctionMap map;
#define ADD_TO_FUNCTIONMAP(name, str) add(map, MediaFeatureNames::name->impl(), name##Evaluate);
        CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
        return map;
    }();

    auto function = map.get().get(expression.mediaFeature().impl());
    if (!function)
        return false;

    if (!document.documentElement())
        return false;

    auto defaultStyle = RenderStyle::create();
    auto fontDescription = defaultStyle.fontDescription();
    auto size = Style::fontSizeForKeyword(CSSValueMedium, false, document);
    fontDescription.setComputedSize(size);
    fontDescription.setSpecifiedSize(size);
    defaultStyle.setFontDescription(WTFMove(fontDescription));
    defaultStyle.fontCascade().update();
    
    // Pass `nullptr` for `parentStyle` because we are in the context of a media query.
    return function(expression.value(), { *m_style, &defaultStyle, nullptr, document.renderView() }, *frame, NoPrefix);
}

bool LegacyMediaQueryEvaluator::mediaAttributeMatches(Document& document, const String& attributeValue)
{
    ASSERT(document.renderView());
    auto mediaQueries = MediaQuerySet::create(attributeValue, MediaQueryParserContext(document));
    return LegacyMediaQueryEvaluator { "screen"_s, document, &document.renderView()->style() }.evaluate(mediaQueries.get());
}


OptionSet<MQ::MediaQueryDynamicDependency> mediaQueryDynamicDependencies(const MediaQuerySet& queries, const LegacyMediaQueryEvaluator& evaluator)
{
    OptionSet<MQ::MediaQueryDynamicDependency> result;

    for (auto& query : queries.queryVector()) {
        if (query.ignored() || (!query.expressions().size() && query.mediaType().isEmpty()))
            continue;

        if (!evaluator.mediaTypeMatch(query.mediaType()))
            continue;

        for (auto& expression : query.expressions()) {
            if (expression.isViewportDependent())
                result.add(MQ::MediaQueryDynamicDependency::Viewport);
            if (isAppearanceDependent(expression.mediaFeature()))
                result.add(MQ::MediaQueryDynamicDependency::Appearance);
            if (isAccessibilitySettingsDependent(expression.mediaFeature()))
                result.add(MQ::MediaQueryDynamicDependency::Accessibility);
        }
    }

    return result;
}

} // WebCore
