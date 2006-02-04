#include "config.h"
#include "css_mediaqueryeval.h"
#include "css_stylesheetimpl.h"
#include "cssmediafeatures.h"
#include "css_valueimpl.h"
#include "rendering/render_style.h"
#include "page/FrameView.h"
#include "Screen.h"


using namespace WebCore;
using namespace DOM;

MediaQueryEvaluator:: MediaQueryEvaluator(bool mediaFeatureResult)
    : m_view(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator:: MediaQueryEvaluator(const QString& acceptedMediaType, bool mediaFeatureResult)
    : m_mediaType(acceptedMediaType)
    , m_view(0)
    , m_style(0)
    , m_expResult(mediaFeatureResult)
{
}

MediaQueryEvaluator:: MediaQueryEvaluator(const QString& acceptedMediaType, FrameView* view, RenderStyle* style)
    : m_mediaType(acceptedMediaType.lower())
    , m_view(view)
    , m_style(style)
    , m_expResult(false) // doesn't matter
{
}

MediaQueryEvaluator::~MediaQueryEvaluator()
{
}

bool MediaQueryEvaluator::mediaTypeMatch(const QString& mediaTypeToMatch) const
{
    return mediaTypeToMatch.isNull()
        || mediaTypeToMatch.isEmpty()
        || (mediaTypeToMatch.startsWith("all", false) && mediaTypeToMatch.length() == 3)
        || mediaTypeToMatch.lower() == m_mediaType;
}

static bool applyRestrictor(MediaQueryImpl::Restrictor r, bool value)
{
    return r == MediaQueryImpl::Not ? !value : value;
}

bool MediaQueryEvaluator::eval(const MediaListImpl* mediaList) const
{
    if (!mediaList)
        return true;

    QPtrListIterator<MediaQueryImpl> it(*mediaList->mediaQueries());
    if (!it.current())
        return true; // empty query list evaluates to true

    // iterate over queries, stop if any of them eval to true (OR semantics)
    bool result = false;
    for (MediaQueryImpl* query = it.current(); query && (result==false); query = ++it) {

        if (mediaTypeMatch(query->mediaType())) {
            QPtrListIterator<MediaQueryExpImpl> exp_it(*query->expressions()->list());
            // iterate through expressions, stop if any of them eval to false (AND semantics)
            MediaQueryExpImpl* exp = 0;
            for (exp = exp_it.current(); exp && eval(exp); exp = ++exp_it) /* empty*/;

            // assume true if we are at the end of the list, otherwise assume false
            result = applyRestrictor(query->restrictor(), !exp);
        } else {
            result = applyRestrictor(query->restrictor(), false);
        }
    }
    return result;
}

static bool parseAspectRatio(CSSValueImpl* value, int* a, int* b)
{
    if (value->isValueList() ){
        CSSValueListImpl* valueList = static_cast<CSSValueListImpl*>(value);
        if (valueList->length() == 3 ) {
            CSSValueImpl* i0 = valueList->item(0);
            CSSValueImpl* i1 = valueList->item(1);
            CSSValueImpl* i2 = valueList->item(2);
            if (i0->isPrimitiveValue() && static_cast<CSSPrimitiveValueImpl*>(i0)->primitiveType() == CSSPrimitiveValue::CSS_NUMBER
                && i1->isPrimitiveValue() && static_cast<CSSPrimitiveValueImpl*>(i1)->primitiveType() == CSSPrimitiveValue::CSS_STRING
                && i2->isPrimitiveValue() && static_cast<CSSPrimitiveValueImpl*>(i2)->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) {
                DOMString str = static_cast<CSSPrimitiveValueImpl*>(i1)->getStringValue();
                if (!str.isNull() && str.length() == 1 && str[0] == QChar('/')) {
                    *a = (int) static_cast<CSSPrimitiveValueImpl*>(i0)->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
                    *b = (int) static_cast<CSSPrimitiveValueImpl*>(i2)->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
                    return true;
                }
            }
        }
    }
    return false;
}

bool MediaQueryEvaluator::eval(const MediaQueryExpImpl* expr) const
{
    if (!m_view || !m_style)
        return m_expResult;
    CSSValueImpl* value = expr->value();
    CSSPrimitiveValueImpl* primitive = 0;

    bool isNumberValue = false;
    double numberValue = -1;
    int primitiveLength = -1;

    if (value && value->isPrimitiveValue()) {
        primitive = static_cast<CSSPrimitiveValueImpl*>(value);
        primitiveLength = primitive->computeLength(m_style);
        if (primitive->primitiveType() == CSSPrimitiveValue::CSS_NUMBER) {
            isNumberValue = true;
            numberValue = primitive->getFloatValue(CSSPrimitiveValue::CSS_NUMBER);
        }
    }

    switch (expr->mediaFeature()) {

        case CSS_MEDIA_FEAT_MONOCHROME:
            if (!screenIsMonochrome(m_view))
                return false;
            /* fall through*/
        case CSS_MEDIA_FEAT_COLOR: {
            int bitsPerComponent = screenDepthPerComponent(m_view);
            if (value)
                return (isNumberValue && bitsPerComponent == (int)numberValue);
            else
                return bitsPerComponent != 0;
        }

        case CSS_MEDIA_FEAT_MIN_MONOCHROME: 
            if (!screenIsMonochrome(m_view))
                return false;
            /* fall through */
        case CSS_MEDIA_FEAT_MIN_COLOR: {
            int bitsPerComponent = screenDepthPerComponent(m_view);
            if (value)
                return (isNumberValue && bitsPerComponent >= (int)numberValue);
            else
                return bitsPerComponent != 0;
        }

        case CSS_MEDIA_FEAT_MAX_MONOCHROME: 
            if (!screenIsMonochrome(m_view))
                return false;
            /* fall through */
        case CSS_MEDIA_FEAT_MAX_COLOR: {
            int bitsPerComponent = screenDepthPerComponent(m_view);
            if (value)
                return (isNumberValue && bitsPerComponent <= (int)numberValue);
            else
                return bitsPerComponent != 0;
        }

        case CSS_MEDIA_FEAT_COLOR_INDEX:
        case CSS_MEDIA_FEAT_MIN_COLOR_INDEX:
        case CSS_MEDIA_FEAT_MAX_COLOR_INDEX:
            /// XXX: how to get display mode from KWQ?
            break;

        case CSS_MEDIA_FEAT_DEVICE_ASPECT_RATIO: {
            if (value) {
                IntRect sg = screenRect(m_view);
                int a;
                int b;
                if (parseAspectRatio(value, &a, &b))
                    return (b!=0) && (a*sg.height() == b*sg.width());
                else
                    return false;
            } else {
                // (device-aspect-ratio), assume if we have a device, its aspect ratio is non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MIN_DEVICE_ASPECT_RATIO: {
            if (value) {
                IntRect sg = screenRect(m_view);
                int a;
                int b;
                if (parseAspectRatio(value, &a, &b))
                    return (b!=0) && (((double)sg.width()/sg.height()) >= ((double)a/b));
                else
                    return false;
            } else {
                // (min-device-aspect-ratio), assume if we have a device, its aspect ratio is non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MAX_DEVICE_ASPECT_RATIO: {
            if (value) {
                IntRect sg = screenRect(m_view);
                int a;
                int b;
                if (parseAspectRatio(value, &a, &b))
                    return (b!=0) && (((double)sg.width()/sg.height()) <= ((double)a/b));
                else
                    return false;
            } else {
                // (max-device-aspect-ratio), assume if we have a device, its aspect ratio is non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_DEVICE_HEIGHT: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.height() == primitiveLength);
            } else {
                // (device-height), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MIN_DEVICE_HEIGHT: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.height() >= primitiveLength);
            } else {
                // (min-device-height), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MAX_DEVICE_HEIGHT: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.height() <= primitiveLength);
            } else {
                // (max-device-height), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_DEVICE_WIDTH: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.width() == primitiveLength);
            } else {
                // (device-width), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MIN_DEVICE_WIDTH: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.width() >= primitiveLength);
            } else {
                // (min-device-width), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_MAX_DEVICE_WIDTH: {
            if (value) {
                IntRect sg = screenRect(m_view);
                return (primitive && sg.width() <= primitiveLength);
            } else {
                // (max-device-width), assume if we have a device, assume non-zero
                return true;
            }
        }

        case CSS_MEDIA_FEAT_GRID: {
            // if output device is bitmap, grid: 0 == true
            if (value)
                return (isNumberValue &&  (int)numberValue == 0 );
            else
                return false;
        }

        case CSS_MEDIA_FEAT_HEIGHT: {
            if (value)
                return primitive && m_view->visibleHeight() == primitiveLength;
            else
                return m_view->visibleHeight() != 0;
        }

        case CSS_MEDIA_FEAT_MIN_HEIGHT: {
            if (value)
                return primitive && m_view->visibleHeight() >= primitiveLength;
            else
                return m_view->visibleHeight() != 0;
        }

        case CSS_MEDIA_FEAT_MAX_HEIGHT: {
            if (value)
                return (primitive && m_view->visibleHeight() <= primitiveLength);
            else
                return (m_view->visibleHeight() != 0);
        }

        case CSS_MEDIA_FEAT_WIDTH: {
            if (value)
                return primitive && m_view->visibleWidth() == primitiveLength;
            else
                return m_view->visibleWidth() != 0;
        }

        case CSS_MEDIA_FEAT_MIN_WIDTH: {
            if (value)
                return primitive && m_view->visibleWidth() >= primitiveLength;
            else
                return m_view->visibleWidth() != 0;
        }

        case CSS_MEDIA_FEAT_MAX_WIDTH:{
            if (value)
                return primitive && m_view->visibleWidth() <= primitiveLength;
            else
                return m_view->visibleWidth() != 0;
        }

        case CSS_MEDIA_FEAT_RESOLUTION:
        case CSS_MEDIA_FEAT_MIN_RESOLUTION:
        case CSS_MEDIA_FEAT_MAX_RESOLUTION:
            //XXX: CSS_DIMENSION doesn't seem to be supported by KHTML
            break;

        case CSS_MEDIA_FEAT_SCAN:
            // scan applies to tv media types
            return false;

        case CSS_MEDIA_FEAT_INVALID:
        default:
            break;
    }

    return false;
}
