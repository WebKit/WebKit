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
#include "MediaQueryExp.h"
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

using namespace MediaFeatureNames;

enum MediaFeaturePrefix { MinPrefix, MaxPrefix, NoPrefix };

typedef bool (*EvalFunc)(CSSValue*, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix);
typedef HashMap<AtomicStringImpl*, EvalFunc> FunctionMap;
static FunctionMap* gFunctionMap;

/*
 * FIXME: following media features are not implemented: scan
 *
 * scan: The "scan" media feature describes the scanning process of
 * tv output devices. It's unknown how to retrieve this information from
 * the platform
 */

MediaQueryEvaluator::MediaQueryEvaluator(bool mediaFeatureResult)
    : m_frame(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const String& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_frame(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator::MediaQueryEvaluator(const String& acceptedMediaType, Frame* frame, RenderStyle* style)
    : m_mediaType(acceptedMediaType)
    , m_frame(frame)
    , m_style(style)
    , m_expResult(false) // doesn't matter when we have m_frame and m_style
{
}

MediaQueryEvaluator::~MediaQueryEvaluator()
{
}

bool MediaQueryEvaluator::mediaTypeMatch(const String& mediaTypeToMatch) const
{
    return mediaTypeToMatch.isEmpty()
        || equalIgnoringCase(mediaTypeToMatch, "all")
        || equalIgnoringCase(mediaTypeToMatch, m_mediaType);
}

bool MediaQueryEvaluator::mediaTypeMatchSpecific(const char* mediaTypeToMatch) const
{
    // Like mediaTypeMatch, but without the special cases for "" and "all".
    ASSERT(mediaTypeToMatch);
    ASSERT(mediaTypeToMatch[0] != '\0');
    ASSERT(!equalIgnoringCase(mediaTypeToMatch, String("all")));
    return equalIgnoringCase(mediaTypeToMatch, m_mediaType);
}

static bool applyRestrictor(MediaQuery::Restrictor r, bool value)
{
    return r == MediaQuery::Not ? !value : value;
}

bool MediaQueryEvaluator::eval(const MediaQuerySet* querySet, StyleResolver* styleResolver) const
{
    if (!querySet)
        return true;

    auto& queries = querySet->queryVector();
    if (!queries.size())
        return true; // empty query list evaluates to true

    // iterate over queries, stop if any of them eval to true (OR semantics)
    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        MediaQuery* query = queries[i].get();

        if (query->ignored())
            continue;

        if (mediaTypeMatch(query->mediaType())) {
            auto& expressions = query->expressions();
            // iterate through expressions, stop if any of them eval to false
            // (AND semantics)
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool exprResult = eval(expressions.at(j).get());
                if (styleResolver && expressions.at(j)->isViewportDependent())
                    styleResolver->addViewportDependentMediaQueryResult(expressions.at(j).get(), exprResult);
                if (!exprResult)
                    break;
            }

            // assume true if we are at the end of the list,
            // otherwise assume false
            result = applyRestrictor(query->restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query->restrictor(), false);
    }

    return result;
}

bool MediaQueryEvaluator::evalCheckingViewportDependentResults(const MediaQuerySet* querySet, Vector<std::unique_ptr<MediaQueryResult>>& results)
{
    if (!querySet)
        return true;

    auto& queries = querySet->queryVector();
    if (!queries.size())
        return true;

    bool result = false;
    for (size_t i = 0; i < queries.size() && !result; ++i) {
        MediaQuery* query = queries[i].get();

        if (query->ignored())
            continue;

        if (mediaTypeMatch(query->mediaType())) {
            auto& expressions = query->expressions();
            size_t j = 0;
            for (; j < expressions.size(); ++j) {
                bool exprResult = eval(expressions.at(j).get());
                if (expressions.at(j)->isViewportDependent())
                    results.append(std::make_unique<MediaQueryResult>(*expressions.at(j), exprResult));
                if (!exprResult)
                    break;
            }
            result = applyRestrictor(query->restrictor(), expressions.size() == j);
        } else
            result = applyRestrictor(query->restrictor(), false);
    }

    return result;
}

template<typename T>
bool compareValue(T a, T b, MediaFeaturePrefix op)
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
    if (is<CSSAspectRatioValue>(*value)) {
        CSSAspectRatioValue& aspectRatio = downcast<CSSAspectRatioValue>(*value);
        return compareValue(width * static_cast<int>(aspectRatio.denominatorValue()), height * static_cast<int>(aspectRatio.numeratorValue()), op);
    }

    return false;
}

