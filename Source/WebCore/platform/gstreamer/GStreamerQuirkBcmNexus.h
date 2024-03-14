/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
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

#include "GStreamerQuirks.h"

namespace WebCore {

class GStreamerQuirkBcmNexus final : public GStreamerQuirk {
public:
    GStreamerQuirkBcmNexus();
    const char* identifier() final { return "BcmNexus"; }

    std::optional<bool> isHardwareAccelerated(GstElementFactory*) final;
    std::optional<GstElementFactoryListType> audioVideoDecoderFactoryListType() const final { return GST_ELEMENT_FACTORY_TYPE_PARSER; }
    Vector<String> disallowedWebAudioDecoders() const final { return m_disallowedWebAudioDecoders; }

private:
    Vector<String> m_disallowedWebAudioDecoders;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
