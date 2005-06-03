/*
 *  WebNewKeyGeneration.h
 *  WebKit
 *
 *  Created by Chris Blumenberg on Mon Aug 23 2004.
 *  Copyright (c) 2003 Apple Computer. All rights reserved.
 *
 */

#ifndef	WEB_KEY_GENERATION_H
#define WEB_KEY_GENERATION_H

#import <WebKit/WebKeyGenerator.h>

#ifdef USE_NEW_KEY_GENERATION

#import <CoreFoundation/CoreFoundation.h>

#ifdef __cplusplus
extern "C" {
#endif
    CFStringRef signedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
    WebCertificateParseResult addCertificatesToKeychainFromData(const void *bytes, unsigned length);
    
#ifdef __cplusplus
}
#endif

#endif /* USE_NEW_KEY_GENERATION */

#endif /* WEB_KEY_GENERATION_H */
