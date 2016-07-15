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
#include "MediaQueryEvaluator.h"

#include "CSSAspectRatioValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include "FloatRect.h"
#include "FrameView.h"
#include "IntRect.h"
#include "MainFrame.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "NodeRenderStyle.h"
#include "Page.h"
#include "PlatformScreen.h"
#include "RenderStyle.h"
#include "RenderView.h"
#include "Screen.h"
#include "Settings.h"
#include "StyleResolver.h"
#include <wtf/HashMap.h>

#if ENABLE(3D_TRANSFORMS)
#include "RenderLayerCompositor.h"
#endif

namespace WebCore {

enum MediaFeaturePrefix { MinPrefix, MaxPrefix, NoPrefix };

typedef bool (*MediaQueryFunction)(CSSValue*, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix);
typedef HashMap<AtomicStringImpl*, MediaQueryFunction> MediaQueryFunctionMap;

static bool isViewportDependent(const AtomicString& mediaFeature)
{
    return mediaFeature == MediaFeatureNames::width
        || mediaFeature == MediaFeatureNames::height
        || mediaFeature == MediaFeatureNames::minWidth
        || mediaFeature == MediaFeatureNames::minHeight
        || mediaFeature == MediaFeatureNames::maxWidth
        || mediaFeature == MediaFeatureNames::maxHeight
        || mediaFeature == MediaFeatureNames::orientation
        || mediaFeature == MediaFeatureNames::aspectRatio
        || mediaFeature == MediaFeatureNames::minAspectRatio
        || mediaFeature == MediaFeatureNames::maxAspectRatio;
}

MediaQueryEvaluator::MediaQueryEvaluator(bool mediaFeatureResult)
    : m_fallbackResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const String& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_fallbackResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const String& acceptedMediaType, Document& document, const RenderStyle* style)
    : m_mediaType(acceptedMediaType)
    , m_frame(document.frame())
    , m_style(style)
{
}

bool MediaQueryEvaluator::mediaTypeMatch(const String& mediaTypeToMatch) const
{
    return mediaTypeToMatch.isEmpty()
        || equalLettersIgnoringASCIICase(mediaTypeToMatch, "all")
        || equalIgnoringASCIICase(mediaTypeToMatch, m_mediaType);
}

bool MediaQueryEvaluator::mediaTypeMatchSpecific(const char* mediaTypeToMatch) const
{
    // Like mediaTypeMatch, but without the special cases for "" and "all".
    ASSERT(mediaTypeToMatch);
    ASSERT(mediaTypeToMatch[0] != '\0');
    ASSERT(!equalLettersIgnoringASCIICase(StringView(mediaTypeToMatch), "all"));
    return equalIgnoringASCIICase(m_mediaType, mediaTypeToMatch);
}

static bool applyRestrictor(MediaQuery::Restrictor r, bool value)
{
    return r == MediaQuery::Not ? !value : value;
}

bool MediaQueryEvaluator::evaluate(const MediaQuerySet& querySet, StyleResolver* styleResolver) const
{
    auto& queries = querySet.queryVector();
    if (!queries.size())
        return true; // empty query list evaluates to true

    // iterate over queries, stop if any of them eval to true (OR semantics)
    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        auto& query = queries[i];

        if (query.ignored() || (!query.expressions().size() && query.mediaType().isEmpty()))
            continue;

        if (mediaTypeMatch(query.mediaType())) {
            auto& expressions = query.expressions();
            // iterate through expressions, stop if any of them eval to false (AND semantics)
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool expressionResult = evaluate(expressions[j]);
                if (styleResolver && isViewportDependent(expressions[j].mediaFeature()))
                    styleResolver->addViewportDependentMediaQueryResult(expressions[j], expressionResult);
                if (!expressionResult)
                    break;
            }

            // assume true if we are at the end of the list,
            // otherwise assume false
            result = applyRestrictor(query.restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query.restrictor(), false);
    }

    return result;
}

bool MediaQueryEvaluator::evaluate(const MediaQuerySet& querySet, Vector<MediaQueryResult>& results) const
{
    auto& queries = querySet.queryVector();
    if (!queries.size())
        return true;

    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        auto& query = queries[i];

        if (query.ignored())
            continue;

        if (mediaTypeMatch(query.mediaType())) {
            auto& expressions = query.expressions();
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool expressionResult = evaluate(expressions[j]);
                if (isViewportDependent(expressions[j].mediaFeature()))
                    results.append({ expressions[j], expressionResult });
                if (!expressionResult)
                    break;
            }
            result = applyRestrictor(query.restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query.restrictor(), false);
    }

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

