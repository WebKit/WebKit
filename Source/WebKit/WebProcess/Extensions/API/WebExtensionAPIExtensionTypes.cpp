/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebExtensionAPIExtensionTypes.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

JSValueRef WebExtensionAPIExtensionTypes::cssOrigin(JSContextRef context)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extensionTypes/CSSOrigin

    return makeEnumObject(context, {
        { "AUTHOR"_s, "author"_s },
        { "USER"_s, "user"_s },
    });
}

JSValueRef WebExtensionAPIExtensionTypes::executionWorld(JSContextRef context)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extensionTypes/ExecutionWorld

    return makeEnumObject(context, {
        { "ISOLATED"_s, "ISOLATED"_s },
        { "MAIN"_s, "MAIN"_s },
    });
}

JSValueRef WebExtensionAPIExtensionTypes::imageFormat(JSContextRef context)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extensionTypes/ImageFormat

    return makeEnumObject(context, {
        { "JPEG"_s, "jpeg"_s },
        { "PNG"_s, "png"_s },
    });
}

JSValueRef WebExtensionAPIExtensionTypes::runAt(JSContextRef context)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/extensionTypes/RunAt

    return makeEnumObject(context, {
        { "DOCUMENT_START"_s, "document_start"_s },
        { "DOCUMENT_END"_s, "document_end"_s },
        { "DOCUMENT_IDLE"_s, "document_idle"_s },
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
