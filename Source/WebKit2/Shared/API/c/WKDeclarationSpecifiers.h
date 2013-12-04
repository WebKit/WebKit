/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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

#ifndef WKDeclarationSpecifiers_h
#define WKDeclarationSpecifiers_h

#undef WK_EXPORT
#if defined(WK_NO_EXPORT)
#define WK_EXPORT
#elif defined(__GNUC__) && !defined(__CC_ARM) && !defined(__ARMCC__)
#define WK_EXPORT __attribute__((visibility("default")))
#else /* !defined(WK_NO_EXPORT) */
#define WK_EXPORT
#endif /* defined(WK_NO_EXPORT) */

#if !defined(WK_INLINE)
#if defined(__cplusplus)
#define WK_INLINE static inline
#elif defined(__GNUC__)
#define WK_INLINE static __inline__
#else
#define WK_INLINE static
#endif
#endif /* !defined(WK_INLINE) */

#ifndef __has_extension
#define __has_extension(x) 0
#endif

#if defined(__has_extension) && __has_extension(enumerator_attributes) && __has_extension(attribute_unavailable_with_message)
#define WK_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#define WK_DEPRECATED(message)
#endif

#if defined(__has_extension) && __has_extension(enumerator_attributes) && __has_extension(attribute_unavailable_with_message)
#define WK_ENUM_DEPRECATED(message) __attribute__((deprecated(message)))
#else
#define WK_ENUM_DEPRECATED(message)
#endif

#endif /* WKDeclarationSpecifiers_h */
