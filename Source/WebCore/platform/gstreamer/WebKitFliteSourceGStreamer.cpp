/*
 *  Copyright (C) 2023 Igalia, S.L
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitFliteSourceGStreamer.h"

#if ENABLE(SPEECH_SYNTHESIS) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GUniquePtrFlite.h"
#include "PlatformSpeechSynthesisVoice.h"
#include <wtf/DataMutex.h>
#include <wtf/glib/WTFGType.h>

extern "C" {
/* There is no header for Flite functions below */
extern void usenglish_init(cst_voice*);
extern cst_lexicon* cmulex_init(void);
extern cst_voice* register_cmu_us_kal(const char*);
extern cst_voice* register_cmu_us_awb(const char*);
extern cst_voice* register_cmu_us_rms(const char*);
extern cst_voice* register_cmu_us_slt(const char*);
}

using namespace WebCore;

typedef struct _WebKitFliteSrcClass WebKitFliteSrcClass;
typedef struct _WebKitFliteSrcPrivate WebKitFliteSrcPrivate;

#define WEBKIT_FLITE_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_FLITE_SRC, WebKitFliteSrcClass))
#define WEBKIT_IS_FLITE_SRC(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_FLITE_SRC))
#define WEBKIT_IS_FLITE_SRC_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_FLITE_SRC))

struct _WebKitFliteSrcClass {
    GstBaseSrcClass parentClass;
};

struct _WebKitFliteSrc {
    GstBaseSrc parent;
    WebKitFliteSrcPrivate* priv;
};

struct _WebKitFliteSrcPrivate {
    struct StreamingMembers {
        bool didLoadUtterance;
        GRefPtr<GstAdapter> adapter;
    };
    DataMutex<StreamingMembers> dataMutex;

    GstAudioInfo info;
    String text;
    cst_voice* currentVoice;
};

GType webkit_flite_src_get_type(void);

GST_DEBUG_CATEGORY_STATIC(webkit_flite_src_debug);
#define GST_CAT_DEFAULT webkit_flite_src_debug

#define DEFAULT_SAMPLES_PER_BUFFER 1024

static GstStaticPadTemplate srcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE(S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) 48000, " "channels = (int) [1, 8]")
);

#define webkit_flite_src_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitFliteSrc, webkit_flite_src, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT(webkit_flite_src_debug, "webkitflitesrc", 0, "flitesrc element"));

// To add more voices, add voice register functions here.
using VoiceRegisterFunction = Function<cst_voice*(const char*)>;
static VoiceRegisterFunction voiceRegisterFunctions[] = {
    register_cmu_us_kal,
    register_cmu_us_slt,
    register_cmu_us_rms,
    register_cmu_us_awb,
};

static void webkitFliteSrcReset(WebKitFliteSrc* src)
{
    WebKitFliteSrcPrivate* priv = src->priv;

    DataMutexLocker members { priv->dataMutex };
    gst_adapter_clear(members->adapter.get());
    members->didLoadUtterance = false;
}

static void webkitFliteSrcConstructed(GObject* object)
{
    GST_CALL_PARENT(G_OBJECT_CLASS, constructed, (object));

    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(object);
    WebKitFliteSrcPrivate* priv = src->priv;

    /* We operate in time */
    gst_base_src_set_format(GST_BASE_SRC(src), GST_FORMAT_TIME);
    gst_base_src_set_blocksize(GST_BASE_SRC(src), -1);
    gst_base_src_set_automatic_eos(GST_BASE_SRC(src), FALSE);

    DataMutexLocker members { priv->dataMutex };
    members->adapter = adoptGRef(gst_adapter_new());

    // Some website does not call initializeVoiceList(), so we ensure flite voices initialized here.
    ensureFliteVoicesInitialized();
}

static gboolean webkitFliteSrcStart(GstBaseSrc* baseSource)
{
    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(baseSource);
    WebKitFliteSrcPrivate* priv = src->priv;

    if (priv->text.isEmpty() || !priv->currentVoice) {
        GST_ERROR_OBJECT(src, "No utterance provided.");
        return FALSE;
    }
    return TRUE;
}

static gboolean webkitFliteSrcStop(GstBaseSrc* baseSource)
{
    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(baseSource);
    webkitFliteSrcReset(src);
    return TRUE;
}

