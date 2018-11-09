/*
 * Copyright (C) 2008-2017 Apple Inc. All Rights Reserved.
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
#include "ImageSourceCG.h"

#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

String MIMETypeForImageType(const String& type)
{
    String mimeType;
    // FIXME: This approach of taking a UTI like public.type and giving back 
    // a MIME type like image/type will work for common image UTIs like jpeg, 
    // png, tiff, gif but won't work for UTIs like: public.jpeg-2000,
    // public.xbitmap-image, com.apple.quicktime-image, and others.
    if (int dotLocation = type.reverseFind('.'))
        mimeType = "image/" + type.substring(dotLocation + 1);
    return mimeType;
}

String preferredExtensionForImageType(const String& type)
{
    if (type.isNull())
        return String();

    static const auto map = makeNeverDestroyed(HashMap<String, String, ASCIICaseInsensitiveHash> {
        { "public.html", "html" },
        { "public.jpeg", "jpeg" },
        { "public.jpeg-2000", "jp2" },
        { "public.plain-text", "txt" },
        { "public.png", "png" },
        { "public.tiff", "tiff" },
        { "public.xbitmap-image", "xbm" },
        { "public.xml", "xml" },
        { "com.adobe.illustrator.ai-image", "ai" },
        { "com.adobe.pdf", "pdf" },
        { "com.adobe.photoshop-image", "psd" },
        { "com.adobe.postscript", "ps" },
        { "com.apple.icns", "icns" },
        { "com.apple.macpaint-image", "pntg" },
        { "com.apple.pict", "pict" },
        { "com.apple.quicktime-image", "qtif" },
        { "com.apple.webarchive", "webarchive" },
        { "com.compuserve.gif", "gif" },
        { "com.ilm.openexr-image", "exr" },
        { "com.kodak.flashpix-image", "fpx" },
        { "com.microsoft.bmp", "bmp" },
        { "com.microsoft.ico", "ico" },
        { "com.netscape.javascript-source", "js" },
        { "com.sgi.sgi-image", "sgi" },
        { "com.truevision.tga-image", "tga" },
    });
    return map.get().get(type);
}

} // namespace WebCore
