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

#include "config.h"
#include "SurfaceUpdateInfo.h"

#include "WebCoreArgumentCoders.h"

namespace WebKit {

void SurfaceUpdateInfo::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(updateRect);
    encoder->encode(scaleFactor);
    encoder->encode(surfaceHandle);
    encoder->encode(surfaceOffset);
}

bool SurfaceUpdateInfo::decode(CoreIPC::ArgumentDecoder* decoder, SurfaceUpdateInfo& result)
{
    if (!decoder->decode(result.updateRect))
        return false;
    if (!decoder->decode(result.scaleFactor))
        return false;
    if (!decoder->decode(result.surfaceHandle))
        return false;
    if (!decoder->decode(result.surfaceOffset))
        return false;

    return true;
}

} // namespace WebKit
