/*
 * Copyright (C) 2015 Igalia S.L.
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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitWebExtensionAutocleanups_h
#define WebKitWebExtensionAutocleanups_h

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
#ifndef __GI_SCANNER__

G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitFrame, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitScriptWorld, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebEditor, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebExtension, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebHitTestResult, g_object_unref)
G_DEFINE_AUTOPTR_CLEANUP_FUNC (WebKitWebPage, g_object_unref)

#endif // __GI_SCANNER__
#endif // G_DEFINE_AUTOPTR_CLEANUP_FUNC

#endif // WebKitWebExtensionAutocleanups_h
