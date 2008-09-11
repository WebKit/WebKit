#ifndef NodeRenderStyle_h
#define NodeRenderStyle_h

#include "RenderObject.h"
#include "RenderStyle.h"
#include "Node.h"

namespace WebCore {

inline RenderStyle* Node::renderStyle() const
{
    return m_renderer ? m_renderer->style() : privateRenderStyle();
}

}
#endif