static bool compareAspectRatioValue(CSSValue* value, int width, int height, MediaFeaturePrefix op)
{
    if (!is<CSSAspectRatioValue>(value))
        return false;
    auto& aspectRatio = downcast<CSSAspectRatioValue>(*value);
    return compareValue(width * aspectRatio.denominatorValue(), height * aspectRatio.numeratorValue(), op);
}

static Optional<double> doubleValue(CSSValue* value)
{
    if (!is<CSSPrimitiveValue>(value) || !downcast<CSSPrimitiveValue>(*value).isNumber())
        return Nullopt;
    return downcast<CSSPrimitiveValue>(*value).getDoubleValue(CSSPrimitiveValue::CSS_NUMBER);
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

    switch (downcast<CSSPrimitiveValue>(*value).getValueID()) {
    case CSSValueSrgb:
        return true;
    case CSSValueP3:
        // FIXME: For the moment we just assume any "extended color" display is at least as good as P3.
        return screenSupportsExtendedColor(frame.mainFrame().view());
    case CSSValueRec2020:
        // FIXME: At some point we should start detecting displays that support more colors.
        return false;
    default:
        ASSERT_NOT_REACHED();
        return true;
    }
}

static bool monochromeEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    if (!screenIsMonochrome(frame.mainFrame().view()))
        return zeroEvaluate(value, op);
    return colorEvaluate(value, conversionData, frame, op);
}

static bool invertedColorsEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix)
{
    bool isInverted = screenHasInvertedColors();
    if (!value)
        return isInverted;
    return downcast<CSSPrimitiveValue>(*value).getValueID() == (isInverted ? CSSValueInverted : CSSValueNone);
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

    auto keyword = downcast<CSSPrimitiveValue>(*value).getValueID();
    if (width > height) // Square viewport is portrait.
        return keyword == CSSValueLandscape;
    return keyword == CSSValuePortrait;
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

    return compareAspectRatioValue(value, view->layoutWidth(), view->layoutHeight(), op);
}

static bool deviceAspectRatioEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    if (!value)
        return true;

    auto size = screenRect(frame.mainFrame().view()).size();
    return compareAspectRatioValue(value, size.width(), size.height(), op);
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
    if (equalLettersIgnoringASCIICase(mediaType, "screen"))
        deviceScaleFactor = frame.page() ? frame.page()->deviceScaleFactor() : 1;
    else if (equalLettersIgnoringASCIICase(mediaType, "print")) {
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
    return compareValue(deviceScaleFactor, resolution.isNumber() ? resolution.getFloatValue() : resolution.getFloatValue(CSSPrimitiveValue::CSS_DPPX), op);
}

static bool devicePixelRatioEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
    return (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isNumber())) && evaluateResolution(value, frame, op);
}

static bool resolutionEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix op)
{
#if ENABLE(RESOLUTION_MEDIA_QUERY)
    return (!value || (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isResolution())) && evaluateResolution(value, frame, op);
#else
    UNUSED_PARAM(value);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(op);
    return false;
#endif
}

static bool gridEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix op)
{
    return zeroEvaluate(value, op);
}

static bool computeLength(CSSValue* value, bool strict, const CSSToLengthConversionData& conversionData, int& result)
{
    if (!is<CSSPrimitiveValue>(value))
        return false;

    auto& primitiveValue = downcast<CSSPrimitiveValue>(*value);

    if (primitiveValue.isNumber()) {
        result = primitiveValue.getIntValue();
        return !strict || !result;
    }

    if (primitiveValue.isLength()) {
        result = primitiveValue.computeLength<int>(conversionData);
        return true;
    }

    return false;
}

static bool deviceHeightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-height)
    // assume if we have a device, assume non-zero
    if (!value)
        return true;
    int length;
    auto height = screenRect(frame.mainFrame().view()).height();
    return computeLength(value, !frame.document()->inQuirksMode(), conversionData, length) && compareValue(height, length, op);
}

static bool deviceWidthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    // ({,min-,max-}device-width)
    // assume if we have a device, assume non-zero
    if (!value)
        return true;
    int length;
    auto width = screenRect(frame.mainFrame().view()).width();
    return computeLength(value, !frame.document()->inQuirksMode(), conversionData, length) && compareValue(width, length, op);
}

