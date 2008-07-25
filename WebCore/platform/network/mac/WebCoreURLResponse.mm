/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#ifndef BUILDING_ON_TIGER
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

@implementation NSURLResponse (WebCoreURLResponse)

- (NSString *)_webcore_MIMEType
{
    NSString *MIMEType = [self MIMEType];
#ifdef BUILDING_ON_LEOPARD
    // Workaround for <rdar://problem/5539824>
    if ([MIMEType isEqualToString:@"text/xml"])
        return @"application/xml";
#endif
    return MIMEType;
}

@end

@implementation NSHTTPURLResponse (WebCoreURLResponse)

- (NSString *)_webcore_MIMEType
{
    NSString *MIMEType = [self MIMEType];
#ifndef BUILDING_ON_TIGER
    if ([MIMEType isEqualToString:@"application/octet-stream"] && [[[self allHeaderFields] objectForKey:@"Content-Type"] hasPrefix:@"text/plain"]) {
        static NSSet *binaryExtensions = createBinaryExtensionsSet();
        return [binaryExtensions containsObject:[[[self suggestedFilename] pathExtension] lowercaseString]] ? MIMEType : @"text/plain";
    }
#endif
    return MIMEType;
}

@end
