/*
 * Copyright (C) 2007-2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "WebArchiveDumpSupport.h"

#import "MIMETypeRegistry.h"
#import <CFNetwork/CFHTTPMessage.h>
#import <CFNetwork/CFNetwork.h>
#import <pal/spi/cf/CFNetworkSPI.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>
#import <wtf/cf/TypeCastsCF.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

using namespace WebCore;

namespace WebCoreTestSupport {

static RetainPtr<CFURLResponseRef> createCFURLResponseFromResponseData(CFDataRef responseData)
{
    NSURLResponse *response;
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:(__bridge NSData *)responseData error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    @try {
        response = [unarchiver decodeObjectOfClass:[NSURLResponse class] forKey:@"WebResourceResponse"]; // WebResourceResponseKey in WebResource.m
        [unarchiver finishDecoding];
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode NS(HTTP)URLResponse: %@", exception);
        response = nil;
    }

    if (![response isKindOfClass:[NSHTTPURLResponse class]])
        return adoptCF(CFURLResponseCreate(kCFAllocatorDefault, (__bridge CFURLRef)response.URL, (__bridge CFStringRef)response.MIMEType, response.expectedContentLength, (__bridge CFStringRef)response.textEncodingName, kCFURLCacheStorageAllowed));

    RetainPtr httpResponse = checked_objc_cast<NSHTTPURLResponse>(response);

    // NSURLResponse is not toll-free bridged to CFURLResponse.
    RetainPtr<CFHTTPMessageRef> httpMessage = adoptCF(CFHTTPMessageCreateResponse(kCFAllocatorDefault, httpResponse.get().statusCode, nullptr, kCFHTTPVersion1_1));

    NSDictionary *headerFields = httpResponse.get().allHeaderFields;
    for (NSString *headerField in [headerFields keyEnumerator])
        CFHTTPMessageSetHeaderFieldValue(httpMessage.get(), (__bridge CFStringRef)headerField, (__bridge CFStringRef)[headerFields objectForKey:headerField]);

    return adoptCF(CFURLResponseCreateWithHTTPResponse(kCFAllocatorDefault, (__bridge CFURLRef)response.URL, httpMessage.get(), kCFURLCacheStorageAllowed));
}

static void convertMIMEType(CFMutableStringRef mimeType)
{
    // Workaround for <rdar://problem/6234318> with Dashcode 2.0
    if (CFStringCompare(mimeType, CFSTR("application/x-javascript"), kCFCompareAnchored | kCFCompareCaseInsensitive) == kCFCompareEqualTo)
        CFStringReplaceAll(mimeType, CFSTR("text/javascript"));
}

static void convertWebResourceDataToString(CFMutableDictionaryRef resource)
{
    RetainPtr mimeType = checked_cf_cast<CFMutableStringRef>(CFDictionaryGetValue(resource, CFSTR("WebResourceMIMEType")));
    CFStringLowercase(mimeType.get(), RetainPtr { CFLocaleGetSystem() }.get());
    convertMIMEType(mimeType.get());

    if (CFStringHasPrefix(mimeType.get(), CFSTR("text/")) || MIMETypeRegistry::isSupportedNonImageMIMEType(mimeType.get())) {
        RetainPtr textEncodingName = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(resource, CFSTR("WebResourceTextEncodingName")));
        CFStringEncoding stringEncoding;
        if (textEncodingName && CFStringGetLength(textEncodingName.get()))
            stringEncoding = CFStringConvertIANACharSetNameToEncoding(textEncodingName.get());
        else
            stringEncoding = kCFStringEncodingUTF8;

        RetainPtr data = checked_cf_cast<CFDataRef>(CFDictionaryGetValue(resource, CFSTR("WebResourceData")));
        RetainPtr<CFStringRef> dataAsString = adoptCF(CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, data.get(), stringEncoding));
        if (dataAsString)
            CFDictionarySetValue(resource, CFSTR("WebResourceData"), dataAsString.get());
    }
}

static void normalizeHTTPResponseHeaderFields(CFMutableDictionaryRef fields)
{
    // Normalize headers
    if (CFDictionaryContainsKey(fields, CFSTR("Date")))
        CFDictionarySetValue(fields, CFSTR("Date"), CFSTR("Sun, 16 Nov 2008 17:00:00 GMT"));
    if (CFDictionaryContainsKey(fields, CFSTR("Last-Modified")))
        CFDictionarySetValue(fields, CFSTR("Last-Modified"), CFSTR("Sun, 16 Nov 2008 16:55:00 GMT"));
    if (CFDictionaryContainsKey(fields, CFSTR("Etag")))
        CFDictionarySetValue(fields, CFSTR("Etag"), CFSTR("\"301925-21-45c7d72d3e780\""));
    if (CFDictionaryContainsKey(fields, CFSTR("Server")))
        CFDictionarySetValue(fields, CFSTR("Server"), CFSTR("Apache/2.2.9 (Unix) mod_ssl/2.2.9 OpenSSL/0.9.7l PHP/5.2.6"));

    // Remove headers
    CFDictionaryRemoveValue(fields, CFSTR("Connection"));
    CFDictionaryRemoveValue(fields, CFSTR("Keep-Alive"));
}

#if USE(QUICK_LOOK)
using QuickLookURLReplacementVector = Vector<std::pair<RetainPtr<CFStringRef>, RetainPtr<CFStringRef>>>;
static QuickLookURLReplacementVector& quickLookURLReplacements()
{
    static NeverDestroyed<QuickLookURLReplacementVector> urlReplacements;
    return urlReplacements.get();
}
#endif

static void normalizeWebResourceURL(CFMutableStringRef webResourceURL)
{
    static CFStringRef fileScheme = CFSTR("file://");
    static CFIndex fileSchemeLength = CFStringGetLength(fileScheme);
    if (CFStringFind(webResourceURL, fileScheme, kCFCompareAnchored).location != kCFNotFound) {
        CFRange layoutTestsWebArchivePathRange = CFStringFind(webResourceURL, CFSTR("/LayoutTests/"), kCFCompareBackwards);
        if (layoutTestsWebArchivePathRange.location == kCFNotFound)
            return;
        CFRange currentWorkingDirectoryRange = CFRangeMake(fileSchemeLength, layoutTestsWebArchivePathRange.location - fileSchemeLength);
        CFStringReplace(webResourceURL, currentWorkingDirectoryRange, CFSTR(""));
        return;
    }

#if USE(QUICK_LOOK)
    static CFStringRef quickLookScheme = CFSTR("x-apple-ql-id://");
    static CFIndex quickLookSchemeLength = CFStringGetLength(quickLookScheme);
    if (CFStringFind(webResourceURL, quickLookScheme, kCFCompareAnchored).location == kCFNotFound)
        return;

    CFIndex replacementEnd = CFStringFind(webResourceURL, CFSTR("."), kCFCompareBackwards).location;
    if (replacementEnd == kCFNotFound)
        replacementEnd = CFStringGetLength(webResourceURL);

    auto oldResourceURL = adoptCF(CFStringCreateCopy(kCFAllocatorDefault, webResourceURL));
    CFStringReplace(webResourceURL, CFRangeMake(quickLookSchemeLength, replacementEnd - quickLookSchemeLength), CFSTR("resource"));
    quickLookURLReplacements().append(std::make_pair(WTF::move(oldResourceURL), webResourceURL));
#endif
}

static void convertWebResourceResponseToDictionary(CFMutableDictionaryRef propertyList)
{
    RetainPtr responseData = dynamic_cf_cast<CFDataRef>(CFDictionaryGetValue(propertyList, CFSTR("WebResourceResponse"))); // WebResourceResponseKey in WebResource.m
    if (!responseData)
        return;

    auto response = createCFURLResponseFromResponseData(responseData.get());
    if (!response)
        return;

    auto responseDictionary = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    RetainPtr urlResponse = CFURLResponseGetURL(response.get());
    RetainPtr urlResponseString = CFURLGetString(urlResponse.get());
    auto urlString = adoptCF(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, urlResponseString.get()));
    normalizeWebResourceURL(urlString.get());
    CFDictionarySetValue(responseDictionary.get(), CFSTR("URL"), urlString.get());

    RetainPtr urlResponseMIMEType = CFURLResponseGetMIMEType(response.get());
    auto mimeTypeString = adoptCF(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, urlResponseMIMEType.get()));
    convertMIMEType(mimeTypeString.get());
    CFDictionarySetValue(responseDictionary.get(), CFSTR("MIMEType"), mimeTypeString.get());

    RetainPtr textEncodingName = CFURLResponseGetTextEncodingName(response.get());
    if (textEncodingName)
        CFDictionarySetValue(responseDictionary.get(), CFSTR("textEncodingName"), textEncodingName.get());

    SInt64 expectedContentLength = CFURLResponseGetExpectedContentLength(response.get());
    auto expectedContentLengthNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt64Type, &expectedContentLength));
    CFDictionarySetValue(responseDictionary.get(), CFSTR("expectedContentLength"), expectedContentLengthNumber.get());

    if (CFHTTPMessageRef httpMessage = CFURLResponseGetHTTPResponse(response.get())) {
        auto allHeaders = adoptCF(CFHTTPMessageCopyAllHeaderFields(httpMessage));
        auto allHeaderFields = adoptCF(CFDictionaryCreateMutableCopy(kCFAllocatorDefault, 0, allHeaders.get()));
        normalizeHTTPResponseHeaderFields(allHeaderFields.get());
        CFDictionarySetValue(responseDictionary.get(), CFSTR("allHeaderFields"), allHeaderFields.get());

        CFIndex statusCode = CFHTTPMessageGetResponseStatusCode(httpMessage);
        auto statusCodeNumber = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberCFIndexType, &statusCode));
        CFDictionarySetValue(responseDictionary.get(), CFSTR("statusCode"), statusCodeNumber.get());
    }

    CFDictionarySetValue(propertyList, CFSTR("WebResourceResponse"), responseDictionary.get());
}

static CFComparisonResult compareResourceURLs(const void *val1, const void *val2, void *)
{
    RetainPtr url1 = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(val1), CFSTR("WebResourceURL")));
    RetainPtr url2 = checked_cf_cast<CFStringRef>(CFDictionaryGetValue(static_cast<CFDictionaryRef>(val2), CFSTR("WebResourceURL")));

    return CFStringCompare(url1.get(), url2.get(), kCFCompareAnchored);
}

RetainPtr<CFStringRef> createXMLStringFromWebArchiveData(CFDataRef webArchiveData)
{
    CFErrorRef error = 0;
    CFPropertyListFormat format = kCFPropertyListBinaryFormat_v1_0;
    RetainPtr<CFMutableDictionaryRef> propertyList = adoptCF(checked_cf_cast<CFMutableDictionaryRef>(CFPropertyListCreateWithData(kCFAllocatorDefault, webArchiveData, kCFPropertyListMutableContainersAndLeaves, &format, &error)));

    if (!propertyList) {
        if (error)
            return adoptCF(CFErrorCopyDescription(error));
        return CFSTR("An unknown error occurred converting data to property list.");
    }

    RetainPtr<CFMutableArrayRef> resources = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 0, &kCFTypeArrayCallBacks));
    CFArrayAppendValue(resources.get(), propertyList.get());

    while (CFArrayGetCount(resources.get())) {
        RetainPtr<CFMutableDictionaryRef> resourcePropertyList = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(resources.get(), 0));
        CFArrayRemoveValueAtIndex(resources.get(), 0);

        RetainPtr mainResource = checked_cf_cast<CFMutableDictionaryRef>(CFDictionaryGetValue(resourcePropertyList.get(), CFSTR("WebMainResource")));
        RetainPtr webResourceURL = checked_cf_cast<CFMutableStringRef>(CFDictionaryGetValue(mainResource.get(), CFSTR("WebResourceURL")));
        normalizeWebResourceURL(webResourceURL.get());
        convertWebResourceDataToString(mainResource.get());

        // Add subframeArchives to list for processing
        RetainPtr subframeArchives = checked_cf_cast<CFMutableArrayRef>(CFDictionaryGetValue(resourcePropertyList.get(), CFSTR("WebSubframeArchives"))); // WebSubframeArchivesKey in WebArchive.m
        if (subframeArchives)
            CFArrayAppendArray(resources.get(), subframeArchives.get(), CFRangeMake(0, CFArrayGetCount(subframeArchives.get())));

        RetainPtr subresources = checked_cf_cast<CFMutableArrayRef>(CFDictionaryGetValue(resourcePropertyList.get(), CFSTR("WebSubresources"))); // WebSubresourcesKey in WebArchive.m
        if (!subresources)
            continue;

        CFIndex subresourcesCount = CFArrayGetCount(subresources.get());
        for (CFIndex i = 0; i < subresourcesCount; ++i) {
            RetainPtr subresourcePropertyList = checked_cf_cast<CFMutableDictionaryRef>(CFArrayGetValueAtIndex(subresources.get(), i));
            RetainPtr subresourceURL = checked_cf_cast<CFMutableStringRef>(CFDictionaryGetValue(subresourcePropertyList.get(), CFSTR("WebResourceURL")));
            normalizeWebResourceURL(subresourceURL.get());
            convertWebResourceResponseToDictionary(subresourcePropertyList.get());
            convertWebResourceDataToString(subresourcePropertyList.get());
        }

        // Sort the subresources so they're always in a predictable order for the dump
        CFArraySortValues(subresources.get(), CFRangeMake(0, CFArrayGetCount(subresources.get())), compareResourceURLs, 0);
    }

    error = 0;

    RetainPtr<CFDataRef> xmlData = adoptCF(CFPropertyListCreateData(kCFAllocatorDefault, propertyList.get(), kCFPropertyListXMLFormat_v1_0, 0, &error));

    if (!xmlData) {
        if (error)
            return adoptCF(CFErrorCopyDescription(error));
        return CFSTR("An unknown error occurred converting property list to data.");
    }

    RetainPtr<CFStringRef> xmlString = adoptCF(CFStringCreateFromExternalRepresentation(kCFAllocatorDefault, xmlData.get(), kCFStringEncodingUTF8));
    RetainPtr<CFMutableStringRef> string = adoptCF(CFStringCreateMutableCopy(kCFAllocatorDefault, 0, xmlString.get()));

    // Replace "Apple Computer" with "Apple" in the DTD declaration.
    CFStringFindAndReplace(string.get(), CFSTR("-//Apple Computer//"), CFSTR("-//Apple//"), CFRangeMake(0, CFStringGetLength(string.get())), 0);

#if USE(QUICK_LOOK)
    for (auto& replacement : quickLookURLReplacements())
        CFStringFindAndReplace(string.get(), replacement.first.get(), replacement.second.get(), CFRangeMake(0, CFStringGetLength(string.get())), 0);
    quickLookURLReplacements().clear();
#endif

    return string;
}

} // namespace WebCoreTestSupport
