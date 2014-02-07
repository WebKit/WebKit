/*
 * Copyright (C) 2008, 2009, 2010, 2014 Apple Inc. All Rights Reserved.
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

#ifndef __WebKitAvailability__
#define __WebKitAvailability__

#ifdef __APPLE__

#include <AvailabilityMacros.h>
#include <CoreFoundation/CoreFoundation.h>

#if !defined(NS_AVAILABLE)
#define NS_AVAILABLE(_mac, _ios)
#endif // !defined(NS_AVAILABLE)
#if !defined(CF_AVAILABLE)
#define CF_AVAILABLE(_mac, _ios)
#endif // !defined(CF_AVAILABLE)

#else // __APPLE__

#define CF_AVAILABLE(_mac, _ios)
#define NS_AVAILABLE(_mac, _ios)

#endif // __APPLE__

#if !defined(__NSi_10_10)
#define __NSi_10_10 introduced=10.10
#endif

#if !defined(__NSi_8_0)
#define __NSi_8_0 introduced=8.0
#endif

#endif /* __WebKitAvailability__ */
