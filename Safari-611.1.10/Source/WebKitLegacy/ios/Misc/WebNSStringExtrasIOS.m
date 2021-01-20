/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#if PLATFORM(IOS_FAMILY)

#import "WebNSStringExtrasIOS.h"
#import "WebNSURLExtras.h"
#import <pal/spi/cocoa/NSStringSPI.h>

@implementation NSString (WebNSStringExtrasIOS)

- (NSArray *)_web_possibleURLsForForUserTypedString:(BOOL)isPrefix
{
    NSMutableArray *result = [NSMutableArray array];
    
    NSMutableString *workString = [NSMutableString stringWithString:self];
    
    // Trim whitespace from ends (likely cut & paste artifact)
    CFStringTrimWhitespace((CFMutableStringRef)workString);
    
    // Strip out newlines and carriage returns (likely cut & paste artifact)
    CFStringFindAndReplace((CFMutableStringRef)workString, CFSTR("\n"), CFSTR(""), CFRangeMake (0, [workString length]), 0);
    CFStringFindAndReplace((CFMutableStringRef)workString, CFSTR("\r"), CFSTR(""), CFRangeMake (0, [workString length]), 0);
    
    if ([workString length] > 0) {
        
        // Looks like an absolute path or a ~-path
        if ([workString characterAtIndex:0] == '/' || 
            [workString characterAtIndex:0] == '~') {
            // - treat it as a pathname
            NSURL *URL = [NSURL fileURLWithPath:[workString _web_stringByExpandingTildeInPath]];
            if (URL != nil) {
                [result addObject:URL];
            }
        } else {
            NSRange firstColonRange = [workString rangeOfString:@":"];
            NSRange firstSlashRange = [workString rangeOfString:@"/"];
            NSRange firstDotRange = [workString rangeOfString:@"."];
            NSRange firstAtRange = [workString rangeOfString:@"@"];
            
            BOOL URLHasScheme = NO;
            if (firstColonRange.location != NSNotFound && (firstSlashRange.location == NSNotFound || firstSlashRange.location >= firstColonRange.location)) {
                // Do a "sanity check" on the scheme, if it is not one of the "well-known"
                // schemes we support, do some work to add some possibilities.
                // This is so typed strings like "localhost:8080" do not get
                // misinterpreted as a URL with the localhost: scheme.
                // Perhaps this could be improved to be a dynamic list of schemes
                // based on loaded protocol handlers, but I do not see the need for
                // the extra complexity.
                if ([workString _web_hasCaseInsensitivePrefix:@"http:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"https:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"file:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"ftp:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"javascript:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"about:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"radar:"] ||
                    [workString _web_hasCaseInsensitivePrefix:@"rdar:"]) {
                    URLHasScheme = YES;
                }
                else if ([workString length] > firstColonRange.location + 1) {
                    // If the first character following the first colon is not a number
                    // guess that this is not is a host:port combination, and that
                    // the string preceding the colon is a scheme we don't know about.
                    unichar c = [workString characterAtIndex:firstColonRange.location + 1];
                    if (c < '0' || c > '9') {
                        URLHasScheme = YES;
                    }
                } else {
                    // If there is no character following the colon, then assume the
                    // string preceding the colon is a scheme.
                    URLHasScheme = YES;
                }
            }
            
            if (URLHasScheme) {
                // - use it unchanged
                NSURL *URL = [NSURL _webkit_URLWithUserTypedString:workString];
                if (URL != nil) {
                    [result addObject:URL];
                }
            } else {
                NSURL *URL;
                
                // apparent hostname contains a dot and starts with "www." 
                if ([[workString lowercaseString] hasPrefix:@"www."]) {
                    // - just prepend "http://"
                    [workString insertString:@"http://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                // apparent hostname contains a dot and starts with "ftp."
                } else if ([[workString lowercaseString] hasPrefix:@"ftp."]) {
                    // - just prepend "ftp://"
                    [workString insertString:@"ftp://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                // apparent hostname contains "@"
                } else if (firstAtRange.location != NSNotFound && firstAtRange.location < firstSlashRange.location) {
                    // - justprepend "http://"
                    [workString insertString:@"http://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                // apparent hostname contains a dot and doesn't start with anything special
                } else if (firstDotRange.location != NSNotFound && firstDotRange.location < firstSlashRange.location) {
                    // - try prepending "http://"
                    [workString insertString:@"http://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                    // - try prepending "http://www."
                    [workString insertString:@"www." atIndex:strlen("http://")];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                // apparent hostname doesn't contain a dot but is equal to "localhost"
                } else if ([[workString lowercaseString] isEqualToString:@"localhost"] ||
                           [[workString lowercaseString] hasPrefix:@"localhost/"]) {
                    // - just prepend "http://"
                    [workString insertString:@"http://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                // apparent hostname doesn't contain a dot and is nothing special
                } else {
                    // - try prepending "http://"
                    [workString insertString:@"http://" atIndex:0];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                    
                    // - try prepending "http://www." and appending .com to the hostname, 
                    // but account for a port number if there is one
                    NSRange secondColonRange = [workString rangeOfString:@":" options:NSLiteralSearch range:NSMakeRange(5, [workString length] - 5)]; // 5 is length of http:
                    unsigned endOfHostnameOrPort = (secondColonRange.location != NSNotFound) ?
                        secondColonRange.location :
                        (firstSlashRange.location != NSNotFound) ?
                        strlen("http://") + firstSlashRange.location : 
                        [workString length];  
                    
                    if (!(isPrefix && endOfHostnameOrPort == [workString length])) {
                        [workString insertString:@".com" atIndex:endOfHostnameOrPort];
                    }
                    [workString insertString:@"www." atIndex:strlen("http://")];
                    URL = [NSURL _webkit_URLWithUserTypedString:workString];
                    if (URL != nil) {
                        [result addObject:URL];
                    }
                }
            }
        }
    }
    
    return result;
}


- (NSArray *)_web_possibleURLPrefixesForUserTypedString
{
    return [self _web_possibleURLsForForUserTypedString:YES];
}

- (NSArray *)_web_possibleURLsForUserTypedString
{
    return [self _web_possibleURLsForForUserTypedString:NO];
}

- (NSURL *)_web_bestURLForUserTypedString
{
    NSArray *URLs = [self _web_possibleURLsForUserTypedString];
    if ([URLs count] == 0) {
        return nil;
    }
    return [[URLs objectAtIndex:0] _webkit_canonicalize];
}

@end

#endif // PLATFORM(IOS_FAMILY)

