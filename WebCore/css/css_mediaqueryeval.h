#ifndef CSS_css_mediaqueryeval_h
#define CSS_css_mediaqueryeval_h

#include <qstring.h>
#include <qvaluelist.h>

#include "css/css_mediaqueryimpl.h"

namespace WebCore {
  class FrameView;
  class RenderStyle;
}

namespace DOM {
  class MediaListImpl;

/**
 * Class that evaluates css media queries as defined in
 * CSS3 Module "Media Queries" (http://www.w3.org/TR/css3-mediaqueries/)
 * Special constructors are needed, if simple media queries are to be
 * evaluated without knowledge of the medium features. This can happen
 * for example when parsing UA stylesheets, if evaluation is done
 * right after parsing.
 *
 * the boolean parameter is used to approximate results of evaluation, if
 * the device characteristics are not known. This can be used to prune the loading
 * of stylesheets to only those which are probable to match.
 */
class MediaQueryEvaluator
{
public:
    /** Creates evaluator which evaluates only simple media queries
     *  Evaluator returns true for "all", and returns value of \mediaFeatureResult
     *  for any media features
     */
    MediaQueryEvaluator(bool mediaFeatureResult = false);

    /** Creates evaluator which evaluates only simple media queries
     *  Evaluator  returns true for acceptedMediaType, but not any media features
     */
    MediaQueryEvaluator(const QString& acceptedMediaType, bool mediaFeatureResult = false);

    /** Creates evaluator which evaluates full media queries
     */
    MediaQueryEvaluator(const QString& acceptedMediaType, FrameView* view,
                        khtml::RenderStyle* style);

    ~MediaQueryEvaluator();

    bool mediaTypeMatch(const QString& mediaTypeToMatch) const;

    /** Evaluates a list of media queries */
    bool eval(const DOM::MediaListImpl* query) const;

    /** Evaluates media query subexpression, ie "and (media-feature: value)" part */
    bool eval(const DOM::MediaQueryExpImpl* expr) const;

private:

    QString m_mediaType;
    WebCore::FrameView* m_view; // not owned
    WebCore::RenderStyle* m_style; // not owned
    bool m_expResult;
};

} // namespace
#endif
