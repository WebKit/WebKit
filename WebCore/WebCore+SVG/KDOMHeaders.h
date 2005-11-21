#include "khtml/dom/dom_node.h"
#include "khtml/dom/dom_string.h"
#include "css_value.h"

class KHTMLPart;
class KHTMLView;

namespace khtml {
    class StyleListImpl;
    class RenderCanvas;
}

namespace KDOM {
    using namespace DOM;
    using namespace khtml;
    using ::KHTMLPart;
    using ::KHTMLView;
    typedef khtml::StyleListImpl CSSStyleSelectorList;
}

namespace KSVG {
    using ::KHTMLPart;
}

typedef khtml::RenderCanvas KCanvas;

#include "KDOMStubClasses.h"

#include "khtml/khtml_part.h"
#include "khtml/css/css_base.h"
#include "khtml/css/css_computedstyle.h"
#include "khtml/css/css_ruleimpl.h"
#include "khtml/css/css_stylesheetimpl.h"
#include "khtml/css/css_valueimpl.h"
#include "khtml/css/csshelper.h"
#include "khtml/css/cssparser.h"
#include "khtml/css/cssstyleselector.h"
#include "khtml/dom/css_rule.h"
#include "khtml/dom/css_stylesheet.h"
#include "khtml/dom/css_value.h"
#include "khtml/dom/dom2_events.h"
#include "khtml/dom/dom2_range.h"
#include "khtml/dom/dom2_traversal.h"
#include "khtml/dom/dom_exception.h"
#include "khtml/dom/dom_misc.h"
#include "khtml/dom/dom_string.h"
#include "khtml/khtml_events.h"
#include "khtml/khtmlpart_p.h"
#include "khtml/khtmlview.h"
#include "khtml/misc/arena.h"
#include "khtml/misc/decoder.h"
#include "khtml/misc/helper.h"
#include "khtml/misc/loader.h"
#include "khtml/misc/loader_client.h"
#include "khtml/misc/shared.h"
#include "khtml/misc/stringit.h"
#include "khtml/rendering/render_object.h"
#include "khtml/rendering/render_style.h"
#include "khtml/rendering/render_canvas.h"
#include "khtml/xml/dom2_eventsimpl.h"
#include "khtml/xml/dom2_rangeimpl.h"
#include "khtml/xml/dom2_traversalimpl.h"
#include "khtml/xml/dom2_viewsimpl.h"
#include "khtml/xml/dom_atomicstring.h"
#include "khtml/xml/dom_atomicstringlist.h"
#include "khtml/xml/dom_docimpl.h"
#include "khtml/xml/dom_elementimpl.h"
#include "khtml/xml/dom_nodeimpl.h"
#include "khtml/xml/dom_position.h"
#include "khtml/xml/dom_qname.h"
#include "khtml/xml/dom_stringimpl.h"
#include "khtml/xml/dom_textimpl.h"
#include "khtml/xml/dom_xmlimpl.h"
#include "khtml/xml/EventNames.h"
#include "khtml/xml/xml_tokenizer.h"
#include "khtml/xsl/xsl_stylesheetimpl.h"

// All temporary:
#include "WebCore+SVG/kdom.h"
#include "WebCore+SVG/kdomevents.h"
#include "WebCore+SVG/kdomcss.h"
#include "WebCore+SVG/kdomrange.h"
#include "WebCore+SVG/kdomtraversal.h"
#include "WebCore+SVG/kdomls.h"
#include "SVGNames.h"
#include "WebCore+SVG/RGBColorImpl.h"
#include "WebCore+SVG/KDOMSettings.h"

