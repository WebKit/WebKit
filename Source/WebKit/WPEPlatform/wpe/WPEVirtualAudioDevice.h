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

#ifndef WPEVirtualAudioDevice_h
#define WPEVirtualAudioDevice_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

/**
 * WPEAudioSampleFormat:
 * @WPE_AUDIO_SAMPLE_FORMAT_S8: signed 8-bit samples
 * @WPE_AUDIO_SAMPLE_FORMAT_U8: unsigned 8-bit samples
 * @WPE_AUDIO_SAMPLE_FORMAT_S16LE: signed 16-bit little-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_S16BE: signed 16-bit big-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_U16LE: unsigned 16-bit little-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_U16BE: unsigned 16-bit big-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_S24LE: signed 24-bit little-endian samples packed in 3 bytes
 * @WPE_AUDIO_SAMPLE_FORMAT_S24BE: signed 24-bit big-endian samples packed in 3 bytes
 * @WPE_AUDIO_SAMPLE_FORMAT_S24_32LE: signed 24-bit little-endian samples in 4 bytes
 * @WPE_AUDIO_SAMPLE_FORMAT_S24_32BE: signed 24-bit big-endian samples in 4 bytes
 * @WPE_AUDIO_SAMPLE_FORMAT_S32LE: signed 32-bit little-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_S32BE: signed 32-bit big-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_U32LE: unsigned 32-bit little-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_U32BE: unsigned 32-bit big-endian samples
 * @WPE_AUDIO_SAMPLE_FORMAT_F32LE: 32-bit little-endian floating point samples
 * @WPE_AUDIO_SAMPLE_FORMAT_F32BE: 32-bit big-endian floating point samples
 * @WPE_AUDIO_SAMPLE_FORMAT_F64LE: 64-bit little-endian floating point samples
 * @WPE_AUDIO_SAMPLE_FORMAT_F64BE: 64-bit big-endian floating point samples
 *
 * Format of the audio samples delivered by a #WPEVirtualAudioDevice.
 *
 * Since: 2.56
 */
typedef enum {
    WPE_AUDIO_SAMPLE_FORMAT_S8,
    WPE_AUDIO_SAMPLE_FORMAT_U8,
    WPE_AUDIO_SAMPLE_FORMAT_S16LE,
    WPE_AUDIO_SAMPLE_FORMAT_S16BE,
    WPE_AUDIO_SAMPLE_FORMAT_U16LE,
    WPE_AUDIO_SAMPLE_FORMAT_U16BE,
    WPE_AUDIO_SAMPLE_FORMAT_S24LE,
    WPE_AUDIO_SAMPLE_FORMAT_S24BE,
    WPE_AUDIO_SAMPLE_FORMAT_S24_32LE,
    WPE_AUDIO_SAMPLE_FORMAT_S24_32BE,
    WPE_AUDIO_SAMPLE_FORMAT_S32LE,
    WPE_AUDIO_SAMPLE_FORMAT_S32BE,
    WPE_AUDIO_SAMPLE_FORMAT_U32LE,
    WPE_AUDIO_SAMPLE_FORMAT_U32BE,
    WPE_AUDIO_SAMPLE_FORMAT_F32LE,
    WPE_AUDIO_SAMPLE_FORMAT_F32BE,
    WPE_AUDIO_SAMPLE_FORMAT_F64LE,
    WPE_AUDIO_SAMPLE_FORMAT_F64BE
} WPEAudioSampleFormat;

/**
 * WPEAudioLayout:
 * @WPE_AUDIO_LAYOUT_INTERLEAVED: samples of all the channels are interleaved
 * @WPE_AUDIO_LAYOUT_NON_INTERLEAVED: the samples of each channel are stored in a separate plane,
 *    one after the other
 *
 * Layout of the channels in the audio buffers delivered by a #WPEVirtualAudioDevice.
 *
 * Since: 2.56
 */
typedef enum {
    WPE_AUDIO_LAYOUT_INTERLEAVED,
    WPE_AUDIO_LAYOUT_NON_INTERLEAVED
} WPEAudioLayout;

