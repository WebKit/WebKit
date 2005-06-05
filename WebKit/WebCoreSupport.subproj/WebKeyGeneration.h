/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#ifndef	WEB_KEY_GENERATION_H
#define WEB_KEY_GENERATION_H

#import <WebKit/WebKeyGenerator.h>

#ifndef USE_NEW_KEY_GENERATION

#import <CoreFoundation/CoreFoundation.h>

#include <SecurityNssAsn1/secasn1t.h>
#include <Security/cssmtype.h>
#include <SecurityNssAsn1/X509Templates.h>
#include <SecurityNssAsn1/keyTemplates.h>

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
    
    extern const SEC_ASN1Template NetscapeCertSequenceTemplate[];
    
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
    
    extern const SEC_ASN1Template PublicKeyAndChallengeTemplate[];
    extern const SEC_ASN1Template SignedPublicKeyAndChallengeTemplate[];

    CFStringRef signedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription);
    WebCertificateParseResult addCertificatesToKeychainFromData(const void *bytes, unsigned length);
    
#ifdef __cplusplus
}
#endif

#endif /* USE_NEW_KEY_GENERATION */

#endif /* WEB_KEY_GENERATION_H */
