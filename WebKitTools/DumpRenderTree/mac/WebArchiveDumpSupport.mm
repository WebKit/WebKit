/*
 * Copyright (C) 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
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

#import "WebArchiveDumpSupport.h"

#import <Foundation/Foundation.h>
#import <WebKit/WebArchive.h>
#import <WebKit/WebHTMLRepresentationInternal.h>

static void convertMIMEType(NSMutableString *mimeType)
{
#ifdef BUILDING_ON_LEOPARD
    // Workaround for <rdar://problem/5539824> on Leopard
    if ([mimeType isEqualToString:@"text/xml"])
        [mimeType setString:@"application/xml"];
#endif
    // Workaround for <rdar://problem/6234318> with Dashcode 2.0
    if ([mimeType isEqualToString:@"application/x-javascript"])
        [mimeType setString:@"text/javascript"];
}

static void convertWebResourceDataToString(NSMutableDictionary *resource)
{
    NSMutableString *mimeType = [resource objectForKey:@"WebResourceMIMEType"];
    convertMIMEType(mimeType);

    if ([mimeType hasPrefix:@"text/"] || [[WebHTMLRepresentation supportedNonImageMIMETypes] containsObject:mimeType]) {
        NSString *textEncodingName = [resource objectForKey:@"WebResourceTextEncodingName"];
        NSStringEncoding stringEncoding;
        if ([textEncodingName length] > 0)
            stringEncoding = CFStringConvertEncodingToNSStringEncoding(CFStringConvertIANACharSetNameToEncoding((CFStringRef)textEncodingName));
        else
            stringEncoding = NSUTF8StringEncoding;

        NSData *data = [resource objectForKey:@"WebResourceData"];
        NSString *dataAsString = [[NSString alloc] initWithData:data encoding:stringEncoding];
        if (dataAsString)
            [resource setObject:dataAsString forKey:@"WebResourceData"];
        [dataAsString release];
    }
}

static void normalizeHTTPResponseHeaderFields(NSMutableDictionary *fields)
{
    // Normalize headers
    if ([fields objectForKey:@"Date"])
        [fields setObject:@"Sun, 16 Nov 2008 17:00:00 GMT" forKey:@"Date"];
    if ([fields objectForKey:@"Last-Modified"])
        [fields setObject:@"Sun, 16 Nov 2008 16:55:00 GMT" forKey:@"Last-Modified"];
    if ([fields objectForKey:@"Etag"])
        [fields setObject:@"\"301925-21-45c7d72d3e780\"" forKey:@"Etag"];
    if ([fields objectForKey:@"Server"])
        [fields setObject:@"Apache/2.2.9 (Unix) mod_ssl/2.2.9 OpenSSL/0.9.7l PHP/5.2.6" forKey:@"Server"];

    // Remove headers
    if ([fields objectForKey:@"Connection"])
        [fields removeObjectForKey:@"Connection"];
    if ([fields objectForKey:@"Keep-Alive"])
        [fields removeObjectForKey:@"Keep-Alive"];
}

static void normalizeWebResourceURL(NSMutableString *webResourceURL)
{
    static int fileUrlLength = [(NSString *)@"file://" length];
    NSRange layoutTestsWebArchivePathRange = [webResourceURL rangeOfString:@"/LayoutTests/" options:NSBackwardsSearch];
    if (layoutTestsWebArchivePathRange.location == NSNotFound)
        return;
    NSRange currentWorkingDirectoryRange = NSMakeRange(fileUrlLength, layoutTestsWebArchivePathRange.location - fileUrlLength);
    [webResourceURL replaceCharactersInRange:currentWorkingDirectoryRange withString:@""];
}

static void convertWebResourceResponseToDictionary(NSMutableDictionary *propertyList)
{
    NSURLResponse *response = nil;
    NSData *responseData = [propertyList objectForKey:@"WebResourceResponse"]; // WebResourceResponseKey in WebResource.m
    if ([responseData isKindOfClass:[NSData class]]) {
        // Decode NSURLResponse
        NSKeyedUnarchiver *unarchiver = [[NSKeyedUnarchiver alloc] initForReadingWithData:responseData];
        response = [unarchiver decodeObjectForKey:@"WebResourceResponse"]; // WebResourceResponseKey in WebResource.m
        [unarchiver finishDecoding];
        [unarchiver release];
    }        

    NSMutableDictionary *responseDictionary = [[NSMutableDictionary alloc] init];

    NSMutableString *urlString = [[[response URL] description] mutableCopy];
    normalizeWebResourceURL(urlString);
    [responseDictionary setObject:urlString forKey:@"URL"];
    [urlString release];

    NSMutableString *mimeTypeString = [[response MIMEType] mutableCopy];
    convertMIMEType(mimeTypeString);
    [responseDictionary setObject:mimeTypeString forKey:@"MIMEType"];
    [mimeTypeString release];

    NSString *textEncodingName = [response textEncodingName];
    if (textEncodingName)
        [responseDictionary setObject:textEncodingName forKey:@"textEncodingName"];
    [responseDictionary setObject:[NSNumber numberWithLongLong:[response expectedContentLength]] forKey:@"expectedContentLength"];

    if ([response isKindOfClass:[NSHTTPURLResponse class]]) {
        NSHTTPURLResponse *httpResponse = (NSHTTPURLResponse *)response;

        NSMutableDictionary *allHeaderFields = [[httpResponse allHeaderFields] mutableCopy];
        normalizeHTTPResponseHeaderFields(allHeaderFields);
        [responseDictionary setObject:allHeaderFields forKey:@"allHeaderFields"];
        [allHeaderFields release];

        [responseDictionary setObject:[NSNumber numberWithInt:[httpResponse statusCode]] forKey:@"statusCode"];
    }

    [propertyList setObject:responseDictionary forKey:@"WebResourceResponse"];
    [responseDictionary release];
}

static NSInteger compareResourceURLs(id resource1, id resource2, void *context)
{
    NSString *url1 = [resource1 objectForKey:@"WebResourceURL"];
    NSString *url2 = [resource2 objectForKey:@"WebResourceURL"];
 
    return [url1 compare:url2];
}

NSString *serializeWebArchiveToXML(WebArchive *webArchive)
{
    NSString *errorString;
    NSMutableDictionary *propertyList = [NSPropertyListSerialization propertyListFromData:[webArchive data]
                                                                         mutabilityOption:NSPropertyListMutableContainersAndLeaves
                                                                                   format:NULL
                                                                         errorDescription:&errorString];
    if (!propertyList)
        return errorString;

    NSMutableArray *resources = [NSMutableArray arrayWithCapacity:1];
    [resources addObject:propertyList];

    while ([resources count]) {
        NSMutableDictionary *resourcePropertyList = [resources objectAtIndex:0];
        [resources removeObjectAtIndex:0];

        NSMutableDictionary *mainResource = [resourcePropertyList objectForKey:@"WebMainResource"];
        normalizeWebResourceURL([mainResource objectForKey:@"WebResourceURL"]);
        convertWebResourceDataToString(mainResource);

        // Add subframeArchives to list for processing
        NSMutableArray *subframeArchives = [resourcePropertyList objectForKey:@"WebSubframeArchives"]; // WebSubframeArchivesKey in WebArchive.m
        if (subframeArchives)
            [resources addObjectsFromArray:subframeArchives];

        NSMutableArray *subresources = [resourcePropertyList objectForKey:@"WebSubresources"]; // WebSubresourcesKey in WebArchive.m
        NSEnumerator *enumerator = [subresources objectEnumerator];
        NSMutableDictionary *subresourcePropertyList;
        while ((subresourcePropertyList = [enumerator nextObject])) {
            normalizeWebResourceURL([subresourcePropertyList objectForKey:@"WebResourceURL"]);
            convertWebResourceResponseToDictionary(subresourcePropertyList);
            convertWebResourceDataToString(subresourcePropertyList);
        }
        
        // Sort the subresources so they're always in a predictable order for the dump
        if (NSArray *sortedSubresources = [subresources sortedArrayUsingFunction:compareResourceURLs context:nil])
            [resourcePropertyList setObject:sortedSubresources forKey:@"WebSubresources"];
    }

    NSData *xmlData = [NSPropertyListSerialization dataFromPropertyList:propertyList
                                                                 format:NSPropertyListXMLFormat_v1_0
                                                       errorDescription:&errorString];
    if (!xmlData)
        return errorString;

    NSMutableString *string = [[[NSMutableString alloc] initWithData:xmlData encoding:NSUTF8StringEncoding] autorelease];

    // Replace "Apple Computer" with "Apple" in the DTD declaration.
    NSRange range = [string rangeOfString:@"-//Apple Computer//"];
    if (range.location != NSNotFound)
        [string replaceCharactersInRange:range withString:@"-//Apple//"];
    
    return string;
}
