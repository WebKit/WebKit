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

#include <wtf/Platform.h>

#include <WebCore/PlatformExportMacros.h>
#include <runtime/JSExportMacros.h>
#include <wtf/DisallowCType.h>
#include <wtf/ExportMacros.h>

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
#elif (PLATFORM(GTK) || PLATFORM(EFL)) && (OS(UNIX) && !OS(MAC_OS_X)) && PLATFORM(X11)
#define PLUGIN_ARCHITECTURE_X11 1
#else
#define PLUGIN_ARCHITECTURE_UNSUPPORTED 1
#endif
#endif

#define PLUGIN_ARCHITECTURE(ARCH) (defined PLUGIN_ARCHITECTURE_##ARCH && PLUGIN_ARCHITECTURE_##ARCH)

#ifndef ENABLE_INSPECTOR_SERVER
#if ENABLE(WEB_SOCKETS) && (PLATFORM(GTK) || PLATFORM(EFL))
#define ENABLE_INSPECTOR_SERVER 1
#endif
#endif

#ifndef ENABLE_SEC_ITEM_SHIM
#if PLATFORM(MAC) || PLATFORM(IOS)
#define ENABLE_SEC_ITEM_SHIM 1
#endif
#endif

#if PLATFORM(MAC)
#ifndef HAVE_WINDOW_SERVER_OCCLUSION_NOTIFICATIONS
#define HAVE_WINDOW_SERVER_OCCLUSION_NOTIFICATIONS 1
#endif
#endif

#if (PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED >= 101200) || (PLATFORM(IOS) && __IPHONE_OS_VERSION_MIN_REQUIRED >= 100000)
#ifndef USE_NETWORK_SESSION
#define USE_NETWORK_SESSION 1
#endif

// FIXME: We should work towards not using CredentialStorage in WebKit2 to not have problems with digest authentication.
#ifndef USE_CREDENTIAL_STORAGE_WITH_NETWORK_SESSION
#define USE_CREDENTIAL_STORAGE_WITH_NETWORK_SESSION 1
#endif
#endif

#ifndef HAVE_SEC_ACCESS_CONTROL
#if PLATFORM(IOS) || PLATFORM(MAC)
#define HAVE_SEC_ACCESS_CONTROL 1
#endif
#endif

#ifndef HAVE_OS_ACTIVITY
#if PLATFORM(IOS) || PLATFORM(MAC)
#define HAVE_OS_ACTIVITY 1
#endif
#endif

#ifndef ENABLE_NETWORK_CACHE
#if PLATFORM(COCOA) || PLATFORM(GTK)
#define ENABLE_NETWORK_CACHE 1
#else
#define ENABLE_NETWORK_CACHE 0
#endif
#endif

#ifndef ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION
#if ENABLE(NETWORK_CACHE) && (PLATFORM(COCOA) || PLATFORM(GTK))
#define ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION 1
#else
#define ENABLE_NETWORK_CACHE_SPECULATIVE_REVALIDATION 0
#endif
#endif

#ifndef HAVE_SAFARI_SERVICES_FRAMEWORK
#if PLATFORM(IOS) && (!defined TARGET_OS_IOS || TARGET_OS_IOS)
#define HAVE_SAFARI_SERVICES_FRAMEWORK 1
#else
#define HAVE_SAFARI_SERVICES_FRAMEWORK 0
#endif
#endif

#ifndef HAVE_LINK_PREVIEW
#if defined TARGET_OS_IOS && TARGET_OS_IOS
#define HAVE_LINK_PREVIEW 1
#else
#define HAVE_LINK_PREVIEW 0
#endif
#endif

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/WebKitAdditionsDefines.h>
#endif
