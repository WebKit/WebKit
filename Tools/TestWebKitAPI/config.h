/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#if defined(BUILDING_TestJSC)
#include <JavaScriptCore/JSExportMacros.h>
#endif

#if defined(BUILDING_TestWebCore)
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

#else

// XCode path
#include <JavaScriptCore/JSExportMacros.h>
#include <WebCore/PlatformExportMacros.h>
#include <pal/ExportMacros.h>
#if !PLATFORM(IOS_FAMILY)
#include <WebKit/WebKit2_C.h>
#endif
#if PLATFORM(COCOA) && defined(__OBJC__)
#import <WebKit/WebKit.h>
#endif

#endif

#include <stdint.h>

#ifdef __clang__
// Work around the less strict coding standards of the gtest framework.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-variable"
#endif

#ifdef __cplusplus
#include <gtest/gtest.h>
#include <wtf/Assertions.h>
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#endif

#include <wtf/DisallowCType.h>

#ifdef __clang__
// Finish working around the less strict coding standards of the gtest framework.
#pragma clang diagnostic pop
#endif

#if !PLATFORM(IOS_FAMILY) && !defined(BUILDING_JSCONLY__)
#define WK_HAVE_C_SPI 1
#endif

#if !PLATFORM(APPLETV)
#define HAVE_SSL 1
#endif
