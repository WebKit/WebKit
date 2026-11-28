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

#include <wtf/Function.h>
#include <wtf/MonotonicTime.h>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>
#include <wtf/glib/GLibExtras.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/MakeString.h>

static WebKitTestServer* kServer;

// Audio rendered on a virtual audio device, as received by its buffer callback.
struct VirtualAudioDeviceOutput {
    VirtualAudioDeviceOutput(WebKitAudioSampleFormat format, unsigned rate, unsigned channels, unsigned framesPerBuffer, WebKitAudioLayout layout = WEBKIT_AUDIO_LAYOUT_INTERLEAVED)
        : format(format)
        , layout(layout)
        , rate(rate)
        , bytesPerSample(sampleSize(format))
        , bytesPerFrame(bytesPerSample * channels)
        , framesPerBuffer(framesPerBuffer)
    {
    }

    static size_t sampleSize(WebKitAudioSampleFormat format)
    {
        switch (format) {
        case WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE:
            return 2;
        case WEBKIT_AUDIO_SAMPLE_FORMAT_S24LE:
            return 3;
        case WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE:
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
        firstTimestamp = std::nullopt;
        nextTimestamp = std::nullopt;
        minimumDeliveryOffset = std::nullopt;
        peak = 0;
        signalFrames = 0;
        signalPeak = 0;
        lastSign = 0;
        analyzedFrames = 0;
        lastRisingCrossing = std::nullopt;
        periods.clear();
    }

    // Sample normalized to [-1, 1].
    static double sampleValue(WebKitAudioSampleFormat format, std::span<const uint8_t> data)
    {
        if (format == WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE) {
            int16_t sample;
            memcpySpan(asMutableByteSpan(sample), data.first(sizeof(sample)));
            return sample / 32768.;
        }
        if (format == WEBKIT_AUDIO_SAMPLE_FORMAT_S24LE) {
            // Sign-extend the packed little endian 24 bits sample.
            int32_t sample = static_cast<int32_t>(static_cast<uint32_t>(data[0]) << 8 | static_cast<uint32_t>(data[1]) << 16 | static_cast<uint32_t>(data[2]) << 24) >> 8;
            return sample / 8388608.;
        }
        float sample;
        memcpySpan(asMutableByteSpan(sample), data.first(sizeof(sample)));
        return sample;
    }

    // The samples of the first channel are interleaved with the other channels, or they are the
    // first plane of a non-interleaved buffer.
    size_t firstChannelStride() const
    {
        return layout == WEBKIT_AUDIO_LAYOUT_INTERLEAVED ? bytesPerFrame : bytesPerSample;
    }

    // Frequency of the signal of the first channel, estimated from the median of its periods, the
    // intervals between its rising zero crossings. A discontinuity of the signal, for instance
    // when the mixer drops late samples on a loaded machine, only changes the period around it:
    // counting all the crossings would include the extra ones it adds.
    double frequency() const
    {
        if (periods.isEmpty())
            return 0;
        auto sortedPeriods = periods;
        std::ranges::sort(sortedPeriods);
        return static_cast<double>(rate) / sortedPeriods[sortedPeriods.size() / 2];
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
        size_t frames = data.size() / bytesPerFrame;
        size_t stride = firstChannelStride();
        unsigned zeroRun = 0;
        bool hasSilence = false;
        for (size_t frame = 0; frame < frames && !hasSilence; ++frame) {
            zeroRun = sampleValue(format, data.subspan(frame * stride)) ? 0 : zeroRun + 1;
            hasSilence = zeroRun >= silenceRunLength;
        }
        auto firstFrame = std::exchange(analyzedFrames, analyzedFrames + frames);
        if (bufferPeak < signalThreshold || hasSilence) {
            lastSign = 0;
            lastRisingCrossing = std::nullopt;
            return;
        }
        // The resampler rings after inserted silence, only check the peak of continuous signal.
        if (lastSign)
            signalPeak = std::max(signalPeak, bufferPeak);
        for (size_t frame = 0; frame < frames; ++frame) {
            auto value = sampleValue(format, data.subspan(frame * stride));
            int sign = value > crossingThreshold ? 1 : (value < -crossingThreshold ? -1 : 0);
            if (!sign)
                continue;
            if (lastSign == -1 && sign == 1) {
                size_t crossing = firstFrame + frame;
                if (lastRisingCrossing)
                    periods.append(crossing - *lastRisingCrossing);
                lastRisingCrossing = crossing;
            }
            lastSign = sign;
        }
        signalFrames += frames;
    }

    static void bufferCallback(WebKitVirtualAudioDevice*, const guint8* data, gsize size, guint64 timestamp, gpointer userData)
    {
        auto& output = *static_cast<VirtualAudioDeviceOutput*>(userData);
        output.buffers++;
        output.bytes += size;
        if (size != output.framesPerBuffer * output.bytesPerFrame)
            output.invalidSizes++;

        // Every buffer must start where the previous one ended, allowing rounding errors.
        static constexpr int64_t timestampTolerance = 1000;
        if (!output.firstTimestamp)
            output.firstTimestamp = timestamp;
        bool isNewStream = !output.nextTimestamp;
        if (output.nextTimestamp && std::abs(static_cast<int64_t>(timestamp - *output.nextTimestamp)) > timestampTolerance) {
            output.timestampGaps++;
            isNewStream = true;
        }
        output.nextTimestamp = timestamp + (size / output.bytesPerFrame) * G_GUINT64_CONSTANT(1000000000) / output.rate;

        // The buffers of a stream are produced in real time, so the delivery time of each one is
        // its timestamp plus a constant offset plus a delivery delay that is never negative. The
        // smallest offset seen in the stream is the closest to that constant. The timestamps of a
        // new stream start from zero again.
        int64_t deliveryOffset = g_get_real_time() - static_cast<int64_t>(timestamp / 1000);
        if (isNewStream) {
            output.minimumDeliveryOffset = deliveryOffset;
            // The onset belongs to the previous stream, its offset can't be refined anymore.
            output.isRefiningOnset = false;
        } else
            output.minimumDeliveryOffset = std::min(*output.minimumDeliveryOffset, deliveryOffset);

        output.analyze(unsafeMakeSpan(data, size));

        if (output.isWaitingForOnset)
            output.recordOnset(unsafeMakeSpan(data, size), timestamp);
        if (output.isRefiningOnset)
            output.onsetDeliveryOffset = output.minimumDeliveryOffset;
    }

    // Records the stream time of the first sample of the first channel above the signal threshold.
    // Its wall clock time is estimated by onsetRealTime() from the buffers of the stream received
    // until then, including the ones received after the onset.
    void recordOnset(std::span<const uint8_t> data, uint64_t timestamp)
    {
        static constexpr double onsetThreshold = 0.05;
        size_t stride = firstChannelStride();
        size_t frames = data.size() / bytesPerFrame;
        for (size_t frame = 0; frame < frames; ++frame) {
            if (std::abs(sampleValue(format, data.subspan(frame * stride))) > onsetThreshold) {
                onsetTimestamp = timestamp + frame * G_GUINT64_CONSTANT(1000000000) / rate;
                isWaitingForOnset = false;
                isRefiningOnset = true;
                return;
            }
        }
    }

    // Wall clock time of the onset in microseconds, dated with the stream timestamps so that a
    // late delivery of its buffer to the UI main thread doesn't delay it.
    std::optional<int64_t> onsetRealTime() const
    {
        if (!onsetTimestamp)
            return std::nullopt;
        return *onsetDeliveryOffset + static_cast<int64_t>(*onsetTimestamp / 1000);
    }

    void startWaitingForOnset()
    {
        onsetTimestamp = std::nullopt;
        onsetDeliveryOffset = std::nullopt;
        isRefiningOnset = false;
        isWaitingForOnset = true;
    }

    bool isWaitingForOnset { false };
    bool isRefiningOnset { false };
    // Stream time of the onset, in nanoseconds.
    std::optional<uint64_t> onsetTimestamp;
    // Smallest difference between the delivery time and the timestamp of the buffers of the stream
    // of the onset, in microseconds.
    std::optional<int64_t> onsetDeliveryOffset;
    // The same for the buffers of the current stream.
    std::optional<int64_t> minimumDeliveryOffset;

    WebKitAudioSampleFormat format;
    WebKitAudioLayout layout;
    unsigned rate;
    size_t bytesPerSample;
    size_t bytesPerFrame;
    size_t framesPerBuffer;
    unsigned buffers { 0 };
    size_t bytes { 0 };
    unsigned invalidSizes { 0 };
    unsigned timestampGaps { 0 };
    std::optional<uint64_t> firstTimestamp;
    std::optional<uint64_t> nextTimestamp;
    double peak { 0 };
    double signalPeak { 0 };
    size_t signalFrames { 0 };
    int lastSign { 0 };
    // Frames analyzed since the last reset, signal or not, to locate the zero crossings.
    size_t analyzedFrames { 0 };
    std::optional<size_t> lastRisingCrossing;
    Vector<size_t> periods;
};

// Makes a web view created by a test visible, like WebViewTest::showInWindow() does for the test web view.
static void showWebView(WebKitWebView* webView)
{
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    if (auto* view = webkit_web_view_get_wpe_view(webView))
        wpe_view_set_visible(view, TRUE);
#else
    UNUSED_PARAM(webView);
#endif
}

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
        : m_defaultDevice(adoptGRef(webkit_virtual_audio_device_new("virtual-default", "Virtual default output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, TRUE)))
        , m_monoDevice(adoptGRef(webkit_virtual_audio_device_new("virtual-mono", "Virtual mono output", WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE, 44100, 1, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 128, FALSE)))
        // Devices covering other formats, layouts and channel layouts.
        , m_planarDevice(adoptGRef(webkit_virtual_audio_device_new("virtual-planar", "Virtual planar output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED, 256, FALSE)))
        , m_s24Device(adoptGRef(webkit_virtual_audio_device_new("virtual-s24", "Virtual S24 output", WEBKIT_AUDIO_SAMPLE_FORMAT_S24LE, 44100, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 441, FALSE)))
        // Front left, front right, rear left and rear right.
        , m_quadDevice(adoptGRef(webkit_virtual_audio_device_new("virtual-quad", "Virtual quad output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 4, 0x33, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 480, FALSE)))
        , m_defaultOutput(WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 480)
        , m_monoOutput(WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE, 44100, 1, 128)
        , m_planarOutput(WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 256, WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED)
        , m_s24Output(WEBKIT_AUDIO_SAMPLE_FORMAT_S24LE, 44100, 2, 441)
        , m_quadOutput(WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 4, 480)
    {
        g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(m_defaultDevice.get()), ==, 480);
        g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(m_monoDevice.get()), ==, 128);
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_defaultDevice.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_monoDevice.get()));
        webkit_virtual_audio_device_set_buffer_callback(m_defaultDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &m_defaultOutput, nullptr);
        webkit_virtual_audio_device_set_buffer_callback(m_monoDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &m_monoOutput, nullptr);
        for (auto& [device, output] : std::initializer_list<std::pair<WebKitVirtualAudioDevice*, VirtualAudioDeviceOutput*>> { { m_planarDevice.get(), &m_planarOutput }, { m_s24Device.get(), &m_s24Output }, { m_quadDevice.get(), &m_quadOutput } }) {
            g_assert_nonnull(device);
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(device));
            webkit_virtual_audio_device_set_buffer_callback(device, VirtualAudioDeviceOutput::bufferCallback, output, nullptr);
        }

        // The second default device is exposed as a regular device.
        GRefPtr<WebKitVirtualAudioDevice> secondDefaultDevice = adoptGRef(webkit_virtual_audio_device_new("virtual-second-default", "Virtual second default output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, TRUE));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(secondDefaultDevice.get()));

        m_devices = adoptGRef(g_list_store_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE));
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
            GRefPtr<WebKitVirtualAudioDevice> duplicatedDevice = adoptGRef(webkit_virtual_audio_device_new("virtual-mono", "Duplicated output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(duplicatedDevice.get()));
            g_list_store_append(m_devices.get(), duplicatedDevice.get());
            g_test_expect_message(nullptr, G_LOG_LEVEL_WARNING, "*more than one device named 'virtual-mono', ignoring item 6.");
        }

        // Listing audio output devices requires the ExposeSpeakers features.
        GRefPtr<WebKitSettings> settings = adoptGRef(webkit_settings_new());
        if (s_enableSpeakerSelection) {
            WebKitFeatureList* features = webkit_settings_get_all_features();
            for (auto identifier : { "ExposeSpeakers"_s, "ExposeSpeakersWithoutMicrophone"_s }) {
                auto* feature = webkit_feature_list_find(features, identifier.characters());
                g_assert_nonnull(feature);
                webkit_settings_set_feature_enabled(settings.get(), feature, TRUE);
            }
            webkit_feature_list_unref(features);
        }

        GRefPtr<WebKitWebsitePolicies> policies = adoptGRef(webkit_website_policies_new_with_policies("autoplay", WEBKIT_AUTOPLAY_ALLOW, nullptr));
        m_webView = createWebView(
            "user-content-manager", m_userContentManager.get(),
            "website-policies", policies.get(),
            "settings", settings.get(),
            "virtual-audio-devices", m_devices.get(),
            nullptr);
        if (s_addDuplicatedDevice)
            g_test_assert_expected_messages();
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_webView.get()));
#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
        if (auto* view = webkit_web_view_get_wpe_view(m_webView.get()))
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(view));
#endif
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

    void loadPage(UTF8CStringView html, WebKitWebView* webView = nullptr)
    {
        auto baseURI = GMallocString::unsafeAdoptFromUTF8(SAFE_G_STRDUP_PRINTF("file://%s/", getResourcesDir()));
        loadHtml(html.utf8(), baseURI.utf8(), webView);
        waitUntilLoadFinished(webView);
    }

    // Runs a script in another web view, where runAsync() can't be used.
    String runInWebView(WebKitWebView* webView, UTF8CStringView script)
    {
        GUniqueOutPtr<GError> error;
        auto* value = runJavaScriptAndWaitUntilFinished(script, &error.outPtr(), webView);
        g_assert_no_error(error.get());
        return GMallocString::unsafeAdoptFromUTF8(javascriptResultToCString(value)).span();
    }

    // Runs a script in another web view until its result satisfies the condition, or a timeout
    // expires. Can't use waitUntil(), whose timer would interrupt the JavaScript evaluation.
    String waitForScriptResult(WebKitWebView* webView, UTF8CStringView script, NOESCAPE const Function<bool(const String&)>& condition)
    {
        auto deadline = MonotonicTime::now() + 4_s;
        String result;
        while (!condition(result = runInWebView(webView, script)) && MonotonicTime::now() < deadline)
            wait(0.02);
        return result;
    }

    // The labels of the audio outputs listed by another web view.
    String audioOutputsOfWebView(WebKitWebView* webView)
    {
        runInWebView(webView,
            "navigator.mediaDevices.enumerateDevices().then(devices => {"
            "    window.audioOutputs = devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"
            "}, error => { window.audioOutputs = 'error: ' + error.name; }); '';"_s);
        auto audioOutputs = waitForScriptResult(webView, "window.audioOutputs === undefined ? '[pending]' : window.audioOutputs"_s, [](const String& result) {
            return result != "[pending]"_s;
        });
        g_assert_true(audioOutputs != "[pending]"_s);
        g_assert_false(audioOutputs.startsWith("error"_s));
        return audioOutputs;
    }

    // Plays an oscillator of the given frequency in another web view, on its default device.
    void startOscillatorInWebView(WebKitWebView* webView, unsigned frequency)
    {
        runInWebView(webView, makeUTF8CString(makeString(
            "window.context = new AudioContext();"
            "const oscillator = context.createOscillator();"
            "oscillator.frequency.value = "_s, frequency, ";"
            "oscillator.connect(context.destination);"
            "oscillator.start();"
            "context.resume(); '';"_s)));
        auto state = waitForScriptResult(webView, "context.state"_s, [](const String& result) {
            return result == "running"_s;
        });
        g_assert_true(state == "running"_s);
    }

    // The device IDs are opaque, scripts find the ID of a device from its label with sinkIdFor().
    GMallocString runAsync(const String& body)
    {
        static constexpr auto helpers =
            "const sinkIdFor = async label => {"
            "    const devices = await navigator.mediaDevices.enumerateDevices();"
            "    const device = devices.find(device => device.kind === 'audiooutput' && device.label === label);"
            "    return device ? device.deviceId : 'not-found: ' + label;"
            "};"_s;
        auto script = makeString(helpers, body);
        GUniqueOutPtr<GError> error;
        auto* value = runAsyncJavaScriptFunctionInWorldAndWaitUntilFinished(makeUTF8CString(script).legacyCStringPointer(), nullptr, nullptr, &error.outPtr());
        g_assert_no_error(error.get());
        if (!value || jsc_value_is_undefined(value))
            return { };
        return GMallocString::unsafeAdoptFromUTF8(javascriptResultToCString(value));
    }

    // The labels of the audio outputs listed by the page.
    String audioOutputs()
    {
        return runAsync(
            "const devices = await navigator.mediaDevices.enumerateDevices();"
            "return devices.filter(device => device.kind === 'audiooutput').map(device => device.label).join(',');"_s).span();
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
    GRefPtr<WebKitVirtualAudioDevice> m_defaultDevice;
    GRefPtr<WebKitVirtualAudioDevice> m_monoDevice;
    GRefPtr<WebKitVirtualAudioDevice> m_planarDevice;
    GRefPtr<WebKitVirtualAudioDevice> m_s24Device;
    GRefPtr<WebKitVirtualAudioDevice> m_quadDevice;
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
    auto devices = test->audioOutputs();

    // The default virtual device is the first audio output device, after the "default" entry that
    // represents it. The virtual devices are listed without microphone access.
    g_assert_true(devices.startsWith("Default - Virtual default output,Virtual default output,"_s));
    g_assert_true(devices.contains("Virtual mono output"_s));
    g_assert_true(devices.contains("Virtual second default output"_s));
    g_assert_true(devices.contains("Virtual planar output"_s));

    // The device IDs are opaque, unique, and not the device names.
    String deviceIds = test->runAsync(
        "const ids = (await navigator.mediaDevices.enumerateDevices()).filter(device => device.kind === 'audiooutput').map(device => device.deviceId);"
        "if (ids.shift() !== 'default' || ids.some(id => !id || id.startsWith('virtual-')))"
        "    return 'invalid: ' + ids.join(',');"
        "return new Set(ids).size === ids.length ? 'unique' : 'duplicated: ' + ids.join(',');"_s).span();
    g_assert_true(deviceIds == "unique"_s);
}

static void testVirtualAudioDevicesDuplicatedName(VirtualAudioDevicesDuplicatedNameTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    auto devices = test->audioOutputs();

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
        // The popup has the settings of its opener, so that it can list audio outputs.
        data->popup = data->test->createWebView("related-view", webView, "settings", webkit_web_view_get_settings(webView), nullptr);
        showWebView(data->popup.get());
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

    g_assert_false(test->audioOutputsOfWebView(popup.get()).contains("Virtual"_s));

#if ENABLE(WEB_AUDIO)
    // The audio of the popup isn't rendered on the virtual devices of its opener.
    test->startOscillatorInWebView(popup.get(), 440);
    test->assertDeviceStopped(test->m_defaultOutput);
    test->assertDeviceStopped(test->m_monoOutput);
#endif
}

static void testVirtualAudioDevicesPopupWithDevices(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The popup has its own devices.
    VirtualAudioDeviceOutput popupOutput(WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 480);
    GRefPtr<WebKitVirtualAudioDevice> popupDevice = adoptGRef(webkit_virtual_audio_device_new("virtual-popup", "Virtual popup output", WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, TRUE));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(popupDevice.get()));
    webkit_virtual_audio_device_set_buffer_callback(popupDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &popupOutput, nullptr);
    GRefPtr<GListStore> popupDevices = adoptGRef(g_list_store_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE));
    g_list_store_append(popupDevices.get(), popupDevice.get());

    struct PopupData {
        VirtualAudioDevicesTest* test;
        GListStore* devices;
        GRefPtr<WebKitWebView> popup;
    } data { test, popupDevices.get(), nullptr };
    g_signal_connect(test->webView(), "create", G_CALLBACK(+[](WebKitWebView* webView, WebKitNavigationAction*, PopupData* data) -> WebKitWebView* {
        data->popup = data->test->createWebView("related-view", webView, "settings", webkit_web_view_get_settings(webView), "virtual-audio-devices", data->devices, nullptr);
        showWebView(data->popup.get());
        return data->popup.get();
    }), &data);

    test->loadPage("<html><body></body></html>"_s);
    test->runAsync("window.popup = window.open('about:blank');"_s);
    g_signal_handlers_disconnect_matched(test->webView(), G_SIGNAL_MATCH_DATA, 0, 0, nullptr, nullptr, &data);
    GRefPtr popup = WTF::move(data.popup);
    g_assert_nonnull(popup.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(popup.get()));
    popupDevices = nullptr;

    // The popup lists its own devices, not the ones of its opener.
    auto audioOutputs = test->audioOutputsOfWebView(popup.get());
    g_assert_true(audioOutputs.contains("Virtual popup output"_s));
    g_assert_false(audioOutputs.contains("Virtual mono output"_s));

#if ENABLE(WEB_AUDIO)
    // The audio of the popup is rendered on its default device.
    test->startOscillatorInWebView(popup.get(), 440);
    test->waitForAudio(popupOutput, 0.5);
    test->assertDeviceStopped(test->m_defaultOutput);
#endif

    // The output is destroyed before the device.
    webkit_virtual_audio_device_set_buffer_callback(popupDevice.get(), nullptr, nullptr, nullptr);
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
        "await context.resume();"_s));
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
    webkit_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, nullptr);
    test->waitForSilence();
    test->m_defaultOutput.reset();
    s_destroyNotifyCount = 0;
    webkit_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), VirtualAudioDeviceOutput::bufferCallback, &test->m_defaultOutput, countDestroyNotify);
    test->waitForAudio(test->m_defaultOutput, 0.5);

    // Replacing the callback releases the previous user data, even when it's null.
    webkit_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, countDestroyNotify);
    g_assert_cmpuint(s_destroyNotifyCount, ==, 1);
    webkit_virtual_audio_device_set_buffer_callback(test->m_defaultDevice.get(), nullptr, nullptr, nullptr);
    g_assert_cmpuint(s_destroyNotifyCount, ==, 2);
}

