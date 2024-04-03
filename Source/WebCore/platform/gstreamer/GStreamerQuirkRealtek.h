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

class GStreamerQuirkRealtek final : public GStreamerQuirk {
public:
    GStreamerQuirkRealtek();
    const char* identifier() final { return "Realtek"; }

    GstElement* createWebAudioSink() final;
    void configureElement(GstElement*, const OptionSet<ElementRuntimeCharacteristics>&) final;
    std::optional<bool> isHardwareAccelerated(GstElementFactory*) final;
    Vector<String> disallowedWebAudioDecoders() const final { return m_disallowedWebAudioDecoders; }
    bool shouldParseIncomingLibWebRTCBitStream() const final { return false; }

private:
    Vector<String> m_disallowedWebAudioDecoders;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
