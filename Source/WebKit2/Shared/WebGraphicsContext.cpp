/*
 *  WebGraphicsContext.cpp
 *  WebKit2
 *
 *  Created by Sam Weinig on 2/1/11.
 *  Copyright 2011 Apple Inc. All rights reserved.
 *
 */

#include "config.h"
#include "WebGraphicsContext.h"

using namespace WebCore;

namespace WebKit {

WebGraphicsContext::WebGraphicsContext(GraphicsContext* graphicsContext)
#if PLATFORM(CG)
    : m_platformContext(graphicsContext->platformContext())
#endif
{
}

} // namespace WebKit
