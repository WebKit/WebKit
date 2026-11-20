/*
 * Copyright (C) 2026 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitTestServer.h"
#include "WebViewTest.h"

#include <wpe/wpe-platform.h>
#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>

static WebKitTestServer* kServer;

// Audio rendered on a virtual audio device, as received by its buffer callback.
struct VirtualAudioDeviceOutput {
    VirtualAudioDeviceOutput(WPEAudioSampleFormat format, unsigned rate, unsigned channels, unsigned framesPerBuffer, WPEAudioLayout layout = WPE_AUDIO_LAYOUT_INTERLEAVED)
        : format(format)
        , layout(layout)
        , rate(rate)
        , bytesPerSample(sampleSize(format))
        , bytesPerFrame(bytesPerSample * channels)
        , framesPerBuffer(framesPerBuffer)
    {
    }

    static size_t sampleSize(WPEAudioSampleFormat format)
    {
        switch (format) {
        case WPE_AUDIO_SAMPLE_FORMAT_S16LE:
            return 2;
        case WPE_AUDIO_SAMPLE_FORMAT_S24LE:
            return 3;
        case WPE_AUDIO_SAMPLE_FORMAT_F32LE:
            return 4;
        default:
            g_assert_not_reached();
        }
        return 0;
    }

    void reset()
    {
        buffers = 0;
        bytes = 0;
        invalidSizes = 0;
        timestampGaps = 0;
        nextTimestamp = std::nullopt;
        peak = 0;
        zeroCrossings = 0;
        signalFrames = 0;
        signalPeak = 0;
        lastSign = 0;
    }

    // Sample normalized to [-1, 1].
    static double sampleValue(WPEAudioSampleFormat format, std::span<const uint8_t> data)
    {
        if (format == WPE_AUDIO_SAMPLE_FORMAT_S16LE) {
            int16_t sample;
            memcpySpan(asMutableByteSpan(sample), data.first(sizeof(sample)));
            return sample / 32768.;
        }
        if (format == WPE_AUDIO_SAMPLE_FORMAT_S24LE) {
            // Sign-extend the packed little endian 24 bits sample.
            int32_t sample = static_cast<int32_t>(static_cast<uint32_t>(data[0]) << 8 | static_cast<uint32_t>(data[1]) << 16 | static_cast<uint32_t>(data[2]) << 24) >> 8;
            return sample / 8388608.;
        }
        float sample;
        memcpySpan(asMutableByteSpan(sample), data.first(sizeof(sample)));
        return sample;
    }

    // Frequency of the signal of the first channel, estimated from its zero crossings.
    double frequency() const
    {
        return signalFrames ? zeroCrossings / 2. * rate / signalFrames : 0;
    }

    void analyze(std::span<const uint8_t> data)
    {
        double bufferPeak = 0;
        for (size_t offset = 0; offset + bytesPerSample <= data.size(); offset += bytesPerSample)
            bufferPeak = std::max(bufferPeak, std::abs(sampleValue(format, data.subspan(offset))));
        peak = std::max(peak, bufferPeak);

        // Only count the zero crossings of the buffers containing a signal, with an hysteresis
        // so that noise around zero isn't counted. Buffers containing silence inserted because
        // a producer didn't provide its audio in time (for instance when it starts) are skipped:
        // a run of exact zeros never happens in the signal, and would bias the estimation.
        static constexpr double signalThreshold = 0.1;
        static constexpr double crossingThreshold = 0.02;
        static constexpr unsigned silenceRunLength = 8;
        // The samples of the first channel are interleaved with the other channels, or they are the
        // first plane of a non-interleaved buffer.
        size_t frames = data.size() / bytesPerFrame;
        size_t stride = layout == WPE_AUDIO_LAYOUT_INTERLEAVED ? bytesPerFrame : bytesPerSample;
        size_t firstChannelSize = frames * stride;
        unsigned zeroRun = 0;
        bool hasSilence = false;
        for (size_t offset = 0; offset + stride <= firstChannelSize && !hasSilence; offset += stride) {
            zeroRun = sampleValue(format, data.subspan(offset)) ? 0 : zeroRun + 1;
            hasSilence = zeroRun >= silenceRunLength;
        }
        if (bufferPeak < signalThreshold || hasSilence) {
            lastSign = 0;
            return;
        }
        // The resampler rings after inserted silence, only check the peak of continuous signal.
        if (lastSign)
            signalPeak = std::max(signalPeak, bufferPeak);
        for (size_t offset = 0; offset + stride <= firstChannelSize; offset += stride) {
            auto value = sampleValue(format, data.subspan(offset));
            int sign = value > crossingThreshold ? 1 : (value < -crossingThreshold ? -1 : 0);
            if (!sign)
                continue;
            if (lastSign && sign != lastSign)
                zeroCrossings++;
            lastSign = sign;
        }
        signalFrames += frames;
    }

    static void bufferCallback(WPEVirtualAudioDevice*, const guint8* data, gsize size, guint64 timestamp, gpointer userData)
    {
        auto& output = *static_cast<VirtualAudioDeviceOutput*>(userData);
        output.buffers++;
        output.bytes += size;
        if (size != output.framesPerBuffer * output.bytesPerFrame)
            output.invalidSizes++;

        // Every buffer must start where the previous one ended, allowing rounding errors.
        static constexpr int64_t timestampTolerance = 1000;
        if (output.nextTimestamp && std::abs(static_cast<int64_t>(timestamp - *output.nextTimestamp)) > timestampTolerance)
            output.timestampGaps++;
        output.nextTimestamp = timestamp + (size / output.bytesPerFrame) * G_GUINT64_CONSTANT(1000000000) / output.rate;

        output.analyze(unsafeMakeSpan(data, size));

        if (output.isWaitingForOnset)
            output.recordOnset(unsafeMakeSpan(data, size));
    }

    // Records the wall clock time of the first sample of the first channel above the signal
    // threshold. Buffers are delivered when they are played.
    void recordOnset(std::span<const uint8_t> data)
    {
        static constexpr double onsetThreshold = 0.05;
        size_t stride = layout == WPE_AUDIO_LAYOUT_INTERLEAVED ? bytesPerFrame : bytesPerSample;
        size_t frames = data.size() / bytesPerFrame;
        for (size_t frame = 0; frame < frames; ++frame) {
            if (std::abs(sampleValue(format, data.subspan(frame * stride))) > onsetThreshold) {
                onsetRealTime = g_get_real_time() + static_cast<int64_t>(frame * G_USEC_PER_SEC / rate);
                isWaitingForOnset = false;
                return;
            }
        }
    }

    bool isWaitingForOnset { false };
    std::optional<int64_t> onsetRealTime;

    WPEAudioSampleFormat format;
    WPEAudioLayout layout;
    unsigned rate;
    size_t bytesPerSample;
    size_t bytesPerFrame;
    size_t framesPerBuffer;
    unsigned buffers { 0 };
    size_t bytes { 0 };
    unsigned invalidSizes { 0 };
    unsigned timestampGaps { 0 };
    std::optional<uint64_t> nextTimestamp;
    double peak { 0 };
    double signalPeak { 0 };
    unsigned zeroCrossings { 0 };
    size_t signalFrames { 0 };
    int lastSign { 0 };
};

class VirtualAudioDevicesTest : public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(VirtualAudioDevicesTest, setup, teardown);

    static void setup()
    {
        WebViewTest::shouldInitializeWebViewInConstructor = false;
    }

    static void teardown()
    {
        WebViewTest::shouldInitializeWebViewInConstructor = true;
    }

    VirtualAudioDevicesTest()
        // The default device uses the default buffer size of 10ms, the mono device a custom one.
        : m_defaultDevice(adoptGRef(wpe_virtual_audio_device_new("virtual-default", "Virtual default output", WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, TRUE)))
        , m_monoDevice(adoptGRef(wpe_virtual_audio_device_new("virtual-mono", "Virtual mono output", WPE_AUDIO_SAMPLE_FORMAT_S16LE, 44100, 1, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 128, FALSE)))
        // Devices covering other formats, layouts and channel layouts.
        , m_planarDevice(adoptGRef(wpe_virtual_audio_device_new("virtual-planar", "Virtual planar output", WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_NON_INTERLEAVED, 256, FALSE)))
        , m_s24Device(adoptGRef(wpe_virtual_audio_device_new("virtual-s24", "Virtual S24 output", WPE_AUDIO_SAMPLE_FORMAT_S24LE, 44100, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 441, FALSE)))
        // Front left, front right, rear left and rear right.
        , m_quadDevice(adoptGRef(wpe_virtual_audio_device_new("virtual-quad", "Virtual quad output", WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 4, 0x33, WPE_AUDIO_LAYOUT_INTERLEAVED, 480, FALSE)))
        , m_defaultOutput(WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 480)
        , m_monoOutput(WPE_AUDIO_SAMPLE_FORMAT_S16LE, 44100, 1, 128)
        , m_planarOutput(WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 256, WPE_AUDIO_LAYOUT_NON_INTERLEAVED)
        , m_s24Output(WPE_AUDIO_SAMPLE_FORMAT_S24LE, 44100, 2, 441)
        , m_quadOutput(WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 4, 480)
    {
        g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(m_defaultDevice.get()), ==, 480);
        g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(m_monoDevice.get()), ==, 128);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_defaultDevice.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_monoDevice.get()));
        wpe_virtual_audio_device_set_buffer_callback(m_defaultDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &m_defaultOutput, nullptr);
        wpe_virtual_audio_device_set_buffer_callback(m_monoDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &m_monoOutput, nullptr);
        for (auto& [device, output] : std::initializer_list<std::pair<WPEVirtualAudioDevice*, VirtualAudioDeviceOutput*>> { { m_planarDevice.get(), &m_planarOutput }, { m_s24Device.get(), &m_s24Output }, { m_quadDevice.get(), &m_quadOutput } }) {
            g_assert_nonnull(device);
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(device));
            wpe_virtual_audio_device_set_buffer_callback(device, VirtualAudioDeviceOutput::bufferCallback, output, nullptr);
        }

        // The second default device is exposed as a regular device.
        GRefPtr<WPEVirtualAudioDevice> secondDefaultDevice = adoptGRef(wpe_virtual_audio_device_new("virtual-second-default", "Virtual second default output", WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, TRUE));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(secondDefaultDevice.get()));

        m_devices = adoptGRef(g_list_store_new(WPE_TYPE_VIRTUAL_AUDIO_DEVICE));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_devices.get()));
        // The default device isn't the first one of the list, it must be exposed first anyway.
        g_list_store_append(m_devices.get(), m_monoDevice.get());
        g_list_store_append(m_devices.get(), m_defaultDevice.get());
        g_list_store_append(m_devices.get(), secondDefaultDevice.get());
        g_list_store_append(m_devices.get(), m_planarDevice.get());
        g_list_store_append(m_devices.get(), m_s24Device.get());
        g_list_store_append(m_devices.get(), m_quadDevice.get());

        // A device reusing an already used name is ignored with a warning.
        if (s_addDuplicatedDevice) {
            GRefPtr<WPEVirtualAudioDevice> duplicatedDevice = adoptGRef(wpe_virtual_audio_device_new("virtual-mono", "Duplicated output", WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(duplicatedDevice.get()));
            g_list_store_append(m_devices.get(), duplicatedDevice.get());
            g_test_expect_message(nullptr, G_LOG_LEVEL_WARNING, "*more than one device named 'virtual-mono', ignoring item 6.");
        }

        // Listing audio output devices requires the ExposeSpeakers features.
        GRefPtr<WebKitSettings> settings = adoptGRef(webkit_settings_new());
        if (s_enableSpeakerSelection) {
            WebKitFeatureList* features = webkit_settings_get_all_features();
            for (const char* identifier : { "ExposeSpeakers", "ExposeSpeakersWithoutMicrophone" }) {
                auto* feature = webkit_feature_list_find(features, identifier);
                g_assert_nonnull(feature);
                webkit_settings_set_feature_enabled(settings.get(), feature, TRUE);
            }
            webkit_feature_list_unref(features);
        }

        GRefPtr<WebKitWebsitePolicies> policies = adoptGRef(webkit_website_policies_new_with_policies("autoplay", WEBKIT_AUTOPLAY_ALLOW, nullptr));
        m_webView = createWebView(
            "user-content-manager", m_userContentManager.get(),
            "network-session", m_networkSession.get(),
            "website-policies", policies.get(),
            "settings", settings.get(),
            "virtual-audio-devices", m_devices.get(),
            nullptr);
        if (s_addDuplicatedDevice)
            g_test_assert_expected_messages();
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webView.get()));
        if (auto* view = webkit_web_view_get_wpe_view(m_webView.get()))
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(view));
        g_signal_connect(m_webView.get(), "web-process-terminated", G_CALLBACK(WebViewTest::webProcessTerminated), this);

        showInWindow();
    }

    ~VirtualAudioDevicesTest()
    {
        // The web view must keep the devices alive, check that they are released with it.
        m_devices = nullptr;
        m_defaultDevice = nullptr;
        m_monoDevice = nullptr;
        m_planarDevice = nullptr;
        m_s24Device = nullptr;
        m_quadDevice = nullptr;
    }

    void loadPage(UTF8CStringView html)
    {
        auto baseURI = GMallocString::unsafeAdoptFromUTF8(SAFE_G_STRDUP_PRINTF("file://%s/", getResourcesDir()));
        loadHtml(html.utf8(), baseURI.utf8());
        waitUntilLoadFinished();
    }

    // The device IDs are opaque, scripts find the ID of a device from its label with sinkIdFor().
    GMallocString runAsync(UTF8CStringView body)
    {
        static constexpr auto helpers =
            "const sinkIdFor = async label => {"
            "    const devices = await navigator.mediaDevices.enumerateDevices();"
            "    const device = devices.find(device => device.kind === 'audiooutput' && device.label === label);"
            "    return device ? device.deviceId : 'not-found: ' + label;"
            "};"_s;
        auto script = makeString(helpers, String::fromUTF8(body.span()));
        GUniqueOutPtr<GError> error;
        auto* value = runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished(script.utf8().legacyCStringPointer(), nullptr, nullptr, &error.outPtr());
        g_assert_no_error(error.get());
        if (!value || jsc_value_is_undefined(value))
            return { };
        return GMallocString::unsafeAdoptFromUTF8(javascriptResultToCString(value));
    }

    // Run the main loop until the condition is satisfied, or the timeout expires. The default
    // timeout is below the 5 seconds limit of the test harness in Release, so that a failing
    // wait is reported by the assertion that checks it rather than as a harness timeout.
    bool waitUntil(Function<bool()>&& condition, Seconds timeout = 4_s)
    {
        auto deadline = MonotonicTime::now() + timeout;
        auto sourceID = g_timeout_add(20, [](gpointer userData) -> gboolean {
            g_main_loop_quit(static_cast<GMainLoop*>(userData));
            return G_SOURCE_CONTINUE;
        }, m_mainLoop);
        while (!condition() && MonotonicTime::now() < deadline)
            g_main_loop_run(m_mainLoop);
        g_source_remove(sourceID);
        return condition();
    }

    // Wait until the device received enough buffers, then check that they are valid.
    void waitForBuffers(VirtualAudioDeviceOutput& output)
    {
        static constexpr unsigned minimumBuffers = 20;
        g_assert_true(waitUntil([&] {
            return output.buffers >= minimumBuffers;
        }));
        g_assert_cmpuint(output.invalidSizes, ==, 0);
        g_assert_cmpuint(output.timestampGaps, ==, 0);
    }

    // Wait until the device received enough audio, then check that it's valid: a signal of the
    // expected frequency, if given, that is not clipped.
    void waitForAudio(VirtualAudioDeviceOutput& output, double minimumPeak, std::optional<double> expectedFrequency = 440.)
    {
        static constexpr unsigned minimumBuffers = 20;
        g_assert_true(waitUntil([&] {
            // A quarter of a second of signal gives an accurate enough frequency estimation.
            return output.buffers >= minimumBuffers && output.peak >= minimumPeak && (!expectedFrequency || output.signalFrames >= output.rate / 4);
        }));
        g_assert_cmpuint(output.invalidSizes, ==, 0);
        g_assert_cmpuint(output.timestampGaps, ==, 0);
        // Allow for the rounding errors of resampling.
        g_assert_cmpfloat(output.signalPeak, <=, 1.01);
        if (expectedFrequency)
            g_assert_cmpfloat_with_epsilon(output.frequency(), *expectedFrequency, *expectedFrequency * 0.05);
    }

    // Let some time for audio to be rendered on a device that is not expected to receive any.
    void waitForSilence()
    {
        wait(0.3);
    }

    // Check that a device no longer receives audio, for instance after switching to another one.
    void assertDeviceStopped(VirtualAudioDeviceOutput& output)
    {
        // Let the buffers that were already sent be delivered first.
        waitForSilence();
        output.reset();
        waitForSilence();
        g_assert_cmpuint(output.buffers, ==, 0);
    }

    static inline bool s_addDuplicatedDevice { false };
    static inline bool s_enableSpeakerSelection { true };

    GRefPtr<GListStore> m_devices;
    GRefPtr<WPEVirtualAudioDevice> m_defaultDevice;
    GRefPtr<WPEVirtualAudioDevice> m_monoDevice;
    GRefPtr<WPEVirtualAudioDevice> m_planarDevice;
    GRefPtr<WPEVirtualAudioDevice> m_s24Device;
    GRefPtr<WPEVirtualAudioDevice> m_quadDevice;
    VirtualAudioDeviceOutput m_defaultOutput;
    VirtualAudioDeviceOutput m_monoOutput;
    VirtualAudioDeviceOutput m_planarOutput;
    VirtualAudioDeviceOutput m_s24Output;
    VirtualAudioDeviceOutput m_quadOutput;
};

// Same as VirtualAudioDevicesTest, with a device whose name is already used by another device.
class VirtualAudioDevicesDuplicatedNameTest : public VirtualAudioDevicesTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(VirtualAudioDevicesDuplicatedNameTest, setup, teardown);

    static void setup()
    {
        VirtualAudioDevicesTest::setup();
        s_addDuplicatedDevice = true;
    }

    static void teardown()
    {
        s_addDuplicatedDevice = false;
        VirtualAudioDevicesTest::teardown();
    }
};

// Same as VirtualAudioDevicesTest, without the features needed to list audio output devices.
class VirtualAudioDevicesWithoutSpeakerSelectionTest : public VirtualAudioDevicesTest {
public:
    MAKE_GLIB_TEST_FIXTURE_WITH_SETUP_TEARDOWN(VirtualAudioDevicesWithoutSpeakerSelectionTest, setup, teardown);

    static void setup()
    {
        VirtualAudioDevicesTest::setup();
        s_enableSpeakerSelection = false;
    }

    static void teardown()
    {
        s_enableSpeakerSelection = true;
        VirtualAudioDevicesTest::teardown();
    }
};

static void testVirtualAudioDevicesProperty(VirtualAudioDevicesTest* test, gconstpointer)
{
    GRefPtr<GListModel> devices;
    g_object_get(test->webView(), "virtual-audio-devices", &devices.outPtr(), nullptr);
    g_assert_true(devices.get() == G_LIST_MODEL(test->m_devices.get()));
}

static void testVirtualAudioDevicesEnumerate(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto devices = String::fromUTF8(test->runAsync(
        "const devices = await navigator.mediaDevices.enumerateDevices();"
        "return devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"_s).span());

    // The default virtual device is the first audio output device, after the "default" entry that
    // represents it. The virtual devices are listed without microphone access.
    g_assert_true(devices.startsWith("Default - Virtual default output,Virtual default output,"_s));
    g_assert_true(devices.contains("Virtual mono output"_s));
    g_assert_true(devices.contains("Virtual second default output"_s));
    g_assert_true(devices.contains("Virtual planar output"_s));

    // The device IDs are opaque, unique, and not the device names.
    auto deviceIds = String::fromUTF8(test->runAsync(
        "const ids = (await navigator.mediaDevices.enumerateDevices()).filter(device => device.kind === 'audiooutput').map(device => device.deviceId);"
        "if (ids.shift() !== 'default' || ids.some(id => !id || id.startsWith('virtual-')))"
        "    return 'invalid: ' + ids.join(',');"
        "return new Set(ids).size === ids.length ? 'unique' : 'duplicated: ' + ids.join(',');"_s).span());
    g_assert_true(deviceIds == "unique"_s);
}

static void testVirtualAudioDevicesDuplicatedName(VirtualAudioDevicesDuplicatedNameTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto devices = String::fromUTF8(test->runAsync(
        "const devices = await navigator.mediaDevices.enumerateDevices();"
        "return devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"_s).span());

    // The first device with a given name is used, the following ones are ignored.
    g_assert_true(devices.contains("Virtual mono output"_s));
    g_assert_false(devices.contains("Duplicated output"_s));
}

static void testVirtualAudioDevicesPopup(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The popup is created without the virtual-audio-devices property.
    struct PopupData {
        VirtualAudioDevicesTest* test;
        GRefPtr<WebKitWebView> popup;
    } data { test, nullptr };
    g_signal_connect(test->webView(), "create", G_CALLBACK(+[](WebKitWebView* webView, WebKitNavigationAction*, PopupData* data) -> WebKitWebView* {
        g_assert_null(data->popup.get());
        data->popup = data->test->createWebView("related-view", webView, nullptr);
        if (auto* view = webkit_web_view_get_wpe_view(data->popup.get()))
            wpe_view_set_visible(view, TRUE);
        // The application owns the web view, it's not adopted by WebKit.
        return data->popup.get();
    }), &data);

    test->loadPage("<html><body></body></html>"_s);
    test->runAsync("window.popup = window.open('about:blank');"_s);
    g_signal_handlers_disconnect_matched(test->webView(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, &data);
    GRefPtr popup = WTF::move(data.popup);
    g_assert_nonnull(popup.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(popup.get()));

    // The popup doesn't inherit the virtual audio devices of its opener.
    GRefPtr<GListModel> devices;
    g_object_get(popup.get(), "virtual-audio-devices", &devices.outPtr(), nullptr);
    g_assert_null(devices.get());

    auto runInPopup = [&](const char* script) {
        GUniqueOutPtr<GError> error;
        auto* value = test->runJavaScriptAndWaitUntilFinished(script, &error.outPtr(), popup.get());
        g_assert_no_error(error.get());
        return String::fromUTF8(GMallocString::unsafeAdoptFromUTF8(WebViewTest::javascriptResultToCString(value)).span());
    };
    // Can't use waitUntil(), whose timer would interrupt the JavaScript evaluation.
    auto waitForPopup = [&](const char* script, NOESCAPE const Function<bool(const String&)>& condition) {
        auto deadline = MonotonicTime::now() + 4_s;
        String result;
        while (!condition(result = runInPopup(script)) && MonotonicTime::now() < deadline) {
            test->waitUntil([] {
                return false;
            }, 20_ms);
        }
        return result;
    };
    runInPopup(
        "navigator.mediaDevices.enumerateDevices().then(devices => {"
        "    window.audioOutputs = devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"
        "}, error => { window.audioOutputs = 'error: ' + error.name; }); '';");
    auto audioOutputs = waitForPopup("window.audioOutputs === undefined ? '[pending]' : window.audioOutputs", [](const String& result) {
        return result != "[pending]"_s;
    });
    g_assert_true(audioOutputs != "[pending]"_s);
    g_assert_false(audioOutputs.startsWith("error"_s));
    g_assert_false(audioOutputs.contains("Virtual"_s));

#if ENABLE(WEB_AUDIO)
    // The audio of the popup isn't rendered on the virtual devices of its opener.
    runInPopup(
        "window.context = new AudioContext();"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(context.destination);"
        "oscillator.start();"
        "context.resume(); '';");
    auto state = waitForPopup("context.state", [](const String& result) {
        return result == "running"_s;
    });
    g_assert_true(state == "running"_s);
    test->assertDeviceStopped(test->m_defaultOutput);
    test->assertDeviceStopped(test->m_monoOutput);
#endif
}

#if ENABLE(WEB_AUDIO)
static unsigned s_destroyNotifyCount;
static void countDestroyNotify(gpointer)
{
    s_destroyNotifyCount++;
}

// Plays a 440 Hz oscillator in window.context, on the device with the given label or on the
// default device.
static void startOscillator(VirtualAudioDevicesTest* test, ASCIILiteral sinkLabel = { })
{
    auto options = sinkLabel.isNull() ? String() : makeString("{ sinkId: await sinkIdFor('"_s, sinkLabel, "') }"_s);
    test->runAsync(makeString(
        "window.context = new AudioContext("_s, options, ");"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(context.destination);"
        "oscillator.start();"
        "await context.resume();"_s).utf8());
}

static void testVirtualAudioDevicesAudioContextDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test);

    test->waitForAudio(test->m_defaultOutput, 0.5);
    test->waitForSilence();
    g_assert_cmpuint(test->m_monoOutput.buffers, ==, 0);

    // The callback can be removed and set again while audio is being rendered. Keep rendering
    // without callback for a while, so that buffers are received while there's none.
    wpe_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, nullptr);
    test->waitForSilence();
    test->m_defaultOutput.reset();
    s_destroyNotifyCount = 0;
    wpe_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &test->m_defaultOutput, countDestroyNotify);
    test->waitForAudio(test->m_defaultOutput, 0.5);

    // Replacing the callback releases the previous user data, even when it's null.
    wpe_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, countDestroyNotify);
    g_assert_cmpuint(s_destroyNotifyCount, ==, 1);
    wpe_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, nullptr);
    g_assert_cmpuint(s_destroyNotifyCount, ==, 2);
}

static void testVirtualAudioDevicesAudioContextOutputTimestamp(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "window.context = new AudioContext();"
        "window.gain = context.createGain();"
        "gain.gain.value = 0;"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(gain);"
        "gain.connect(context.destination);"
        "oscillator.start();"
        "await context.resume();"_s);
    test->waitForSilence();

    // Unmute the oscillator a bit later and compare when it's played with the time predicted by
    // getOutputTimestamp() and outputLatency, which include the latency of the mixer. The error
    // depends on how much audio is waiting in the mixer, which isn't known, and on the delivery
    // of the buffers to the UI main thread, it's around 10ms. The bounds only catch big errors,
    // such as counting the 100ms latency of the interaudiosrc twice.
    static constexpr double minError = -50;
    static constexpr double maxError = 100;
    for (unsigned i = 0; i < 2; ++i) {
        test->m_defaultOutput.onsetRealTime = std::nullopt;
        test->m_defaultOutput.isWaitingForOnset = true;
        auto predictions = test->runAsync(
            "const timestamp = context.getOutputTimestamp();"
            "const now = performance.timeOrigin + performance.now();"
            "const onsetTime = context.currentTime + 0.3;"
            "gain.gain.setValueAtTime(1, onsetTime);"
            "gain.gain.setValueAtTime(0, onsetTime + 0.2);"
            "const fromTimestamp = performance.timeOrigin + timestamp.performanceTime + (onsetTime - timestamp.contextTime) * 1000;"
            "const fromLatency = now + (onsetTime - context.currentTime + context.outputLatency) * 1000;"
            "return fromTimestamp + ' ' + fromLatency;"_s);
        g_assert_true(test->waitUntil([&] {
            return !!test->m_defaultOutput.onsetRealTime;
        }));

        double fromTimestamp = 0;
        double fromLatency = 0;
        g_assert_cmpint(sscanf(predictions.utf8(), "%lf %lf", &fromTimestamp, &fromLatency), ==, 2);
        double onset = *test->m_defaultOutput.onsetRealTime / 1000.;
        g_assert_cmpfloat(onset - fromTimestamp, >, minError);
        g_assert_cmpfloat(onset - fromTimestamp, <, maxError);
        g_assert_cmpfloat(onset - fromLatency, >, minError);
        g_assert_cmpfloat(onset - fromLatency, <, maxError);
        test->waitForSilence();
    }
}

static void testVirtualAudioDevicesAudioContextSinkId(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto sinkId = test->runAsync(
        "window.monoId = await sinkIdFor('Virtual mono output');"
        "window.context = new AudioContext({ sinkId: monoId });"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(context.destination);"
        "oscillator.start();"
        "await context.resume();"
        "return context.sinkId === monoId ? 'mono' : context.sinkId;"_s);
    g_assert_true(sinkId == "mono"_s);

    // The configuration of the device doesn't change the rate of the context.
    auto sampleRate = test->runAsync("return String(context.sampleRate);"_s);
    g_assert_true(sampleRate == "44100"_s);

    test->waitForAudio(test->m_monoOutput, 0.5);
    test->waitForSilence();
    g_assert_cmpuint(test->m_defaultOutput.buffers, ==, 0);

    // Switch to the default device, the previous device stops receiving audio.
    sinkId = test->runAsync(
        "const sinkChanged = new Promise(resolve => context.addEventListener('sinkchange', resolve, { once: true }));"
        "await context.setSinkId('');"
        "await sinkChanged;"
        "return '[' + context.sinkId + ']';"_s);
    g_assert_true(sinkId == "[]"_s);
    test->waitForAudio(test->m_defaultOutput, 0.5);
    test->assertDeviceStopped(test->m_monoOutput);
}

static void testVirtualAudioDevicesAudioContextSinkIdDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test, "Virtual mono output"_s);
    test->waitForAudio(test->m_monoOutput, 0.5);

    // The "default" device is the virtual default device.
    auto sinkId = test->runAsync("await context.setSinkId('default'); return context.sinkId;"_s);
    g_assert_true(sinkId == "default"_s);
    test->waitForAudio(test->m_defaultOutput, 0.5);
    test->assertDeviceStopped(test->m_monoOutput);
}

static void testVirtualAudioDevicesAudioContextSinkIdUnknown(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto error = test->runAsync(
        "const context = new AudioContext();"
        "try {"
        "    await context.setSinkId('unknown');"
        "    return 'resolved';"
        "} catch (error) {"
        "    return error.name;"
        "}"_s);
    g_assert_true(error == "NotFoundError"_s);
}

static void testVirtualAudioDevicesAudioContextMix(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "for (const frequency of [440, 660]) {"
        "    const context = new AudioContext();"
        "    const oscillator = context.createOscillator();"
        "    const gain = context.createGain();"
        "    oscillator.frequency.value = frequency;"
        "    gain.gain.value = 0.4;"
        "    oscillator.connect(gain).connect(context.destination);"
        "    oscillator.start();"
        "    await context.resume();"
        "}"_s);

    // Both contexts are mixed in a single stream, with an amplitude bigger than each one.
    test->waitForAudio(test->m_defaultOutput, 0.6, std::nullopt);
}

static void testVirtualAudioDevicesAudioContextFormat(VirtualAudioDevicesTest* test, ASCIILiteral label, VirtualAudioDeviceOutput& output)
{
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test, label);

    // The 440 Hz signal is delivered in the format and layout of the device.
    test->waitForAudio(output, 0.5);
    test->waitForSilence();
    g_assert_cmpuint(test->m_defaultOutput.buffers, ==, 0);
}

static void testVirtualAudioDevicesAudioContextPlanar(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesAudioContextFormat(test, "Virtual planar output"_s, test->m_planarOutput);
}

static void testVirtualAudioDevicesAudioContextS24(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesAudioContextFormat(test, "Virtual S24 output"_s, test->m_s24Output);
}

static void testVirtualAudioDevicesAudioContextChannelMask(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The first channel is the front left one.
    testVirtualAudioDevicesAudioContextFormat(test, "Virtual quad output"_s, test->m_quadOutput);
}

static void testVirtualAudioDevicesWithoutSpeakerSelection(VirtualAudioDevicesWithoutSpeakerSelectionTest* test, gconstpointer)
{
    // Without the ExposeSpeakers features, the virtual devices are not listed.
    test->loadPage("<html><body></body></html>"_s);
    auto devices = String::fromUTF8(test->runAsync(
        "const devices = await navigator.mediaDevices.enumerateDevices();"
        "return devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"_s).span());
    g_assert_false(devices.contains("Virtual"_s));

    // But the default virtual device is still used.
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
}
#endif // ENABLE(WEB_AUDIO)

// sine-440hz.webm is 1 second of a 440 Hz sine (Opus, 48 kHz stereo) with a small VP8 video
// track, generated with:
// gst-launch-1.0 -e audiotestsrc freq=440 num-buffers=50 samplesperbuffer=960 ! audio/x-raw,rate=48000,channels=2 ! opusenc bitrate=32000 ! queue ! mux.
//     videotestsrc num-buffers=30 ! video/x-raw,width=80,height=60,framerate=30/1 ! vp8enc ! queue ! webmmux name=mux ! filesink location=sine-440hz.webm

static void testVirtualAudioDevicesMediaElementDefault(VirtualAudioDevicesTest* test, ASCIILiteral elementName)
{
    test->loadPage("<html><body></body></html>"_s);
    auto script = GMallocString::unsafeAdoptFromUTF8(SAFE_G_STRDUP_PRINTF(
        "const element = document.createElement('%s');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "document.body.appendChild(element);"
        "await element.play();", elementName));
    test->runAsync(script);

    test->waitForAudio(test->m_defaultOutput, 0.2);
    test->waitForSilence();
    g_assert_cmpuint(test->m_monoOutput.buffers, ==, 0);
}

static void testVirtualAudioDevicesMediaElementSinkId(VirtualAudioDevicesTest* test, ASCIILiteral elementName)
{
    test->loadPage("<html><body></body></html>"_s);
    auto script = GMallocString::unsafeAdoptFromUTF8(SAFE_G_STRDUP_PRINTF(
        "window.element = document.createElement('%s');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "document.body.appendChild(element);"
        "const monoId = await sinkIdFor('Virtual mono output');"
        "await element.setSinkId(monoId);"
        "await element.play();"
        "return element.sinkId === monoId ? 'mono' : element.sinkId;", elementName));
    auto sinkId = test->runAsync(script);
    g_assert_true(sinkId == "mono"_s);

    test->waitForAudio(test->m_monoOutput, 0.2);
    test->waitForSilence();
    g_assert_cmpuint(test->m_defaultOutput.buffers, ==, 0);

    // Switch to the default device while playing, the previous device stops receiving audio.
    sinkId = test->runAsync("await element.setSinkId(''); return '[' + element.sinkId + ']';"_s);
    g_assert_true(sinkId == "[]"_s);
    test->waitForAudio(test->m_defaultOutput, 0.2);
    test->assertDeviceStopped(test->m_monoOutput);
}

static void testVirtualAudioDevicesAudioElementDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesMediaElementDefault(test, "audio"_s);
}

static void testVirtualAudioDevicesVideoElementDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesMediaElementDefault(test, "video"_s);
}

static void testVirtualAudioDevicesAudioElementSinkId(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesMediaElementSinkId(test, "audio"_s);
}

static void testVirtualAudioDevicesVideoElementSinkId(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesMediaElementSinkId(test, "video"_s);
}

static void testVirtualAudioDevicesAudioElementSinkIdDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "window.element = document.createElement('audio');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "document.body.appendChild(element);"
        "await element.setSinkId(await sinkIdFor('Virtual mono output'));"
        "await element.play();"_s);
    test->waitForAudio(test->m_monoOutput, 0.2);

    // The "default" device is the virtual default device.
    auto sinkId = test->runAsync("await element.setSinkId('default'); return element.sinkId;"_s);
    g_assert_true(sinkId == "default"_s);
    test->waitForAudio(test->m_defaultOutput, 0.2);
    test->assertDeviceStopped(test->m_monoOutput);
}

static void testVirtualAudioDevicesAudioElementSinkIdUnknown(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto error = test->runAsync(
        "const element = document.createElement('audio');"
        "try {"
        "    await element.setSinkId('unknown');"
        "    return 'resolved';"
        "} catch (error) {"
        "    return error.name;"
        "}"_s);
    g_assert_true(error == "NotFoundError"_s);
}

static void testVirtualAudioDevicesAudioElementSinkIdWhileLoading(VirtualAudioDevicesTest* test, gconstpointer)
{
    // Select the device before playing a media that is not preloaded, while the audio sink is
    // not ready yet.
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "window.element = document.createElement('audio');"
        "element.loop = true;"
        "element.preload = 'none';"
        "element.src = 'sine-440hz.webm';"
        "document.body.appendChild(element);"
        "await new Promise(resolve => element.addEventListener('loadstart', resolve, { once: true }));"_s);
    // Selecting a device requires a user gesture, which script run by the API provides.
    auto sinkId = test->runAsync(
        "const monoId = await sinkIdFor('Virtual mono output');"
        "await element.setSinkId(monoId);"
        "await element.play();"
        "return element.sinkId === monoId ? 'mono' : element.sinkId;"_s);
    g_assert_true(sinkId == "mono"_s);
    test->waitForAudio(test->m_monoOutput, 0.2);
    test->waitForSilence();
    g_assert_cmpuint(test->m_defaultOutput.buffers, ==, 0);

    // The device stops receiving audio when the element is paused.
    test->runAsync("element.pause();"_s);
    test->assertDeviceStopped(test->m_monoOutput);
}

static void testVirtualAudioDevicesAudioElementSinkIdKeepsVolume(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The sine of the test file has an amplitude of 0.8.
    static constexpr double volume = 0.25;
    static constexpr double maximumPeak = 0.8 * volume * 1.2;
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "window.element = document.createElement('audio');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "element.volume = 0.25;"
        "document.body.appendChild(element);"
        "await element.play();"_s);
    test->waitForAudio(test->m_defaultOutput, 0.1);
    g_assert_cmpfloat(test->m_defaultOutput.peak, <, maximumPeak);

    // The volume is kept when switching device.
    test->runAsync("await element.setSinkId(await sinkIdFor('Virtual mono output'));"_s);
    test->waitForAudio(test->m_monoOutput, 0.1);
    g_assert_cmpfloat(test->m_monoOutput.peak, <, maximumPeak);

    // A muted element stays silent when switching device.
    test->runAsync("element.muted = true;"_s);
    test->waitForSilence();
    test->m_monoOutput.reset();
    test->waitForBuffers(test->m_monoOutput);
    g_assert_cmpfloat(test->m_monoOutput.peak, ==, 0);
    test->m_defaultOutput.reset();
    test->runAsync("await element.setSinkId('');"_s);
    test->waitForBuffers(test->m_defaultOutput);
    g_assert_cmpfloat(test->m_defaultOutput.peak, ==, 0);
}

#if ENABLE(WEB_AUDIO)
static void serverCallback(SoupServer*, SoupServerMessage* message, const char* path, GHashTable*, gpointer)
{
    if (soup_server_message_get_method(message) != SOUP_METHOD_GET) {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_IMPLEMENTED, nullptr);
        return;
    }

    // The frame posts the labels of the audio outputs it can list to its parent.
    static constexpr auto frameHTML = "<html><body><script>"
        "navigator.mediaDevices.enumerateDevices().then(devices => {"
        "    parent.postMessage(devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(','), '*');"
        "}, error => parent.postMessage('error: ' + error.name, '*'));"
        "</script></body></html>"_s;
    static constexpr auto html = "<html><body><script>window.addEventListener('pageshow', event => { window.wasPersisted = event.persisted; });</script></body></html>"_s;
    ASCIILiteral content;
    if (g_str_equal(path, "/"))
        content = html;
    else if (g_str_equal(path, "/frame"))
        content = frameHTML;
    else {
        soup_server_message_set_status(message, SOUP_STATUS_NOT_FOUND, nullptr);
        return;
    }

    soup_server_message_set_status(message, SOUP_STATUS_OK, nullptr);
    auto* body = soup_server_message_get_response_body(message);
    soup_message_body_append(body, SOUP_MEMORY_STATIC, content.characters(), content.length());
    soup_message_body_complete(body);
}

static String audioOutputsOfCrossOriginFrame(VirtualAudioDevicesTest* test, ASCIILiteral allow)
{
    // The main frame is loaded from 127.0.0.1 and the frame from localhost, both are secure
    // contexts.
    test->loadURI(kServer->getURIForPath("/"));
    test->waitUntilLoadFinished();
    auto script = makeString(
        "const message = new Promise(resolve => window.addEventListener('message', event => resolve(event.data), { once: true }));"
        "const frame = document.createElement('iframe');"_s,
        "frame.allow = '"_s, allow, "';"
        "frame.src = 'http://localhost:"_s, kServer->port(), "/frame';"
        "document.body.appendChild(frame);"
        "return await message;"_s);
    auto utf8Script = script.utf8();
    return String::fromUTF8(test->runAsync(utf8Script).span());
}

static void testVirtualAudioDevicesCrossOriginFrame(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The speaker-selection permissions policy only allows same origin frames by default. Devices
    // are only enumerated when the camera or microphone policy is allowed too, but no microphone
    // access is needed to list the virtual devices.
    auto audioOutputs = audioOutputsOfCrossOriginFrame(test, "microphone"_s);
    g_assert_false(audioOutputs.startsWith("error"_s));
    g_assert_false(audioOutputs.contains("Virtual"_s));

    audioOutputs = audioOutputsOfCrossOriginFrame(test, "microphone; speaker-selection"_s);
    g_assert_true(audioOutputs.startsWith("Default - Virtual default output,Virtual default output,"_s));
    g_assert_true(audioOutputs.contains("Virtual mono output"_s));
}

static void testVirtualAudioDevicesCrossSiteNavigation(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The page is restored from the back/forward cache in its previous web process, with the
    // virtual audio devices it was created with.
    test->loadURI(kServer->getURIForPath("/"));
    test->waitUntilLoadFinished();

    test->loadURI(makeString("http://localhost:"_s, kServer->port(), '/').utf8());
    test->waitUntilLoadFinished();
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
    test->runAsync("await context.close();"_s);
    test->assertDeviceStopped(test->m_defaultOutput);

    test->goBack();
    test->waitUntilLoadFinished();
    auto wasPersisted = test->runAsync("return window.wasPersisted ? 'yes' : 'no';"_s);
    g_assert_true(wasPersisted == "yes"_s);
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
}

static void testVirtualAudioDevicesWebProcessCrash(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);

    // The devices are available again in the new web process.
    test->m_expectedWebProcessCrash = true;
    webkit_web_view_terminate_web_process(test->webView());
    test->m_defaultOutput.reset();
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
}
#endif // ENABLE(WEB_AUDIO)

void beforeAll()
{
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-property", testVirtualAudioDevicesProperty);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-enumerate", testVirtualAudioDevicesEnumerate);
    VirtualAudioDevicesDuplicatedNameTest::add("WebKitWebView", "virtual-audio-devices-duplicated-name", testVirtualAudioDevicesDuplicatedName);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-popup", testVirtualAudioDevicesPopup);
#if ENABLE(WEB_AUDIO)
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-default", testVirtualAudioDevicesAudioContextDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-output-timestamp", testVirtualAudioDevicesAudioContextOutputTimestamp);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id", testVirtualAudioDevicesAudioContextSinkId);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id-default", testVirtualAudioDevicesAudioContextSinkIdDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id-unknown", testVirtualAudioDevicesAudioContextSinkIdUnknown);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-mix", testVirtualAudioDevicesAudioContextMix);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-planar", testVirtualAudioDevicesAudioContextPlanar);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-s24", testVirtualAudioDevicesAudioContextS24);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-channel-mask", testVirtualAudioDevicesAudioContextChannelMask);
    VirtualAudioDevicesWithoutSpeakerSelectionTest::add("WebKitWebView", "virtual-audio-devices-without-speaker-selection", testVirtualAudioDevicesWithoutSpeakerSelection);
#endif
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-default", testVirtualAudioDevicesAudioElementDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-video-element-default", testVirtualAudioDevicesVideoElementDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-sink-id", testVirtualAudioDevicesAudioElementSinkId);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-video-element-sink-id", testVirtualAudioDevicesVideoElementSinkId);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-sink-id-default", testVirtualAudioDevicesAudioElementSinkIdDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-sink-id-unknown", testVirtualAudioDevicesAudioElementSinkIdUnknown);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-sink-id-while-loading", testVirtualAudioDevicesAudioElementSinkIdWhileLoading);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-element-sink-id-keeps-volume", testVirtualAudioDevicesAudioElementSinkIdKeepsVolume);
#if ENABLE(WEB_AUDIO)
    kServer = new WebKitTestServer();
    kServer->run(serverCallback);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-cross-site-navigation", testVirtualAudioDevicesCrossSiteNavigation);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-web-process-crash", testVirtualAudioDevicesWebProcessCrash);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-cross-origin-frame", testVirtualAudioDevicesCrossOriginFrame);
#endif
}

void afterAll()
{
    delete kServer;
}
