#include "config.h"
#include "css_mediaqueryimpl.h"
#include "cssparser.h"
#include "css_base.h"
#include "css_valueimpl.h"

using namespace DOM;
using namespace khtml;

MediaQueryExpImpl::MediaQueryExpImpl(int mediaFeatureId, DOM::ValueList* valueList)
    : m_mediaFeatureId(mediaFeatureId)
    , m_value(0)
{   
    if (valueList) {

        if ( valueList->numValues == 1 ) {
            DOM::Value* value = valueList->current();

            if ( value->id != 0 ) {
                m_value = new CSSPrimitiveValueImpl( value->id );
            } else if ( value->unit == CSSPrimitiveValue::CSS_STRING )
                m_value = new CSSPrimitiveValueImpl( domString( value->string ),
                                                     (CSSPrimitiveValue::UnitTypes) value->unit );
            else if ( value->unit >= CSSPrimitiveValue::CSS_NUMBER &&
                      value->unit <= CSSPrimitiveValue::CSS_KHZ ) {
                m_value = new CSSPrimitiveValueImpl( value->fValue,
                                                     (CSSPrimitiveValue::UnitTypes) value->unit );
            }
            valueList->next();
        } else if ( valueList->numValues > 1) {
            // create list of values
            // currently accepts only <integer>/<integer>

            CSSValueListImpl* list = new CSSValueListImpl();
            Value* value = 0;
            bool isValid = true;

            while ((value = valueList->current()) && isValid) {
                if ( value->unit == Value::Operator && value->iValue == '/' ) {
                    list->append(new CSSPrimitiveValueImpl("/", CSSPrimitiveValue::CSS_STRING));
                } else if (value->unit == CSSPrimitiveValue::CSS_NUMBER) {
                    list->append(new CSSPrimitiveValueImpl(value->fValue, CSSPrimitiveValue::CSS_NUMBER));
                } else {
                    isValid = false;
                }
                value = valueList->next();
            }
            if (isValid)
                m_value = list;
            else
                delete(list);

        }
    }
}

MediaQueryExpImpl::~MediaQueryExpImpl()
{
    delete (m_value);
}

int MediaQueryExpImpl::mediaFeature() const
{
    return m_mediaFeatureId;
}

CSSValueImpl* MediaQueryExpImpl::value() const
{
    return m_value;
}

MediaQueryImpl::MediaQueryImpl(Restrictor r, const DOM::DOMString &mediaType, MediaQueryExpListImpl* exprs)
    : m_restrictor(r)
    , m_mediaType(mediaType)
    , m_expressions(exprs)
{
}

MediaQueryImpl::~MediaQueryImpl()
{
    delete(m_expressions);
}

MediaQueryImpl::Restrictor MediaQueryImpl::restrictor() const
{
    return m_restrictor;
}

MediaQueryExpListImpl* MediaQueryImpl::expressions() const
{
    return m_expressions;
}

QString MediaQueryImpl::mediaType() const
{
    return m_mediaType.qstring();
}

bool MediaQueryImpl::operator==(const MediaQueryImpl& other) const
{
    // XXX: case sensitiveness?
    if (m_restrictor != other.m_restrictor || m_mediaType != other.m_mediaType)
        return false;

    QPtrListIterator<MediaQueryExpImpl> exp_it(*m_expressions->list());
    QPtrListIterator<MediaQueryExpImpl> oexp_it(*other.m_expressions->list());
    MediaQueryExpImpl* exp = 0;
    MediaQueryExpImpl* oexp = 0;
    for (exp = exp_it.current(), oexp = oexp_it.current(); exp && oexp; exp = ++exp_it, oexp = ++oexp_it) {
        if (!(*exp == *oexp) )
            return false;
    }

    return (!exp && !oexp);
}
