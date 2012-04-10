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

#ifndef SurfaceUpdateInfo_h
#define SurfaceUpdateInfo_h

#include "ShareableSurface.h"
#include <WebCore/IntRect.h>
#include <wtf/Noncopyable.h>

namespace CoreIPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class SurfaceUpdateInfo {
    WTF_MAKE_NONCOPYABLE(SurfaceUpdateInfo);

public:
    SurfaceUpdateInfo() { }

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, SurfaceUpdateInfo&);

    // The rect to be updated.
    WebCore::IntRect updateRect;

    // The page scale factor used to render this update.
    float scaleFactor;

    // The handle of the shareable bitmap containing the updates. Will be null if there are no updates.
    ShareableSurface::Handle surfaceHandle;

    // The offset in the bitmap where the rendered contents are.
    WebCore::IntPoint surfaceOffset;
};

} // namespace WebKit

#endif // UpdateInfo_h
