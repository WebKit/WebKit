/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
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
#import "LoaderNSURLExtras.h"

#import <wtf/Assertions.h>
#import <wtf/Vector.h>
#import "KURL.h"
#import "LocalizedStrings.h"
#import "MIMETypeRegistry.h"
#import "PlatformString.h"
#import "WebCoreNSStringExtras.h"
#import "WebCoreSystemInterface.h"

using namespace WebCore;

NSURL *urlByRemovingComponent(NSURL *url, CFURLComponentType component)
{
    if (!url)
        return nil;

    CFRange fragRg = CFURLGetByteRangeForComponent((CFURLRef)url, component, NULL);
    // Check to see if a fragment exists before decomposing the URL.
    if (fragRg.location == kCFNotFound)
        return url;

    UInt8 *urlBytes, buffer[2048];
    CFIndex numBytes = CFURLGetBytes((CFURLRef)url, buffer, 2048);
    if (numBytes == -1) {
        numBytes = CFURLGetBytes((CFURLRef)url, NULL, 0);
        urlBytes = static_cast<UInt8*>(malloc(numBytes));
        CFURLGetBytes((CFURLRef)url, urlBytes, numBytes);
    } else
        urlBytes = buffer;
    
    NSURL *result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, fragRg.location - 1, kCFStringEncodingUTF8, NULL));
    if (!result)
        result = (NSURL *)CFMakeCollectable(CFURLCreateWithBytes(NULL, urlBytes, fragRg.location - 1, kCFStringEncodingISOLatin1, NULL));
    
    if (urlBytes != buffer) free(urlBytes);
    return result ? [result autorelease] : url;
}

NSURL *urlByRemovingFragment(NSURL *url)
{
    return urlByRemovingComponent(url, kCFURLComponentFragment);
}

NSString *urlOriginalDataAsString(NSURL *url)
{
    return [[[NSString alloc] initWithData:urlOriginalData(url) encoding:NSISOLatin1StringEncoding] autorelease];
}

#define URL_BYTES_BUFFER_LENGTH 2048

NSData *urlOriginalData(NSURL *url)
{
    if (!url)
        return nil;

    UInt8 *buffer = (UInt8 *)malloc(URL_BYTES_BUFFER_LENGTH);
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)url, buffer, URL_BYTES_BUFFER_LENGTH);
    if (bytesFilled == -1) {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)url, NULL, 0);
        buffer = (UInt8 *)realloc(buffer, bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)url, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
    }
    
    // buffer is adopted by the NSData
    NSData *data = [NSData dataWithBytesNoCopy:buffer length:bytesFilled freeWhenDone:YES];
    
    NSURL *baseURL = (NSURL *)CFURLGetBaseURL((CFURLRef)url);
    if (baseURL)
        return urlOriginalData(urlWithDataRelativeToURL(data, baseURL));
    return data;
}

NSURL *urlWithData(NSData *data)
{
    if (data == nil)
        return nil;

    return urlWithDataRelativeToURL(data, nil);
}      

static inline id WebCFAutorelease(CFTypeRef obj)
{
    if (obj)
        CFMakeCollectable(obj);
    [(id)obj autorelease];
    return (id)obj;
}

NSURL *urlWithDataRelativeToURL(NSData *data, NSURL *baseURL)
{
    if (data == nil)
        return nil;
    
    NSURL *result = nil;
    size_t length = [data length];
    if (length > 0) {
        // work around <rdar://4470771>: CFURLCreateAbsoluteURLWithBytes(.., TRUE) doesn't remove non-path components.
        baseURL = urlByRemovingResourceSpecifier(baseURL);
        
        const UInt8 *bytes = static_cast<const UInt8*>([data bytes]);
        // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
        // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
        // could either be a malformed string or bytes in a different encoding, like shift-jis, so we fall back
        // onto using ISO Latin 1 in those cases.
        result = WebCFAutorelease(CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingUTF8, (CFURLRef)baseURL, YES));
        if (!result)
            result = WebCFAutorelease(CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingISOLatin1, (CFURLRef)baseURL, YES));
    } else
        result = [NSURL URLWithString:@""];

    return result;
}

