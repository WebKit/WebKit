/*
 * Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitSettingsPrivate_h
#define WebKitSettingsPrivate_h

#include <WebKit/WKBase.h>
#include "WebKitSettings.h"
#include "WebPreferences.h"

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionContext.h"
#endif

WebKit::WebPreferences* webkitSettingsGetPreferences(WebKitSettings*);

WK_EXPORT void webkitSettingsSetMediaCaptureRequiresSecureConnection(WebKitSettings*, bool required);
WK_EXPORT void webkitSettingsSetGetUserMediaRequiresFocus(WebKitSettings*, bool required);

#if ENABLE(WK_WEB_EXTENSIONS)
WK_EXPORT WebKit::WebExtensionContext* webkitSettingsGetWebExtensionContext(WebKitSettings*);
WK_EXPORT void webkitSettingsSetWebExtensionContext(WebKitSettings*, const WebKit::WebExtensionContext&);
#endif // ENABLE(WK_WEB_EXTENSIONS)

#endif // WebKitSettingsPrivate_h
