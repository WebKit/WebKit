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

#import "FoundationExtras.h"
#import "MIMETypeRegistry.h"
#import <objc/objc-class.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>

#ifndef BUILDING_ON_TIGER
// <rdar://problem/5321972> Plain text document from HTTP server detected as application/octet-stream
// When we sniff a resource as application/octet-stream but the http response headers had "text/plain",
// we have a hard decision to make about which of the two generic MIME types to go with.
// When the URL's extension is a known binary type, we'll go with application/octet-stream.
// Otherwise, we'll trust the server.
static NSSet *createBinaryExtensionsSet()
{
    return [[NSSet alloc] initWithObjects:
        @"3g2",
        @"3gp",
        @"ai",
        @"aif",
        @"aifc",
        @"aiff",
        @"au",
        @"avi",
        @"bcpio",
        @"bin",
        @"bmp",
        @"boz",
        @"bpk",
        @"bz",
        @"bz2",
        @"chm",
        @"class",
        @"com",
        @"cpio",
        @"dcr",
        @"dir",
        @"dist",
        @"distz",
        @"dll",
        @"dmg",
        @"dms",
        @"doc",
        @"dot",
        @"dump",
        @"dv",
        @"dvi",
        @"dxr",
        @"elc",
        @"eot",
        @"eps",
        @"exe",
        @"fgd",
        @"gif",
        @"gtar",
        @"h261",
        @"h263",
        @"h264",
        @"ico",
        @"ims",
        @"indd",
        @"iso",
        @"jp2",
        @"jpe",
        @"jpeg",
        @"jpg",
        @"jpgm",
        @"jpgv",
        @"jpm",
        @"kar",
        @"kmz",
        @"lha",
        @"lrm",
        @"lzh",
        @"m1v",
        @"m2a",
        @"m2v",
        @"m3a",
        @"m3u",
        @"m4a",
        @"m4p",
        @"m4v",
        @"mdb",
        @"mid",
        @"midi",
        @"mj2",
        @"mjp2",
        @"mov",
        @"movie",
        @"mp2",
        @"mp2a",
        @"mp3",
        @"mp4",
        @"mp4a",
        @"mp4s",
        @"mp4v",
        @"mpe",
        @"mpeg",
        @"mpg",
        @"mpg4",
        @"mpga",
        @"mpp",
        @"mpt",
        @"msi",
        @"ogg",
        @"otf",
        @"pct",
        @"pdf",
        @"pfa",
        @"pfb",
        @"pic",
        @"pict",
        @"pkg",
        @"png",
        @"pot",
        @"pps",
        @"ppt",
        @"ps",
        @"psd",
        @"qt",
        @"qti",
        @"qtif",
        @"qwd",
        @"qwt",
        @"qxb",
        @"qxd",
        @"qxl",
        @"qxp",
        @"qxt",
        @"ra",
        @"ram",
        @"rm",
        @"rmi",
        @"rmp",
        @"scpt",
        @"sit",
        @"sitx",
        @"snd",
        @"so",
        @"swf",
        @"tar",
        @"tif",
        @"tiff",
        @"ttf",
        @"wav",
        @"wcm",
        @"wdb",
        @"wks",
        @"wm",
        @"wma",
        @"wmd",
        @"wmf",
        @"wmv",
        @"wmx",
        @"wmz",
        @"wpd",
        @"wpl",
        @"wps",
        @"wvx",
        @"xla",
        @"xlc",
        @"xlm",
        @"xls",
        @"xlt",
        @"xlw",
        @"xps",
        @"zip",
        nil
    ];
}
#endif

