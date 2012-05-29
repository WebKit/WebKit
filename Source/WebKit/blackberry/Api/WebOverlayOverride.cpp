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

#include "WebOverlayOverride.h"

#if USE(ACCELERATED_COMPOSITING)
#include "WebAnimation.h"
#include "WebAnimation_p.h"
#include "WebOverlay_p.h"
#include "WebString.h"

#include <BlackBerryPlatformMessageClient.h>

namespace BlackBerry {
namespace WebKit {

using namespace WebCore;

WebOverlayOverride::WebOverlayOverride(WebOverlayPrivate* d, bool owned)
    : d(d)
    , m_owned(owned)
{
}

WebOverlayOverride::~WebOverlayOverride()
{
    if (m_owned)
        delete d;
}

void WebOverlayOverride::setPosition(const Platform::FloatPoint& position)
{
    d->setPosition(position);
}

void WebOverlayOverride::setAnchorPoint(const Platform::FloatPoint& anchor)
{
    d->setAnchorPoint(anchor);
}

void WebOverlayOverride::setSize(const Platform::FloatSize& size)
{
    d->setSize(size);
}

void WebOverlayOverride::setTransform(const Platform::TransformationMatrix& transform)
{
    d->setTransform(reinterpret_cast<const TransformationMatrix&>(transform));
}

void WebOverlayOverride::setOpacity(float opacity)
{
    d->setOpacity(opacity);
}

void WebOverlayOverride::addAnimation(const WebAnimation& animation)
{
    d->addAnimation(animation.d->name, animation.d->animation.get(), animation.d->keyframes);
}

void WebOverlayOverride::removeAnimation(const WebString& name)
{
    d->removeAnimation(String(PassRefPtr<StringImpl>(name.impl())));
}

}
}
#else // USE(ACCELERATED_COMPOSITING)
namespace BlackBerry {
namespace WebKit {

WebOverlayOverride::WebOverlayOverride(WebOverlayPrivate*, bool)
{
}

WebOverlayOverride::~WebOverlayOverride()
{
}

void WebOverlayOverride::setPosition(const Platform::FloatPoint&)
{
}

void WebOverlayOverride::setAnchorPoint(const Platform::FloatPoint&)
{
}

void WebOverlayOverride::setSize(const Platform::FloatSize&)
{
}

void WebOverlayOverride::setTransform(const Platform::TransformationMatrix&)
{
}

void WebOverlayOverride::setOpacity(float)
{
}

void WebOverlayOverride::addAnimation(const WebAnimation&)
{
}

void WebOverlayOverride::removeAnimation(const WebString&)
{
}

}
}
#endif // USE(ACCELERATED_COMPOSITING)
