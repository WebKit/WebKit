//
//  WebNSPasteboardExtras.m
//  WebKit
//
//  Created by John Sullivan on Thu Sep 19 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebKit/WebNSPasteboardExtras.h>

#import <WebKit/WebArchive.h>
#import <WebKit/WebAssertions.h>
#import <WebKit/WebImageRenderer.h>
#import <WebKit/WebImageRendererFactory.h>
#import <WebKit/WebNSURLExtras.h>
#import <WebKit/WebResourcePrivate.h>
#import <WebKit/WebURLsWithTitles.h>
#import <WebKit/WebViewPrivate.h>

#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURL_NSURLExtras.h>

#import <HIServices/CoreTranslationFlavorTypeNames.h>

NSString *WebURLPboardType = nil;
NSString *WebURLNamePboardType = nil;

@implementation NSPasteboard (WebExtras)

+ (void)initialize
{
    // FIXME  The code below addresses 3446192.  However we can't enable until 3446669 has been fixed.
#ifdef UTI_PB_API
    CFStringRef osTypeString = UTCreateStringForOSType('url ');
    CFStringRef utiTypeString = UTTypeCreatePreferredIdentifierForTag( kUTTagClassOSType, osTypeString, NULL );
    WebURLPboardType = (NSString *)UTTypeCopyPreferredTagWithClass( kUTTagClassNSPboardType, utiTypeString );
    
    osTypeString = UTCreateStringForOSType('urln');
    utiTypeString = UTTypeCreatePreferredIdentifierForTag( kUTTagClassOSType, osTypeString, NULL );
    WebURLNamePboardType = (NSString *)UTTypeCopyPreferredTagWithClass( kUTTagClassNSPboardType, utiTypeString );
#else
    CreatePasteboardFlavorTypeName('url ', (CFStringRef*)&WebURLPboardType);
    CreatePasteboardFlavorTypeName('urln', (CFStringRef*)&WebURLNamePboardType);
#endif
}

+ (NSArray *)_web_writableDragTypesForURL
{
    return [NSArray arrayWithObjects:
        WebURLsWithTitlesPboardType,
        NSURLPboardType,
        WebURLPboardType,
        WebURLNamePboardType,
        NSStringPboardType,
        nil];
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

-(NSURL *)_web_bestURL
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
        if ([URLString _web_looksLikeAbsoluteURL]) {
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

- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title withOwner:(id)owner types:(NSArray *)types
{
    ASSERT(URL);
    ASSERT(types);
    
    [self declareTypes:types owner:owner];

    if(!title || [title isEqualToString:@""]){
        title = [[URL path] lastPathComponent];
        if(!title || [title isEqualToString:@""]){
            title = [URL _web_userVisibleString];
        }
    }
    
    [URL writeToPasteboard:self];
    [self setString:title forType:WebURLNamePboardType];
    [self setString:[URL _web_userVisibleString] forType:WebURLPboardType];
    [self setString:[URL _web_userVisibleString] forType:NSStringPboardType];
    [WebURLsWithTitles writeURLs:[NSArray arrayWithObject:URL] andTitles:[NSArray arrayWithObject:title] toPasteboard:self];
}

- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title withOwner:(id)owner
{
    [self _web_writeURL:URL andTitle:title withOwner:owner types:[NSPasteboard _web_writableDragTypesForURL]];
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
{
    ASSERT(image);
    ASSERT(URL);
    
    BOOL isDrag = (self == [NSPasteboard pasteboardWithName:NSDragPboard]);
    NSMutableArray *types = [NSMutableArray arrayWithObject:NSTIFFPboardType];
    
    [types addObjectsFromArray:[NSPasteboard _web_writableDragTypesForURL]];
    
    if (archive) {
        [types addObject:NSRTFDPboardType];
        [types addObject:WebArchivePboardType];
    }
    if (isDrag) {
        [types addObject:NSFilesPromisePboardType];
    }

    [self _web_writeURL:URL andTitle:title withOwner:self types:types];
    [self setData:[image TIFFRepresentation] forType:NSTIFFPboardType];
    
    if (archive) {
        // This image data is either the only subresource of an archive (HTML image case)
        // or the main resource (standalone image case).
        WebResource *resource = [[archive subresources] objectAtIndex:0];
        if (resource == nil) {
            resource = [archive mainResource];
        }
        ASSERT([[[WebImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:[resource MIMEType]]);
        [self _web_writeFileWrapperAsRTFDAttachment:[resource _fileWrapperRepresentation]];
        [self setData:[archive data] forType:WebArchivePboardType];
    }
    if (isDrag) {
        NSString *filename = [URL _web_suggestedFilenameWithMIMEType:[image MIMEType]];
        NSString *fileType = [filename pathExtension];
        if (!fileType) {
            fileType = @"";
        }
        [self setPropertyList:[NSArray arrayWithObject:fileType] forType:NSFilesPromisePboardType];
    }
}

@end
