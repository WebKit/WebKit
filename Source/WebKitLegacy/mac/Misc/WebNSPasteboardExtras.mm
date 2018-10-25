/*
 * Copyright (C) 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#if !PLATFORM(IOS_FAMILY)

#import "WebNSPasteboardExtras.h"

#import "DOMElementInternal.h"
#import "WebArchive.h"
#import "WebFrameInternal.h"
#import "WebHTMLViewInternal.h"
#import "WebNSURLExtras.h"
#import "WebResourcePrivate.h"
#import "WebURLsWithTitles.h"
#import "WebViewPrivate.h"
#import <WebCore/CachedImage.h>
#import <WebCore/Element.h>
#import <WebCore/Image.h>
#import <WebCore/LegacyNSPasteboardTypes.h>
#import <WebCore/MIMETypeRegistry.h>
#import <WebCore/RenderAttachment.h>
#import <WebCore/RenderImage.h>
#import <WebKitLegacy/DOMExtensions.h>
#import <WebKitLegacy/DOMPrivate.h>
#import <wtf/Assertions.h>
#import <wtf/RetainPtr.h>
#import <wtf/StdLibExtras.h>

using namespace WebCore;

NSString *WebURLPboardType = @"public.url";
NSString *WebURLNamePboardType = @"public.url-name";

@implementation NSPasteboard (WebExtras)

+ (NSArray *)_web_writableTypesForURL
{
    static NSArray *types = [[NSArray alloc] initWithObjects:
        WebURLsWithTitlesPboardType,
        legacyURLPasteboardType(),
        WebURLPboardType,
        WebURLNamePboardType,
        legacyStringPasteboardType(),
        nil];
    return types;
}

static inline NSArray *_createWritableTypesForImageWithoutArchive()
{
    NSMutableArray *types = [[NSMutableArray alloc] initWithObjects:legacyTIFFPasteboardType(), nil];
    [types addObjectsFromArray:[NSPasteboard _web_writableTypesForURL]];
    return types;
}

static NSArray *_writableTypesForImageWithoutArchive (void)
{
    static NSArray *types = _createWritableTypesForImageWithoutArchive();
    return types;
}

static inline NSArray *_createWritableTypesForImageWithArchive()
{
    NSMutableArray *types = [_writableTypesForImageWithoutArchive() mutableCopy];
    [types addObject:legacyRTFDPasteboardType()];
    [types addObject:WebArchivePboardType];
    return types;
}

static NSArray *_writableTypesForImageWithArchive (void)
{
    static NSArray *types = _createWritableTypesForImageWithArchive();
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
        legacyURLPasteboardType(),
        WebURLPboardType,
        WebURLNamePboardType,
        legacyStringPasteboardType(),
        legacyFilenamesPasteboardType(),
        legacyFilesPromisePasteboardType(),
        nil];
}

- (NSURL *)_web_bestURL
{
    NSArray *types = [self types];

    if ([types containsObject:legacyURLPasteboardType()]) {
        NSURL *URLFromPasteboard = [NSURL URLFromPasteboard:self];
        NSString *scheme = [URLFromPasteboard scheme];
        if ([scheme isEqualToString:@"http"] || [scheme isEqualToString:@"https"]) {
            return [URLFromPasteboard _webkit_canonicalize];
        }
    }

    if ([types containsObject:legacyStringPasteboardType()]) {
        NSString *URLString = [self stringForType:legacyStringPasteboardType()];
        if ([URLString _webkit_looksLikeAbsoluteURL]) {
            NSURL *URL = [[NSURL _webkit_URLWithUserTypedString:URLString] _webkit_canonicalize];
            if (URL) {
                return URL;
            }
        }
    }

    if ([types containsObject:legacyFilenamesPasteboardType()]) {
        NSArray *files = [self propertyListForType:legacyFilenamesPasteboardType()];
        // FIXME: Maybe it makes more sense to allow multiple files and only use the first one?
        if ([files count] == 1) {
            NSString *file = [files objectAtIndex:0];
            // FIXME: We are filtering out directories because that's what the original code used to
            // do. Without this check, if the URL points to a local directory, Safari will open the
            // parent directory of the directory in Finder. This check should go away as soon as
            // possible.
            BOOL isDirectory;
            if ([[NSFileManager defaultManager] fileExistsAtPath:file isDirectory:&isDirectory] && isDirectory)
                return nil;
            return [[NSURL fileURLWithPath:file] _webkit_canonicalize];
        }
    }

    return nil;
}

- (void)_web_writeURL:(NSURL *)URL andTitle:(NSString *)title types:(NSArray *)types
{
    ASSERT(URL);

    if ([title length] == 0) {
        title = [[URL path] lastPathComponent];
        if ([title length] == 0)
            title = [URL _web_userVisibleString];
    }
    
    if ([types containsObject:legacyURLPasteboardType()])
        [URL writeToPasteboard:self];
    if ([types containsObject:WebURLPboardType])
        [self setString:[URL _web_originalDataAsString] forType:WebURLPboardType];
    if ([types containsObject:WebURLNamePboardType])
        [self setString:title forType:WebURLNamePboardType];
    if ([types containsObject:legacyStringPasteboardType()])
        [self setString:[URL _web_userVisibleString] forType:legacyStringPasteboardType()];
    if ([types containsObject:WebURLsWithTitlesPboardType])
        [WebURLsWithTitles writeURLs:[NSArray arrayWithObject:URL] andTitles:[NSArray arrayWithObject:title] toPasteboard:self];
}

+ (int)_web_setFindPasteboardString:(NSString *)string withOwner:(id)owner
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSPasteboard *findPasteboard = [NSPasteboard pasteboardWithName:NSFindPboard];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [findPasteboard declareTypes:[NSArray arrayWithObject:legacyStringPasteboardType()] owner:owner];
    [findPasteboard setString:string forType:legacyStringPasteboardType()];
    return [findPasteboard changeCount];
}

- (void)_web_writeFileWrapperAsRTFDAttachment:(NSFileWrapper *)wrapper
{
    NSTextAttachment *attachment = [[NSTextAttachment alloc] initWithFileWrapper:wrapper];
    
    NSAttributedString *string = [NSAttributedString attributedStringWithAttachment:attachment];
    [attachment release];
    
    NSData *RTFDData = [string RTFDFromRange:NSMakeRange(0, [string length]) documentAttributes:@{ }];
    [self setData:RTFDData forType:legacyRTFDPasteboardType()];
}


- (void)_web_writePromisedRTFDFromArchive:(WebArchive*)archive containsImage:(BOOL)containsImage
{
    ASSERT(archive);
    // This image data is either the only subresource of an archive (HTML image case)
    // or the main resource (standalone image case).
    NSArray *subresources = [archive subresources];
    WebResource *resource = [archive mainResource];
    if (containsImage && [subresources count] > 0) {
        WebResource *subresource = [subresources objectAtIndex:0];
        NSString *subresourceMIMEType = [subresource MIMEType];
        if (MIMETypeRegistry::isSupportedImageMIMEType(subresourceMIMEType) || MIMETypeRegistry::isPDFOrPostScriptMIMEType(subresourceMIMEType))
            resource = subresource;
    }
    ASSERT(resource != nil);
    
    ASSERT(!containsImage || MIMETypeRegistry::isSupportedImageMIMEType([resource MIMEType]) || MIMETypeRegistry::isPDFOrPostScriptMIMEType([resource MIMEType]));
    if (!containsImage || MIMETypeRegistry::isSupportedImageMIMEType([resource MIMEType]) || MIMETypeRegistry::isPDFOrPostScriptMIMEType([resource MIMEType]))
        [self _web_writeFileWrapperAsRTFDAttachment:[resource _fileWrapperRepresentation]];
    
}

static CachedImage* imageFromElement(DOMElement *domElement)
{
    auto* element = core(domElement);
    if (!element)
        return nullptr;
    auto* renderer = element->renderer();
    if (!is<RenderImage>(renderer))
        return nullptr;
    auto* image = downcast<RenderImage>(*renderer).cachedImage();
    if (!image || image->errorOccurred())
        return nullptr;
    return image;
}

- (void)_web_writeImage:(NSImage *)image
                element:(DOMElement *)element
                    URL:(NSURL *)URL 
                  title:(NSString *)title
                archive:(WebArchive *)archive
                  types:(NSArray *)types
                 source:(WebHTMLView *)source
{
    ASSERT(image || element);
    ASSERT(URL);

    [self _web_writeURL:URL andTitle:title types:types];
    
    if ([types containsObject:legacyTIFFPasteboardType()]) {
        if (image)
            [self setData:[image TIFFRepresentation] forType:legacyTIFFPasteboardType()];
        else if (source && element)
            [source setPromisedDragTIFFDataSource:imageFromElement(element)];
        else if (element)
            [self setData:[element _imageTIFFRepresentation] forType:legacyTIFFPasteboardType()];
    }
    
    if (archive) {
        if ([types containsObject:WebArchivePboardType])
            [self setData:[archive data] forType:WebArchivePboardType];
        return;
    }

    // We should not have declared types that we aren't going to write (4031826).
    ASSERT(![types containsObject:legacyRTFDPasteboardType()]);
    ASSERT(![types containsObject:WebArchivePboardType]);
}

- (id)_web_declareAndWriteDragImageForElement:(DOMElement *)element
                                       URL:(NSURL *)URL 
                                     title:(NSString *)title
                                   archive:(WebArchive *)archive
                                    source:(WebHTMLView *)source
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    ASSERT(self == [NSPasteboard pasteboardWithName:NSDragPboard]);
    ALLOW_DEPRECATED_DECLARATIONS_END

    NSString *extension = @"";
    RetainPtr<NSMutableArray> types = adoptNS([[NSMutableArray alloc] initWithObjects:legacyFilesPromisePasteboardType(), nil]);
    if (auto* renderer = core(element)->renderer()) {
        if (is<RenderImage>(*renderer)) {
            if (auto* image = downcast<RenderImage>(*renderer).cachedImage()) {
                // FIXME: This doesn't check errorOccured the way imageFromElement does.
                extension = image->image()->filenameExtension();
                if (![extension length])
                    return nullptr;
                [types addObjectsFromArray:[NSPasteboard _web_writableTypesForImageIncludingArchive:(archive != nil)]];
                [self declareTypes:types.get() owner:source];
            }
        }
#if ENABLE(ATTACHMENT_ELEMENT)
        else if (is<RenderAttachment>(*renderer)) {
            extension = URL.pathExtension;
            [types addObjectsFromArray:[NSPasteboard _web_dragTypesForURL]];
            [self declareTypes:types.get() owner:source];
            RetainPtr<NSMutableArray> paths = adoptNS([[NSMutableArray alloc] init]);
            [paths.get() addObject:title];
            [self setPropertyList:paths.get() forType:legacyFilenamesPasteboardType()];
        }
#endif
    }

    [self _web_writeImage:nil element:element URL:URL title:title archive:archive types:types.get() source:source];

    NSArray *extensions = [[NSArray alloc] initWithObjects:extension, nil];
    [self setPropertyList:extensions forType:legacyFilesPromisePasteboardType()];
    [extensions release];

    return source;
}

@end

#endif // !PLATFORM(IOS_FAMILY)
