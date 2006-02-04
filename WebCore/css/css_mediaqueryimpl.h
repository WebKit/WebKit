#ifndef CSS_css_mediaqueryimpl_h
#define CSS_css_mediaqueryimpl_h

#include <qvaluelist.h>
#include <qptrlist.h>

#include "dom/dom_string.h"
#include "css/css_base.h"
#include "css/css_valueimpl.h"
namespace DOM {
class ValueList;
class CSSPrimitiveValue;

class MediaQueryExpImpl {
public:
    MediaQueryExpImpl(int mediaFeatureId, ValueList* values);
    ~MediaQueryExpImpl();

    int mediaFeature() const;
    CSSValueImpl* value() const;

    bool operator==(const MediaQueryExpImpl& other) const  {
        return (other.m_mediaFeatureId == m_mediaFeatureId)
            && ((!other.m_value && !m_value)
                || (other.m_value && m_value && other.m_value->cssText() == m_value->cssText()));
    }

private:
    int m_mediaFeatureId;
    CSSValueImpl* m_value;
};

class MediaQueryExpListImpl
{
public:
    MediaQueryExpListImpl() { exps.setAutoDelete(true); }

    void append(MediaQueryExpImpl* newExp) { exps.append(newExp); }
    const QPtrList<MediaQueryExpImpl>* list() const { return &exps; }

private:
    QPtrList<MediaQueryExpImpl> exps;
};


class MediaQueryImpl {
public:
    enum Restrictor{
        Only, Not, None
    };

    MediaQueryImpl(Restrictor r, const DOMString &mediaType, MediaQueryExpListImpl* exprs);
    ~MediaQueryImpl();

    Restrictor restrictor() const;
    MediaQueryExpListImpl* expressions() const;
    QString mediaType() const;

    bool operator==(const MediaQueryImpl& other) const;

 private:
    Restrictor m_restrictor;
    DOMString m_mediaType;
    MediaQueryExpListImpl* m_expressions;
};

} // namespace

#endif