static void testVirtualAudioDevicesAudioContextNewStream(VirtualAudioDevicesTest* test, gconstpointer)
{
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
    g_assert_true(test->waitUntil([&] {
        return *test->m_defaultOutput.nextTimestamp >= 1 * G_GUINT64_CONSTANT(1000000000);
    }));

    // Once all the producers are gone, the stream ends: the timestamps of the next one start from
    // zero again, even when the next stream starts before the idle mixer pipeline of the device
    // is torn down.
    test->runAsync("await context.close();"_s);
    test->waitForSilence();
    test->m_defaultOutput.reset();
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
    g_assert_cmpuint(*test->m_defaultOutput.firstTimestamp, <, 500 * G_GUINT64_CONSTANT(1000000));
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
    // depends on how much audio is waiting in the mixer, which isn't known, it's around 10ms. The
    // onset is dated with the stream timestamps, so a late delivery of its buffer to the UI main
    // thread doesn't count. The bounds only catch big errors, such as counting the 100ms latency
    // of the interaudiosrc twice.
    static constexpr double minError = -50;
    static constexpr double maxError = 100;
    for (unsigned i = 0; i < 2; ++i) {
        test->m_defaultOutput.startWaitingForOnset();
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
            return !!test->m_defaultOutput.onsetRealTime();
        }));

        double fromTimestamp = 0;
        double fromLatency = 0;
        g_assert_cmpint(sscanf(predictions.utf8(), "%lf %lf", &fromTimestamp, &fromLatency), ==, 2);
        double onset = *test->m_defaultOutput.onsetRealTime() / 1000.;
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

