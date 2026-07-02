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

#pragma once

// Handle __IOS_PROHIBITED and friends.
#undef __OS_AVAILABILITY
#define __OS_AVAILABILITY(...)

#undef SWIFT_AVAILABILITY
#define SWIFT_AVAILABILITY __NULL_AVAILABILITY

// Take care of {A,S}PI_DEPRECATED{,WITH_REPLACEMENT}{,_BEGIN,_END}
#undef __API_DEPRECATED_MSG_GET_MACRO
#define __API_DEPRECATED_MSG_GET_MACRO(...) __NULL_AVAILABILITY

// Take care of {A,S}PI_AVAILABLE{,_BEGIN,_END}
#undef __API_AVAILABLE_GET_MACRO
#define __API_AVAILABLE_GET_MACRO(...) __NULL_AVAILABILITY

// Take care of API_UNAVAILABLE{,_BEGIN,_END}
#undef __API_UNAVAILABLE_GET_MACRO
#define __API_UNAVAILABLE_GET_MACRO(...) __NULL_AVAILABILITY

// Starting in iOS 18.4 and aligned SDKs, AvailabilityInternal.h has a hash
// number used to detect accidental use of multiple copies of the library. Keep
// track of known hashes and undef the hashed macro names. When encountering an
// unknown hash, emit an error to avoid confusing build failures.
#define __WEBKIT_OVERRIDDEN_AVAILABILITY_VERSIONS_VERSION_HASH 93585900U
#if defined(__AVAILABILITY_VERSIONS_VERSION_HASH) && (__AVAILABILITY_VERSIONS_VERSION_HASH != 93585900U)
#error __AVAILABILITY_VERSIONS_VERSION_HASH not recognized, please add it to WebKit's AvailabilityProhibitedInternal.h to silence availability errors.
#endif

#ifdef __API_AVAILABLE_GET_MACRO_93585900
#undef __API_AVAILABLE_GET_MACRO_93585900
#define __API_AVAILABLE_GET_MACRO_93585900(...) __NULL_AVAILABILITY
#undef __API_UNAVAILABLE_GET_MACRO_93585900
#define __API_UNAVAILABLE_GET_MACRO_93585900(...) __NULL_AVAILABILITY
#endif

#define __NULL_AVAILABILITY(...)