NSURL *urlByRemovingResourceSpecifier(NSURL *url)
{
    return urlByRemovingComponent(url, kCFURLComponentResourceSpecifier);
}


BOOL urlIsFileURL(NSURL *url)
{
    if (!url)
        return false;

    return stringIsFileURL(urlOriginalDataAsString(url));
}

BOOL stringIsFileURL(NSString *urlString)
{
    return [urlString rangeOfString:@"file:" options:(NSCaseInsensitiveSearch | NSAnchoredSearch)].location != NSNotFound;
}

BOOL urlIsEmpty(NSURL *url)
{
    if (!url)
        return false;
    if (!CFURLGetBaseURL((CFURLRef)url))
        return CFURLGetBytes((CFURLRef)url, NULL, 0) == 0;
    return [urlOriginalData(url) length] == 0;
}

NSURL *canonicalURL(NSURL *url)
{
    if (!url)
        return nil;
    
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:url];
    Class concreteClass = wkNSURLProtocolClassForReqest(request);
    if (!concreteClass) {
        [request release];
        return url;
    }
    
    // This applies NSURL's concept of canonicalization, but not KURL's concept. It would
    // make sense to apply both, but when we tried that it caused a performance degradation
    // (see 5315926). It might make sense to apply only the KURL concept and not the NSURL
    // concept, but it's too risky to make that change for WebKit 3.0.
    NSURLRequest *newRequest = [concreteClass canonicalRequestForRequest:request];
    NSURL *newURL = [newRequest URL]; 
    NSURL *result = [[newURL retain] autorelease]; 
    [request release];
    
    return result;
}

static bool vectorContainsString(Vector<String> vector, String string)
{
    int size = vector.size();
    for (int i = 0; i < size; i++)
        if (vector[i] == string)
            return true;
    return false;
}

NSString *suggestedFilenameWithMIMEType(NSURL *url, NSString *MIMEType)
{
    // Get the filename from the URL. Try the lastPathComponent first.
    NSString *lastPathComponent = [[url path] lastPathComponent];
    NSString *filename = filenameByFixingIllegalCharacters(lastPathComponent);
    NSString *extension = nil;

    if ([filename length] == 0 || [lastPathComponent isEqualToString:@"/"]) {
        // lastPathComponent is no good, try the host.
        NSString *host = (NSString *)(KURL(url).host());
        filename = filenameByFixingIllegalCharacters(host);
        if ([filename length] == 0) {
            // Can't make a filename using this URL, use "unknown".
            filename = copyImageUnknownFileLabel();
        }
    } else {
        // Save the extension for later correction. Only correct the extension of the lastPathComponent.
        // For example, if the filename ends up being the host, we wouldn't want to correct ".com" in "www.apple.com".
        extension = [filename pathExtension];
    }

    // No mime type reported. Just return the filename we have now.
    if (!MIMEType) {
        return filename;
    }

    // Do not correct filenames that are reported with a mime type of tar, and 
    // have a filename which has .tar in it or ends in .tgz
    if (([MIMEType isEqualToString:@"application/tar"] || [MIMEType isEqualToString:@"application/x-tar"]) 
        && (hasCaseInsensitiveSubstring(filename, @".tar")
        || hasCaseInsensitiveSuffix(filename, @".tgz"))) {
        return filename;
    }

    // I don't think we need to worry about this for the image case
    // If the type is known, check the extension and correct it if necessary.
    if (![MIMEType isEqualToString:@"application/octet-stream"] && ![MIMEType isEqualToString:@"text/plain"]) {
        Vector<String> extensions = MIMETypeRegistry::getExtensionsForMIMEType(MIMEType);

        if (extensions.isEmpty() || !vectorContainsString(extensions, extension)) {
            // The extension doesn't match the MIME type. Correct this.
            NSString *correctExtension = MIMETypeRegistry::getPreferredExtensionForMIMEType(MIMEType);
            if ([correctExtension length] != 0) {
                // Append the correct extension.
                filename = [filename stringByAppendingPathExtension:correctExtension];
            }
        }
    }

    return filename;
}