// <rdar://problem/7007389> CoreTypes UTI map is missing 100+ file extensions that GateKeeper knew about
// When we disabled content sniffing for file URLs we caused problems with these 100+ extensions that CoreTypes
// doesn't know about.
// If CoreTypes is ever brought up to speed we can remove this table and associated code.
static NSDictionary *createExtensionToMIMETypeMap()
{
    return [[NSDictionary alloc] initWithObjectsAndKeys:
        @"application/postscript", @"ai",
        @"text/plain", @"asc",
        @"application/x-bcpio", @"bcpio",
        @"image/bmp", @"bmp",
        @"application/x-netcdf", @"cdf",
        @"application/octet-stream", @"class",
        @"application/x-gzip", @"cpgz",
        @"application/x-cpio", @"cpio",
        @"application/mac-compactpro", @"cpt",
        @"application/x-csh", @"csh",
        @"text/css", @"css",
        @"application/x-director", @"dcr",
        @"application/x-director", @"dir",
        @"application/x-diskcopy", @"dmg",
        @"application/octet-stream", @"dms",
        @"application/x-dvi", @"dvi",
        @"application/x-director", @"dxr",
        @"application/postscript", @"eps",
        @"text/x-setext", @"etx",
        @"application/andrew-inset", @"ez",
        @"application/vnd.fdf", @"fdf",
        @"application/octet-stream", @"fla",
        @"application/x-filemaker", @"fp",
        @"application/x-filemaker", @"fp2",
        @"application/x-filemaker", @"fp3",
        @"application/x-filemaker", @"fp4",
        @"application/x-filemaker", @"fp5",
        @"application/x-filemaker", @"fp6",
        @"application/x-hdf", @"hdf",
        @"x-conference/x-cooltalk", @"ice",
        @"image/x-icon", @"ico",
        @"text/calendar", @"ics",
        @"image/ief", @"ief",
        @"model/iges", @"iges",
        @"model/iges", @"igs",
        @"application/octet-stream", @"iso",
        @"text/html", @"jhtml",
        @"application/x-latex", @"latex",
        @"application/octet-stream", @"lha",
        @"application/octet-stream", @"lzh",
        @"audio/x-mpegurl", @"m3u",
        @"audio/x-m4p", @"m4p",
        @"image/x-macpaint", @"mac",
        @"application/x-troff-man", @"man",
        @"application/x-troff-me", @"me",
        @"model/mesh", @"mesh",
        @"application/vnd.mif", @"mif",
        @"video/x-sgi-movie", @"movie",
        @"audio/mpeg", @"mp2",
        @"audio/mpeg", @"mpga",
        @"application/x-troff-ms", @"ms",
        @"model/mesh", @"msh",
        @"video/vnd.mpegurl", @"mxu",
        @"application/x-netcdf", @"nc",
        @"application/oda", @"oda",
        @"image/x-portable-bitmap", @"pbm",
        @"image/x-pcx", @"pcx",
        @"chemical/x-pdb", @"pdb",
        @"image/x-portable-graymap", @"pgm",
        @"application/x-chess-pgn", @"pgn",
        @"audio/scpls", @"pls",
        @"image/x-portable-anymap", @"pnm",
        @"image/x-macpaint", @"pnt",
        @"image/x-macpaint", @"pntg",
        @"image/x-portable-pixmap", @"ppm",
        @"image/x-cmu-raster", @"ras",
        @"image/x-rgb", @"rgb",
        @"application/x-troff", @"roff",
        @"audio/x-pn-realaudio-plugin", @"rpm",
        @"text/richtext", @"rtx",
        @"text/sgml", @"sgm",
        @"text/sgml", @"sgml",
        @"application/x-sh", @"sh",
        @"application/x-shar", @"shar",
        @"model/mesh", @"silo",
        @"application/x-koan", @"skd",
        @"application/x-koan", @"skm",
        @"application/x-koan", @"skp",
        @"application/x-koan", @"skt",
        @"application/x-diskcopy", @"smi",
        @"application/octet-stream", @"so",
        @"application/x-futuresplash", @"spl",
        @"application/x-wais-source", @"src",
        @"application/x-sv4cpio", @"sv4cpio",
        @"application/x-sv4crc", @"sv4crc",
        @"application/x-shockwave-flash", @"swf",
        @"application/x-troff", @"t",
        @"image/x-targa", @"targa",
        @"application/x-tcl", @"tcl",
        @"application/x-tex", @"tex",
        @"application/x-texinfo", @"texi",
        @"application/x-texinfo", @"texinfo",
        @"application/x-gzip", @"tgz",
        @"application/x-bittorrent", @"torrent",
        @"application/x-troff", @"tr",
        @"text/tab-separated-values", @"tsv",
        @"application/x-ustar", @"ustar",
        @"application/x-cdlink", @"vcd",
        @"model/vrml", @"vrml",
        @"image/vnd.wap.wbmp", @"wbmp",
        @"application/vnd.wap.wbxml", @"wbxml",
        @"application/x-webarchive", @"webarchive",
        @"application/x-ms-wmd", @"wmd",
        @"text/vnd.wap.wml", @"wml",
        @"application/vnd.wap.wmlc", @"wmlc",
        @"text/vnd.wap.wmlscript", @"wmls",
        @"application/vnd.wap.wmlscriptc", @"wmlsc",
        @"model/vrml", @"wrl",
        @"application/vnd.adobe.xdp+xml", @"xdp",
        @"application/vnd.adobe.xfd+xml", @"xfd",
        @"application/vnd.adobe.xfdf", @"xfdf",
        @"image/x-xpixmap", @"xpm",
        @"text/xml", @"xsl",
        @"image/x-xwindowdump", @"xwd",
        @"chemical/x-xyz", @"xyz",
        @"application/x-compress", @"z",
        nil
    ];
}