static void testVirtualAudioDevicesAudioContextSilentSink(VirtualAudioDevicesTest* test, gconstpointer)
{
    // A context rendering on no device renders silence on the default device of the page, rather
    // than opening the system output.
    test->loadPage("<html><body></body></html>"_s);
    auto sinkType = test->runAsync(
        "window.context = new AudioContext({ sinkId: { type: 'none' } });"
        "const oscillator = context.createOscillator();"
        "oscillator.connect(context.destination);"
        "oscillator.start();"
        "await context.resume();"
        "return context.sinkId.type;"_s);
    g_assert_true(sinkType == "none"_s);
    test->waitForBuffers(test->m_defaultOutput);
    g_assert_cmpfloat(test->m_defaultOutput.peak, ==, 0);

    // It's audible again on the same device once the sink is the default one.
    test->runAsync("await context.setSinkId('');"_s);
    test->m_defaultOutput.reset();
    test->waitForAudio(test->m_defaultOutput, 0.5);
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

static void testVirtualAudioDevicesAudioContextWhileElementPrerolls(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The oscillator and the element are mixed on the same device, and the element's sine has an
    // amplitude of 0.8: attenuate the oscillator so that their sum never clips.
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(
        "window.context = new AudioContext();"
        "const oscillator = context.createOscillator();"
        "const gain = context.createGain();"
        "gain.gain.value = 0.2;"
        "oscillator.connect(gain);"
        "gain.connect(context.destination);"
        "oscillator.start();"
        "await context.resume();"_s);
    static constexpr double oscillatorPeak = 0.15;
    test->waitForAudio(test->m_defaultOutput, oscillatorPeak);

    // A media element prerolling on the same device doesn't pause the device for the context.
    test->runAsync(
        "window.element = document.createElement('audio');"
        "element.preload = 'auto';"
        "element.src = 'sine-440hz.webm';"
        "document.body.appendChild(element);"
        "await new Promise(resolve => element.addEventListener('canplaythrough', resolve, { once: true }));"_s);
    test->m_defaultOutput.reset();
    test->waitForAudio(test->m_defaultOutput, oscillatorPeak);

    // Nor does pausing it after it played.
    test->runAsync("await element.play(); element.pause();"_s);
    test->m_defaultOutput.reset();
    test->waitForAudio(test->m_defaultOutput, oscillatorPeak);

    // And the device keeps playing the element when the context is suspended.
    test->runAsync("element.loop = true; await element.play(); await context.suspend();"_s);
    test->waitForSilence();
    test->m_defaultOutput.reset();
    test->waitForAudio(test->m_defaultOutput, 0.2);
}

static void testVirtualAudioDevicesWithoutSpeakerSelection(VirtualAudioDevicesWithoutSpeakerSelectionTest* test, gconstpointer)
{
    // Without the ExposeSpeakers features, the virtual devices are not listed.
    test->loadPage("<html><body></body></html>"_s);
    auto devices = test->audioOutputs();
    g_assert_false(devices.contains("Virtual"_s));

    // But the default virtual device is still used.
    startOscillator(test);
    test->waitForAudio(test->m_defaultOutput, 0.5);
}
#endif // ENABLE(WEB_AUDIO)

static void testVirtualAudioDevicesSharedDevice(VirtualAudioDevicesTest* test, gconstpointer)
{
    // A device already used by a web view is ignored by another web view, and its name can still
    // be used by another device of the list.
    GRefPtr<GListStore> devices = adoptGRef(g_list_store_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE));
    g_list_store_append(devices.get(), test->m_monoDevice.get());
    GRefPtr<WebKitVirtualAudioDevice> otherMonoDevice = adoptGRef(webkit_virtual_audio_device_new("virtual-mono", "Other mono output", WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE, 44100, 1, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(otherMonoDevice.get()));
    g_list_store_append(devices.get(), otherMonoDevice.get());
    otherMonoDevice = nullptr;
    g_test_expect_message(nullptr, G_LOG_LEVEL_WARNING, "*device 'virtual-mono' is already used by another web view, ignoring item 0.");
    GRefPtr<WebKitWebView> otherView = test->createWebView(
        "settings", webkit_web_view_get_settings(test->webView()),
        "website-policies", webkit_web_view_get_website_policies(test->webView()),
        "virtual-audio-devices", devices.get(),
        nullptr);
    g_test_assert_expected_messages();
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(otherView.get()));
    showWebView(otherView.get());
    devices = nullptr;

    test->loadPage("<html><body></body></html>"_s, otherView.get());
    auto otherViewOutputs = test->audioOutputsOfWebView(otherView.get());
    g_assert_false(otherViewOutputs.contains("Virtual mono output"_s));
    g_assert_true(otherViewOutputs.contains("Other mono output"_s));

#if ENABLE(WEB_AUDIO)
    // The audio of the other web view isn't mixed in the stream of the first one, whose buffers
    // stay contiguous.
    test->loadPage("<html><body></body></html>"_s);
    startOscillator(test, "Virtual mono output"_s);
    test->waitForAudio(test->m_monoOutput, 0.5);
    test->startOscillatorInWebView(otherView.get(), 660);
    test->m_monoOutput.reset();
    test->waitForAudio(test->m_monoOutput, 0.5);
#endif
}

