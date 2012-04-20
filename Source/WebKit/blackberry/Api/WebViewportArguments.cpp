/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebViewportArguments.h"

#include "ViewportArguments.h"

namespace BlackBerry {
namespace WebKit {

WebViewportArguments::WebViewportArguments()
    :d(new WebCore::ViewportArguments(WebCore::ViewportArguments::ViewportMeta))
{
}

WebViewportArguments::~WebViewportArguments()
{
    delete d;
    d = 0;
}

float WebViewportArguments::initialScale() const
{
    return d->initialScale;
}

void WebViewportArguments::setInitialScale(float scale)
{
    d->initialScale = scale;
}

float WebViewportArguments::minimumScale() const
{
    return d->minimumScale;
}

void WebViewportArguments::setMinimumScale(float scale)
{
    d->minimumScale = scale;
}

float WebViewportArguments::maximumScale() const
{
    return d->maximumScale;
}

void WebViewportArguments::setMaximumScale(float scale)
{
    d->maximumScale = scale;
}

float WebViewportArguments::width() const
{
    return d->width;
}

void WebViewportArguments::setWidth(float width)
{
    d->width = width;
}

float WebViewportArguments::height() const
{
    return d->height;
}

void WebViewportArguments::setHeight(float height)
{
    d->height = height;
}

float WebViewportArguments::targetDensityDpi() const
{
    return d->targetDensityDpi;
}

void WebViewportArguments::setTargetDensityDpi(float dpi)
{
    d->targetDensityDpi = dpi;
}

float WebViewportArguments::userScalable() const
{
    return d->userScalable;
}

void WebViewportArguments::setUserScalable(float scalable)
{
    d->userScalable = scalable;
}

bool WebViewportArguments::operator==(const WebViewportArguments& other)
{
    return *d == *(other.d);
}

bool WebViewportArguments::operator!=(const WebViewportArguments& other)
{
    return !(*this == other);
}

} // namespace WebKit
} // namespace BlackBerry