static NSString *mimeTypeFromUTITree(CFStringRef uti)
{
    // Check if this UTI has a MIME type.
    RetainPtr<CFStringRef> mimeType(AdoptCF, UTTypeCopyPreferredTagWithClass(uti, kUTTagClassMIMEType));
    if (mimeType)
        return (NSString *)HardAutorelease(mimeType.releaseRef());
    
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

            if (NSString *mimeType = mimeTypeFromUTITree((CFStringRef)object))
                return mimeType;
        }
    }
    
    return nil;
}

@implementation NSURLResponse (WebCoreURLResponse)

-(void)adjustMIMETypeIfNecessary
{
    NSString *result = [self MIMEType];
    NSString *originalResult = result;

#ifdef BUILDING_ON_TIGER
    // When content sniffing is disabled, Tiger's CFNetwork automatically returns application/octet-stream for certain
    // extensions even when scouring the UTI maps would end up with a better result, so we'll give a chance for that to happen.
    if ([[self URL] isFileURL] && [result caseInsensitiveCompare:@"application/octet-stream"] == NSOrderedSame)
        result = nil;
#endif

    if (!result) {
        NSURL *url = [self URL];
        if ([url isFileURL]) {
            if (NSString *extension = [[url path] pathExtension]) {
                // <rdar://problem/7007389> CoreTypes UTI map is missing 100+ file extensions that GateKeeper knew about
                // When this radar is resolved, we can remove this file:// url specific code.
                static NSDictionary *extensionMap = createExtensionToMIMETypeMap();
                result = [extensionMap objectForKey:[extension lowercaseString]];
                
                if (!result) {
                    // If the Gatekeeper-based map doesn't have a MIME type, we'll try to figure out what it should be by
                    // looking up the file extension in the UTI maps.
                    RetainPtr<CFStringRef> uti(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, (CFStringRef)extension, 0));
                    result = mimeTypeFromUTITree(uti.get());
                }
            }
        }
    }
    
    if (!result) {
        static NSString *defaultMIMETypeString = [(NSString *)WebCore::defaultMIMEType() retain];
        result = defaultMIMETypeString;
    }

#ifndef BUILDING_ON_TIGER
    // <rdar://problem/5321972> Plain text document from HTTP server detected as application/octet-stream
    // Make the best guess when deciding between "generic binary" and "generic text" using a table of known binary MIME types.
    if ([result isEqualToString:@"application/octet-stream"] && [self respondsToSelector:@selector(allHeaderFields)] && [[[self performSelector:@selector(allHeaderFields)] objectForKey:@"Content-Type"] hasPrefix:@"text/plain"]) {
        static NSSet *binaryExtensions = createBinaryExtensionsSet();
        if (![binaryExtensions containsObject:[[[self suggestedFilename] pathExtension] lowercaseString]])
            result = @"text/plain";
    }

#endif

#ifdef BUILDING_ON_LEOPARD
    // Workaround for <rdar://problem/5539824>
    if ([result isEqualToString:@"text/xml"])
        result = @"application/xml";
#endif

    if (result != originalResult)
        [self _setMIMEType:result];
}

@end
