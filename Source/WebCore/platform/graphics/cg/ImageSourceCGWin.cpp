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
    if (type == "com.microsoft.cur" || type == "com.microsoft.ico")
        mimeType = "image/vnd.microsoft.icon"_s;
    else if (int dotLocation = type.reverseFind('.'))
        mimeType = "image/" + type.substring(dotLocation + 1);
    return mimeType;
}

String preferredExtensionForImageType(const String& type)
{
    if (type.isNull())
        return String();

    static const auto map = makeNeverDestroyed(HashMap<String, String, ASCIICaseInsensitiveHash> {
        { "public.html"_s, "html"_s },
        { "public.jpeg"_s, "jpeg"_s },
        { "public.jpeg-2000"_s, "jp2"_s },
        { "public.plain-text"_s, "txt"_s },
        { "public.png"_s, "png"_s },
        { "public.tiff"_s, "tiff"_s },
        { "public.xbitmap-image"_s, "xbm"_s },
        { "public.xml"_s, "xml"_s },
        { "com.adobe.illustrator.ai-image"_s, "ai"_s },
        { "com.adobe.pdf"_s, "pdf"_s },
        { "com.adobe.photoshop-image"_s, "psd"_s },
        { "com.adobe.postscript"_s, "ps"_s },
        { "com.apple.icns"_s, "icns"_s },
        { "com.apple.macpaint-image"_s, "pntg"_s },
        { "com.apple.pict"_s, "pict"_s },
        { "com.apple.quicktime-image"_s, "qtif"_s },
        { "com.apple.webarchive"_s, "webarchive"_s },
        { "com.compuserve.gif"_s, "gif"_s },
        { "com.ilm.openexr-image"_s, "exr"_s },
        { "com.kodak.flashpix-image"_s, "fpx"_s },
        { "com.microsoft.bmp"_s, "bmp"_s },
        { "com.microsoft.ico"_s, "ico"_s },
        { "com.netscape.javascript-source"_s, "js"_s },
        { "com.sgi.sgi-image"_s, "sgi"_s },
        { "com.truevision.tga-image"_s, "tga"_s },
    });
    return map.get().get(type);
}

} // namespace WebCore