static GstFlowReturn webkitFliteSrcCreate(GstBaseSrc* baseSource, guint64 offset, guint length, GstBuffer** buffer)
{
    UNUSED_PARAM(offset);
    UNUSED_PARAM(length);

    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(baseSource);
    WebKitFliteSrcPrivate* priv = src->priv;

    gsize bytes = DEFAULT_SAMPLES_PER_BUFFER * sizeof(gint16) * priv->info.channels;
    DataMutexLocker members { priv->dataMutex };
    if (!members->didLoadUtterance) {
        members->didLoadUtterance = true;

        GUniquePtr<cst_wave> wave(flite_text_to_wave(priv->text.utf8().data(), priv->currentVoice));
        cst_wave_resample(wave.get(), priv->info.rate);

        gsize bufferSize = priv->info.channels * sizeof(gint16) * wave->num_samples;
        GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_allocate(nullptr, bufferSize, nullptr));
        GstMappedBuffer map(buf, GST_MAP_WRITE);
        gint16* data = reinterpret_cast<gint16*>(map.data());
        memset(data, 0, bufferSize);
        for (int i = 0; i < wave->num_samples; i++)
            data[i * priv->info.channels] = wave->samples[i];

        gst_adapter_push(members->adapter.get(), buf.leakRef());
    }

    size_t queueSize = gst_adapter_available(members->adapter.get());
    if (members->didLoadUtterance && !queueSize)
        return GST_FLOW_EOS;

    *buffer = gst_adapter_take_buffer(members->adapter.get(), std::min(queueSize, bytes));
    return GST_FLOW_OK;
}

static gboolean webkitFliteSrcSetCaps(GstBaseSrc* baseSource, GstCaps* caps)
{
    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(baseSource);
    WebKitFliteSrcPrivate* priv = src->priv;

    gst_audio_info_init(&priv->info);
    if (!gst_audio_info_from_caps(&priv->info, caps)) {
        GST_ERROR_OBJECT(src, "Invalid caps");
        return FALSE;
    }

    return TRUE;
}

static void webkit_flite_src_class_init(WebKitFliteSrcClass* klass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(klass);
    objectClass->constructed = webkitFliteSrcConstructed;

    GstElementClass* elementClass = GST_ELEMENT_CLASS(klass);
    gst_element_class_add_static_pad_template(elementClass, &srcTemplate);
    gst_element_class_set_static_metadata(elementClass,
        "WebKit WebSpeech GstFlite source element", "Source",
        "Handles WebSpeech data from WebCore",
        "ChangSeok Oh <changseok@webkit.org>");

    GstBaseSrcClass* baseSrcClass = GST_BASE_SRC_CLASS(klass);
    baseSrcClass->start = GST_DEBUG_FUNCPTR(webkitFliteSrcStart);
    baseSrcClass->stop = GST_DEBUG_FUNCPTR(webkitFliteSrcStop);
    baseSrcClass->create = GST_DEBUG_FUNCPTR(webkitFliteSrcCreate);
    baseSrcClass->set_caps = GST_DEBUG_FUNCPTR(webkitFliteSrcSetCaps);
}

static Vector<GUniquePtr<cst_voice>>& fliteVoices()
{
    static Vector<GUniquePtr<cst_voice>> voices;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        const unsigned voiceRegisterFunctionCount = sizeof(voiceRegisterFunctions) / sizeof(VoiceRegisterFunction);
        for (unsigned i = 0; i < voiceRegisterFunctionCount; ++i) {
            GUniquePtr<cst_voice> voice(voiceRegisterFunctions[i](nullptr));
            voices.append(WTFMove(voice));
        }
    });
    return voices;
}

static cst_voice* fliteVoice(const char* name)
{
    if (!name)
        return nullptr;

    for (auto& voice : fliteVoices()) {
        if (String::fromUTF8(voice->name) == String::fromUTF8(name))
            return voice.get();
    }

    return nullptr;
}

Vector<Ref<PlatformSpeechSynthesisVoice>>& ensureFliteVoicesInitialized()
{
    static Vector<Ref<PlatformSpeechSynthesisVoice>> voiceList;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        flite_init();

        flite_add_lang("eng", usenglish_init, cmulex_init);
        flite_add_lang("usenglish", usenglish_init, cmulex_init);

        for (auto& voice : fliteVoices())
            voiceList.append(PlatformSpeechSynthesisVoice::create(""_s, String::fromUTF8(voice->name), "en-US"_s, false, false));
        voiceList[0]->setIsDefault(true);
    });
    return voiceList;
}

void webKitFliteSrcSetUtterance(WebKitFliteSrc* src, const PlatformSpeechSynthesisVoice* platformSpeechSynthesisVoice, const String& text)
{
    WebKitFliteSrcPrivate* priv = src->priv;

    ASSERT(!fliteVoices().isEmpty());

    cst_voice* voice = nullptr;
    if (platformSpeechSynthesisVoice && !platformSpeechSynthesisVoice->name().isEmpty())
        voice = fliteVoice(platformSpeechSynthesisVoice->name().utf8().data());

    // We use the first registered voice as default, where no voice is specified.
    priv->currentVoice = voice ? voice : fliteVoices()[0].get();
    priv->text = text;
}

#endif // ENABLE(SPEECH_SYNTHESIS) && USE(GSTREAMER)