static bool numberValue(CSSValue* value, float& result)
{
    if (is<CSSPrimitiveValue>(*value) && downcast<CSSPrimitiveValue>(*value).isNumber()) {
        result = downcast<CSSPrimitiveValue>(*value).getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
        return true;
    }
    return false;
}

static bool colorMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    int bitsPerComponent = screenDepthPerComponent(frame->page()->mainFrame().view());
    float number;
    if (value)
        return numberValue(value, number) && compareValue(bitsPerComponent, static_cast<int>(number), op);

    return bitsPerComponent != 0;
}

static bool color_indexMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix op)
{
    // FIXME: It's unknown how to retrieve the information if the display mode is indexed
    // Assume we don't support indexed display.
    if (!value)
        return false;

    float number;
    return numberValue(value, number) && compareValue(0, static_cast<int>(number), op);
}

static bool monochromeMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix op)
{
    if (!screenIsMonochrome(frame->page()->mainFrame().view())) {
        if (value) {
            float number;
            return numberValue(value, number) && compareValue(0, static_cast<int>(number), op);
        }
        return false;
    }

    return colorMediaFeatureEval(value, conversionData, frame, op);
}

static bool inverted_colorsMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix)
{
    bool isInverted = screenHasInvertedColors();

    if (!value)
        return isInverted;

    const CSSValueID id = downcast<CSSPrimitiveValue>(*value).getValueID();
    return (isInverted && id == CSSValueInverted) || (!isInverted && id == CSSValueNone);
}

static bool orientationMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix)
{
    FrameView* view = frame->view();
    if (!view)
        return false;

    int width = view->layoutWidth();
    int height = view->layoutHeight();
    if (is<CSSPrimitiveValue>(value)) {
        const CSSValueID id = downcast<CSSPrimitiveValue>(*value).getValueID();
        if (width > height) // Square viewport is portrait.
            return CSSValueLandscape == id;
        return CSSValuePortrait == id;
    }

    // Expression (orientation) evaluates to true if width and height >= 0.
    return height >= 0 && width >= 0;
}

static bool aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    FrameView* view = frame->view();
    if (!view)
        return true;

    if (value)
        return compareAspectRatioValue(value, view->layoutWidth(), view->layoutHeight(), op);

    // ({,min-,max-}aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    return true;
}

static bool device_aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame().view());
        return compareAspectRatioValue(value, static_cast<int>(sg.width()), static_cast<int>(sg.height()), op);
    }

    // ({,min-,max-}device-aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    return true;
}

static bool evalResolution(CSSValue* value, Frame* frame, MediaFeaturePrefix op)
{
    // FIXME: Possible handle other media types than 'screen' and 'print'.
    FrameView* view = frame->view();
    if (!view)
        return false;

    float deviceScaleFactor = 0;
    // This checks the actual media type applied to the document, and we know
    // this method only got called if this media type matches the one defined
    // in the query. Thus, if if the document's media type is "print", the
    // media type of the query will either be "print" or "all".
    String mediaType = view->mediaType();
    if (equalIgnoringCase(mediaType, "screen"))
        deviceScaleFactor = frame->page()->deviceScaleFactor();
    else if (equalIgnoringCase(mediaType, "print")) {
        // The resolution of images while printing should not depend on the dpi
        // of the screen. Until we support proper ways of querying this info
        // we use 300px which is considered minimum for current printers.
        deviceScaleFactor = 3.125; // 300dpi / 96dpi;
    }

    if (!value)
        return !!deviceScaleFactor;

    if (!is<CSSPrimitiveValue>(*value))
        return false;

    CSSPrimitiveValue& resolution = downcast<CSSPrimitiveValue>(*value);
    return compareValue(deviceScaleFactor, resolution.isNumber() ? resolution.getFloatValue() : resolution.getFloatValue(CSSPrimitiveValue::CSS_DPPX), op);
}

static bool device_pixel_ratioMediaFeatureEval(CSSValue *value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    return (!value || downcast<CSSPrimitiveValue>(*value).isNumber()) && evalResolution(value, frame, op);
}

