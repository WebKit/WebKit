/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
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
#import "WebCoreSystemInterface.h"
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

// <rdar://problem/5321972> Plain text document from HTTP server detected as application/octet-stream
// When we sniff a resource as application/octet-stream but the http response headers had "text/plain",
// we have a hard decision to make about which of the two generic MIME types to go with.
// When the URL's extension is a known binary type, we'll go with application/octet-stream.
// Otherwise, we'll trust the server.
static CFSetRef createBinaryExtensionsSet()
{
    CFStringRef extensions[] = {
        CFSTR("3g2"),
        CFSTR("3gp"),
        CFSTR("ai"),
        CFSTR("aif"),
        CFSTR("aifc"),
        CFSTR("aiff"),
        CFSTR("au"),
        CFSTR("avi"),
        CFSTR("bcpio"),
        CFSTR("bin"),
        CFSTR("bmp"),
        CFSTR("boz"),
        CFSTR("bpk"),
        CFSTR("bz"),
        CFSTR("bz2"),
        CFSTR("chm"),
        CFSTR("class"),
        CFSTR("com"),
        CFSTR("cpio"),
        CFSTR("dcr"),
        CFSTR("dir"),
        CFSTR("dist"),
        CFSTR("distz"),
        CFSTR("dll"),
        CFSTR("dmg"),
        CFSTR("dms"),
        CFSTR("doc"),
        CFSTR("dot"),
        CFSTR("dump"),
        CFSTR("dv"),
        CFSTR("dvi"),
        CFSTR("dxr"),
        CFSTR("elc"),
        CFSTR("eot"),
        CFSTR("eps"),
        CFSTR("exe"),
        CFSTR("fgd"),
        CFSTR("gif"),
        CFSTR("gtar"),
        CFSTR("h261"),
        CFSTR("h263"),
        CFSTR("h264"),
        CFSTR("ico"),
        CFSTR("ims"),
        CFSTR("indd"),
        CFSTR("iso"),
        CFSTR("jp2"),
        CFSTR("jpe"),
        CFSTR("jpeg"),
        CFSTR("jpg"),
        CFSTR("jpgm"),
        CFSTR("jpgv"),
        CFSTR("jpm"),
        CFSTR("kar"),
        CFSTR("kmz"),
        CFSTR("lha"),
        CFSTR("lrm"),
        CFSTR("lzh"),
        CFSTR("m1v"),
        CFSTR("m2a"),
        CFSTR("m2v"),
        CFSTR("m3a"),
        CFSTR("m3u"),
        CFSTR("m4a"),
        CFSTR("m4p"),
        CFSTR("m4v"),
        CFSTR("mdb"),
        CFSTR("mid"),
        CFSTR("midi"),
        CFSTR("mj2"),
        CFSTR("mjp2"),
        CFSTR("mov"),
        CFSTR("movie"),
        CFSTR("mp2"),
        CFSTR("mp2a"),
        CFSTR("mp3"),
        CFSTR("mp4"),
        CFSTR("mp4a"),
        CFSTR("mp4s"),
        CFSTR("mp4v"),
        CFSTR("mpe"),
        CFSTR("mpeg"),
        CFSTR("mpg"),
        CFSTR("mpg4"),
        CFSTR("mpga"),
        CFSTR("mpp"),
        CFSTR("mpt"),
        CFSTR("msi"),
        CFSTR("ogg"),
        CFSTR("otf"),
        CFSTR("pct"),
        CFSTR("pdf"),
        CFSTR("pfa"),
        CFSTR("pfb"),
        CFSTR("pic"),
        CFSTR("pict"),
        CFSTR("pkg"),
        CFSTR("png"),
        CFSTR("pot"),
        CFSTR("pps"),
        CFSTR("ppt"),
        CFSTR("ps"),
        CFSTR("psd"),
        CFSTR("qt"),
        CFSTR("qti"),
        CFSTR("qtif"),
        CFSTR("qwd"),
        CFSTR("qwt"),
        CFSTR("qxb"),
        CFSTR("qxd"),
        CFSTR("qxl"),
        CFSTR("qxp"),
        CFSTR("qxt"),
        CFSTR("ra"),
        CFSTR("ram"),
        CFSTR("rm"),
        CFSTR("rmi"),
        CFSTR("rmp"),
        CFSTR("scpt"),
        CFSTR("sit"),
        CFSTR("sitx"),
        CFSTR("snd"),
        CFSTR("so"),
        CFSTR("swf"),
        CFSTR("tar"),
        CFSTR("tif"),
        CFSTR("tiff"),
        CFSTR("ttf"),
        CFSTR("wav"),
        CFSTR("wcm"),
        CFSTR("wdb"),
        CFSTR("wks"),
        CFSTR("wm"),
        CFSTR("wma"),
        CFSTR("wmd"),
        CFSTR("wmf"),
        CFSTR("wmv"),
        CFSTR("wmx"),
        CFSTR("wmz"),
        CFSTR("wpd"),
        CFSTR("wpl"),
        CFSTR("wps"),
        CFSTR("wvx"),
        CFSTR("xla"),
        CFSTR("xlc"),
        CFSTR("xlm"),
        CFSTR("xls"),
        CFSTR("xlt"),
        CFSTR("xlw"),
        CFSTR("xps"),
        CFSTR("zip")
    };
    return CFSetCreate(kCFAllocatorDefault, (const void **)&extensions, sizeof(extensions)/sizeof(CFStringRef), &kCFTypeSetCallBacks);
}

