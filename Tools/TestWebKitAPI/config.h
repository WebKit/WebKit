/*
 * Copyright (C) 2010-2026 Apple Inc. All rights reserved.
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

#if defined(__APPLE__) && __APPLE__
#ifdef __OBJC__
#if PLATFORM(IOS_FAMILY)
#import <Foundation/Foundation.h>
#else
#import <Cocoa/Cocoa.h>
#endif
#endif
#endif

#if defined(BUILDING_WITH_CMAKE)

// CMake path
#if defined(BUILDING_TestJSC) || defined(BUILDING_TestJavaScriptCore) || defined(BUILDING_TEST_WGSL)
#include <JavaScriptCore/JSExportMacros.h>
#endif

#if defined(BUILDING_TestWebCore) || defined(BUILDING_TEST_IPC)
#include <JavaScriptCore/JSExportMacros.h>
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#endif

#if defined(BUILDING_TestWebKit)
#include <JavaScriptCore/JSExportMacros.h>
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#include <WebKit/WebKit2_C.h>
#endif

#if defined(BUILDING_TestWebKit) && !defined(TestWebKitAPIInjectedBundle_EXPORTS) && PLATFORM(COCOA) && defined(__OBJC__)
#import <WebKit/WebKit.h>
#endif

#else

// XCode path
#include <JavaScriptCore/JSExportMacros.h>
#if !defined(BUILDING_TEST_WGSL) && !defined(BUILDING_TEST_WTF)
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#if !PLATFORM(IOS_FAMILY) && !defined(BUILDING_TEST_IPC)
#include <WebKit/WebKit2_C.h>
#endif
#if PLATFORM(COCOA) && defined(__OBJC__)
#import <WebKit/WebKit.h>
#if PLATFORM(MACCATALYST)
// Many tests depend on WebKitLegacy.h being implicitly included; however,
// on macCatalyst, WebKit.h does not include WebKitLegacy.h, so we need
// to do it explicitly here.
#import <WebKit/WebKitLegacy.h>
#endif // PLATFORM(MACCATALYST)
#endif // PLATFORM(COCOA) && defined(__OBJC__)
#endif // !defined(BUILDING_TEST_WGSL) && !defined(BUILDING_TEST_WTF)

#endif

#include <stdint.h>

#ifdef __clang__
// Work around the less strict coding standards of the gtest framework.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wundef"
#endif

#ifdef __cplusplus
// The TestJSC executable doesn't use gtest it uses glib's testing
#if !defined(BUILDING_TestJSC) && !defined(NO_GTEST_USAGE)
#undef UniversalPrint
#include <gtest/gtest.h>
#endif
#include <wtf/Assertions.h>
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#include <wtf/TZoneMalloc.h>
#endif

#ifdef __clang__
// Finish working around the less strict coding standards of the gtest framework.
#pragma clang diagnostic pop
#endif

#if !PLATFORM(IOS_FAMILY) && !defined(BUILDING_JSCONLY__)
#define WK_HAVE_C_SPI 1
#endif

// FIXME: Move this to PlatformHave.h.
#if !PLATFORM(APPLETV) && !PLATFORM(MACCATALYST) && !PLATFORM(WATCHOS)
#define HAVE_SSL 1
#endif

// FIXME: Move this to PlatformHave.h.
#if PLATFORM(IOS_FAMILY)
#define HAVE_UIWEBVIEW 1
#endif

// FIXME: Move this to PlatformHave.h.
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
#define HAVE_TLS_VERSION_DURING_CHALLENGE 1
#endif
