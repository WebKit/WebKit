/*
 * Copyright (C) 2017 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "GLContextEGL.h"

#if USE(EGL) && PLATFORM(WPE)

#include "PlatformDisplayWPE.h"

namespace WebCore {

std::unique_ptr<GLContextEGL> GLContextEGL::createWPEContext(PlatformDisplay& platformDisplay, EGLContext sharingContext)
{
    auto offscreenTarget = downcast<PlatformDisplayWPE>(platformDisplay).createEGLOffscreenTarget();

    std::unique_ptr<GLContextEGL> context;
    if (offscreenTarget->nativeWindow())
        context = createWindowContext(offscreenTarget->nativeWindow(), platformDisplay, sharingContext);
    if (!context)
        context = createPbufferContext(platformDisplay, sharingContext);

    // FIXME: if available, we could also fallback to the surfaceless-based GLContext
    // before falling back to the pbuffer-based one.

    if (context)
        context->m_wpeTarget = WTFMove(offscreenTarget);
    return context;
}

void GLContextEGL::destroyWPETarget()
{
    m_wpeTarget = nullptr;
}

} // namespace WebCore

#endif // USE(EGL) && PLATFORM(WPE)
