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
#include "WebKitVirtualAudioDevice.h"

#include "WebKitEnumTypes.h"
#include "WebKitVirtualAudioDevicePrivate.h"
#include <array>
#include <bit>
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/UTF8CStringView.h>

/**
 * WebKitVirtualAudioDevice:
 *
 * An audio output device implemented by the application.
 *
 * A #WebKitVirtualAudioDevice is exposed to web content as an audio output device whose
 * `MediaDeviceInfo.label` is its description, or its name when it has no description.
 * Like for any other device, its `MediaDeviceInfo.deviceId` is an opaque identifier
 * specific to the web page and origin. Web content can select it with
 * `HTMLMediaElement.setSinkId()` or `AudioContext.setSinkId()`, or it can replace the
 * default audio output device of media elements and audio contexts when it's created
 * as a default device. Speech synthesis is not affected: it always uses the system
 * audio output. See #WebKitWebView:virtual-audio-devices for the features needed to
 * expose the devices to web content.
 *
 * All the audio rendered on the device is mixed and converted to the format given
 * to webkit_virtual_audio_device_new(), then delivered to the application by calling the
 * function set with webkit_virtual_audio_device_set_buffer_callback(). That function is
 * always called on the UI main thread, see #WebKitVirtualAudioDeviceBufferFunc for the
 * pacing and timestamps of the delivered buffers.
 *
 * Web Audio doesn't take the configuration of virtual devices into account: the default
 * sample rate of an `AudioContext` is always 44100 Hz, and the `maxChannelCount` of its
 * destination is detected from the system audio output, so it can be 0 when there's none.
 * The audio is converted to the device rate, format and number of channels when it's
 * rendered on the device.
 *
 * Virtual audio devices are only supported when WebKit is built with VIRTUAL_AUDIO_DEVICES
 * enabled: otherwise #WebKitWebView doesn't have the #WebKitWebView:virtual-audio-devices
 * property, and the devices can't be used.
 *
 * If the virtual audio devices cannot be initialized by the web process, the devices are
 * not exposed to web content and an error is logged by the web process.
 *
 * Virtual audio devices are not supported on platforms whose media playback requires the
 * platform audio sinks, such as Rialto: the devices are not exposed to web content and an
 * error is logged by the web process. On platforms using their own sink for Web Audio, the
 * `AudioContext`s of a web view with virtual audio devices use the regular audio sinks.
 *
 * Virtual audio devices are not supported when media is played in the GPU process
 * (the `UseGPUProcessForMediaEnabled` feature, see webkit_settings_set_feature_enabled()):
 * the devices only exist in the web process, so the audio played in the GPU process is
 * rendered on the system audio output instead.
 *
 * The device configuration is given by construct-only properties. Prefer
 * webkit_virtual_audio_device_new(), which returns %NULL for an invalid configuration:
 * a device created with g_object_new() and an invalid configuration is ignored by
 * web views.
 *
 * Since: 2.56
 */
struct _WebKitVirtualAudioDevicePrivate {
    UTF8CString name;
    UTF8CString description;
    WebKitAudioSampleFormat format { WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE };
    guint rate { 48000 };
    guint channels { 2 };
    guint64 channelMask { 0 };
    WebKitAudioLayout layout { WEBKIT_AUDIO_LAYOUT_INTERLEAVED };
    guint framesPerBuffer { 0 };
    bool isDefault { false };
    bool isValid { false };

    WebKitVirtualAudioDeviceBufferFunc callback { nullptr };
    gpointer userData { nullptr };
    GDestroyNotify destroyNotify { nullptr };

    GWeakPtr<WebKitWebView> owner;
};

WEBKIT_DEFINE_FINAL_TYPE(WebKitVirtualAudioDevice, webkit_virtual_audio_device, G_TYPE_OBJECT, GObject)

enum {
    PROP_0,

