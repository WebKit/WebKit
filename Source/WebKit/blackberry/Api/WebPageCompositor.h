/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
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

#ifndef WebPageCompositor_h
#define WebPageCompositor_h

#include "BlackBerryGlobal.h"

#include <BlackBerryPlatformGLES2Context.h>
#include <BlackBerryPlatformGraphics.h>
#include <BlackBerryPlatformPrimitives.h>

namespace BlackBerry {
namespace WebKit {

class WebPage;
class WebPageCompositorClient;
class WebPageCompositorPrivate;

class BLACKBERRY_EXPORT WebPageCompositor {
public:
    WebPageCompositor(WebPage*, WebPageCompositorClient*);
    virtual ~WebPageCompositor();

    WebPageCompositorClient* client() const;

    void prepareFrame(Platform::Graphics::GLES2Context*, double timestamp);

    // FIXME: dstRect should be a Platform::TransformationMatrix instead. PR142628
    void render(Platform::Graphics::GLES2Context*, const Platform::IntRect& dstRect, const Platform::IntRect& contents);

    void cleanup(Platform::Graphics::GLES2Context*);

    void contextLost();

private:
    WebPageCompositorPrivate *d;
};

} // namespace WebKit
} // namespace BlackBerry

#endif // WebPageCompositor_h
