//
//  WebNSURLResponseExtras.m
//  WebKit
//
//  Created by Chris Blumenberg on Thu Jan 09 2003.
//  Copyright (c) 2003 Apple Inc. All rights reserved.
//

#import <WebKit/WebNSURLResponseExtras.h>

#import <WebFoundation/WebAssertions.h>
#import <WebFoundation/WebFileTypeMappings.h>
#import <WebFoundation/WebLocalizableStrings.h>
#import <WebFoundation/WebNSStringExtras.h>

@interface NSURL (WebNSURLResponseInternalURLExtras)
- (NSString *)_web_suggestedFilenameForSavingWithMIMEType:(NSString *)MIMEType;
@end

@implementation NSURLResponse (WebNSURLResponseExtras)

- (NSString *)suggestedFilenameForSaving
{
    ASSERT([self URL]);
    
    // Get the filename from the URL and the MIME type. Always returns something valid.
    return [[self URL] _web_suggestedFilenameForSavingWithMIMEType:[self MIMEType]];
}

@end

@implementation NSHTTPURLResponse (WebNSURLResponseExtras)

- (NSString *)suggestedFilenameForSaving
{
    NSString *filename = nil;

    // Use the content disposition of the filename if present.
    NSString *contentDispositionHeader = [[self allHeaderFields] objectForKey:@"Content-Disposition"];
    filename = [contentDispositionHeader _web_fileNameFromContentDispositionHeader];
    filename = [filename _web_filenameByFixingIllegalCharacters];

    if ([filename length] == 0) {
        // Get the filename from the URL and the MIME type. Always returns something valid.
        filename = [super suggestedFilenameForSaving];
    }

    return filename;
}

@end

@implementation NSURL (WebNSURLResponseInternalURLExtras)

- (NSString *)_web_suggestedFilenameForSavingWithMIMEType:(NSString *)MIMEType
{
    // Get the filename from the URL. Try the lastPathComponent first.
    NSString *lastPathComponent = [[self path] lastPathComponent];
    NSString *filename = [lastPathComponent _web_filenameByFixingIllegalCharacters];
    NSString *extension = nil;

    if ([filename length] == 0 || [lastPathComponent isEqualToString:@"/"]) {
        // lastPathComponent is no good, try the host.
        filename = [[self host] _web_filenameByFixingIllegalCharacters];
        if ([filename length] == 0) {
            // Can't make a filename using this URL, use "unknown".
            filename = UI_STRING("unknown", "Unknown filename");
        }
    } else {
        // Save the extension for later correction. Only correct the extension of the lastPathComponent.
        // For example, if the filename ends up being the host, we wouldn't want to correct ".com" in "www.apple.com".
        extension = [filename pathExtension];
    }

    // If the type is known, check the extension and correct it if necessary.
    if (MIMEType && ![MIMEType isEqualToString:@"application/octet-stream"] && ![MIMEType isEqualToString:@"text/plain"]) {
        WebFileTypeMappings *mappings = [WebFileTypeMappings sharedMappings];
        NSArray *extensions = [mappings extensionsForMIMEType:MIMEType];

        if (![extension length] || (extensions && ![extensions containsObject:extension])) {
            // The extension doesn't match the MIME type. Correct this.
            NSString *correctExtension = [mappings preferredExtensionForMIMEType:MIMEType];
            if ([correctExtension length] != 0) {
                if ([extension length] != 0) {
                    // Remove the incorrect extension.
                    filename = [filename stringByDeletingPathExtension];
                }

                // Append the correct extension.
                filename = [filename stringByAppendingPathExtension:correctExtension];
            }
        }
    }

    return filename;
}

@end