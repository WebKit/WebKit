/*
 * CSS Media Query Evaluator
 *
 * Copyright (C) 2006 Kimmo Kinnunen <kimmo.t.kinnunen@nokia.com>.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "Chrome.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueList.h"
#include "FloatRect.h"
#include "Frame.h"
#include "FrameView.h"
#include "IntRect.h"
#include "MediaFeatureNames.h"
#include "MediaList.h"
#include "MediaQuery.h"
#include "MediaQueryExp.h"
#include "Page.h"
#include "RenderStyle.h"
#include "PlatformScreen.h"
#include <wtf/HashMap.h>

namespace WebCore {

using namespace MediaFeatureNames;

enum MediaFeaturePrefix { MinPrefix, MaxPrefix, NoPrefix };

typedef bool (*EvalFunc)(CSSValue*, RenderStyle*, Page*,  MediaFeaturePrefix);
typedef HashMap<AtomicStringImpl*, EvalFunc> FunctionMap;
static FunctionMap* gFunctionMap;

/*
 * FIXME: following media features are not implemented: color_index, scan, resolution
 *
 * color_index, min-color-index, max_color_index: It's unknown how to retrieve
 * the information if the display mode is indexed
 * scan: The "scan" media feature describes the scanning process of
 * tv output devices. It's unknown how to retrieve this information from
 * the platform
 * resolution, min-resolution,  max-resolution: css parser doesn't seem to
 * support CSS_DIMENSION
 */

MediaQueryEvaluator::MediaQueryEvaluator(bool mediaFeatureResult)
    : m_page(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator:: MediaQueryEvaluator(const String& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_page(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator:: MediaQueryEvaluator(const char* acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_page(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator:: MediaQueryEvaluator(const String& acceptedMediaType, Page* page, RenderStyle* style)
    : m_mediaType(acceptedMediaType.lower())
    , m_page(page)
    , m_style(style)
    , m_expResult(false) // doesn't matter when we have m_page and m_style
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

bool MediaQueryEvaluator::eval(const MediaList* mediaList) const
{
    if (!mediaList)
        return true;

    const Vector<MediaQuery*>* queries = mediaList->mediaQueries();
    if (!queries->size())
        return true; // empty query list evaluates to true

    // iterate over queries, stop if any of them eval to true (OR semantics)
    bool result = false;
    for (size_t i = 0; i < queries->size() && !result; ++i) {
        MediaQuery* query = queries->at(i);

        if (mediaTypeMatch(query->mediaType())) {
            const Vector<MediaQueryExp*>* exps = query->expressions();
            // iterate through expressions, stop if any of them eval to false
            // (AND semantics)
            size_t j = 0;
            for (; j < exps->size() && eval(exps->at(j)); ++j) /* empty*/;

            // assume true if we are at the end of the list,
            // otherwise assume false
            result = applyRestrictor(query->restrictor(), exps->size() == j);
        } else
            result = applyRestrictor(query->restrictor(), false);
    }

    return result;
}

static bool parseAspectRatio(CSSValue* value, int& h, int& v)
{
    if (value->isValueList()){
        CSSValueList* valueList = static_cast<CSSValueList*>(value);
        if (valueList->length() == 3) {
            CSSValue* i0 = valueList->item(0);
            CSSValue* i1 = valueList->item(1);
            CSSValue* i2 = valueList->item(2);
            if (i0->isPrimitiveValue() && static_cast<CSSPrimitiveValue*>(i0)->primitiveType() == CSSPrimitiveValue::CSS_NUMBER
                && i1->isPrimitiveValue() && static_cast<CSSPrimitiveValue*>(i1)->primitiveType() == CSSPrimitiveValue::CSS_STRING
                && i2->isPrimitiveValue() && static_cast<CSSPrimitiveValue*>(i2)->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) {
                String str = static_cast<CSSPrimitiveValue*>(i1)->getStringValue();
                if (!str.isNull() && str.length() == 1 && str[0] == '/') {
                    h = static_cast<CSSPrimitiveValue*>(i0)->getIntValue(CSSPrimitiveValue::CSS_NUMBER);
                    v = static_cast<CSSPrimitiveValue*>(i2)->getIntValue(CSSPrimitiveValue::CSS_NUMBER);
                    return true;
                }
            }
        }
    }
    return false;
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

static bool numberValue(CSSValue* value, float& result)
{
    if (value->isPrimitiveValue()
        && static_cast<CSSPrimitiveValue*>(value)->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) {
        result = static_cast<CSSPrimitiveValue*>(value)->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
        return true;
    }
    return false;
}

static bool colorMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    int bitsPerComponent = screenDepthPerComponent(page->mainFrame()->view());
    float number;
    if (value)
        return numberValue(value, number) && compareValue(bitsPerComponent, static_cast<int>(number), op);

    return bitsPerComponent != 0;
}

static bool monochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    if (!screenIsMonochrome(page->mainFrame()->view()))
        return false;

    return colorMediaFeatureEval(value, style, page, op);
}

static bool device_aspect_ratioMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(page->mainFrame()->view());
        int h = 0;
        int v = 0;
        if (parseAspectRatio(value, h, v))
            return v != 0  && compareValue(static_cast<int>(sg.width()) * v, static_cast<int>(sg.height()) * h, op);
        return false;
    }

    // ({,min-,max-}device-aspect-ratio)
    // assume if we have a device, its aspect ratio is non-zero
    return true;
}

static bool device_pixel_ratioMediaFeatureEval(CSSValue *value, RenderStyle* style, Page* page, MediaFeaturePrefix op)
{
    if (value)
        return value->isPrimitiveValue() && compareValue(page->chrome()->scaleFactor(), static_cast<CSSPrimitiveValue*>(value)->getFloatValue(), op);

    return page->chrome()->scaleFactor() != 0;
}

static bool gridMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    // if output device is bitmap, grid: 0 == true
    // assume we have bitmap device
    float number;
    if (value && numberValue(value, number))
        return compareValue(static_cast<int>(number), 0, op);
    return false;
}

static bool device_heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(page->mainFrame()->view());
        return value->isPrimitiveValue() && compareValue(static_cast<int>(sg.height()), static_cast<CSSPrimitiveValue*>(value)->computeLengthInt(style), op);
    }
    // ({,min-,max-}device-height)
    // assume if we have a device, assume non-zero
    return true;
}