// sine-440hz.webm is 1 second of a 440 Hz sine (Opus, 48 kHz stereo) with a small VP8 video
// track, generated with:
// gst-launch-1.0 -e audiotestsrc freq=440 num-buffers=50 samplesperbuffer=960 ! audio/x-raw,rate=48000,channels=2 ! opusenc bitrate=32000 ! queue ! mux.
//     videotestsrc num-buffers=30 ! video/x-raw,width=80,height=60,framerate=30/1 ! vp8enc ! queue ! webmmux name=mux ! filesink location=sine-440hz.webm

static void testVirtualAudioDevicesMediaElementDefault(VirtualAudioDevicesTest* test, ASCIILiteral elementName)
{
    test->loadPage("<html><body></body></html>"_s);
    test->runAsync(makeString(
        "const element = document.createElement('"_s, elementName, "');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "document.body.appendChild(element);"
        "await element.play();"_s));

    test->waitForAudio(test->m_defaultOutput, 0.2);
    test->waitForSilence();
    g_assert_cmpuint(test->m_monoOutput.buffers, ==, 0);
}

static void testVirtualAudioDevicesMediaElementSinkId(VirtualAudioDevicesTest* test, ASCIILiteral elementName)
{
    test->loadPage("<html><body></body></html>"_s);
    auto sinkId = test->runAsync(makeString(
        "window.element = document.createElement('"_s, elementName, "');"
        "element.src = 'sine-440hz.webm';"
        "element.loop = true;"
        "document.body.appendChild(element);"
        "const monoId = await sinkIdFor('Virtual mono output');"
        "await element.setSinkId(monoId);"
        "await element.play();"
        "return element.sinkId === monoId ? 'mono' : element.sinkId;"_s));
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

// beep-1s-440hz.webm is 2 seconds of a small video, with a 440 Hz sine (Opus, 48 kHz stereo) from
// 1 to 1.5 seconds and silence otherwise, generated with:
// ffmpeg -f lavfi -i "aevalsrc='if(between(t,1,1.5),0.8*sin(2*PI*440*t),0)|if(between(t,1,1.5),0.8*sin(2*PI*440*t),0)':s=48000:d=2"
//     -f lavfi -i "testsrc=size=80x60:rate=30:d=2" -c:a libopus -b:a 32k -c:v libvpx -b:v 50k -map_metadata -1 -fflags +bitexact beep-1s-440hz.webm
static void testVirtualAudioDevicesVideoElementLipSync(VirtualAudioDevicesTest* test, ASCIILiteral sinkLabel, VirtualAudioDeviceOutput& output)
{
    test->loadPage("<html><body></body></html>"_s);
    output.startWaitingForOnset();
    auto setSinkId = sinkLabel.isNull() ? String() : makeString("await element.setSinkId(await sinkIdFor('"_s, sinkLabel, "'));"_s);
    test->runAsync(makeString(
        "window.element = document.createElement('video');"
        "element.src = 'beep-1s-440hz.webm';"
        "document.body.appendChild(element);"_s,
        setSinkId,
        "await element.play();"_s));
    g_assert_true(test->waitUntil([&] {
        return !!output.onsetRealTime();
    }));

    // The beep must be played when the media time, which is the time of the video frames, is 1
    // second: estimate the media time when the device received it.
    auto times = test->runAsync("return element.currentTime + ' ' + (performance.timeOrigin + performance.now());"_s);
    double currentTime = 0;
    double now = 0;
    g_assert_cmpint(sscanf(times.utf8(), "%lf %lf", &currentTime, &now), ==, 2);
    double onset = *output.onsetRealTime() / 1000.;
    double mediaTimeAtOnset = currentTime - (now - onset) / 1000;
    // Positive when the audio is late. It's around 1ms, the audio used to be late by the latency of
    // the mixer pipeline, around 100ms.
    double error = (mediaTimeAtOnset - 1) * 1000;
    g_test_message("Lip-sync error: %.1f ms", error);
    g_assert_cmpfloat(std::abs(error), <, 20);
}

static void testVirtualAudioDevicesVideoElementLipSyncDefault(VirtualAudioDevicesTest* test, gconstpointer)
{
    testVirtualAudioDevicesVideoElementLipSync(test, { }, test->m_defaultOutput);
}

static void testVirtualAudioDevicesVideoElementLipSyncSinkId(VirtualAudioDevicesTest* test, gconstpointer)
{
    // The mono device has a different rate and buffer size, and a different mixer latency.
    testVirtualAudioDevicesVideoElementLipSync(test, "Virtual mono output"_s, test->m_monoOutput);
}

// Plays media elements in a row on the default device, with the source given by the sourceURL
// script expression. The mixer pipeline of a device is kept for a while after its last producer
// is gone, so the producers of the second and third elements are registered while its teardown
// timer is armed. When that happens off the main thread, the timer must not be stopped there.
static void testVirtualAudioDevicesMediaElementsInARow(VirtualAudioDevicesTest* test, ASCIILiteral sourceURL)
{
    for (auto elementName : { "audio"_s, "video"_s, "audio"_s }) {
        test->m_defaultOutput.reset();
        test->runAsync(makeString(
            "const element = document.createElement('"_s, elementName, "');"
            "element.src = "_s, sourceURL, ";"
            "element.loop = true;"
            "document.body.appendChild(element);"
            "await element.play();"_s));
        test->waitForAudio(test->m_defaultOutput, 0.2);

        // Destroy the player, which unregisters its producer.
        test->runAsync(
            "const element = document.querySelector('audio, video');"
            "element.pause();"
            "element.removeAttribute('src');"
            "element.load();"
            "element.remove();"_s);
        test->waitForSilence();
    }
}

static void testVirtualAudioDevicesMediaElementsInARowFile(VirtualAudioDevicesTest* test, gconstpointer)
{
    // A regular URL plays through playbin3 only with GStreamer >= 1.28, and only when
    // WEBKIT_GST_USE_PLAYBIN2 isn't set to a non-zero integer. playbin3 registers the producers
    // on a streaming thread since GStreamer 1.22. Otherwise (playbin, or playbin3 of GStreamer
    // 1.20), they are registered on the main thread, while the pipeline goes to PAUSED.
    test->loadPage("<html><body></body></html>"_s);
    testVirtualAudioDevicesMediaElementsInARow(test, "'sine-440hz.webm'"_s);
}

static void testVirtualAudioDevicesMediaElementsInARowBlob(VirtualAudioDevicesTest* test, gconstpointer)
{
    // Blob URLs always play through playbin3, which registers the producers on a streaming thread
    // since GStreamer 1.22, whatever WEBKIT_GST_USE_PLAYBIN2 is set to. With GStreamer 1.20, the
    // oldest supported version, every playbin registers them on the main thread: no test covers
    // a registration off the main thread there.
    test->loadPage("<html><body></body></html>"_s);
    // One second of a 440 Hz sine with an amplitude of 0.8, as a 48 kHz mono 16-bit WAV file.
    test->runAsync(
        "const rate = 48000;"
        "const view = new DataView(new ArrayBuffer(44 + rate * 2));"
        "const writeString = (offset, string) => {"
        "    for (let i = 0; i < string.length; ++i)"
        "        view.setUint8(offset + i, string.charCodeAt(i));"
        "};"
        "writeString(0, 'RIFF');"
        "view.setUint32(4, 36 + rate * 2, true);"
        "writeString(8, 'WAVEfmt ');"
        "view.setUint32(16, 16, true);"
        "view.setUint16(20, 1, true);"
        "view.setUint16(22, 1, true);"
        "view.setUint32(24, rate, true);"
        "view.setUint32(28, rate * 2, true);"
        "view.setUint16(32, 2, true);"
        "view.setUint16(34, 16, true);"
        "writeString(36, 'data');"
        "view.setUint32(40, rate * 2, true);"
        "for (let i = 0; i < rate; ++i)"
        "    view.setInt16(44 + i * 2, Math.round(0x7fff * 0.8 * Math.sin(2 * Math.PI * 440 * i / rate)), true);"
        "window.sineURL = URL.createObjectURL(new Blob([view.buffer], { type: 'audio/wav' }));"_s);
    testVirtualAudioDevicesMediaElementsInARow(test, "sineURL"_s);
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
    auto pathView = UTF8CStringView::unsafeFromUTF8(path);
    if (pathView == "/"_s)
        content = html;
    else if (pathView == "/frame"_s)
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
    return test->runAsync(script).span();
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

    test->loadURI(makeUTF8CString(makeString("http://localhost:"_s, kServer->port(), '/')));
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

static void testVirtualAudioDeviceNew(Test* test, gconstpointer)
{
    GRefPtr<WebKitVirtualAudioDevice> device = adoptGRef(webkit_virtual_audio_device_new("device", "Description", WEBKIT_AUDIO_SAMPLE_FORMAT_S24_32BE, 96000, 3, 0x7, WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED, 64, TRUE));
    g_assert_nonnull(device.get());
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(device.get()));
    g_assert_cmpstr(webkit_virtual_audio_device_get_name(device.get()), ==, "device");
    g_assert_cmpstr(webkit_virtual_audio_device_get_description(device.get()), ==, "Description");
    g_assert_cmpint(webkit_virtual_audio_device_get_format(device.get()), ==, WEBKIT_AUDIO_SAMPLE_FORMAT_S24_32BE);
    g_assert_cmpuint(webkit_virtual_audio_device_get_rate(device.get()), ==, 96000);
    g_assert_cmpuint(webkit_virtual_audio_device_get_channels(device.get()), ==, 3);
    g_assert_cmpuint(webkit_virtual_audio_device_get_channel_mask(device.get()), ==, 0x7);
    g_assert_cmpint(webkit_virtual_audio_device_get_layout(device.get()), ==, WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED);
    g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(device.get()), ==, 64);
    g_assert_true(webkit_virtual_audio_device_get_is_default(device.get()));

    // The configuration is exposed as properties too.
    char* name;
    guint rate;
    WebKitAudioSampleFormat format;
    gboolean isDefault;
    g_object_get(device.get(), "name", &name, "rate", &rate, "format", &format, "is-default", &isDefault, nullptr);
    g_assert_true(GMallocString::unsafeAdoptFromUTF8(name) == "device"_s);
    g_assert_cmpuint(rate, ==, 96000);
    g_assert_cmpint(format, ==, WEBKIT_AUDIO_SAMPLE_FORMAT_S24_32BE);
    g_assert_true(isDefault);

    // Without description, the description is null.
    GRefPtr<WebKitVirtualAudioDevice> withoutDescription = adoptGRef(webkit_virtual_audio_device_new("device", nullptr, WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(withoutDescription.get()));
    g_assert_null(webkit_virtual_audio_device_get_description(withoutDescription.get()));
}

static void testVirtualAudioDeviceDefaultFramesPerBuffer(Test* test, gconstpointer)
{
    // The default buffers are about 10 milliseconds long.
    GRefPtr<WebKitVirtualAudioDevice> device = adoptGRef(webkit_virtual_audio_device_new("device", nullptr, WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 44100, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(device.get()));
    g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(device.get()), ==, 441);

    // They are shortened to fit in 64 KiB: 65536 / (64 channels × 8 bytes) = 128 frames.
    GRefPtr<WebKitVirtualAudioDevice> bigFramesDevice = adoptGRef(webkit_virtual_audio_device_new("device", nullptr, WEBKIT_AUDIO_SAMPLE_FORMAT_F64LE, 768000, 64, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(bigFramesDevice.get()));
    g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(bigFramesDevice.get()), ==, 128);

    // The same applies to devices created with g_object_new().
    GRefPtr<WebKitVirtualAudioDevice> objectDevice = adoptGRef(WEBKIT_VIRTUAL_AUDIO_DEVICE(g_object_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE, "name", "device", "rate", 8000, nullptr)));
    test->assertObjectIsDeletedWhenTestFinishes(G_OBJECT(objectDevice.get()));
    g_assert_cmpuint(webkit_virtual_audio_device_get_frames_per_buffer(objectDevice.get()), ==, 80);
    g_assert_cmpuint(webkit_virtual_audio_device_get_channels(objectDevice.get()), ==, 2);
    g_assert_cmpint(webkit_virtual_audio_device_get_format(objectDevice.get()), ==, WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE);
}