#define WPE_TYPE_VIRTUAL_AUDIO_DEVICE (wpe_virtual_audio_device_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEVirtualAudioDevice, wpe_virtual_audio_device, WPE, VIRTUAL_AUDIO_DEVICE, GObject)

/**
 * WPEVirtualAudioDeviceBufferFunc:
 * @device: the #WPEVirtualAudioDevice
 * @data: (array length=size) (element-type guint8): the audio data
 * @size: the size of @data in bytes
 * @timestamp: the timestamp of the first sample of @data, in nanoseconds
 * @user_data: user data passed to wpe_virtual_audio_device_set_buffer_callback()
 *
 * Function called every time an audio buffer has been rendered on @device.
 *
 * The callback is always called from the main thread of the UI process, dispatched
 * by the global default #GMainContext. Buffers are produced in real time, paced
 * by the monotonic clock of the web process, not by any audio output of the application:
 * an application that plays the audio on its own output device must handle the drift
 * between both clocks, for instance by resampling or by dropping or inserting samples.
 *
 * @data is formatted as described by the device configuration and is only valid during
 * the call, it must be copied if it is needed afterwards. It always contains the number
 * of frames returned by wpe_virtual_audio_device_get_frames_per_buffer().
 *
 * @timestamp is the position of @data in the audio stream rendered on @device, in
 * nanoseconds: while audio is being rendered, every buffer starts where the previous one
 * ended, so the timestamp of a buffer is the timestamp of the previous one plus its
 * duration (computed from its size, the sample format, the number of channels and the
 * sample rate). A bigger difference means that buffers were dropped because they were
 * not consumed in time.
 *
 * While at least one producer (media element or audio context) is rendering on the
 * device, buffers are delivered continuously, other producers attached to the device
 * but paused contribute silence. When all the producers attached to the device are
 * paused, no buffers are delivered, and the timestamps continue from where they
 * stopped when rendering resumes. When all the producers are stopped or detached from
 * the device, the stream ends: the timestamps of the next stream start from zero again.
 *
 * Since: 2.56
 */
typedef void (* WPEVirtualAudioDeviceBufferFunc) (WPEVirtualAudioDevice *device,
                                                  const guint8          *data,
                                                  gsize                  size,
                                                  guint64                timestamp,
                                                  gpointer               user_data);

WPE_API WPEVirtualAudioDevice *wpe_virtual_audio_device_new                   (const char                       *name,
                                                                               const char                       *description,
                                                                               WPEAudioSampleFormat              format,
                                                                               guint                             rate,
                                                                               guint                             channels,
                                                                               guint64                           channel_mask,
                                                                               WPEAudioLayout                    layout,
                                                                               guint                             frames_per_buffer,
                                                                               gboolean                          is_default);

WPE_API const char            *wpe_virtual_audio_device_get_name              (WPEVirtualAudioDevice            *device);
WPE_API const char            *wpe_virtual_audio_device_get_description       (WPEVirtualAudioDevice            *device);
WPE_API WPEAudioSampleFormat   wpe_virtual_audio_device_get_format            (WPEVirtualAudioDevice            *device);
WPE_API guint                  wpe_virtual_audio_device_get_rate              (WPEVirtualAudioDevice            *device);
WPE_API guint                  wpe_virtual_audio_device_get_channels          (WPEVirtualAudioDevice            *device);
WPE_API guint64                wpe_virtual_audio_device_get_channel_mask      (WPEVirtualAudioDevice            *device);
WPE_API WPEAudioLayout         wpe_virtual_audio_device_get_layout            (WPEVirtualAudioDevice            *device);
WPE_API guint                  wpe_virtual_audio_device_get_frames_per_buffer (WPEVirtualAudioDevice            *device);
WPE_API gboolean               wpe_virtual_audio_device_get_is_default        (WPEVirtualAudioDevice            *device);

WPE_API void                   wpe_virtual_audio_device_set_buffer_callback   (WPEVirtualAudioDevice            *device,
                                                                               WPEVirtualAudioDeviceBufferFunc   callback,
                                                                               gpointer                          user_data,
                                                                               GDestroyNotify                    destroy_notify);

G_END_DECLS

#endif /* WPEVirtualAudioDevice_h */