static bool device_widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    if (value) {
        FloatRect sg = screenRect(page->mainFrame()->view());
        return value->isPrimitiveValue() && compareValue(static_cast<int>(sg.width()), static_cast<CSSPrimitiveValue*>(value)->computeLengthInt(style), op);
    }
    // ({,min-,max-}device-width)
    // assume if we have a device, assume non-zero
    return true;
}

static bool heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    FrameView* view = page->mainFrame()->view();
    
    if (value)
        return value->isPrimitiveValue() && compareValue(view->visibleHeight(), static_cast<CSSPrimitiveValue*>(value)->computeLengthInt(style), op);

    return view->visibleHeight() != 0;
}

static bool widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix op)
{
    FrameView* view = page->mainFrame()->view();
    
    if (value)
        return value->isPrimitiveValue() && compareValue(view->visibleWidth(), static_cast<CSSPrimitiveValue*>(value)->computeLengthInt(style), op);

    return view->visibleWidth() != 0;
}

// rest of the functions are trampolines which set the prefix according to the media feature expression used

static bool min_colorMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return colorMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_colorMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return colorMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_monochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return monochromeMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_monochromeMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return monochromeMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_device_aspect_ratioMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_aspect_ratioMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_device_aspect_ratioMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_aspect_ratioMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_device_pixel_ratioMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_pixel_ratioMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_device_pixel_ratioMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_pixel_ratioMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return heightMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return heightMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return widthMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return widthMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_device_heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_heightMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_device_heightMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_heightMediaFeatureEval(value, style, page, MaxPrefix);
}

static bool min_device_widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_widthMediaFeatureEval(value, style, page, MinPrefix);
}

static bool max_device_widthMediaFeatureEval(CSSValue* value, RenderStyle* style, Page* page,  MediaFeaturePrefix /*op*/)
{
    return device_widthMediaFeatureEval(value, style, page, MaxPrefix);
}

static void createFunctionMap()
{
    // Create the table.
    gFunctionMap = new FunctionMap;
#define ADD_TO_FUNCTIONMAP(name, str)  \
    gFunctionMap->set(name##MediaFeature.impl(), name##MediaFeatureEval);
    CSS_MEDIAQUERY_NAMES_FOR_EACH_MEDIAFEATURE(ADD_TO_FUNCTIONMAP);
#undef ADD_TO_FUNCTIONMAP
}

bool MediaQueryEvaluator::eval(const MediaQueryExp* expr) const
{
    if (!m_page || !m_style)
        return m_expResult;

    if (!gFunctionMap)
        createFunctionMap();

    // call the media feature evaluation function. Assume no prefix
    // and let trampoline functions override the prefix if prefix is
    // used
    EvalFunc func = gFunctionMap->get(expr->mediaFeature().impl());
    if (func)
        return func(expr->value(), m_style, m_page, NoPrefix);

    return false;
}

} // namespace
