/*
 * Copyright (C) 2022 Igalia S.L
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

#if USE(GSTREAMER)

#include "GRefPtrGStreamer.h"
#include "IntSize.h"
#include <wtf/Forward.h>
#include <wtf/text/CStringView.h>

namespace WebCore {

namespace GStreamerCodecUtilities {

std::pair<CStringView, String> parseH264ProfileAndLevel(const String& codec);
CStringView parseHEVCProfile(const String& codec);
std::pair<GRefPtr<GstCaps>, GRefPtr<GstCaps>> capsFromCodecString(const String&, const IntSize&, std::optional<double> frameRate = std::nullopt);

} // namespace GStreamerCodecUtilities

} // namespace WebCore

#endif // USE(GSTREAMER)