static bool resolutionMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
#if ENABLE(RESOLUTION_MEDIA_QUERY)
    return (!value || downcast<CSSPrimitiveValue>(*value).isResolution()) && evalResolution(value, frame, op);
#else
    UNUSED_PARAM(value);
    UNUSED_PARAM(frame);
    UNUSED_PARAM(op);
    return false;
#endif
}

static bool gridMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix op)
{
    // if output device is bitmap, grid: 0 == true
    // assume we have bitmap device
    float number;
    if (value && numberValue(value, number))
        return compareValue(static_cast<int>(number), 0, op);
    return false;
}

static bool computeLength(CSSValue* value, bool strict, const CSSToLengthConversionData& conversionData, int& result)
{
    if (!is<CSSPrimitiveValue>(*value))
        return false;

    CSSPrimitiveValue& primitiveValue = downcast<CSSPrimitiveValue>(*value);

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

static bool device_heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame().view());
        int length;
        long height = sg.height();
        return computeLength(value, !frame->document()->inQuirksMode(), conversionData, length) && compareValue(static_cast<int>(height), length, op);
    }
    // ({,min-,max-}device-height)
    // assume if we have a device, assume non-zero
    return true;
}

static bool device_widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(frame->page()->mainFrame().view());
        int length;
        long width = sg.width();
        return computeLength(value, !frame->document()->inQuirksMode(), conversionData, length) && compareValue(static_cast<int>(width), length, op);
    }
    // ({,min-,max-}device-width)
    // assume if we have a device, assume non-zero
    return true;
}

static bool heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix op)
{
    FrameView* view = frame->view();
    if (!view)
        return false;

    if (value) {
        int height = view->layoutHeight();
        if (RenderView* renderView = frame->document()->renderView())
            height = adjustForAbsoluteZoom(height, *renderView);
        int length;
        return computeLength(value, !frame->document()->inQuirksMode(), conversionData, length) && compareValue(height, length, op);
    }

    return view->layoutHeight() != 0;
}

static bool widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix op)
{
    FrameView* view = frame->view();
    if (!view)
        return false;

    if (value) {
        int width = view->layoutWidth();
        if (RenderView* renderView = frame->document()->renderView())
            width = adjustForAbsoluteZoom(width, *renderView);
        int length;
        return computeLength(value, !frame->document()->inQuirksMode(), conversionData, length) && compareValue(width, length, op);
    }

    return view->layoutWidth() != 0;
}

// rest of the functions are trampolines which set the prefix according to the media feature expression used

static bool min_colorMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return colorMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_colorMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return colorMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_color_indexMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return color_indexMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_color_indexMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return color_indexMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_monochromeMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return monochromeMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_monochromeMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return monochromeMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return aspect_ratioMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return aspect_ratioMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_device_aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_aspect_ratioMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_device_aspect_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_aspect_ratioMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_device_pixel_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_pixel_ratioMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_device_pixel_ratioMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_pixel_ratioMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return heightMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return heightMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return widthMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return widthMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_device_heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_heightMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_device_heightMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_heightMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_device_widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_widthMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_device_widthMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return device_widthMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool min_resolutionMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return resolutionMediaFeatureEval(value, conversionData, frame, MinPrefix);
}

static bool max_resolutionMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& conversionData, Frame* frame, MediaFeaturePrefix)
{
    return resolutionMediaFeatureEval(value, conversionData, frame, MaxPrefix);
}

static bool animationMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix op)
{
    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool transitionMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix op)
{
    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool transform_2dMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix op)
{
    if (value) {
        float number;
        return numberValue(value, number) && compareValue(1, static_cast<int>(number), op);
    }
    return true;
}

static bool transform_3dMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    bool returnValueIfNoParameter;
    int have3dRendering;

#if ENABLE(3D_TRANSFORMS)
    bool threeDEnabled = false;
    if (RenderView* view = frame->contentRenderer())
        threeDEnabled = view->compositor().canRender3DTransforms();

    returnValueIfNoParameter = threeDEnabled;
    have3dRendering = threeDEnabled ? 1 : 0;
#else
    UNUSED_PARAM(frame);
    returnValueIfNoParameter = false;
    have3dRendering = 0;
