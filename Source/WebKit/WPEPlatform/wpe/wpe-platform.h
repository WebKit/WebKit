/*
 * Copyright (C) 2023 Igalia S.L.
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
#ifndef __WPE_PLATFORM_H__
#define __WPE_PLATFORM_H__

#define __WPE_PLATFORM_H_INSIDE__

#include <wpe/WPEBuffer.h>
#include <wpe/WPEBufferDMABuf.h>
#include <wpe/WPEBufferFormats.h>
#include <wpe/WPEBufferSHM.h>
#include <wpe/WPEClipboard.h>
#include <wpe/WPEColor.h>
#include <wpe/WPEConfig.h>
#include <wpe/WPEDRMDevice.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEDisplay.h>
#include <wpe/WPEEGLError.h>
#include <wpe/WPEEnumTypes.h>
#include <wpe/WPEEvent.h>
#include <wpe/WPEGamepad.h>
#include <wpe/WPEGamepadManager.h>
#include <wpe/WPEGestureController.h>
#include <wpe/WPEInputMethodContext.h>
#include <wpe/WPEKeymap.h>
#include <wpe/WPEKeyUnicode.h>
#include <wpe/WPEKeymapXKB.h>
#include <wpe/WPEKeysyms.h>
#include <wpe/WPEKeysyms.h>
#include <wpe/WPERectangle.h>
#include <wpe/WPEScreen.h>
#include <wpe/WPEScreenSyncObserver.h>
#include <wpe/WPEToplevel.h>
#include <wpe/WPEVersion.h>
#include <wpe/WPEView.h>
#include <wpe/WPEViewAccessible.h>

#ifdef WPE_PLATFORM_BUFFER_ANDROID
#include <wpe/WPEBufferAndroid.h>
#endif

#undef __WPE_PLATFORM_H_INSIDE__

#endif /* __WPE_PLATFORM_H__ */
