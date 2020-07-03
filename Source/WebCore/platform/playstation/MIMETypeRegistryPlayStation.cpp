/*
 * Copyright (C) 2018 Sony Interactive Entertainment Inc.
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
#include "MIMETypeRegistry.h"

namespace WebCore {

static const std::initializer_list<TypeExtensionPair>& platformMediaTypes()
{
    static std::initializer_list<TypeExtensionPair> platformMediaTypes = {
        { "image/bmp"_s, "bmp"_s },
        { "text/css"_s, "css"_s },
        { "image/gif"_s, "gif"_s },
        { "text/html"_s, "htm"_s },
        { "text/html"_s, "html"_s },
        { "image/x-icon"_s, "ico"_s },
        { "image/jpeg"_s, "jpeg"_s },
        { "image/jpeg"_s, "jpg"_s },
        { "application/x-javascript"_s, "js"_s },
        { "application/pdf"_s, "pdf"_s },
        { "image/png"_s, "png"_s },
        { "application/rss+xml"_s, "rss"_s },
        { "image/svg+xml"_s, "svg"_s },
        { "application/x-shockwave-flash"_s, "swf"_s },
        { "text/plain"_s, "text"_s },
        { "text/plain"_s, "txt"_s },
        { "text/vnd.wap.wml"_s, "wml"_s },
        { "application/vnd.wap.wmlc"_s, "wmlc"_s },
        { "image/x-xbitmap"_s, "xbm"_s },
        { "application/xhtml+xml"_s, "xhtml"_s },
        { "text/xml"_s, "xml"_s },
        { "text/xsl"_s, "xsl"_s },
    };
    return platformMediaTypes;
}

String MIMETypeRegistry::mimeTypeForExtension(const String& extension)
{
    for (auto& entry : platformMediaTypes()) {
        if (equalIgnoringASCIICase(extension, entry.extension.characters()))
            return entry.type;
    }
    return emptyString();
}

bool MIMETypeRegistry::isApplicationPluginMIMEType(const String&)
{
    return false;
}

String MIMETypeRegistry::preferredExtensionForMIMEType(const String& mimeType)
{
    for (auto& entry : platformMediaTypes()) {
        if (equalIgnoringASCIICase(mimeType, entry.type.characters()))
            return entry.extension;
    }
    return emptyString();
}

Vector<String> MIMETypeRegistry::extensionsForMIMEType(const String&)
{
    ASSERT_NOT_IMPLEMENTED_YET();
    return { };
}

} // namespace WebCore
