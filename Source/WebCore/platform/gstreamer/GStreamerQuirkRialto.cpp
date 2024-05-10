/*
 * Copyright 2024 RDK Management
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GStreamerQuirkRialto.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include <wtf/OptionSet.h>

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_rialto_quirks_debug);
#define GST_CAT_DEFAULT webkit_rialto_quirks_debug

GStreamerQuirkRialto::GStreamerQuirkRialto()
{
    GST_DEBUG_CATEGORY_INIT(webkit_rialto_quirks_debug, "webkitquirksrialto", 0, "WebKit Rialto Quirks");

    std::array<const char *, 2> rialtoSinks = { "rialtomsevideosink", "rialtomseaudiosink" };

    for (const auto* sink : rialtoSinks) {
        auto sinkFactory = adoptGRef(gst_element_factory_find(sink));
        if (UNLIKELY(!sinkFactory))
            continue;

        gst_object_unref(gst_plugin_feature_load(GST_PLUGIN_FEATURE(sinkFactory.get())));
        for (auto* padTemplateListElement = gst_element_factory_get_static_pad_templates(sinkFactory.get());
            padTemplateListElement; padTemplateListElement = g_list_next(padTemplateListElement)) {

            auto* padTemplate = static_cast<GstStaticPadTemplate*>(padTemplateListElement->data);
            if (padTemplate->direction != GST_PAD_SINK)
                continue;
            GRefPtr<GstCaps> templateCaps = adoptGRef(gst_static_caps_get(&padTemplate->static_caps));
            if (!templateCaps)
                continue;
            if (gst_caps_is_empty(templateCaps.get()) || gst_caps_is_any(templateCaps.get()))
                continue;
            if (m_sinkCaps)
                m_sinkCaps = adoptGRef(gst_caps_merge(m_sinkCaps.leakRef(), templateCaps.leakRef()));
            else
                m_sinkCaps = WTFMove(templateCaps);
        }
    }
}

void GStreamerQuirkRialto::configureElement(GstElement* element, const OptionSet<ElementRuntimeCharacteristics>&)
{
    if (!g_strcmp0(G_OBJECT_TYPE_NAME(G_OBJECT(element)), "GstURIDecodeBin3")) {
        GRefPtr<GstCaps> defaultCaps;
        g_object_get(element, "caps", &defaultCaps.outPtr(), nullptr);
        defaultCaps = adoptGRef(gst_caps_merge(gst_caps_ref(m_sinkCaps.get()), defaultCaps.leakRef()));
        GST_INFO("Setting stop caps to %" GST_PTR_FORMAT, defaultCaps.get());
        g_object_set(element, "caps", defaultCaps.get(), nullptr);
    }
}

GstElement* GStreamerQuirkRialto::createAudioSink()
{
    auto sink = makeGStreamerElement("rialtomseaudiosink", nullptr);
    RELEASE_ASSERT_WITH_MESSAGE(sink, "rialtomseaudiosink should be available in the system but it is not");
    return sink;
}

GstElement* GStreamerQuirkRialto::createWebAudioSink()
{
    auto sink = makeGStreamerElement("rialtowebaudiosink", nullptr);
    RELEASE_ASSERT_WITH_MESSAGE(sink, "rialtowebaudiosink should be available in the system but it is not");
    return sink;
}

std::optional<bool> GStreamerQuirkRialto::isHardwareAccelerated(GstElementFactory* factory)
{
    if (g_str_has_prefix(GST_OBJECT_NAME(factory), "rialto"))
        return true;

    return std::nullopt;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
