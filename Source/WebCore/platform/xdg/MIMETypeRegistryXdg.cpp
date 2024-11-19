/*
 * Copyright (C) 2017 Igalia S.L.
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

#include "config.h"
#include "MIMETypeRegistry.h"

#include <wtf/text/MakeString.h>

#define XDG_PREFIX _wk_xdg
#include "xdgmime.h"

#define MAX_EXTENSION_COUNT 10
namespace WebCore {

String MIMETypeRegistry::mimeTypeForExtension(StringView string)
{
    if (string.isEmpty())
        return String();

    // Build any filename with the given extension.
    auto filename = makeString("a."_s, string);
    if (const char* mimeType = xdg_mime_get_mime_type_from_file_name(filename.utf8().data())) {
        if (mimeType != XDG_MIME_TYPE_UNKNOWN)
            return String::fromUTF8(mimeType);
    }

    return String();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return String();

    if (mimeType.startsWith("text/plain"_s))
        return String();

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    String returnValue;
    char* extension;
    if (xdg_mime_get_simple_globs(mimeType.utf8().data(), &extension, 1)) {
        if (extension[0] == '.' && extension[1])
            returnValue = String::fromUTF8(extension + 1);
        free(extension);
    }
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return returnValue;
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return { };

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port
    Vector<String> returnValue;
    char* extensions[MAX_EXTENSION_COUNT];
    int n = xdg_mime_get_simple_globs(mimeType.utf8().data(), extensions, MAX_EXTENSION_COUNT);
    for (int i = 0; i < n; ++i) {
        returnValue.append(String::fromUTF8(extensions[i]));
        free(extensions[i]);
    }
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
    return returnValue;
}

}
