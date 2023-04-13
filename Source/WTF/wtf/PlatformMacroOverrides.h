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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#ifndef WTF_PLATFORM_GUARD_AGAINST_INDIRECT_INCLUSION
#error "Please #include <wtf/Platform.h> instead of this file directly."
#endif

/* Macros for configuring platform headers that subsequent 
   WTF using code might include.
*/ 

#if !USE(APPLE_INTERNAL_SDK) && (PLATFORM(WATCHOS) || PLATFORM(APPLETV) || PLATFORM(IOS_FAMILY))
// Configure availability macros in SDK headers to not apply, since WebKit uses APIs for building
// the platform.
#include <os/availability.h>

// Remove limitations so that WebKit can be compiled with the public SDK against
// APIs intended for system components only.

#if PLATFORM(WATCHOS)
#ifdef __WATCHOS_PROHIBITED
#undef __WATCHOS_PROHIBITED
#define __WATCHOS_PROHIBITED
#endif
#elif PLATFORM(APPLETV)
#ifdef __TVOS_PROHIBITED
#undef __TVOS_PROHIBITED
#define __TVOS_PROHIBITED
#endif
#else
#ifdef __IOS_PROHIBITED
#undef __IOS_PROHIBITED
#define __IOS_PROHIBITED
#endif
#endif

// Remove API_AVAILABLE, API_UNAVAILABLE, since the public SDK is not compatible with above relaxation
// of APIs. Note: this causes compile failures, for example turning comprehensive
// switch statements into non-comprehensive.
#undef API_UNAVAILABLE
#define API_UNAVAILABLE(...)
#undef API_AVAILABLE
#define API_AVAILABLE(...)

#endif

// FIXME: Possibly move DisallowCType.h contents here.
