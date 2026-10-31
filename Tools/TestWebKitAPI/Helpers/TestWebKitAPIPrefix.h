/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#ifdef __cplusplus

#if defined(__APPLE__) && __APPLE__
#ifdef __OBJC__
#if PLATFORM(IOS_FAMILY)
#import <Foundation/Foundation.h>
#else
#import <Cocoa/Cocoa.h>
#endif
#endif
#endif

#ifdef __cplusplus
#include <algorithm> // needed for exception_defines.h
#include <chrono>
#include <functional>
#include <list>
#include <memory>
#include <mutex>
#include <string>
#include <typeinfo>

// The TestJSC executable doesn't use gtest it uses glib's testing
#if !defined(BUILDING_TestJSC) && !defined(NO_GTEST_USAGE)
#ifdef __clang__
// Same as config.h, for targets that don't pass TestWebKitAPI_DISABLED_WARNINGS.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#pragma clang diagnostic ignored "-Wundef"
#endif
#undef UniversalPrint
#include <gtest/gtest.h>
#ifdef __clang__
#pragma clang diagnostic pop
#endif
#endif
#include <wtf/Assertions.h>
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#include <wtf/text/StringView.h>

// config.h's TestWebKit imports, under the same conditions. Every TU of
// TestWebKit, TestWebKitAPIInjectedBundle and TestWebKitAPIWebProcessPlugIn
// would otherwise parse them, WebKit.h in particular. TestWebKit therefore
// has its own PCH rather than reusing TestWTF's.
#if defined(BUILDING_WITH_CMAKE) && defined(BUILDING_TestWebKit)
#include <JavaScriptCore/JSExportMacros.h>
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#include <WebKit/WebKit2_C.h>
#include <wtf/TZoneMalloc.h>
#if PLATFORM(COCOA) && defined(__OBJC__) && !defined(TestWebKitAPIInjectedBundle_EXPORTS)
#import <WebKit/WebKit.h>
#endif
// Headers that most of this subtarget's sources parse (measured with -ftime-trace).
#if PLATFORM(MAC)
#include <WebKit/WKRetainPtr.h>
#include "Test.h"
#if !defined(__OBJC__)
#include <wtf/JSONValues.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/WeakPtr.h>
#include <wtf/HashSet.h>
#endif
#endif // PLATFORM(MAC)
#endif
#endif

#if USE(OS_LOG)
#include <os/log.h>
#endif

#endif // __cplusplus
