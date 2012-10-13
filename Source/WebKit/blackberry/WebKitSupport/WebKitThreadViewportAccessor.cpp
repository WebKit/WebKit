/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 */

#include "config.h"

#include "WebKitThreadViewportAccessor.h"

#include "WebPage_p.h"

#include <BlackBerryPlatformMessageClient.h>
#include <BlackBerryPlatformPrimitives.h>

using BlackBerry::Platform::IntPoint;
using BlackBerry::Platform::IntSize;
using BlackBerry::Platform::ViewportAccessor;

namespace BlackBerry {
namespace WebKit {

WebKitThreadViewportAccessor::WebKitThreadViewportAccessor(WebPagePrivate* webPagePrivate)
    : m_webPagePrivate(webPagePrivate)
{
}

IntSize WebKitThreadViewportAccessor::pixelContentsSize() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->transformedContentsSize();
}

IntSize WebKitThreadViewportAccessor::documentContentsSize() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->contentsSize();
}

IntPoint WebKitThreadViewportAccessor::pixelScrollPosition() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->transformedScrollPosition();
}

IntPoint WebKitThreadViewportAccessor::documentScrollPosition() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->scrollPosition();
}

IntSize WebKitThreadViewportAccessor::pixelViewportSize() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->transformedActualVisibleSize();
}

IntSize WebKitThreadViewportAccessor::documentViewportSize() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->actualVisibleSize();
}

IntPoint WebKitThreadViewportAccessor::destinationSurfaceOffset() const
{
    // FIXME: This should somehow get its offset from a reliable source.
    return IntPoint(0, 0);
}

double WebKitThreadViewportAccessor::scale() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->currentScale();
}

} // namespace WebKit
} // namespace BlackBerry
