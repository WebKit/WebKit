/*
 Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)

 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Library General Public
 License as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later version.

 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Library General Public License for more details.

 You should have received a copy of the GNU Library General Public License
 along with this library; see the file COPYING.LIB.  If not, write to
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#ifndef GraphicsSurface_h
#define GraphicsSurface_h

#include "GraphicsContext.h"
#include "IntRect.h"
#include "OwnPtr.h"
#include "PassOwnPtr.h"
#include "RefCounted.h"
#include "RefPtr.h"

#if USE(GRAPHICS_SURFACE)

#if OS(DARWIN)
typedef struct __IOSurface* IOSurfaceRef;
typedef IOSurfaceRef PlatformGraphicsSurface;
#endif

namespace WebCore {

class GraphicsSurface : public RefCounted<GraphicsSurface> {
public:
    enum Flag {
        SupportsAlpha = 0x01,
        SupportsSoftwareWrite = 0x02,
        SupportsSoftwareRead = 0x04,
        SupportsTextureTarget = 0x08,
        SupportsTextureSource = 0x10,
        SupportsCopyToTexture = 0x20,
        SupportsCopyFromTexture = 0x40,
        SupportsSharing = 0x80
    };

    enum LockOption {
        RetainPixels = 0x01,
        ReadOnly = 0x02
    };

    typedef int Flags;
    typedef int LockOptions;

    Flags flags() const { return m_flags; }
    PlatformGraphicsSurface platformSurface() const { return m_platformSurface; }
    IntSize size() const { return m_size; }

    static PassRefPtr<GraphicsSurface> create(const IntSize&, Flags);
    static PassRefPtr<GraphicsSurface> create(const IntSize&, Flags, uint32_t token);
    void copyToGLTexture(uint32_t target, uint32_t texture, const IntRect& targetRect, const IntPoint& sourceOffset);
    uint32_t exportToken();
    PassOwnPtr<GraphicsContext> beginPaint(const IntRect&, LockOptions);
    PassRefPtr<Image> createReadOnlyImage(const IntRect&);

protected:
    bool platformCreate(const IntSize&, Flags);
    bool platformImport(uint32_t);
    uint32_t platformExport();
    void platformDestroy();

    char* platformLock(const IntRect&, int* stride, LockOptions);
    void platformUnlock();
    void platformCopyToGLTexture(uint32_t target, uint32_t texture, const IntRect&, const IntPoint&);

    PassOwnPtr<GraphicsContext> platformBeginPaint(const IntSize&, char* bits, int stride);

protected:
    LockOptions m_lockOptions;
    Flags m_flags;

private:
    GraphicsSurface(const IntSize&, Flags);

#if PLATFORM(QT)
    static void didReleaseImage(void*);
#endif

private:
    IntSize m_size;
    PlatformGraphicsSurface m_platformSurface;
    uint32_t m_texture;
    uint32_t m_fbo;
};

}
#endif // USE(GRAPHICS_SURFACE)

#endif // GraphicsSurface_h