static void testVirtualAudioDeviceBufferTooBig(Test*, gconstpointer)
{
    // 65536 / (2 channels × 4 bytes) = 8192 frames at most.
    GRefPtr<WebKitVirtualAudioDevice> device = adoptGRef(webkit_virtual_audio_device_new("device", nullptr, WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 8192, FALSE));
    g_assert_nonnull(device.get());
    device = adoptGRef(webkit_virtual_audio_device_new("device", nullptr, WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WEBKIT_AUDIO_LAYOUT_INTERLEAVED, 8193, FALSE));
    g_assert_null(device.get());
}

static void testVirtualAudioDeviceInvalidProperties(Test*, gconstpointer)
{
    if (g_test_subprocess()) {
        // The number of bits of the channel mask doesn't match the number of channels.
        GRefPtr<GObject> device = adoptGRef(G_OBJECT(g_object_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE, "name", "device", "channels", 2, "channel-mask", G_GUINT64_CONSTANT(0x7), nullptr)));
        return;
    }
    g_test_trap_subprocess(nullptr, 0, G_TEST_SUBPROCESS_DEFAULT);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*channel mask doesn't match the number of channels*");
}

void beforeAll()
{
    Test::add("WebKitVirtualAudioDevice", "new", testVirtualAudioDeviceNew);
    Test::add("WebKitVirtualAudioDevice", "default-frames-per-buffer", testVirtualAudioDeviceDefaultFramesPerBuffer);
    Test::add("WebKitVirtualAudioDevice", "buffer-too-big", testVirtualAudioDeviceBufferTooBig);
    Test::add("WebKitVirtualAudioDevice", "invalid-properties", testVirtualAudioDeviceInvalidProperties);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-property", testVirtualAudioDevicesProperty);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-enumerate", testVirtualAudioDevicesEnumerate);
    VirtualAudioDevicesDuplicatedNameTest::add("WebKitWebView", "virtual-audio-devices-duplicated-name", testVirtualAudioDevicesDuplicatedName);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-popup", testVirtualAudioDevicesPopup);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-popup-with-devices", testVirtualAudioDevicesPopupWithDevices);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-shared-device", testVirtualAudioDevicesSharedDevice);
