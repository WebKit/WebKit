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
            { "ai"_s, @"application/postscript" },
            { "asc"_s, @"text/plain" },
            { "bcpio"_s, @"application/x-bcpio" },
            { "bmp"_s, @"image/bmp" },
            { "cdf"_s, @"application/x-netcdf" },
            { "class"_s, @"application/octet-stream" },
            { "cpgz"_s, @"application/x-gzip" },
            { "cpio"_s, @"application/x-cpio" },
            { "cpt"_s, @"application/mac-compactpro" },
            { "csh"_s, @"application/x-csh" },
            { "css"_s, @"text/css" },
            { "dcr"_s, @"application/x-director" },
            { "dir"_s, @"application/x-director" },
            { "dmg"_s, @"application/x-diskcopy" },
            { "dms"_s, @"application/octet-stream" },
            { "dvi"_s, @"application/x-dvi" },
            { "dxr"_s, @"application/x-director" },
            { "eps"_s, @"application/postscript" },
            { "etx"_s, @"text/x-setext" },
            { "ez"_s, @"application/andrew-inset" },
            { "fdf"_s, @"application/vnd.fdf" },
            { "fla"_s, @"application/octet-stream" },
            { "fp"_s, @"application/x-filemaker" },
            { "fp2"_s, @"application/x-filemaker" },
            { "fp3"_s, @"application/x-filemaker" },
            { "fp4"_s, @"application/x-filemaker" },
            { "fp5"_s, @"application/x-filemaker" },
            { "fp6"_s, @"application/x-filemaker" },
            { "hdf"_s, @"application/x-hdf" },
            { "ice"_s, @"x-conference/x-cooltalk" },
            { "ico"_s, @"image/x-icon" },
            { "ics"_s, @"text/calendar" },
            { "ief"_s, @"image/ief" },
            { "iges"_s, @"model/iges" },
            { "igs"_s, @"model/iges" },
            { "iso"_s, @"application/octet-stream" },
            { "jhtml"_s, @"text/html" },
            { "latex"_s, @"application/x-latex" },
            { "lha"_s, @"application/octet-stream" },
            { "lzh"_s, @"application/octet-stream" },
            { "m3u"_s, @"audio/x-mpegurl" },
            { "m4p"_s, @"audio/x-m4p" },
            { "mac"_s, @"image/x-macpaint" },
            { "man"_s, @"application/x-troff-man" },
            { "me"_s, @"application/x-troff-me" },
            { "mesh"_s, @"model/mesh" },
            { "mif"_s, @"application/vnd.mif" },
            { "mjs"_s, @"text/javascript" },
            { "movie"_s, @"video/x-sgi-movie" },
            { "mp2"_s, @"audio/mpeg" },
            { "mpga"_s, @"audio/mpeg" },
            { "ms"_s, @"application/x-troff-ms" },
            { "msh"_s, @"model/mesh" },
            { "mxu"_s, @"video/vnd.mpegurl" },
            { "nc"_s, @"application/x-netcdf" },
            { "oda"_s, @"application/oda" },
            { "pbm"_s, @"image/x-portable-bitmap" },
            { "pcx"_s, @"image/x-pcx" },
            { "pdb"_s, @"chemical/x-pdb" },
            { "pgm"_s, @"image/x-portable-graymap" },
            { "pgn"_s, @"application/x-chess-pgn" },
            { "pls"_s, @"audio/scpls" },
            { "pnm"_s, @"image/x-portable-anymap" },
            { "pnt"_s, @"image/x-macpaint" },
            { "pntg"_s, @"image/x-macpaint" },
            { "ppm"_s, @"image/x-portable-pixmap" },
            { "ras"_s, @"image/x-cmu-raster" },
            { "rgb"_s, @"image/x-rgb" },
            { "roff"_s, @"application/x-troff" },
            { "rpm"_s, @"audio/x-pn-realaudio-plugin" },
            { "rtx"_s, @"text/richtext" },
            { "sgm"_s, @"text/sgml" },
            { "sgml"_s, @"text/sgml" },
            { "sh"_s, @"application/x-sh" },
            { "shar"_s, @"application/x-shar" },
            { "silo"_s, @"model/mesh" },
            { "skd"_s, @"application/x-koan" },
            { "skm"_s, @"application/x-koan" },
            { "skp"_s, @"application/x-koan" },
            { "skt"_s, @"application/x-koan" },
            { "smi"_s, @"application/x-diskcopy" },
            { "so"_s, @"application/octet-stream" },
            { "spl"_s, @"application/x-futuresplash" },
            { "src"_s, @"application/x-wais-source" },
            { "sv4cpio"_s, @"application/x-sv4cpio" },
            { "sv4crc"_s, @"application/x-sv4crc" },
            { "swf"_s, @"application/x-shockwave-flash" },
            { "t"_s, @"application/x-troff" },
            { "targa"_s, @"image/x-targa" },
            { "tcl"_s, @"application/x-tcl" },
            { "tex"_s, @"application/x-tex" },
            { "texi"_s, @"application/x-texinfo" },
            { "texinfo"_s, @"application/x-texinfo" },
            { "tgz"_s, @"application/x-gzip" },
            { "torrent"_s, @"application/x-bittorrent" },
            { "tr"_s, @"application/x-troff" },
            { "tsv"_s, @"text/tab-separated-values" },
            { "ustar"_s, @"application/x-ustar" },
            { "vcd"_s, @"application/x-cdlink" },
            { "vrml"_s, @"model/vrml" },
            { "wbmp"_s, @"image/vnd.wap.wbmp" },
            { "wbxml"_s, @"application/vnd.wap.wbxml" },
            { "webarchive"_s, @"application/x-webarchive" },
            { "webm"_s, @"video/webm" },
            { "wmd"_s, @"application/x-ms-wmd" },
            { "wml"_s, @"text/vnd.wap.wml" },
            { "wmlc"_s, @"application/vnd.wap.wmlc" },
            { "wmls"_s, @"text/vnd.wap.wmlscript" },
            { "wmlsc"_s, @"application/vnd.wap.wmlscriptc" },
            { "wrl"_s, @"model/vrml" },
            { "xdp"_s, @"application/vnd.adobe.xdp+xml" },
            { "xfd"_s, @"application/vnd.adobe.xfd+xml" },
            { "xfdf"_s, @"application/vnd.adobe.xfdf" },
            { "xpm"_s, @"image/x-xpixmap" },
            { "xsl"_s, @"text/xml" },
            { "xwd"_s, @"image/x-xwindowdump" },
            { "xyz"_s, @"chemical/x-xyz" },
            { "z"_s, @"application/x-compress" },
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
