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

#pragma once

#include "WebKitVirtualAudioDevice.h"
#include "WebKitWebView.h"
#include <WebCore/VirtualAudioDevice.h>
#include <span>

WebCore::VirtualAudioSampleFormat webkitAudioSampleFormatToVirtualAudioSampleFormat(WebKitAudioSampleFormat);

// Whether the construct-only properties of @device form a valid configuration.
bool webkitVirtualAudioDeviceIsValid(WebKitVirtualAudioDevice*);

void webkitVirtualAudioDeviceRenderBuffer(WebKitVirtualAudioDevice*, std::span<const uint8_t>, uint64_t timestamp);

// A device belongs to a single web view. Returns false if @device is already used by another
// web view still alive, in which case the owner is not changed.
bool webkitVirtualAudioDeviceSetOwner(WebKitVirtualAudioDevice*, WebKitWebView*);