    PROP_NAME,
    PROP_DESCRIPTION,
    PROP_FORMAT,
    PROP_RATE,
    PROP_CHANNELS,
    PROP_CHANNEL_MASK,
    PROP_LAYOUT,
    PROP_FRAMES_PER_BUFFER,
    PROP_IS_DEFAULT,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

// Same range as the sample rates supported by Web Audio.
static constexpr guint minimumRate = 3000;
static constexpr guint maximumRate = 768000;

static constexpr guint maximumChannels = WebCore::maximumVirtualAudioDeviceChannels;

static bool webkitAudioSampleFormatIsValid(WebKitAudioSampleFormat format)
{
    return format >= WEBKIT_AUDIO_SAMPLE_FORMAT_S8 && format <= WEBKIT_AUDIO_SAMPLE_FORMAT_F64BE;
}

static bool webkitAudioLayoutIsValid(WebKitAudioLayout layout)
{
    return layout == WEBKIT_AUDIO_LAYOUT_INTERLEAVED || layout == WEBKIT_AUDIO_LAYOUT_NON_INTERLEAVED;
}

WebCore::VirtualAudioSampleFormat webkitAudioSampleFormatToVirtualAudioSampleFormat(WebKitAudioSampleFormat format)
{
    switch (format) {
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S8:
        return WebCore::VirtualAudioSampleFormat::S8;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_U8:
        return WebCore::VirtualAudioSampleFormat::U8;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S16LE:
        return WebCore::VirtualAudioSampleFormat::S16LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S16BE:
        return WebCore::VirtualAudioSampleFormat::S16BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_U16LE:
        return WebCore::VirtualAudioSampleFormat::U16LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_U16BE:
        return WebCore::VirtualAudioSampleFormat::U16BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S24LE:
        return WebCore::VirtualAudioSampleFormat::S24LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S24BE:
        return WebCore::VirtualAudioSampleFormat::S24BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S24_32LE:
        return WebCore::VirtualAudioSampleFormat::S24In32LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S24_32BE:
        return WebCore::VirtualAudioSampleFormat::S24In32BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S32LE:
        return WebCore::VirtualAudioSampleFormat::S32LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_S32BE:
        return WebCore::VirtualAudioSampleFormat::S32BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_U32LE:
        return WebCore::VirtualAudioSampleFormat::U32LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_U32BE:
        return WebCore::VirtualAudioSampleFormat::U32BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE:
        return WebCore::VirtualAudioSampleFormat::F32LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_F32BE:
        return WebCore::VirtualAudioSampleFormat::F32BigEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_F64LE:
        return WebCore::VirtualAudioSampleFormat::F64LittleEndian;
    case WEBKIT_AUDIO_SAMPLE_FORMAT_F64BE:
        return WebCore::VirtualAudioSampleFormat::F64BigEndian;
    }

    return WebCore::VirtualAudioSampleFormat::F32LittleEndian;
}

static guint webkitAudioSampleFormatSize(WebKitAudioSampleFormat format)
{
    return WebCore::virtualAudioSampleSize(webkitAudioSampleFormatToVirtualAudioSampleFormat(format));
}

static void webkitVirtualAudioDeviceClearBufferCallback(WebKitVirtualAudioDevice* device)
{
    auto* priv = device->priv;
    auto destroyNotify = std::exchange(priv->destroyNotify, nullptr);
    auto userData = std::exchange(priv->userData, nullptr);
    priv->callback = nullptr;
    if (destroyNotify)
        destroyNotify(userData);
}

static void webkitVirtualAudioDeviceDispose(GObject* object)
{
    webkitVirtualAudioDeviceClearBufferCallback(WEBKIT_VIRTUAL_AUDIO_DEVICE(object));

    G_OBJECT_CLASS(webkit_virtual_audio_device_parent_class)->dispose(object);
}

static guint webkitVirtualAudioDeviceMaximumFramesPerBuffer(WebKitAudioSampleFormat format, guint channels)
{
    return WebCore::maximumVirtualAudioDeviceBufferSize / (channels * webkitAudioSampleFormatSize(format));
}

static void webkitVirtualAudioDeviceConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_virtual_audio_device_parent_class)->constructed(object);

    // The ranges of the format, rate, channels and layout are checked by the properties.
    auto* priv = WEBKIT_VIRTUAL_AUDIO_DEVICE(object)->priv;
    if (priv->name.isEmpty() || !g_utf8_validate(priv->name.legacyCStringPointer(), -1, nullptr)) {
        g_critical("WebKitVirtualAudioDevice: the device must have a valid UTF-8 name");
        return;
    }
    if (!priv->description.isNull() && !g_utf8_validate(priv->description.legacyCStringPointer(), -1, nullptr)) {
        g_critical("WebKitVirtualAudioDevice '%s': the description must be valid UTF-8", priv->name.legacyCStringPointer());
        return;
    }
    if (priv->channelMask && static_cast<guint>(std::popcount(priv->channelMask)) != priv->channels) {
        g_critical("WebKitVirtualAudioDevice '%s': the channel mask doesn't match the number of channels", priv->name.legacyCStringPointer());
        return;
    }
    auto maximumFramesPerBuffer = webkitVirtualAudioDeviceMaximumFramesPerBuffer(priv->format, priv->channels);
    if (priv->framesPerBuffer > maximumFramesPerBuffer) {
        g_critical("WebKitVirtualAudioDevice '%s': buffers of %u frames exceed 64 KiB", priv->name.legacyCStringPointer(), priv->framesPerBuffer);
        return;
    }
    if (!priv->framesPerBuffer)
        priv->framesPerBuffer = std::clamp<guint>(priv->rate / 100, 1, maximumFramesPerBuffer);
    priv->isValid = true;
}

static void webkitVirtualAudioDeviceSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* priv = WEBKIT_VIRTUAL_AUDIO_DEVICE(object)->priv;

    switch (propId) {
    case PROP_NAME:
        priv->name = UTF8CString::unsafeFromUTF8(g_value_get_string(value));
        break;
    case PROP_DESCRIPTION:
        priv->description = UTF8CString::unsafeFromUTF8(g_value_get_string(value));
        break;
    case PROP_FORMAT:
        priv->format = static_cast<WebKitAudioSampleFormat>(g_value_get_enum(value));
        break;
    case PROP_RATE:
        priv->rate = g_value_get_uint(value);
        break;
    case PROP_CHANNELS:
        priv->channels = g_value_get_uint(value);
        break;
    case PROP_CHANNEL_MASK:
        priv->channelMask = g_value_get_uint64(value);
        break;
    case PROP_LAYOUT:
        priv->layout = static_cast<WebKitAudioLayout>(g_value_get_enum(value));
        break;
    case PROP_FRAMES_PER_BUFFER:
        priv->framesPerBuffer = g_value_get_uint(value);
        break;
    case PROP_IS_DEFAULT:
        priv->isDefault = g_value_get_boolean(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitVirtualAudioDeviceGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* device = WEBKIT_VIRTUAL_AUDIO_DEVICE(object);

    switch (propId) {
    case PROP_NAME:
        g_value_set_string(value, webkit_virtual_audio_device_get_name(device));
        break;
    case PROP_DESCRIPTION:
        g_value_set_string(value, webkit_virtual_audio_device_get_description(device));
        break;
    case PROP_FORMAT:
        g_value_set_enum(value, webkit_virtual_audio_device_get_format(device));
        break;
    case PROP_RATE:
        g_value_set_uint(value, webkit_virtual_audio_device_get_rate(device));
        break;
    case PROP_CHANNELS:
        g_value_set_uint(value, webkit_virtual_audio_device_get_channels(device));
        break;
    case PROP_CHANNEL_MASK:
        g_value_set_uint64(value, webkit_virtual_audio_device_get_channel_mask(device));
        break;
    case PROP_LAYOUT:
        g_value_set_enum(value, webkit_virtual_audio_device_get_layout(device));
        break;
    case PROP_FRAMES_PER_BUFFER:
        g_value_set_uint(value, webkit_virtual_audio_device_get_frames_per_buffer(device));
        break;
    case PROP_IS_DEFAULT:
        g_value_set_boolean(value, webkit_virtual_audio_device_get_is_default(device));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_virtual_audio_device_class_init(WebKitVirtualAudioDeviceClass* deviceClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(deviceClass);
    objectClass->constructed = webkitVirtualAudioDeviceConstructed;
    objectClass->set_property = webkitVirtualAudioDeviceSetProperty;
    objectClass->get_property = webkitVirtualAudioDeviceGetProperty;
    objectClass->dispose = webkitVirtualAudioDeviceDispose;

    /**
     * WebKitVirtualAudioDevice:name:
     *
     * The name of the device, it must be unique among the virtual audio devices of a web view.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_NAME] =
        g_param_spec_string(
            "name",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:description:
     *
     * The human readable description of the device, used as the `MediaDeviceInfo.label`,
     * or %NULL to use the name as the label.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_DESCRIPTION] =
        g_param_spec_string(
            "description",
            nullptr, nullptr,
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:format:
     *
     * The #WebKitAudioSampleFormat of the audio delivered by the device.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_FORMAT] =
        g_param_spec_enum(
            "format",
            nullptr, nullptr,
            WEBKIT_TYPE_AUDIO_SAMPLE_FORMAT,
            WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:rate:
     *
     * The sample rate of the audio delivered by the device, in Hz.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_RATE] =
        g_param_spec_uint(
            "rate",
            nullptr, nullptr,
            minimumRate, maximumRate, 48000,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:channels:
     *
     * The number of channels of the audio delivered by the device.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_CHANNELS] =
        g_param_spec_uint(
            "channels",
            nullptr, nullptr,
            1, maximumChannels, 2,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:channel-mask:
     *
     * The channel positions, see webkit_virtual_audio_device_new(), or 0 to use the default
     * positions for the number of channels.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_CHANNEL_MASK] =
        g_param_spec_uint64(
            "channel-mask",
            nullptr, nullptr,
            0, G_MAXUINT64, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:layout:
     *
     * The #WebKitAudioLayout of the audio buffers delivered by the device.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_LAYOUT] =
        g_param_spec_enum(
            "layout",
            nullptr, nullptr,
            WEBKIT_TYPE_AUDIO_LAYOUT,
            WEBKIT_AUDIO_LAYOUT_INTERLEAVED,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:frames-per-buffer:
     *
     * The number of frames of the audio buffers delivered by the device, or 0 when
     * constructing the device to use buffers of about 10 milliseconds. The buffers
     * can't exceed 64 KiB, see webkit_virtual_audio_device_new().
     *
     * Since: 2.56
     */
    sObjProperties[PROP_FRAMES_PER_BUFFER] =
        g_param_spec_uint(
            "frames-per-buffer",
            nullptr, nullptr,
            0, G_MAXUINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitVirtualAudioDevice:is-default:
     *
     * Whether the device replaces the default audio output device of the media elements
     * and audio contexts of the web view.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_IS_DEFAULT] =
        g_param_spec_boolean(
            "is-default",
            nullptr, nullptr,
            FALSE,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

/**
 * webkit_virtual_audio_device_new:
 * @name: the name of the device, a valid UTF-8 string, it must be unique among the virtual
 *    audio devices of a web view, devices with an already used name are ignored by the web view
 * @description: (nullable): the human readable description of the device, a valid UTF-8
 *    string used as the `MediaDeviceInfo.label`, or %NULL to use @name as the label
 * @format: the #WebKitAudioSampleFormat of the audio delivered by the device
 * @rate: the sample rate of the audio delivered by the device, in Hz, between 3000 and 768000
 * @channels: the number of channels of the audio delivered by the device, up to 64
 * @channel_mask: the channel positions, as a bit mask following the GStreamer
 *    `GstAudioChannelPosition` bit order (bit 0 is front left, bit 1 is front right,
 *    bit 2 is front center, bit 3 is LFE...), the number of bits set must match @channels;
 *    or 0 to use the default positions for the number of channels. The channels are
 *    delivered in ascending bit order: the first channel of a frame (or the first plane
 *    of a non-interleaved buffer) is the position of the lowest bit set in the mask
 * @layout: the #WebKitAudioLayout of the audio buffers delivered by the device
 * @frames_per_buffer: the number of frames of the audio buffers delivered by the device,
 *    or 0 to use buffers of about 10 milliseconds
 * @is_default: whether the device replaces the default audio output device of the media
 *    elements and audio contexts of the web view (speech synthesis is not affected)
 *
 * Create a new #WebKitVirtualAudioDevice.
 *
 * A frame contains one sample of each channel, so the audio buffers delivered by the device
 * are @frames_per_buffer × @channels × the sample size bytes long. That size can't exceed
 * 64 KiB: if @frames_per_buffer gives bigger buffers, the device is not created and %NULL
 * is returned. When @frames_per_buffer is 0, the default number of frames is reduced if
 * needed to fit in that limit, use webkit_virtual_audio_device_get_frames_per_buffer() to get
 * the number of frames actually used.
 *
 * This is equivalent to creating the device with g_object_new() and its construct-only
 * properties, except that an invalid configuration returns %NULL.
 *
 * If more than one virtual audio device of a web view is marked as default, only the
 * first one in the list is used as the default device, the others are exposed as
 * regular devices.
 *
 * Returns: (transfer full) (nullable): a new #WebKitVirtualAudioDevice, or %NULL if the
 *    configuration is invalid
 *
 * Since: 2.56
 */
WebKitVirtualAudioDevice* webkit_virtual_audio_device_new(const char* name, const char* description, WebKitAudioSampleFormat format, guint rate, guint channels, guint64 channel_mask, WebKitAudioLayout layout, guint frames_per_buffer, gboolean is_default)
{
    g_return_val_if_fail(name && *name, nullptr);
    g_return_val_if_fail(g_utf8_validate(name, -1, nullptr), nullptr);
    g_return_val_if_fail(!description || g_utf8_validate(description, -1, nullptr), nullptr);
    g_return_val_if_fail(webkitAudioSampleFormatIsValid(format), nullptr);
    g_return_val_if_fail(webkitAudioLayoutIsValid(layout), nullptr);
    g_return_val_if_fail(rate >= minimumRate && rate <= maximumRate, nullptr);
    g_return_val_if_fail(channels > 0 && channels <= maximumChannels, nullptr);
    g_return_val_if_fail(!channel_mask || static_cast<guint>(std::popcount(channel_mask)) == channels, nullptr);

    // Documented failure, not a programming error: don't use g_return_val_if_fail(), which can be
    // compiled out.
    if (frames_per_buffer > webkitVirtualAudioDeviceMaximumFramesPerBuffer(format, channels))
        return nullptr;

    return WEBKIT_VIRTUAL_AUDIO_DEVICE(g_object_new(WEBKIT_TYPE_VIRTUAL_AUDIO_DEVICE,
        "name", name,
        "description", description,
        "format", format,
        "rate", rate,
        "channels", channels,
        "channel-mask", channel_mask,
        "layout", layout,
        "frames-per-buffer", frames_per_buffer,
        "is-default", is_default,
        nullptr));
}

/**
 * webkit_virtual_audio_device_get_name:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the name of @device.
 *
 * Returns: (transfer none): the name of @device
 *
 * Since: 2.56
 */
const char* webkit_virtual_audio_device_get_name(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), nullptr);

    return device->priv->name.legacyCStringPointer();
}

/**
 * webkit_virtual_audio_device_get_description:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the description of @device.
 *
 * Returns: (transfer none) (nullable): the description of @device
 *
 * Since: 2.56
 */
const char* webkit_virtual_audio_device_get_description(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), nullptr);

    return device->priv->description.legacyCStringPointer();
}

/**
 * webkit_virtual_audio_device_get_format:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the format of the audio samples delivered by @device.
 *
 * Returns: a #WebKitAudioSampleFormat
 *
 * Since: 2.56
 */
WebKitAudioSampleFormat webkit_virtual_audio_device_get_format(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), WEBKIT_AUDIO_SAMPLE_FORMAT_F32LE);

    return device->priv->format;
}

