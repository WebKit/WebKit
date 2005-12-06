namespace khtml {
    class StyleListImpl;
    class RenderCanvas;
}

namespace DOM {
    class NodeImpl;
}

namespace KDOM {
    using namespace DOM;
    using namespace khtml;
    typedef khtml::StyleListImpl CSSStyleSelectorList;
    typedef NodeImpl EventTargetImpl;
}

#include "khtml/misc/shared.h"
#include "khtml/xml/dom_atomicstring.h"
#include "khtml/xml/dom_qname.h"
