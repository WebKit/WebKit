//
//  ResourceHandleIPhone.mm
//  WebCore
//
//  Copyright 2011 Apple Inc. All rights reserved.
//

#import "config.h"
#import "ResourceHandleInternal.h"

#if USE(CFNETWORK)

#import <CFNetwork/CFSocketStreamPriv.h>
#import <Foundation/NSURLRequestPrivate.h>

using namespace WebCore;

namespace WebCore {

CFMutableDictionaryRef ResourceHandle::createSSLPropertiesFromNSURLRequest(const ResourceRequest& request)
{
    NSString *host = request.url().host();
    NSArray *certArray = [NSURLRequest allowsSpecificHTTPSCertificateForHost:host];
    BOOL allowsAnyCertificate = [NSURLRequest allowsAnyHTTPSCertificateForHost:host];
    if (!certArray && !allowsAnyCertificate)
        return 0;

    CFMutableDictionaryRef sslProps = CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (certArray)
        CFDictionarySetValue(sslProps, _kCFStreamSSLTrustedLeafCertificates, (CFTypeRef) certArray);

    if (allowsAnyCertificate)
        CFDictionarySetValue(sslProps, kCFStreamSSLValidatesCertificateChain, kCFBooleanFalse);
    return sslProps;
}

}

#endif
