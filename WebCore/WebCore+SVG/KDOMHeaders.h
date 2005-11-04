#include "khtml/dom/dom_node.h"
#include "khtml/dom/dom_string.h"
#include "css_value.h"

class KHTMLPart;
class KHTMLView;

namespace khtml {
    class StyleListImpl;
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

//
//namespace KDOM = DOM;

#include "KDOMStubClasses.h"

#include "khtml/khtml_part.h"

#include "khtml/config.h"
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
#include "khtml/editing/append_node_command.h"
#include "khtml/editing/apply_style_command.h"
#include "khtml/editing/break_blockquote_command.h"
#include "khtml/editing/composite_edit_command.h"
#include "khtml/editing/delete_from_text_node_command.h"
#include "khtml/editing/delete_selection_command.h"
#include "khtml/editing/edit_actions.h"
#include "khtml/editing/edit_command.h"
#include "khtml/editing/html_interchange.h"
#include "khtml/editing/htmlediting.h"
#include "khtml/editing/insert_into_text_node_command.h"
#include "khtml/editing/insert_line_break_command.h"
#include "khtml/editing/insert_node_before_command.h"
#include "khtml/editing/insert_paragraph_separator_command.h"
#include "khtml/editing/insert_text_command.h"
#include "khtml/editing/join_text_nodes_command.h"
#include "khtml/editing/jsediting.h"
#include "khtml/editing/markup.h"
#include "khtml/editing/merge_identical_elements_command.h"
#include "khtml/editing/move_selection_command.h"
#include "khtml/editing/rebalance_whitespace_command.h"
#include "khtml/editing/remove_css_property_command.h"
#include "khtml/editing/remove_node_attribute_command.h"
#include "khtml/editing/remove_node_command.h"
#include "khtml/editing/remove_node_preserving_children_command.h"
#include "khtml/editing/replace_selection_command.h"
#include "khtml/editing/SelectionController.h"
#include "khtml/editing/set_node_attribute_command.h"
#include "khtml/editing/split_element_command.h"
#include "khtml/editing/split_text_node_command.h"
#include "khtml/editing/split_text_node_containing_element_command.h"
#include "khtml/editing/text_affinity.h"
#include "khtml/editing/text_granularity.h"
#include "khtml/editing/typing_command.h"
#include "khtml/editing/visible_position.h"
#include "khtml/editing/visible_range.h"
#include "khtml/editing/visible_text.h"
#include "khtml/editing/visible_units.h"
#include "khtml/editing/wrap_contents_in_dummy_span_command.h"
#include "khtml/khtml_events.h"
#include "khtml/khtmlpart_p.h"
#include "khtml/khtmlview.h"
#include "khtml/misc/arena.h"
#include "khtml/misc/decoder.h"
#include "khtml/misc/formdata.h"
#include "khtml/misc/hashfunctions.h"
#include "khtml/misc/helper.h"
#include "khtml/misc/khtmldata.h"
#include "khtml/misc/khtmllayout.h"
#include "khtml/misc/loader.h"
#include "khtml/misc/loader_client.h"
#include "khtml/misc/shared.h"
#include "khtml/misc/stringit.h"
#include "khtml/rendering/bidi.h"
#include "khtml/rendering/font.h"
#include "khtml/rendering/render_object.h"
#include "khtml/rendering/render_replaced.h"
#include "khtml/rendering/render_style.h"
#include "khtml/rendering/table_layout.h"
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
#include "khtml/xsl/xslt_processorimpl.h"

// All temporary:
#include "WebCore+SVG/kdom.h"
#include "WebCore+SVG/kdomevents.h"
#include "WebCore+SVG/kdomcss.h"
#include "WebCore+SVG/kdomrange.h"
#include "WebCore+SVG/kdomtraversal.h"
#include "WebCore+SVG/kdomls.h"
#include "WebCore+SVG/Namespace.h"
#include "ksvg2/svg/SVGNames.h"
#include "WebCore+SVG/RGBColorImpl.h"
#include "WebCore+SVG/KDOMSettings.h"