#endif

    if (value) {
        float number;
        return numberValue(value, number) && compareValue(have3dRendering, static_cast<int>(number), op);
    }
    return returnValueIfNoParameter;
}

#if ENABLE(VIEW_MODE_CSS_MEDIA)
static bool view_modeMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix op)
{
    UNUSED_PARAM(op);
    if (!value)
        return true;

    const int viewModeCSSKeywordID = downcast<CSSPrimitiveValue>(*value).getValueID();
    const Page::ViewMode viewMode = frame->page()->viewMode();
    bool result = false;
    switch (viewMode) {
    case Page::ViewModeWindowed:
        result = viewModeCSSKeywordID == CSSValueWindowed;
        break;
    case Page::ViewModeFloating:
        result = viewModeCSSKeywordID == CSSValueFloating;
        break;
    case Page::ViewModeFullscreen:
        result = viewModeCSSKeywordID == CSSValueFullscreen;
        break;
    case Page::ViewModeMaximized:
        result = viewModeCSSKeywordID == CSSValueMaximized;
        break;
    case Page::ViewModeMinimized:
        result = viewModeCSSKeywordID == CSSValueMinimized;
        break;
    default:
        result = false;
        break;
    }

    return result;
}
#endif // ENABLE(VIEW_MODE_CSS_MEDIA)

static bool video_playable_inlineMediaFeatureEval(CSSValue*, const CSSToLengthConversionData&, Frame* frame, MediaFeaturePrefix)
{
    return frame->settings().allowsInlineMediaPlayback();
}

static bool hoverMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix)
{
    if (!is<CSSPrimitiveValue>(value)) {
#if ENABLE(TOUCH_EVENTS)
        return false;
#else
        return true;
#endif
    }

    int hoverCSSKeywordID = downcast<CSSPrimitiveValue>(*value).getValueID();
#if ENABLE(TOUCH_EVENTS)
    return hoverCSSKeywordID == CSSValueNone;
#else
    return hoverCSSKeywordID == CSSValueHover;
#endif
}

static bool any_hoverMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& cssToLengthConversionData, Frame* frame, MediaFeaturePrefix prefix)
{
    return hoverMediaFeatureEval(value, cssToLengthConversionData, frame, prefix);
}

static bool pointerMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData&, Frame*, MediaFeaturePrefix)
{
    if (!is<CSSPrimitiveValue>(value))
        return true;

    int pointerCSSKeywordID = downcast<CSSPrimitiveValue>(*value).getValueID();
#if ENABLE(TOUCH_EVENTS)
    return pointerCSSKeywordID == CSSValueCoarse;
#else
    return pointerCSSKeywordID == CSSValueFine;
#endif
}

static bool any_pointerMediaFeatureEval(CSSValue* value, const CSSToLengthConversionData& cssToLengthConversionData, Frame* frame, MediaFeaturePrefix prefix)
{
    return pointerMediaFeatureEval(value, cssToLengthConversionData, frame, prefix);
}

// FIXME: Remove unnecessary '&' from the following 'ADD_TO_FUNCTIONMAP' definition
// once we switch to a non-broken Visual Studio compiler.  https://bugs.webkit.org/show_bug.cgi?id=121235
static void createFunctionMap()
{
    // Create the table.
    gFunctionMap = new FunctionMap;
#define ADD_TO_FUNCTIONMAP(name, str)  \
    gFunctionMap->set(name##MediaFeature.impl(), &name##MediaFeatureEval);
    CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
}

bool MediaQueryEvaluator::eval(const MediaQueryExp* expr) const
{
    if (!m_frame || !m_frame->view() || !m_style)
        return m_expResult;

    if (!expr->isValid())
        return false;

    if (!gFunctionMap)
        createFunctionMap();

    // call the media feature evaluation function. Assume no prefix
    // and let trampoline functions override the prefix if prefix is
    // used
    EvalFunc func = gFunctionMap->get(expr->mediaFeature().impl());
    if (func) {
        CSSToLengthConversionData conversionData(m_style.get(),
            m_frame->document()->documentElement()->renderStyle(),
            m_frame->document()->renderView(), 1, false);
        return func(expr->value(), conversionData, m_frame, NoPrefix);
    }

    return false;
}

} // namespace
