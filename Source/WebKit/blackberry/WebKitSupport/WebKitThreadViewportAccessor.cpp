/*
 * Copyright (C) 2012, 2013 Research In Motion Limited. All rights reserved.
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

    double scaleFactor = scale();

    if (scaleFactor != 1.0) {
        // Round down to avoid showing partially rendered pixels.
        IntSize size = documentContentsSize();
        return IntSize(
            floorf(size.width() * scaleFactor),
            floorf(size.height() * scaleFactor));
    }

    return documentContentsSize();
}

IntSize WebKitThreadViewportAccessor::documentContentsSize() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return m_webPagePrivate->contentsSize();
}

IntPoint WebKitThreadViewportAccessor::pixelScrollPosition() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    return roundToPixelFromDocumentContents(documentScrollPosition());
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

    return roundToDocumentFromPixelContents(pixelViewportRect()).size();
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

bool WebKitThreadViewportAccessor::cannotScrollIfHasFloatLayoutSizeRoundingError() const
{
    ASSERT(Platform::webKitThreadMessageClient()->isCurrentThread());

    // floatLayoutSizeRoundingError can cause inaccurate pixelContentsSize and result in
    // unexpected scroll only when the current scale is zoomToFitScale. We have to exclude this case.
    return m_webPagePrivate->hasFloatLayoutSizeRoundingError()
        && fabsf(m_webPagePrivate->currentScale() - m_webPagePrivate->zoomToFitScale()) < FLT_EPSILON;
}

} // namespace WebKit
} // namespace BlackBerry
