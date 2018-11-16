/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
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

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)

#include "GStreamerCaptureDevice.h"
#include "GStreamerCommon.h"
#include "LibWebRTCMacros.h"

#include <gst/gst.h>

namespace WebCore {

class GStreamerCapturer {
public:
    GStreamerCapturer(GStreamerCaptureDevice, GRefPtr<GstCaps>);
    GStreamerCapturer(const char* sourceFactory, GRefPtr<GstCaps>);
    ~GStreamerCapturer();

    void setupPipeline();
    virtual void play();
    virtual void stop();
    GstCaps* caps();
    void addSink(GstElement *newSink);
    GstElement* makeElement(const char* factoryName);
    virtual GstElement* createSource();
    GstElement* source() { return m_src.get();  }
    virtual const char* name() = 0;

    GstElement* sink() const { return m_sink.get(); }
    void setSink(GstElement* sink) { m_sink = adoptGRef(sink); };

    GstElement* pipeline() const { return m_pipeline.get(); }
    virtual GstElement* createConverter() = 0;

protected:
    GRefPtr<GstElement> m_sink;
    GRefPtr<GstElement> m_src;
    GRefPtr<GstElement> m_tee;
    GRefPtr<GstElement> m_capsfilter;
    GRefPtr<GstDevice> m_device;
    GRefPtr<GstCaps> m_caps;
    GRefPtr<GstElement> m_pipeline;

private:
    const char* m_sourceFactory;

};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
