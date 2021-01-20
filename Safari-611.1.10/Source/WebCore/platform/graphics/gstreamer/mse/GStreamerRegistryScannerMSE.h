/*
 * Copyright (C) 2019 Igalia S.L
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
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if USE(GSTREAMER) && ENABLE(MEDIA_SOURCE)

#include "GStreamerRegistryScanner.h"

#include <wtf/Forward.h>

namespace WebCore {

class GStreamerRegistryScannerMSE : public GStreamerRegistryScanner {
public:
    static GStreamerRegistryScannerMSE& singleton();
    static void getSupportedDecodingTypes(HashSet<String, ASCIICaseInsensitiveHash>&);
    GStreamerRegistryScannerMSE();
};

} // namespace WebCore

#endif // USE(GSTREAMER)
