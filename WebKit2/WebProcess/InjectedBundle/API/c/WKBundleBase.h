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

#ifndef WKBundleBase_h
#define WKBundleBase_h

typedef const struct OpaqueWKBundle* WKBundleRef;
typedef const struct OpaqueWKBundleFrame* WKBundleFrameRef;
typedef const struct OpaqueWKBundleNodeHandle* WKBundleNodeHandleRef;
typedef const struct OpaqueWKBundlePage* WKBundlePageRef;
typedef const struct OpaqueWKBundleScriptWorld* WKBundleScriptWorldRef;

typedef const struct OpaqueWKBundleDOMCSSStyleDeclaration* WKBundleCSSStyleDeclarationRef;
typedef const struct OpaqueWKBundleNode* WKBundleNodeRef;
typedef const struct OpaqueWKBundleRange* WKBundleRangeRef;

#undef WK_EXPORT
#if defined(WK_NO_EXPORT)
#define WK_EXPORT
#elif defined(__GNUC__)
#define WK_EXPORT __attribute__((visibility("default")))
#elif defined(WIN32) || defined(_WIN32)
#if BUILDING_WEBKIT
#define WK_EXPORT __declspec(dllexport)
#else
#define WK_EXPORT __declspec(dllimport)
#endif
#else
#define WK_EXPORT
#endif

#endif /* WKBundleBase_h */
