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

#import <Security/asn1Templates.h>
#import <Security/SecAsn1Coder.h>
#import <Security/secasn1t.h>
#import <Security/Security.h>

#ifdef __cplusplus
extern "C" {
#endif
    
    /* 
    * Netscape Certifiate Sequence is defined by Netscape as a PKCS7
     * ContentInfo with a contentType of netscape-cert-sequence and a content
     * consisting of a sequence of certificates.
     *
     * For simplicity - i.e., to avoid the general purpose ContentInfo
     * polymorphism - we'll just hard-code this particular type right here.
     *
     * Inside the ContentInfo is an array of standard X509 certificates.
     * We don't need to parse the certs themselves so they remain as 
     * opaque data blobs. 
     */
    typedef struct {
        CSSM_OID		contentType;		// netscape-cert-sequence
        CSSM_DATA		**certs;
    } NetscapeCertSequence;
    
    extern const SecAsn1Template NetscapeCertSequenceTemplate[];
    
    /*
     * Public key/challenge, to send to CA.
     *
     * PublicKeyAndChallenge ::= SEQUENCE {
         *
         * ¬† 	spki SubjectPublicKeyInfo,
         *   	challenge IA5STRING
         * }
     *
     * SignedPublicKeyAndChallenge ::= SEQUENCE {
         * 		publicKeyAndChallenge PublicKeyAndChallenge,
         *		signatureAlgorithm AlgorithmIdentifier,
         *		signature BIT STRING
         * }
     */
    typedef struct {
        CSSM_X509_SUBJECT_PUBLIC_KEY_INFO	spki;
        CSSM_DATA							challenge;	// ASCII
    } PublicKeyAndChallenge;
    
    typedef struct {
        PublicKeyAndChallenge				pubKeyAndChallenge;
        CSSM_X509_ALGORITHM_IDENTIFIER		algId;
        CSSM_DATA							signature; // length in BITS
    } SignedPublicKeyAndChallenge;
    
    extern const SecAsn1Template PublicKeyAndChallengeTemplate[];
    extern const SecAsn1Template SignedPublicKeyAndChallengeTemplate[];

    CFStringRef signedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
    WebCertificateParseResult addCertificatesToKeychainFromData(const void *bytes, unsigned length);
    
#ifdef __cplusplus
}
#endif

#endif /* USE_NEW_KEY_GENERATION */

#endif /* WEB_KEY_GENERATION_H */
