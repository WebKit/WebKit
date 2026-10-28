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

#if USE(FLITE) && USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GUniquePtrFlite.h"
#include "PlatformSpeechSynthesisVoice.h"
#include <dlfcn.h>
#include <wtf/DataMutex.h>
#include <wtf/glib/WTFGType.h>

// Flite is loaded when speech synthesis is first used rather than linked. Its voice
// libraries carry tens of thousands of symbol relocations, which every WebKit process
// would otherwise resolve at startup.
struct FliteLibrary {
    decltype(&flite_init) init;
    decltype(&flite_add_lang) addLang;
    decltype(&flite_text_to_wave) textToWave;
    decltype(&cst_wave_resample) waveResample;
    decltype(&delete_voice) deleteVoice;
    decltype(&delete_wave) deleteWave;
    void (*usenglishInit)(cst_voice*);
    cst_lexicon* (*cmulexInit)();
    Vector<cst_voice* (*)(const char*)> voiceRegisterFunctions;
};

static void* fliteSymbol(const char* library, const char* symbol)
{
    void* handle = dlopen(library, RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        g_warning("Speech synthesis is unavailable: %s", dlerror());
        return nullptr;
    }
    void* address = dlsym(handle, symbol);
    if (!address)
        g_warning("Speech synthesis is unavailable: %s", dlerror());
    return address;
}

static const FliteLibrary* fliteLibrary()
{
    static std::optional<FliteLibrary> library;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        FliteLibrary loaded {
            reinterpret_cast<decltype(&flite_init)>(fliteSymbol("libflite.so.1", "flite_init")),
            reinterpret_cast<decltype(&flite_add_lang)>(fliteSymbol("libflite.so.1", "flite_add_lang")),
            reinterpret_cast<decltype(&flite_text_to_wave)>(fliteSymbol("libflite.so.1", "flite_text_to_wave")),
            reinterpret_cast<decltype(&cst_wave_resample)>(fliteSymbol("libflite.so.1", "cst_wave_resample")),
            reinterpret_cast<decltype(&delete_voice)>(fliteSymbol("libflite.so.1", "delete_voice")),
            reinterpret_cast<decltype(&delete_wave)>(fliteSymbol("libflite.so.1", "delete_wave")),
            reinterpret_cast<void (*)(cst_voice*)>(fliteSymbol("libflite_usenglish.so.1", "usenglish_init")),
            reinterpret_cast<cst_lexicon* (*)()>(fliteSymbol("libflite_cmulex.so.1", "cmulex_init")),
            { }
        };
        if (!loaded.init || !loaded.addLang || !loaded.textToWave || !loaded.waveResample || !loaded.deleteVoice || !loaded.deleteWave || !loaded.usenglishInit || !loaded.cmulexInit)
            return;

        static constexpr std::array<std::pair<const char*, const char*>, 4> voices { {
            { "libflite_cmu_us_kal.so.1", "register_cmu_us_kal" },
            { "libflite_cmu_us_slt.so.1", "register_cmu_us_slt" },
            { "libflite_cmu_us_rms.so.1", "register_cmu_us_rms" },
            { "libflite_cmu_us_awb.so.1", "register_cmu_us_awb" },
        } };
        for (auto [voiceLibrary, registerFunction] : voices) {
            if (auto function = reinterpret_cast<cst_voice* (*)(const char*)>(fliteSymbol(voiceLibrary, registerFunction)))
                loaded.voiceRegisterFunctions.append(function);
        }
        if (loaded.voiceRegisterFunctions.isEmpty())
            return;

        library = WTF::move(loaded);
    });
    return library ? &*library : nullptr;
}

namespace WebCore {

void fliteDeleteVoice(cst_voice* voice)
{
    fliteLibrary()->deleteVoice(voice);
}

void fliteDeleteWave(cst_wave* wave)
{
    fliteLibrary()->deleteWave(wave);
}

} // namespace WebCore

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

static GstStaticPadTemplate fliteSrcTemplate = GST_STATIC_PAD_TEMPLATE("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE(S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) 48000, " "channels = (int) [1, 8]")
);