#if ENABLE(WEB_AUDIO)
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-default", testVirtualAudioDevicesAudioContextDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-output-timestamp", testVirtualAudioDevicesAudioContextOutputTimestamp);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-new-stream", testVirtualAudioDevicesAudioContextNewStream);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id", testVirtualAudioDevicesAudioContextSinkId);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id-default", testVirtualAudioDevicesAudioContextSinkIdDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-sink-id-unknown", testVirtualAudioDevicesAudioContextSinkIdUnknown);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-silent-sink", testVirtualAudioDevicesAudioContextSilentSink);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-mix", testVirtualAudioDevicesAudioContextMix);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-planar", testVirtualAudioDevicesAudioContextPlanar);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-s24", testVirtualAudioDevicesAudioContextS24);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-channel-mask", testVirtualAudioDevicesAudioContextChannelMask);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-audio-context-while-element-prerolls", testVirtualAudioDevicesAudioContextWhileElementPrerolls);
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
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-video-element-lip-sync", testVirtualAudioDevicesVideoElementLipSyncDefault);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-video-element-lip-sync-sink-id", testVirtualAudioDevicesVideoElementLipSyncSinkId);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-media-elements-in-a-row", testVirtualAudioDevicesMediaElementsInARowFile);
    VirtualAudioDevicesTest::add("WebKitWebView", "virtual-audio-devices-media-elements-in-a-row-blob", testVirtualAudioDevicesMediaElementsInARowBlob);
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
