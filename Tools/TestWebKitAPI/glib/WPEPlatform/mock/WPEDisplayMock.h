/*
 * Copyright (C) 2025 Igalia S.L.
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

#include <gio/gio.h>
#include <glib-object.h>
#include <wpe/wpe-platform.h>

G_BEGIN_DECLS

#define WPE_TYPE_DISPLAY_MOCK (wpe_display_mock_get_type())
G_DECLARE_FINAL_TYPE(WPEDisplayMock, wpe_display_mock, WPE, DISPLAY_MOCK, WPEDisplay)

void wpeDisplayMockRegister(GIOModule*);
WPEDisplay* wpeDisplayMockNew();
void wpeDisplayMockDisconnect(WPEDisplayMock*);
void wpeDisplayMockUseFakeDRMNodes(WPEDisplayMock*, gboolean);
void wpeDisplayMockUseFakeBufferFormats(WPEDisplayMock*, gboolean);
void wpeDisplayMockSetUseExplicitSync(WPEDisplayMock*, gboolean);
void wpeDisplayMockSetInitialInputDevices(WPEDisplayMock*, WPEAvailableInputDevices);
void wpeDisplayMockAddInputDevice(WPEDisplayMock*, WPEAvailableInputDevices);
void wpeDisplayMockRemoveInputDevice(WPEDisplayMock*, WPEAvailableInputDevices);
void wpeDisplayMockAddSecondaryScreen(WPEDisplayMock*);
void wpeDisplayMockRemoveSecondaryScreen(WPEDisplayMock*);

G_END_DECLS
