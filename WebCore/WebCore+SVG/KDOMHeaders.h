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

#include "Shared.h"
#include "AtomicString.h"
#include "QualifiedName.h"