/**
 * webkit_virtual_audio_device_get_rate:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the sample rate of the audio delivered by @device.
 *
 * Returns: the sample rate in Hz
 *
 * Since: 2.56
 */
guint webkit_virtual_audio_device_get_rate(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), 0);

    return device->priv->rate;
}

/**
 * webkit_virtual_audio_device_get_channels:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the number of channels of the audio delivered by @device.
 *
 * Returns: the number of channels
 *
 * Since: 2.56
 */
guint webkit_virtual_audio_device_get_channels(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), 0);

    return device->priv->channels;
}

/**
 * webkit_virtual_audio_device_get_channel_mask:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the channel positions mask of the audio delivered by @device.
 *
 * Returns: the channel positions mask, or 0 if the default positions are used
 *
 * Since: 2.56
 */
guint64 webkit_virtual_audio_device_get_channel_mask(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), 0);

    return device->priv->channelMask;
}

/**
 * webkit_virtual_audio_device_get_layout:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the layout of the channels in the audio buffers delivered by @device.
 *
 * Returns: a #WebKitAudioLayout
 *
 * Since: 2.56
 */
WebKitAudioLayout webkit_virtual_audio_device_get_layout(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), WEBKIT_AUDIO_LAYOUT_INTERLEAVED);

    return device->priv->layout;
}

