/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */ 

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <JavaScriptCore/JSExportMacros.h>
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#include <wtf/DisallowCType.h>

#ifdef __cplusplus

// These undefs match up with defines in WebKit2Prefix.h for Mac OS X.
// Helps us catch if anyone uses new or delete by accident in code and doesn't include "config.h".
#undef new
#undef delete
#include <wtf/FastMalloc.h>

#endif

#ifndef PLUGIN_ARCHITECTURE_UNSUPPORTED
#if PLATFORM(MAC)
#define PLUGIN_ARCHITECTURE_MAC 1
#elif PLATFORM(GTK) && OS(UNIX) && !OS(MACOS)
#define PLUGIN_ARCHITECTURE_UNIX 1
#else
#define PLUGIN_ARCHITECTURE_UNSUPPORTED 1
#endif
#endif

#define PLUGIN_ARCHITECTURE(ARCH) (defined PLUGIN_ARCHITECTURE_##ARCH && PLUGIN_ARCHITECTURE_##ARCH)

// FIXME: We should work towards not using CredentialStorage in WebKit to not have problems with digest authentication.
#ifndef USE_CREDENTIAL_STORAGE_WITH_NETWORK_SESSION
#define USE_CREDENTIAL_STORAGE_WITH_NETWORK_SESSION 1
#endif

// ENABLE_WEBDRIVER_ACTIONS_API represents whether mouse, keyboard, touch or wheel interactions are defined
#if ENABLE(WEBDRIVER_MOUSE_INTERACTIONS) || ENABLE(WEBDRIVER_KEYBOARD_INTERACTIONS) || ENABLE(WEBDRIVER_TOUCH_INTERACTIONS) || ENABLE(WEBDRIVER_WHEEL_INTERACTIONS)
#define ENABLE_WEBDRIVER_ACTIONS_API 1
#endif