// <rdar://problem/7007389> CoreTypes UTI map is missing 100+ file extensions that GateKeeper knew about
// When we disabled content sniffing for file URLs we caused problems with these 100+ extensions that CoreTypes
// doesn't know about.
// If CoreTypes is ever brought up to speed we can remove this table and associated code.
static CFDictionaryRef createExtensionToMIMETypeMap()
{
    CFStringRef keys[] = {
        CFSTR("ai"),
        CFSTR("asc"),
        CFSTR("bcpio"),
        CFSTR("bmp"),
        CFSTR("cdf"),
        CFSTR("class"),
        CFSTR("cpgz"),
        CFSTR("cpio"),
        CFSTR("cpt"),
        CFSTR("csh"),
        CFSTR("css"),
        CFSTR("dcr"),
        CFSTR("dir"),
        CFSTR("dmg"),
        CFSTR("dms"),
        CFSTR("dvi"),
        CFSTR("dxr"),
        CFSTR("eps"),
        CFSTR("etx"),
        CFSTR("ez"),
        CFSTR("fdf"),
        CFSTR("fla"),
        CFSTR("fp"),
        CFSTR("fp2"),
        CFSTR("fp3"),
        CFSTR("fp4"),
        CFSTR("fp5"),
        CFSTR("fp6"),
        CFSTR("hdf"),
        CFSTR("ice"),
        CFSTR("ico"),
        CFSTR("ics"),
        CFSTR("ief"),
        CFSTR("iges"),
        CFSTR("igs"),
        CFSTR("iso"),
        CFSTR("jhtml"),
        CFSTR("latex"),
        CFSTR("lha"),
        CFSTR("lzh"),
        CFSTR("m3u"),
        CFSTR("m4p"),
        CFSTR("mac"),
        CFSTR("man"),
        CFSTR("me"),
        CFSTR("mesh"),
        CFSTR("mif"),
        CFSTR("movie"),
        CFSTR("mp2"),
        CFSTR("mpga"),
        CFSTR("ms"),
        CFSTR("msh"),
        CFSTR("mxu"),
        CFSTR("nc"),
        CFSTR("oda"),
        CFSTR("pbm"),
        CFSTR("pcx"),
        CFSTR("pdb"),
        CFSTR("pgm"),
        CFSTR("pgn"),
        CFSTR("pls"),
        CFSTR("pnm"),
        CFSTR("pnt"),
        CFSTR("pntg"),
        CFSTR("ppm"),
        CFSTR("ras"),
        CFSTR("rgb"),
        CFSTR("roff"),
        CFSTR("rpm"),
        CFSTR("rtx"),
        CFSTR("sgm"),
        CFSTR("sgml"),
        CFSTR("sh"),
        CFSTR("shar"),
        CFSTR("silo"),
        CFSTR("skd"),
        CFSTR("skm"),
        CFSTR("skp"),
        CFSTR("skt"),
        CFSTR("smi"),
        CFSTR("so"),
        CFSTR("spl"),
        CFSTR("src"),
        CFSTR("sv4cpio"),
        CFSTR("sv4crc"),
        CFSTR("swf"),
        CFSTR("t"),
        CFSTR("targa"),
        CFSTR("tcl"),
        CFSTR("tex"),
        CFSTR("texi"),
        CFSTR("texinfo"),
        CFSTR("tgz"),
        CFSTR("torrent"),
        CFSTR("tr"),
        CFSTR("tsv"),
        CFSTR("ustar"),
        CFSTR("vcd"),
        CFSTR("vrml"),
        CFSTR("wbmp"),
        CFSTR("wbxml"),
        CFSTR("webarchive"),
        CFSTR("wmd"),
        CFSTR("wml"),
        CFSTR("wmlc"),
        CFSTR("wmls"),
        CFSTR("wmlsc"),
        CFSTR("wrl"),
        CFSTR("xdp"),
        CFSTR("xfd"),
        CFSTR("xfdf"),
        CFSTR("xpm"),
        CFSTR("xsl"),
        CFSTR("xwd"),
        CFSTR("xyz"),
        CFSTR("z")
    };

    CFStringRef values[] = {
        CFSTR("application/postscript"),
        CFSTR("text/plain"),
        CFSTR("application/x-bcpio"),
        CFSTR("image/bmp"),
        CFSTR("application/x-netcdf"),
        CFSTR("application/octet-stream"),
        CFSTR("application/x-gzip"),
        CFSTR("application/x-cpio"),
        CFSTR("application/mac-compactpro"),
        CFSTR("application/x-csh"),
        CFSTR("text/css"),
        CFSTR("application/x-director"),
        CFSTR("application/x-director"),
        CFSTR("application/x-diskcopy"),
        CFSTR("application/octet-stream"),
        CFSTR("application/x-dvi"),
        CFSTR("application/x-director"),
        CFSTR("application/postscript"),
        CFSTR("text/x-setext"),
        CFSTR("application/andrew-inset"),
        CFSTR("application/vnd.fdf"),
        CFSTR("application/octet-stream"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-filemaker"),
        CFSTR("application/x-hdf"),
        CFSTR("x-conference/x-cooltalk"),
        CFSTR("image/x-icon"),
        CFSTR("text/calendar"),
        CFSTR("image/ief"),
        CFSTR("model/iges"),
        CFSTR("model/iges"),
        CFSTR("application/octet-stream"),
        CFSTR("text/html"),
        CFSTR("application/x-latex"),
        CFSTR("application/octet-stream"),
        CFSTR("application/octet-stream"),
        CFSTR("audio/x-mpegurl"),
        CFSTR("audio/x-m4p"),
        CFSTR("image/x-macpaint"),
        CFSTR("application/x-troff-man"),
        CFSTR("application/x-troff-me"),
        CFSTR("model/mesh"),
        CFSTR("application/vnd.mif"),
        CFSTR("video/x-sgi-movie"),
        CFSTR("audio/mpeg"),
        CFSTR("audio/mpeg"),
        CFSTR("application/x-troff-ms"),
        CFSTR("model/mesh"),
        CFSTR("video/vnd.mpegurl"),
        CFSTR("application/x-netcdf"),
        CFSTR("application/oda"),
        CFSTR("image/x-portable-bitmap"),
        CFSTR("image/x-pcx"),
        CFSTR("chemical/x-pdb"),
        CFSTR("image/x-portable-graymap"),
        CFSTR("application/x-chess-pgn"),
        CFSTR("audio/scpls"),
        CFSTR("image/x-portable-anymap"),
        CFSTR("image/x-macpaint"),
        CFSTR("image/x-macpaint"),
        CFSTR("image/x-portable-pixmap"),
        CFSTR("image/x-cmu-raster"),
        CFSTR("image/x-rgb"),
        CFSTR("application/x-troff"),
        CFSTR("audio/x-pn-realaudio-plugin"),
        CFSTR("text/richtext"),
        CFSTR("text/sgml"),
        CFSTR("text/sgml"),
        CFSTR("application/x-sh"),
        CFSTR("application/x-shar"),
        CFSTR("model/mesh"),
        CFSTR("application/x-koan"),
        CFSTR("application/x-koan"),
        CFSTR("application/x-koan"),
        CFSTR("application/x-koan"),
        CFSTR("application/x-diskcopy"),
        CFSTR("application/octet-stream"),
        CFSTR("application/x-futuresplash"),
        CFSTR("application/x-wais-source"),
        CFSTR("application/x-sv4cpio"),
        CFSTR("application/x-sv4crc"),
        CFSTR("application/x-shockwave-flash"),
        CFSTR("application/x-troff"),
        CFSTR("image/x-targa"),
        CFSTR("application/x-tcl"),
        CFSTR("application/x-tex"),
        CFSTR("application/x-texinfo"),
        CFSTR("application/x-texinfo"),
        CFSTR("application/x-gzip"),
        CFSTR("application/x-bittorrent"),
        CFSTR("application/x-troff"),
        CFSTR("text/tab-separated-values"),
        CFSTR("application/x-ustar"),
        CFSTR("application/x-cdlink"),
        CFSTR("model/vrml"),
        CFSTR("image/vnd.wap.wbmp"),
        CFSTR("application/vnd.wap.wbxml"),
        CFSTR("application/x-webarchive"),
        CFSTR("application/x-ms-wmd"),
        CFSTR("text/vnd.wap.wml"),
        CFSTR("application/vnd.wap.wmlc"),
        CFSTR("text/vnd.wap.wmlscript"),
        CFSTR("application/vnd.wap.wmlscriptc"),
        CFSTR("model/vrml"),
        CFSTR("application/vnd.adobe.xdp+xml"),
        CFSTR("application/vnd.adobe.xfd+xml"),
        CFSTR("application/vnd.adobe.xfdf"),
        CFSTR("image/x-xpixmap"),
        CFSTR("text/xml"),
        CFSTR("image/x-xwindowdump"),
        CFSTR("chemical/x-xyz"),
        CFSTR("application/x-compress")
    };

    ASSERT(sizeof(keys) == sizeof(values));
    return CFDictionaryCreate(kCFAllocatorDefault, (const void**)&keys, (const void**)&values, sizeof(keys)/sizeof(CFStringRef), &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
}

static RetainPtr<CFStringRef> mimeTypeFromUTITree(CFStringRef uti)
{
    // Check if this UTI has a MIME type.
    RetainPtr<CFStringRef> mimeType(AdoptCF, UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
    if (mimeType)
        return mimeType.get();
    
    // If not, walk the ancestory of this UTI via its "ConformsTo" tags and return the first MIME type we find.
    RetainPtr<CFDictionaryRef> decl(AdoptCF, UTTypeCopyDeclaration(uti));
    if (!decl)
        return nil;
    CFTypeRef value = CFDictionaryGetValue(decl.get(), kUTTypeConformsToKey);
    if (!value)
        return nil;
    CFTypeID typeID = CFGetTypeID(value);
    
    if (typeID == CFStringGetTypeID())
        return mimeTypeFromUTITree((CFStringRef)value);

    if (typeID == CFArrayGetTypeID()) {
        CFArrayRef newTypes = (CFArrayRef)value;
        CFIndex count = CFArrayGetCount(newTypes);
        for (CFIndex i = 0; i < count; ++i) {
            CFTypeRef object = CFArrayGetValueAtIndex(newTypes, i);
            if (CFGetTypeID(object) != CFStringGetTypeID())
                continue;

            if (RetainPtr<CFStringRef> mimeType = mimeTypeFromUTITree((CFStringRef)object))
                return mimeType;
        }
    }
    
    return nil;
}

void adjustMIMETypeIfNecessary(CFURLResponseRef cfResponse)
{
    RetainPtr<CFStringRef> result = wkGetCFURLResponseMIMEType(cfResponse);
    RetainPtr<CFStringRef> originalResult = result;

    if (!result) {
        CFURLRef url = wkGetCFURLResponseURL(cfResponse);
        NSURL *nsURL = (NSURL *)url;
        if ([nsURL isFileURL]) {
            RetainPtr<CFStringRef> extension(AdoptCF, CFURLCopyPathExtension(url));
            if (extension) {
                // <rdar://problem/7007389> CoreTypes UTI map is missing 100+ file extensions that GateKeeper knew about
                // When this radar is resolved, we can remove this file:// url specific code.
                static CFDictionaryRef extensionMap = createExtensionToMIMETypeMap();
                CFMutableStringRef mutableExtension = CFStringCreateMutableCopy(kCFAllocatorDefault, 0, extension.get());
                CFStringLowercase(mutableExtension, NULL);
                extension.adoptCF(mutableExtension);
                result = (CFStringRef) CFDictionaryGetValue(extensionMap, extension.get());
                
                if (!result) {
                    // If the Gatekeeper-based map doesn't have a MIME type, we'll try to figure out what it should be by
                    // looking up the file extension in the UTI maps.
                    RetainPtr<CFStringRef> uti(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, extension.get(), 0));
                    result = mimeTypeFromUTITree(uti.get());
                }
            }
        }
    }
    
    if (!result) {
        static CFStringRef defaultMIMETypeString = WebCore::defaultMIMEType().createCFString();
        result = defaultMIMETypeString;
    }

    // <rdar://problem/5321972> Plain text document from HTTP server detected as application/octet-stream
    // Make the best guess when deciding between "generic binary" and "generic text" using a table of known binary MIME types.
    if (CFStringCompare(result.get(), CFSTR("application/octet-stream"), 0) == kCFCompareEqualTo) {
        CFHTTPMessageRef message = wkGetCFURLResponseHTTPResponse(cfResponse);
        if (message) {
            RetainPtr<CFStringRef> contentType(AdoptCF, CFHTTPMessageCopyHeaderFieldValue(message, CFSTR("Content-Type")));
            if (contentType && CFStringHasPrefix(contentType.get(), CFSTR("text/plain"))) {
                static CFSetRef binaryExtensions = createBinaryExtensionsSet();
                RetainPtr<NSString> suggestedFilename(AdoptNS, (NSString *)wkCopyCFURLResponseSuggestedFilename(cfResponse));
                if (!CFSetContainsValue(binaryExtensions, (CFStringRef) [[suggestedFilename.get() pathExtension] lowercaseString]))
                    result = CFSTR("text/plain");
            }
        }
    }

#ifdef BUILDING_ON_LEOPARD
    // Workaround for <rdar://problem/5539824>
    if (CFStringCompare(result.get(), CFSTR("text/xml"), 0) == kCFCompareEqualTo)
        result = CFSTR("application/xml");
#endif

    if (result != originalResult)
        wkSetCFURLResponseMIMEType(cfResponse, result.get());
}

}