/**
 * webkit_virtual_audio_device_get_frames_per_buffer:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get the number of frames of the audio buffers delivered by @device.
 *
 * Returns: the number of frames per buffer
 *
 * Since: 2.56
 */
guint webkit_virtual_audio_device_get_frames_per_buffer(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), 0);

    return device->priv->framesPerBuffer;
}

/**
 * webkit_virtual_audio_device_get_is_default:
 * @device: a #WebKitVirtualAudioDevice
 *
 * Get whether @device replaces the default audio output device.
 *
 * Returns: %TRUE if @device is the default audio output device, or %FALSE otherwise
 *
 * Since: 2.56
 */
gboolean webkit_virtual_audio_device_get_is_default(WebKitVirtualAudioDevice* device)
{
    g_return_val_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device), FALSE);

    return device->priv->isDefault;
}

/**
 * webkit_virtual_audio_device_set_buffer_callback:
 * @device: a #WebKitVirtualAudioDevice
 * @callback: (scope notified) (closure user_data) (destroy destroy_notify) (nullable): a #WebKitVirtualAudioDeviceBufferFunc
 * @user_data: data to pass to @callback
 * @destroy_notify: (nullable): function to call to release @user_data
 *
 * Set the function called every time an audio buffer has been rendered on @device.
 *
 * This function must only be called from the UI process main thread, which is also the
 * thread @callback is always called on. It can be called at any time, the new callback
 * is used from the next audio buffer. Any previously set callback is released immediately,
 * calling @destroy_notify on its user data, so a callback that replaces itself must not
 * use its user data after calling this function. Pass %NULL as @callback to stop
 * receiving the audio buffers.
 *
 * Since: 2.56
 */
