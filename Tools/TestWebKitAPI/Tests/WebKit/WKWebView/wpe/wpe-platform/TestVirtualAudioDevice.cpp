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

#include "WPEPlatformTest.h"
#include <wpe/wpe-platform.h>
#include <wtf/glib/GRefPtr.h>

namespace TestWebKitAPI {

static void testVirtualAudioDeviceNew(WPEPlatformTest* test, gconstpointer)
{
    GRefPtr<WPEVirtualAudioDevice> device = adoptGRef(wpe_virtual_audio_device_new("device", "Description", WPE_AUDIO_SAMPLE_FORMAT_S24_32BE, 96000, 3, 0x7, WPE_AUDIO_LAYOUT_NON_INTERLEAVED, 64, TRUE));
    g_assert_nonnull(device.get());
    test->assertObjectIsDeletedWhenTestFinishes(device.get());
    g_assert_cmpstr(wpe_virtual_audio_device_get_name(device.get()), ==, "device");
    g_assert_cmpstr(wpe_virtual_audio_device_get_description(device.get()), ==, "Description");
    g_assert_cmpint(wpe_virtual_audio_device_get_format(device.get()), ==, WPE_AUDIO_SAMPLE_FORMAT_S24_32BE);
    g_assert_cmpuint(wpe_virtual_audio_device_get_rate(device.get()), ==, 96000);
    g_assert_cmpuint(wpe_virtual_audio_device_get_channels(device.get()), ==, 3);
    g_assert_cmpuint(wpe_virtual_audio_device_get_channel_mask(device.get()), ==, 0x7);
    g_assert_cmpint(wpe_virtual_audio_device_get_layout(device.get()), ==, WPE_AUDIO_LAYOUT_NON_INTERLEAVED);
    g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(device.get()), ==, 64);
    g_assert_true(wpe_virtual_audio_device_get_is_default(device.get()));

    // The configuration is exposed as properties too.
    GUniqueOutPtr<char> name;
    guint rate;
    WPEAudioSampleFormat format;
    gboolean isDefault;
    g_object_get(device.get(), "name", &name.outPtr(), "rate", &rate, "format", &format, "is-default", &isDefault, nullptr);
    g_assert_cmpstr(name.get(), ==, "device");
    g_assert_cmpuint(rate, ==, 96000);
    g_assert_cmpint(format, ==, WPE_AUDIO_SAMPLE_FORMAT_S24_32BE);
    g_assert_true(isDefault);

    // Without description, the description is null.
    GRefPtr<WPEVirtualAudioDevice> withoutDescription = adoptGRef(wpe_virtual_audio_device_new("device", nullptr, WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(withoutDescription.get());
    g_assert_null(wpe_virtual_audio_device_get_description(withoutDescription.get()));
}

static void testVirtualAudioDeviceDefaultFramesPerBuffer(WPEPlatformTest* test, gconstpointer)
{
    // The default buffers are about 10 milliseconds long.
    GRefPtr<WPEVirtualAudioDevice> device = adoptGRef(wpe_virtual_audio_device_new("device", nullptr, WPE_AUDIO_SAMPLE_FORMAT_F32LE, 44100, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(device.get());
    g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(device.get()), ==, 441);

    // They are shortened to fit in 64 KiB: 65536 / (64 channels × 8 bytes) = 128 frames.
    GRefPtr<WPEVirtualAudioDevice> bigFramesDevice = adoptGRef(wpe_virtual_audio_device_new("device", nullptr, WPE_AUDIO_SAMPLE_FORMAT_F64LE, 768000, 64, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 0, FALSE));
    test->assertObjectIsDeletedWhenTestFinishes(bigFramesDevice.get());
    g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(bigFramesDevice.get()), ==, 128);

    // The same applies to devices created with g_object_new().
    GRefPtr<WPEVirtualAudioDevice> objectDevice = adoptGRef(WPE_VIRTUAL_AUDIO_DEVICE(g_object_new(WPE_TYPE_VIRTUAL_AUDIO_DEVICE, "name", "device", "rate", 8000, nullptr)));
    test->assertObjectIsDeletedWhenTestFinishes(objectDevice.get());
    g_assert_cmpuint(wpe_virtual_audio_device_get_frames_per_buffer(objectDevice.get()), ==, 80);
    g_assert_cmpuint(wpe_virtual_audio_device_get_channels(objectDevice.get()), ==, 2);
    g_assert_cmpint(wpe_virtual_audio_device_get_format(objectDevice.get()), ==, WPE_AUDIO_SAMPLE_FORMAT_F32LE);
}

static void testVirtualAudioDeviceBufferTooBig(WPEPlatformTest*, gconstpointer)
{
    // 65536 / (2 channels × 4 bytes) = 8192 frames at most.
    GRefPtr<WPEVirtualAudioDevice> device = adoptGRef(wpe_virtual_audio_device_new("device", nullptr, WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 8192, FALSE));
    g_assert_nonnull(device.get());
    device = adoptGRef(wpe_virtual_audio_device_new("device", nullptr, WPE_AUDIO_SAMPLE_FORMAT_F32LE, 48000, 2, 0, WPE_AUDIO_LAYOUT_INTERLEAVED, 8193, FALSE));
    g_assert_null(device.get());
}

static void testVirtualAudioDeviceInvalidProperties(WPEPlatformTest*, gconstpointer)
{
    if (g_test_subprocess()) {
        // The number of bits of the channel mask doesn't match the number of channels.
        GRefPtr<GObject> device = adoptGRef(G_OBJECT(g_object_new(WPE_TYPE_VIRTUAL_AUDIO_DEVICE, "name", "device", "channels", 2, "channel-mask", G_GUINT64_CONSTANT(0x7), nullptr)));
        return;
    }
    g_test_trap_subprocess(nullptr, 0, G_TEST_SUBPROCESS_DEFAULT);
    g_test_trap_assert_failed();
    g_test_trap_assert_stderr("*channel mask doesn't match the number of channels*");
}

void beforeAll()
{
    WPEPlatformTest::add("VirtualAudioDevice", "new", testVirtualAudioDeviceNew);
    WPEPlatformTest::add("VirtualAudioDevice", "default-frames-per-buffer", testVirtualAudioDeviceDefaultFramesPerBuffer);
    WPEPlatformTest::add("VirtualAudioDevice", "buffer-too-big", testVirtualAudioDeviceBufferTooBig);
    WPEPlatformTest::add("VirtualAudioDevice", "invalid-properties", testVirtualAudioDeviceInvalidProperties);
}

void afterAll()
{
}

} // namespace TestWebKitAPI
