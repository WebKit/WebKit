/*
 * Copyright (C) 2025 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && OS(ANDROID)
#include "GraphicsContextGLTextureMapperANGLE.h"
#include <wtf/android/RefPtrAndroid.h>

namespace WebCore {

class GraphicsContextGLTextureMapperAndroid final : public GraphicsContextGLTextureMapperANGLE {
public:
    static RefPtr<GraphicsContextGLTextureMapperAndroid> create(GraphicsContextGLAttributes&&);

#if ENABLE(WEBXR)
    GCGLExternalImage createExternalImage(ExternalImageSource&&, GCGLenum internalFormat, GCGLint layer) final;
    void bindExternalImage(GCGLenum target, GCGLExternalImage) final;
    bool enableRequiredWebXRExtensions() final;
#endif // ENABLE(WEBXR)

private:
    explicit GraphicsContextGLTextureMapperAndroid(GraphicsContextGLAttributes&& attributes)
        : GraphicsContextGLTextureMapperANGLE(WTFMove(attributes))
    {
    }

    bool platformInitializeExtensions() override;
};

} // namespace WebCore

#endif // ENABLE(WEBGL) && USE(COORDINATED_GRAPHICS) && OS(ANDROID)
