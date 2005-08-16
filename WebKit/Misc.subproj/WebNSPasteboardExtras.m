/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import <WebKit/WebNSPasteboardExtras.h>

#import <WebKit/WebArchive.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebURLsWithTitles.h>
#import <WebKit/WebViewPrivate.h>
#import <WebKitSystemInterface.h>

NSString *WebURLPboardType = nil;
NSString *WebURLNamePboardType = nil;

@interface NSFilePromiseDragSource : NSObject
- initWithSource:(id)draggingSource;
- (void)setTypes:(NSArray *)types onPasteboard:(NSPasteboard *)pboard;
@end

@implementation NSPasteboard (WebExtras)

+ (void)initialize
{
    // FIXME  The code below addresses 3446192.  It was awaiting a fix for 3446669. Now that bug has been fixed,
    // but this code still does not work; UTTypeCopyPreferredTagWithClass returns nil, which caused 4145214. Some
    // day we'll need to investigate why this code is still not working.
#ifdef UTI_PB_API
    CFStringRef osTypeString = UTCreateStringForOSType('url ');
    CFStringRef utiTypeString = UTTypeCreatePreferredIdentifierForTag( kUTTagClassOSType, osTypeString, NULL );
    WebURLPboardType = (NSString *)UTTypeCopyPreferredTagWithClass( kUTTagClassNSPboardType, utiTypeString );
    if (osTypeString != NULL) {
        CFRelease(osTypeString);
    }
    if (utiTypeString != NULL) {
        CFRelease(utiTypeString);
    }
    
    osTypeString = UTCreateStringForOSType('urln');
    utiTypeString = UTTypeCreatePreferredIdentifierForTag( kUTTagClassOSType, osTypeString, NULL );
    WebURLNamePboardType = (NSString *)UTTypeCopyPreferredTagWithClass( kUTTagClassNSPboardType, utiTypeString );
    if (osTypeString != NULL) {
        CFRelease(osTypeString);
    }
    if (utiTypeString != NULL) {
        CFRelease(utiTypeString);
    }
#else
    WebURLPboardType = WKCreateURLPasteboardFlavorTypeName();
    WebURLNamePboardType = WKCreateURLNPasteboardFlavorTypeName();
#endif
}

+ (NSArray *)_web_writableTypesForURL
{
    static NSArray *types = nil;
    if (!types) {
        types = [[NSArray alloc] initWithObjects:
            WebURLsWithTitlesPboardType,
            NSURLPboardType,
            WebURLPboardType,
            WebURLNamePboardType,
            NSStringPboardType,
            nil];
    }
    return types;
}

static NSArray *_writableTypesForImageWithoutArchive (void)
{
    static NSMutableArray *types = nil;
    if (types == nil) {
        types = [[NSMutableArray alloc] initWithObjects:NSTIFFPboardType, nil];
        [types addObjectsFromArray:[NSPasteboard _web_writableTypesForURL]];
    }
    return types;
}

static NSArray *_writableTypesForImageWithArchive (void)
{
    static NSMutableArray *types = nil;
    if (types == nil) {
        types = [_writableTypesForImageWithoutArchive() mutableCopy];
        [types addObject:NSRTFDPboardType];
        [types addObject:WebArchivePboardType];
    }
    return types;
}

+ (NSArray *)_web_writableTypesForImageIncludingArchive:(BOOL)hasArchive
{
    return hasArchive 
        ? _writableTypesForImageWithArchive()
        : _writableTypesForImageWithoutArchive();
}

+ (NSArray *)_web_dragTypesForURL
{
    return [NSArray arrayWithObjects:
        WebURLsWithTitlesPboardType,
        NSURLPboardType,
        WebURLPboardType,
        WebURLNamePboardType,
        NSStringPboardType,
        NSFilenamesPboardType,
        nil];
}

- (NSURL *)_web_bestURL
{
    NSArray *types = [self types];

    if ([types containsObject:NSURLPboardType]) {
        NSURL *URLFromPasteboard = [NSURL URLFromPasteboard:self];
        NSString *scheme = [URLFromPasteboard scheme];
        if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
            return [URLFromPasteboard _webkit_canonicalize];
        }
    }

    if ([types containsObject:NSStringPboardType]) {
        NSString *URLString = [self stringForType:NSStringPboardType];
        if ([URLString _webkit_looksLikeAbsoluteURL]) {
            NSURL *URL = [[NSURL _web_URLWithUserTypedString:URLString] _webkit_canonicalize];
            if (URL) {
                return URL;
            }
        }
    }

    if ([types containsObject:NSFilenamesPboardType]) {
        NSArray *files = [self propertyListForType:NSFilenamesPboardType];
        if ([files count] == 1) {
            NSString *file = [files objectAtIndex:0];
            BOOL isDirectory;
            if([[NSFileManager defaultManager] fileExistsAtPath:file isDirectory:&isDirectory] && !isDirectory){
                if ([WebView canShowFile:file]) {
                    return [[NSURL fileURLWithPath:file] _webkit_canonicalize];
                }
            }
        }
    }

    return nil;
}

- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title types:(NSArray *)types
{
    ASSERT(URL);
    
    if ([title length] == 0) {
        title = [[URL path] lastPathComponent];
        if ([title length] == 0) {
            title = [URL _web_userVisibleString];
        }
    }
    
    if ([types containsObject:NSURLPboardType]) {
        [URL writeToPasteboard:self];
    }
    if ([types containsObject:WebURLPboardType]) {
        [self setString:[URL _web_userVisibleString] forType:WebURLPboardType];
    }
    if ([types containsObject:WebURLNamePboardType]) {
        [self setString:title forType:WebURLNamePboardType];
    }
    if ([types containsObject:NSStringPboardType]) {
        [self setString:[URL _web_userVisibleString] forType:NSStringPboardType];
    }
    if ([types containsObject:WebURLsWithTitlesPboardType]) {
        [WebURLsWithTitles writeURLs:[NSArray arrayWithObject:URL] andTitles:[NSArray arrayWithObject:title] toPasteboard:self];
    }
}

+ (int)_web_setFindPasteboardString:(NSString *)string withOwner:(id)owner
{
    NSPasteboard *findPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    [findPasteboard declareTypes:[NSArray arrayWithObject:NSStringPboardType] owner:owner];
    [findPasteboard setString:string forType:NSStringPboardType];
    return [findPasteboard changeCount];
}

- (void)_web_writeFileWrapperAsRTFDAttachment:(NSFileWrapper *)wrapper
{
    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
    
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment];
    [attachment release];
    
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:nil];
    [self setData:RTFDData forType:NSRTFDPboardType];
}

- (void)_web_writeImage:(WebImageRenderer *)image 
                    URL:(NSURL *)URL 
                  title:(NSString *)title
                archive:(WebArchive *)archive
                  types:(NSArray *)types
{
    ASSERT(image);
    ASSERT(URL);

    [self _web_writeURL:URL andTitle:title types:types];
    
    if ([types containsObject:NSTIFFPboardType]) {
        [self setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
    }
    
    if (archive) {
        if ([types containsObject:NSRTFDPboardType]) {
            // This image data is either the only subresource of an archive (HTML image case)
            // or the main resource (standalone image case).
            NSArray *subresources = [archive subresources];
            WebResource *resource = [subresources count] > 0 ? (WebResource *)[subresources objectAtIndex:0] : [archive mainResource];
            ASSERT(resource != nil);
            
            ASSERT([[[WebImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:[resource MIMEType]]);
            [self _web_writeFileWrapperAsRTFDAttachment:[resource _fileWrapperRepresentation]];
        }
        if ([types containsObject:WebArchivePboardType]) {
            [self setData:[archive data] forType:WebArchivePboardType];
        }
    } else {
        // We should not have declared types that we aren't going to write (4031826).
        ASSERT(![types containsObject:NSRTFDPboardType]);
        ASSERT(![types containsObject:WebArchivePboardType]);
    }
}

- (id)_web_declareAndWriteDragImage:(WebImageRenderer *)image 
                                URL:(NSURL *)URL 
                              title:(NSString *)title
                            archive:(WebArchive *)archive
                             source:(id)source
{
    ASSERT(self == [NSPasteboard pasteboardWithName:NSDragPboard]);
    NSMutableArray *types = [[NSMutableArray alloc] initWithObjects:NSFilesPromisePboardType, nil];
    [types addObjectsFromArray:[NSPasteboard _web_writableTypesForImageIncludingArchive:(archive != nil)]];
    [self declareTypes:types owner:source];    
    [self _web_writeImage:image URL:URL title:title archive:archive types:types];
    [types release];
    
    NSString *extension = WKGetPreferredExtensionForMIMEType([image MIMEType]);
    if (extension == nil) {
        extension = @"";
    }
    NSArray *extensions = [NSArray arrayWithObject:extension];
    
    [self setPropertyList:extensions forType:NSFilesPromisePboardType];
    return source;
}

@end