static bool heightEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    FrameView* view = frame.view();
    if (!view)
        return false;
    int height = view->layoutHeight();
    if (!value)
        return height;
    if (auto* renderView = frame.document()->renderView())
        height = adjustForAbsoluteZoom(height, *renderView);
    int length;
    return computeLength(value, !frame.document()->inQuirksMode(), conversionData, length) && compareValue(height, length, op);
}

static bool widthEvaluate(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame& frame, MediaFeaturePrefix op)
{
    FrameView* view = frame.view();
    if (!view)
        return false;
    int width = view->layoutWidth();
    if (!value)
        return width;
    if (auto* renderView = frame.document()->renderView())
        width = adjustForAbsoluteZoom(width, *renderView);
    int length;
    return computeLength(value, !frame.document()->inQuirksMode(), conversionData, length) && compareValue(width, length, op);
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

#if ENABLE(VIEW_MODE_CSS_MEDIA)

static bool viewModeEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    if (!value)
        return true;

    auto keyword = downcast<CSSPrimitiveValue>(*value).getValueID();

    switch (frame.page()->viewMode()) {
    case Page::ViewModeWindowed:
        return keyword == CSSValueWindowed;
    case Page::ViewModeFloating:
        return keyword == CSSValueFloating;
    case Page::ViewModeFullscreen:
        return keyword == CSSValueFullscreen;
    case Page::ViewModeMaximized:
        return keyword == CSSValueMaximized;
    case Page::ViewModeMinimized:
        return keyword == CSSValueMinimized;
    default:
        break;
    }

    return false;
}

#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

static bool videoPlayableInlineEvaluate(CSSValue*, const CSSToLengthConversionData&, Frame& frame, MediaFeaturePrefix)
{
    return frame.settings().allowsInlineMediaPlayback();
}

static bool hoverEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix)
{
    if (!is<CSSPrimitiveValue>(value)) {
#if ENABLE(TOUCH_EVENTS)
        return false;
#else
        return true;
#endif
    }

    auto keyword = downcast<CSSPrimitiveValue>(*value).getValueID();
#if ENABLE(TOUCH_EVENTS)
    return keyword == CSSValueNone;
#else
    return keyword == CSSValueHover;
#endif
}

static bool anyHoverEvaluate(CSSValue* value, const CSSToLengthConversionData& cssToLengthConversionData, Frame& frame, MediaFeaturePrefix prefix)
{
    return hoverEvaluate(value, cssToLengthConversionData, frame, prefix);
}

static bool pointerEvaluate(CSSValue* value, const CSSToLengthConversionData&, Frame&, MediaFeaturePrefix)
{
    if (!is<CSSPrimitiveValue>(value))
        return true;

    auto keyword = downcast<CSSPrimitiveValue>(*value).getValueID();
#if ENABLE(TOUCH_EVENTS)
    return keyword == CSSValueCoarse;
#else
    return keyword == CSSValueFine;
#endif
}

static bool anyPointerEvaluate(CSSValue* value, const CSSToLengthConversionData& cssToLengthConversionData, Frame& frame, MediaFeaturePrefix prefix)
{
    return pointerEvaluate(value, cssToLengthConversionData, frame, prefix);
}

// Use this function instead of calling add directly to avoid inlining.
static void add(MediaQueryFunctionMap& map, AtomicStringImpl* key, MediaQueryFunction value)
{
    map.add(key, value);
}

bool MediaQueryEvaluator::evaluate(const MediaQueryExpression& expression) const
{
    if (!m_frame || !m_frame->view() || !m_style)
        return m_fallbackResult;

    if (!expression.isValid())
        return false;

    static NeverDestroyed<MediaQueryFunctionMap> map = [] {
        MediaQueryFunctionMap map;
#define ADD_TO_FUNCTIONMAP(name, str) add(map, MediaFeatureNames::name.impl(), name##Evaluate);
        CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
        return map;
    }();

    auto function = map.get().get(expression.mediaFeature().impl());
    if (!function)
        return false;

    Document& document = *m_frame->document();
    if (!document.documentElement())
        return false;
    return function(expression.value(), { m_style, document.documentElement()->renderStyle(), document.renderView(), 1, false }, *m_frame, NoPrefix);
}

} // namespace