WEBKIT_DEFINE_TYPE_WITH_CODE(WebKitFliteSrc, webkit_flite_src, GST_TYPE_BASE_SRC,
    GST_DEBUG_CATEGORY_INIT(webkit_flite_src_debug, "webkitflitesrc", 0, "flitesrc element"));

static void webkitFliteSrcReset(WebKitFliteSrc* src)
{
    WebKitFliteSrcPrivate* priv = src->priv;

    DataMutexLocker members { priv->dataMutex };
    gst_adapter_clear(members->adapter.get());
    members->didLoadUtterance = false;
}

static void webkitFliteSrcConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_flite_src_parent_class)->constructed(object);

    WebKitFliteSrc* src = WEBKIT_FLITE_SRC(object);
    WebKitFliteSrcPrivate* priv = src->priv;

    /* We operate in time */
    auto baseSrc = GST_BASE_SRC_CAST(src);
    gst_base_src_set_format(baseSrc, GST_FORMAT_TIME);
    gst_base_src_set_blocksize(baseSrc, -1);
    gst_base_src_set_automatic_eos(baseSrc, FALSE);
    gst_base_src_set_do_timestamp(baseSrc, TRUE);
    gst_base_src_set_live(baseSrc, TRUE);

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

        GUniquePtr<cst_wave> wave(fliteLibrary()->textToWave(priv->text.utf8().data(), priv->currentVoice));
        fliteLibrary()->waveResample(wave.get(), priv->info.rate);

        gsize bufferSize = priv->info.channels * sizeof(gint16) * wave->num_samples;
        GRefPtr<GstBuffer> buf = adoptGRef(gst_buffer_new_allocate(nullptr, bufferSize, nullptr));
        WebCore::GstMappedBuffer map(buf, GST_MAP_WRITE);
        gint16* data = reinterpret_cast<gint16*>(map.data());
        auto dataSpan = unsafeMakeSpan(data, bufferSize / sizeof(gint16));
        zeroSpan(dataSpan);
        unsigned i = 0;
        for (auto& sample : unsafeMakeSpan(wave->samples, wave->num_samples))
            dataSpan[i++ * priv->info.channels] = sample;

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
    gst_element_class_add_static_pad_template(elementClass, &fliteSrcTemplate);
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
        auto* library = fliteLibrary();
        if (!library)
            return;
        for (auto& function : library->voiceRegisterFunctions) {
            GUniquePtr<cst_voice> voice(function(nullptr));
            voices.append(WTF::move(voice));
        }
    });
    return voices;
}

static cst_voice* fliteVoice(const String& name)
{
    if (!name)
        return nullptr;

    for (auto& voice : fliteVoices()) {
        if (equal(name, byteCast<char8_t>(unsafeSpan(voice->name))))
            return voice.get();
    }

    return nullptr;
}

Vector<Ref<WebCore::PlatformSpeechSynthesisVoice>>& ensureFliteVoicesInitialized()
{
    static Vector<Ref<WebCore::PlatformSpeechSynthesisVoice>> voiceList;
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        auto* library = fliteLibrary();
        if (!library)
            return;

        library->init();

        library->addLang("eng", library->usenglishInit, library->cmulexInit);
        library->addLang("usenglish", library->usenglishInit, library->cmulexInit);

        for (auto& voice : fliteVoices())
            voiceList.append(WebCore::PlatformSpeechSynthesisVoice::create(""_s, String::fromUTF8(voice->name), "en-US"_s, false, false));
        if (!voiceList.isEmpty())
            voiceList[0]->setIsDefault(true);
    });
    return voiceList;
}

void webKitFliteSrcSetUtterance(WebKitFliteSrc* src, const WebCore::PlatformSpeechSynthesisVoice* platformSpeechSynthesisVoice, const String& text)
{
    WebKitFliteSrcPrivate* priv = src->priv;

    cst_voice* voice = nullptr;
    if (platformSpeechSynthesisVoice && !platformSpeechSynthesisVoice->name().isEmpty())
        voice = fliteVoice(platformSpeechSynthesisVoice->name());

    // We use the first registered voice as default, where no voice is specified.
    if (!voice && !fliteVoices().isEmpty())
        voice = fliteVoices()[0].get();
    priv->currentVoice = voice;
    priv->text = text;
}

#undef GST_CAT_DEFAULT

#endif // USE(FLITE) && USE(GSTREAMER)
