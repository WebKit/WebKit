namespace khtml {
    class StyleListImpl;
    class RenderCanvas;
}

namespace DOM {
    class EventTargetNodeImpl;
}

namespace KDOM {
    using namespace DOM;
    using namespace khtml;
    typedef khtml::StyleListImpl CSSStyleSelectorList;
    typedef EventTargetNodeImpl EventTargetImpl;
}

#include "Shared.h"
#include "AtomicString.h"
#include "QualifiedName.h"
