/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WebCoreURLResponse.h"

#import "MIMETypeRegistry.h"
#import "ResourceResponse.h"
#import "UTIUtilities.h"
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/SortedArrayMap.h>

namespace WebCore {

#if PLATFORM(MAC)

void adjustMIMETypeIfNecessary(CFURLResponseRef response, IsMainResourceLoad, IsNoSniffSet isNoSniffSet)
{
    if (CFURLResponseGetMIMEType(response))
        return;

    RetainPtr<CFStringRef> type;

    if (auto extension = filePathExtension(response); extension && isNoSniffSet == IsNoSniffSet::No) {
        // <rdar://problem/7007389> CoreTypes UTI map is missing 100+ file extensions that GateKeeper knew about
        // Once UTType matches one of these mappings on all versions of macOS we support, we can remove that pair.
        // Alternatively, we could remove any pairs that we determine we no longer need.
        // And then remove this code entirely once they are all gone.
        static constexpr std::pair<ComparableLettersLiteral, NSString *> extensionPairs[] = {
            { "ai", @"application/postscript" },
            { "asc", @"text/plain" },
            { "bcpio", @"application/x-bcpio" },
            { "bmp", @"image/bmp" },
            { "cdf", @"application/x-netcdf" },
            { "class", @"application/octet-stream" },
            { "cpgz", @"application/x-gzip" },
            { "cpio", @"application/x-cpio" },
            { "cpt", @"application/mac-compactpro" },
            { "csh", @"application/x-csh" },
            { "css", @"text/css" },
            { "dcr", @"application/x-director" },
            { "dir", @"application/x-director" },
            { "dmg", @"application/x-diskcopy" },
            { "dms", @"application/octet-stream" },
            { "dvi", @"application/x-dvi" },
            { "dxr", @"application/x-director" },
            { "eps", @"application/postscript" },
            { "etx", @"text/x-setext" },
            { "ez", @"application/andrew-inset" },
            { "fdf", @"application/vnd.fdf" },
            { "fla", @"application/octet-stream" },
            { "fp", @"application/x-filemaker" },
            { "fp2", @"application/x-filemaker" },
            { "fp3", @"application/x-filemaker" },
            { "fp4", @"application/x-filemaker" },
            { "fp5", @"application/x-filemaker" },
            { "fp6", @"application/x-filemaker" },
            { "hdf", @"application/x-hdf" },
            { "ice", @"x-conference/x-cooltalk" },
            { "ico", @"image/x-icon" },
            { "ics", @"text/calendar" },
            { "ief", @"image/ief" },
            { "iges", @"model/iges" },
            { "igs", @"model/iges" },
            { "iso", @"application/octet-stream" },
            { "jhtml", @"text/html" },
            { "latex", @"application/x-latex" },
            { "lha", @"application/octet-stream" },
            { "lzh", @"application/octet-stream" },
            { "m3u", @"audio/x-mpegurl" },
            { "m4p", @"audio/x-m4p" },
            { "mac", @"image/x-macpaint" },
            { "man", @"application/x-troff-man" },
            { "me", @"application/x-troff-me" },
            { "mesh", @"model/mesh" },
            { "mif", @"application/vnd.mif" },
            { "mjs", @"text/javascript" },
            { "movie", @"video/x-sgi-movie" },
            { "mp2", @"audio/mpeg" },
            { "mpga", @"audio/mpeg" },
            { "ms", @"application/x-troff-ms" },
            { "msh", @"model/mesh" },
            { "mxu", @"video/vnd.mpegurl" },
            { "nc", @"application/x-netcdf" },
            { "oda", @"application/oda" },
            { "pbm", @"image/x-portable-bitmap" },
            { "pcx", @"image/x-pcx" },
            { "pdb", @"chemical/x-pdb" },
            { "pgm", @"image/x-portable-graymap" },
            { "pgn", @"application/x-chess-pgn" },
            { "pls", @"audio/scpls" },
            { "pnm", @"image/x-portable-anymap" },
            { "pnt", @"image/x-macpaint" },
            { "pntg", @"image/x-macpaint" },
            { "ppm", @"image/x-portable-pixmap" },
            { "ras", @"image/x-cmu-raster" },
            { "rgb", @"image/x-rgb" },
            { "roff", @"application/x-troff" },
            { "rpm", @"audio/x-pn-realaudio-plugin" },
            { "rtx", @"text/richtext" },
            { "sgm", @"text/sgml" },
            { "sgml", @"text/sgml" },
            { "sh", @"application/x-sh" },
            { "shar", @"application/x-shar" },
            { "silo", @"model/mesh" },
            { "skd", @"application/x-koan" },
            { "skm", @"application/x-koan" },
            { "skp", @"application/x-koan" },
            { "skt", @"application/x-koan" },
            { "smi", @"application/x-diskcopy" },
            { "so", @"application/octet-stream" },
            { "spl", @"application/x-futuresplash" },
            { "src", @"application/x-wais-source" },
            { "sv4cpio", @"application/x-sv4cpio" },
            { "sv4crc", @"application/x-sv4crc" },
            { "swf", @"application/x-shockwave-flash" },
            { "t", @"application/x-troff" },
            { "targa", @"image/x-targa" },
            { "tcl", @"application/x-tcl" },
            { "tex", @"application/x-tex" },
            { "texi", @"application/x-texinfo" },
            { "texinfo", @"application/x-texinfo" },
            { "tgz", @"application/x-gzip" },
            { "torrent", @"application/x-bittorrent" },
            { "tr", @"application/x-troff" },
            { "tsv", @"text/tab-separated-values" },
            { "ustar", @"application/x-ustar" },
            { "vcd", @"application/x-cdlink" },
            { "vrml", @"model/vrml" },
            { "wbmp", @"image/vnd.wap.wbmp" },
            { "wbxml", @"application/vnd.wap.wbxml" },
            { "webarchive", @"application/x-webarchive" },
            { "webm", @"video/webm" },
            { "wmd", @"application/x-ms-wmd" },
            { "wml", @"text/vnd.wap.wml" },
            { "wmlc", @"application/vnd.wap.wmlc" },
            { "wmls", @"text/vnd.wap.wmlscript" },
            { "wmlsc", @"application/vnd.wap.wmlscriptc" },
            { "wrl", @"model/vrml" },
            { "xdp", @"application/vnd.adobe.xdp+xml" },
            { "xfd", @"application/vnd.adobe.xfd+xml" },
            { "xfdf", @"application/vnd.adobe.xfdf" },
            { "xpm", @"image/x-xpixmap" },
            { "xsl", @"text/xml" },
            { "xwd", @"image/x-xwindowdump" },
            { "xyz", @"chemical/x-xyz" },
            { "z", @"application/x-compress" },
        };
        static constexpr SortedArrayMap extensionMap { extensionPairs };
        type = (__bridge CFStringRef)extensionMap.get(String { extension.get() });
        if (!type)
            type = preferredMIMETypeForFileExtensionFromUTType(extension.get());
    }

    CFURLResponseSetMIMEType(response, type ? type.get() : CFSTR("application/octet-stream"));
}

#endif

NSURLResponse *synthesizeRedirectResponseIfNecessary(NSURLRequest *currentRequest, NSURLRequest *newRequest, NSURLResponse *redirectResponse)
{
    if (redirectResponse)
        return redirectResponse;

    if ([[[newRequest URL] scheme] isEqualToString:[[currentRequest URL] scheme]] && ![newRequest _schemeWasUpgradedDueToDynamicHSTS])
        return nil;

    return retainPtr(ResourceResponse::syntheticRedirectResponse(URL([currentRequest URL]), URL([newRequest URL])).nsURLResponse()).autorelease();
}

RetainPtr<CFStringRef> filePathExtension(CFURLResponseRef response)
{
    auto responseURL = CFURLResponseGetURL(response);
    if (![(__bridge NSURL *)responseURL isFileURL])
        return nullptr;
    return adoptCF(CFURLCopyPathExtension(responseURL));
}

RetainPtr<CFStringRef> preferredMIMETypeForFileExtensionFromUTType(CFStringRef extension)
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return mimeTypeFromUTITree(adoptCF(UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension, nullptr)).get());
ALLOW_DEPRECATED_DECLARATIONS_END
}

}