void webkit_virtual_audio_device_set_buffer_callback(WebKitVirtualAudioDevice* device, WebKitVirtualAudioDeviceBufferFunc callback, gpointer userData, GDestroyNotify destroyNotify)
{
    g_return_if_fail(WEBKIT_IS_VIRTUAL_AUDIO_DEVICE(device));

    webkitVirtualAudioDeviceClearBufferCallback(device);
    device->priv->callback = callback;
    device->priv->userData = userData;
    device->priv->destroyNotify = destroyNotify;
}

bool webkitVirtualAudioDeviceIsValid(WebKitVirtualAudioDevice* device)
{
    return device->priv->isValid;
}

bool webkitVirtualAudioDeviceSetOwner(WebKitVirtualAudioDevice* device, WebKitWebView* owner)
{
    auto* priv = device->priv;
    if (priv->owner && priv->owner.get() != owner)
        return false;

    priv->owner.reset(owner);
    return true;
}

void webkitVirtualAudioDeviceRenderBuffer(WebKitVirtualAudioDevice* device, std::span<const uint8_t> data, uint64_t timestamp)
{
    auto* priv = device->priv;
    if (!priv->callback)
        return;

    // The callback is promised buffers of exactly frames_per_buffer frames: the data channel drops
    // packets of any other size.
    ASSERT(data.size() == static_cast<size_t>(priv->framesPerBuffer) * priv->channels * webkitAudioSampleFormatSize(priv->format));
    priv->callback(device, data.data(), data.size(), timestamp, priv->userData);
}
