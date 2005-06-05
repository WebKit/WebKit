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

#import <WebKit/WebNewKeyGeneration.h>

#ifdef USE_NEW_KEY_GENERATION

#import <Security/asn1Templates.h>
#import <Security/SecAsn1Coder.h>
#import <Security/secasn1t.h>
#import <Security/Security.h>

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
  CSSM_OID              contentType;            // netscape-cert-sequence
  CSSM_DATA             **certs;
} NetscapeCertSequence;

extern const SecAsn1Template NetscapeCertSequenceTemplate[];

/*
 * Public key/challenge, to send to CA.
 *
 * PublicKeyAndChallenge ::= SEQUENCE {
 *
 * ???\200?     spki SubjectPublicKeyInfo,
 *      challenge IA5STRING
 * }
 *
 * SignedPublicKeyAndChallenge ::= SEQUENCE {
 *              publicKeyAndChallenge PublicKeyAndChallenge,
 *              signatureAlgorithm AlgorithmIdentifier,
 *              signature BIT STRING
 * }
 */
typedef struct {
  CSSM_X509_SUBJECT_PUBLIC_KEY_INFO     spki;
  CSSM_DATA                                                     challenge;      // ASCII
} PublicKeyAndChallenge;

typedef struct {
  PublicKeyAndChallenge                         pubKeyAndChallenge;
  CSSM_X509_ALGORITHM_IDENTIFIER                algId;
  CSSM_DATA                                                     signature; // length in BITS
} SignedPublicKeyAndChallenge;

extern const SecAsn1Template PublicKeyAndChallengeTemplate[];
extern const SecAsn1Template SignedPublicKeyAndChallengeTemplate[];


#import <WebKit/WebAssertions.h>

#import <Security/keyTemplates.h>
#import <Security/SecKeyPriv.h>                /* Security.framework SPI */

#import <security_cdsa_utils/cuEnc64.h>

/* hard coded params, some of which may come from the user in real life */
#define GNR_KEY_ALG			CSSM_ALGID_RSA
#define GNR_SIG_ALG			CSSM_ALGID_MD5WithRSA
#define GNR_SIG_ALGOID                  CSSMOID_MD5WithRSA

const SecAsn1Template NetscapeCertSequenceTemplate[] = {
{ SEC_ASN1_SEQUENCE,
    0, NULL, sizeof(NetscapeCertSequence) },
{ SEC_ASN1_OBJECT_ID,
    offsetof(NetscapeCertSequence, contentType), 0, 0 },
{ SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 
    SEC_ASN1_CONTEXT_SPECIFIC | 0 , 
    offsetof(NetscapeCertSequence, certs),
    kSecAsn1SequenceOfAnyTemplate, 0 },
{ 0, 0, 0, 0 }
};

const SecAsn1Template PublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(PublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
        offsetof(PublicKeyAndChallenge, spki),
        kSecAsn1SubjectPublicKeyInfoTemplate, 0},
    { SEC_ASN1_INLINE,
        offsetof(PublicKeyAndChallenge, challenge),
        kSecAsn1IA5StringTemplate, 0 },
    { 0, 0, 0, 0}
};

const SecAsn1Template SignedPublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SignedPublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
        offsetof(SignedPublicKeyAndChallenge, pubKeyAndChallenge),
        PublicKeyAndChallengeTemplate, 0 },
    { SEC_ASN1_INLINE,
        offsetof(SignedPublicKeyAndChallenge, algId),
        kSecAsn1AlgorithmIDTemplate, 0 },
    { SEC_ASN1_BIT_STRING,
        offsetof(SignedPublicKeyAndChallenge, signature), 0, 0 },
    { 0, 0, 0, 0 }
};

void gnrNullAlgParams(CSSM_X509_ALGORITHM_IDENTIFIER *algId);
CSSM_RETURN gnrSign(CSSM_CSP_HANDLE		cspHand,
                    const CSSM_DATA		*plainText,
                    SecKeyRef			privKey,
                    CSSM_ALGORITHMS		sigAlg,		// e.g., CSSM_ALGID_SHA1WithRSA
                    CSSM_DATA			*sig);
unsigned nssArraySize(const void **array);
bool addCertificateToKeychainFromData(const unsigned char *certData,
                                      unsigned certDataLen,
                                      unsigned certNum);

/*
 * Given a context specified via a CSSM_CC_HANDLE, add a new
 * CSSM_CONTEXT_ATTRIBUTE to the context as specified by AttributeType,
 * AttributeLength, and an untyped pointer.
 *
 * This is currently used to add a second CSSM_KEY attribute when performing
 * ops with algorithm CSSM_ALGID_FEED and CSSM_ALGID_FEECFILE.
 */
static CSSM_RETURN gnrAddContextAttribute(CSSM_CC_HANDLE CCHandle,
                                          uint32 AttributeType,
                                          uint32 AttributeLength,
                                          const void *AttributePtr)
{
    CSSM_CONTEXT_ATTRIBUTE		newAttr;	
    CSSM_RETURN					crtn;
    
    newAttr.AttributeType     = AttributeType;
    newAttr.AttributeLength   = AttributeLength;
    newAttr.Attribute.Data    = (CSSM_DATA_PTR)AttributePtr;
    crtn = CSSM_UpdateContextAttributes(CCHandle, 1, &newAttr);
    if(crtn) {
        ERROR("CSSM_UpdateContextAttributes", crtn);
    }
    return crtn;
}

/*
 * Given a public key as a SecKeyRef, obtain the key material in
 * SubjectPublicKeyInfo format. This entails a NULL wrap to format
 * in CSSM_KEYBLOB_RAW_FORMAT_X509 form. Caller must eventually
 * free the returned key via CSSM_FreeKey().
 */
static OSStatus gnrGetSubjPubKey(
                                 CSSM_CSP_HANDLE	cspHand,
                                 SecKeyRef secKey,
                                 CSSM_KEY_PTR subjPubKey)		// RETURNED
{
    CSSM_CC_HANDLE		ccHand;
    CSSM_RETURN			crtn;
    CSSM_ACCESS_CREDENTIALS	creds;
    const CSSM_KEY		*refPubKey;
    OSStatus 			ortn;
    
    /* Get public key in CSSM form */
    ortn = SecKeyGetCSSMKey(secKey, &refPubKey);
    if(ortn) {
        ERROR("SecKeyGetCSSMKey", ortn);
        return ortn;
    }
    
    /* NULL wrap via CSPDL */
    memset(subjPubKey, 0, sizeof(CSSM_KEY));
    memset(&creds, 0, sizeof(CSSM_ACCESS_CREDENTIALS));
    crtn = CSSM_CSP_CreateSymmetricContext(cspHand,
                                           CSSM_ALGID_NONE,
                                           CSSM_ALGMODE_NONE,
                                           &creds,				// passPhrase
                                           NULL,				// wrapping key
                                           NULL,				// init vector
                                           CSSM_PADDING_NONE,	// Padding
                                           0,					// Params
                                           &ccHand);
    if(crtn) {
        ERROR("gnrGetSubjPubKey CSSM_CSP_CreateSymmetricContext", 
                     crtn);
        return crtn;
    }
    
    /*
     * Specify X509 format' that is NOT the default for RSA (PKCS1 is)
     */
    crtn = gnrAddContextAttribute(ccHand,
                                  CSSM_ATTRIBUTE_PUBLIC_KEY_FORMAT,
                                  sizeof(uint32),
                                  (void *)CSSM_KEYBLOB_RAW_FORMAT_X509);
    if(crtn) {
        ERROR("gnrAddContextAttribute", crtn);
        goto errOut;
    }
    
    crtn = CSSM_WrapKey(ccHand,
                        &creds,
                        refPubKey,
                        NULL,			// DescriptiveData
                        subjPubKey);
    if(crtn) {
        ERROR("CSSM_WrapKey", crtn);
    }
errOut:
        CSSM_DeleteContext(ccHand);
    return crtn;
}

/* 
* Set up a encoded NULL for CSSM_X509_ALGORITHM_IDENTIFIER.parameters.
 */
void gnrNullAlgParams(CSSM_X509_ALGORITHM_IDENTIFIER *algId)
{
    static const uint8 encNull[2] = { SEC_ASN1_NULL, 0 };
    algId->parameters.Data = (uint8 *)encNull;
    algId->parameters.Length = 2;
}

/*
 * Sign specified plaintext. Caller must free signature data via
 * gnrFreeCssmData().
 */
CSSM_RETURN gnrSign(CSSM_CSP_HANDLE		cspHand,
                    const CSSM_DATA		*plainText,
                    SecKeyRef			privKey,
                    CSSM_ALGORITHMS		sigAlg,		// e.g., CSSM_ALGID_SHA1WithRSA
                    CSSM_DATA			*sig)		// allocated by CSP and RETURNED
{
    CSSM_CC_HANDLE		ccHand;
    CSSM_RETURN			crtn;
    const CSSM_KEY		*refPrivKey;
    OSStatus 			ortn;
    const CSSM_ACCESS_CREDENTIALS *creds;
    
    /* Get private key in CSSM form */
    ortn = SecKeyGetCSSMKey(privKey, &refPrivKey);
    if(ortn) {
        ERROR("SecKeyGetCSSMKey", ortn);
        return ortn;
    }
    
    /* Get appropriate access credentials */
    ortn = SecKeyGetCredentials(privKey,
                                CSSM_ACL_AUTHORIZATION_SIGN,
                                kSecCredentialTypeDefault,
                                &creds);
    if(ortn) {
        ERROR("SecKeyGetCredentials", ortn);
        return ortn;
    }
    
    /* cook up signature context */
    crtn = CSSM_CSP_CreateSignatureContext(cspHand,
                                           sigAlg,
                                           creds,	
                                           refPrivKey,
                                           &ccHand);
    if(crtn) {
        ERROR("CSSM_CSP_CreateSignatureContext", ortn);
        return crtn;
    }
    
    /* go for it */
    sig->Data = NULL;
    sig->Length = 0;
    crtn = CSSM_SignData(ccHand,
                         plainText,
                         1,
                         CSSM_ALGID_NONE,
                         sig);
    if(crtn) {
        ERROR("CSSM_SignData", ortn);
    }
    CSSM_DeleteContext(ccHand);
    return crtn;
}

/*
 * Free data mallocd on app's behalf by a CSSM module.
 */
static void gnrFreeCssmData(
                            CSSM_HANDLE		modHand,
                            CSSM_DATA 		*cdata)
{
    CSSM_API_MEMORY_FUNCS memFuncs;
    CSSM_RETURN crtn = CSSM_GetAPIMemoryFunctions(modHand, &memFuncs);
    if(crtn) {
        ERROR("CSSM_GetAPIMemoryFunctions", crtn);
        /* oh well, leak and continue */
    }
    else {
        memFuncs.free_func(cdata->Data, memFuncs.AllocRef);
    }
    return;
}

unsigned nssArraySize(const void **array)
{
    unsigned count = 0;
    if (array) {
        while (*array++) {
            count++;
        }
    }
    return count;
}

CFStringRef signedPublicKeyAndChallengeString(unsigned keySize, CFStringRef challenge, CFStringRef keyDescription)
{
    OSStatus 		ortn;
    CSSM_RETURN		crtn;
    SecKeyRef 		pubKey = NULL;
    SecKeyRef 		privKey = NULL;
    CSSM_KEY		subjectPubKey;
    bool                freeSubjPubKey = false;
    CSSM_CSP_HANDLE	cspHand;
    SecAsn1CoderRef     coder = NULL;
    SignedPublicKeyAndChallenge	spkc;
    PublicKeyAndChallenge		*pkc = &spkc.pubKeyAndChallenge;
    /* DER encoded spkc.pubKeyAndChallenge and spkc */
    CSSM_DATA		encodedPkc = {0, NULL};		
    CSSM_DATA		encodedSpkc = {0, NULL};
    CSSM_DATA		signature = {0, NULL};
    unsigned char	*spkcB64 = NULL;		// base64 encoded encodedSpkc
    unsigned		spkcB64Len;
    SecAccessRef        accessRef;
    CFArrayRef          acls;
    SecACLRef           acl;
    CFStringRef         result = NULL;
    
    ortn = SecAccessCreate(keyDescription, NULL, &accessRef);
    if (ortn) {
        ERROR("***SecAccessCreate %d", ortn);
        goto errOut;
    }
    ortn = SecAccessCopySelectedACLList(accessRef, CSSM_ACL_AUTHORIZATION_DECRYPT, &acls);
    if (ortn) {
        ERROR("***SecAccessCopySelectedACLList %d", ortn);
        goto errOut;
    }
    acl = (SecACLRef)CFArrayGetValueAtIndex(acls, 0);
    CFRelease(acls);
    ortn = SecACLSetSimpleContents(acl, NULL, keyDescription, NULL);
    if (ortn) {
        ERROR("***SecACLSetSimpleContents %d", ortn);
        goto errOut;
    }
    
    // Cook up a key pair, just use any old params for now
    ortn = SecKeyCreatePair(nil,                                        // in default KC
                            GNR_KEY_ALG,                                // normally spec'd by user
                            keySize,                                    // key size, ditto
                            0,                                          // ContextHandle
                            CSSM_KEYUSE_ANY,                            // might want to restrict this
                            CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | 
                            CSSM_KEYATTR_RETURN_REF,                    // pub attrs
                            CSSM_KEYUSE_ANY,				// might want to restrict this
                            CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF |
                            CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE,
                            accessRef,
                            &pubKey,
                            &privKey);
    if (ortn != noErr) {
        ERROR("***SecKeyCreatePair %d", ortn);
        goto errOut;
    }
    
    /* get handle of CSPDL for crypto ops */
    ortn = SecKeyGetCSPHandle(privKey, &cspHand);
    if (ortn != noErr) {
        ERROR("***SecKeyGetCSPHandle", ortn);
        goto errOut;
    }
    
    /*
     * Get the public key in encoded SubjectPublicKeyInfo form.
     */
    ortn = gnrGetSubjPubKey(cspHand, pubKey, &subjectPubKey);
    if (ortn != noErr) {
        goto errOut;
    }
    freeSubjPubKey = true;
    
    ortn = SecAsn1CoderCreate(&coder);
    if (ortn != noErr) {
        ERROR("***SecAsn1CoderCreate", ortn);
        goto errOut;
    }
    
    /*
     * Cook up PublicKeyAndChallenge and DER-encode it.
     * First, DER-decode the key's SubjectPublicKeyInfo.
     */
    memset(&spkc, 0, sizeof(spkc));
    
    ortn = SecAsn1DecodeData(coder, &subjectPubKey.KeyData, kSecAsn1SubjectPublicKeyInfoTemplate, &pkc->spki);
    if (ortn != noErr) {
        /* should never happen */
        ERROR("***Error decoding subject public key info\n");
        goto errOut;
    }
    
    pkc->challenge.Length = CFStringGetLength(challenge);
    if (pkc->challenge.Length == 0) {
        pkc->challenge.Length = 1;
        pkc->challenge.Data = (uint8 *)strdup("\0");
    } else {
        pkc->challenge.Data = (uint8 *)malloc(pkc->challenge.Length + 1);
        CFStringGetCString(challenge,  (char *)pkc->challenge.Data, pkc->challenge.Length + 1, kCFStringEncodingASCII);
    }
    ortn = SecAsn1EncodeItem(coder, pkc, PublicKeyAndChallengeTemplate, &encodedPkc);
    if (ortn != noErr) {
        /* should never happen */
        ERROR("***Error encoding PublicKeyAndChallenge\n");
        goto errOut;
    }
    
    /*
     * Sign the encoded PublicKeyAndChallenge.
     */
    crtn = gnrSign(cspHand, &encodedPkc, privKey, GNR_SIG_ALG, &signature);
    if (crtn) {
        goto errOut;
    }
    
    /*
     * Cook up SignedPublicKeyAndChallenge, DER-encode that. 
     * The PublicKeyAndChallenge stays in place where we originally
     * created it before we signed it. Now we just add the algId
     * and the signature proper.
     */
    spkc.algId.algorithm = GNR_SIG_ALGOID;
    gnrNullAlgParams(&spkc.algId);
    spkc.signature = signature;
    /* convert to BIT length */
    spkc.signature.Length *= 8;
    ortn = SecAsn1EncodeItem(coder, &spkc, SignedPublicKeyAndChallengeTemplate, &encodedSpkc);
    if (ortn != noErr) {
        /* should never happen */
        ERROR("***Error encoding SignedPublicKeyAndChallenge\n");
        goto errOut;
    }
    
    /*
     * Finally base64 the result and write that to outFile.
     * cuEnc64() gives us a NULL-terminated string; we strip off the NULL.
     */
    spkcB64 = cuEnc64(encodedSpkc.Data, encodedSpkc.Length, &spkcB64Len);
    if (spkcB64 == NULL) {
        /* should never happen */
        FATAL("***Error base64-encoding the result\n");
        goto errOut;
    }
    
errOut:
    if (coder != NULL) {
        SecAsn1CoderRelease(coder);
    }
    if (freeSubjPubKey) {
        CSSM_FreeKey(cspHand, NULL, &subjectPubKey, CSSM_FALSE);
    }
    if (signature.Data) {
        gnrFreeCssmData(cspHand, &signature);
    }
    if (pubKey) {
        CFRelease(pubKey);
    }
    if (privKey) {
        CFRelease(privKey);
    }
    if (accessRef) {
        CFRelease(accessRef);
    }    
    if (pkc->challenge.Data) {
        free(pkc->challenge.Data);
    }
    if (spkcB64) {
        result = CFStringCreateWithCString(NULL, (const char *)spkcB64, kCFStringEncodingASCII);
        free(spkcB64);
    }
    return result;
}

/* 
* Per-cert processing, called for each cert we extract from the 
 * incoming blob.
 */
bool addCertificateToKeychainFromData(const unsigned char *certData,
                                      unsigned certDataLen,
                                      unsigned certNum)
{
    CSSM_DATA cert = {certDataLen, (uint8 *)certData};
    SecCertificateRef certRef;
    
    /* Make a SecCertificateRef */
    OSStatus ortn = SecCertificateCreateFromData(&cert, 
                                                 CSSM_CERT_X_509v3,
                                                 CSSM_CERT_ENCODING_DER,
                                                 &certRef);
    if (ortn != noErr) {
        ERROR("SecCertificateCreateFromData returned %d", (int)ortn);
        return false;
    }
    
    /* 
        * Add it to default keychain.
        * Many people will be surprised that this op works without
        * the user having to unlock the keychain. 
        */
    ortn = SecCertificateAddToKeychain(certRef, nil);
    
    /* Free the cert in any case */
    CFRelease(certRef);
    switch(ortn) {
        case noErr:
            break;
        case errSecDuplicateItem:
            /* Not uncommon, definitely not an error */
            ERROR("cert %u already present in keychain", certNum);
            break;
        default:
            ERROR("SecCertificateAddToKeychain returned %d", (int)ortn);
            return false;
    }

    return true;
}

WebCertificateParseResult addCertificatesToKeychainFromData(const void *bytes, unsigned length)
{   
    WebCertificateParseResult result = WebCertificateParseResultFailed;

    /* DER-decode, first as NetscapeCertSequence */
    SecAsn1CoderRef coder = NULL;
    NetscapeCertSequence certSeq;
    OSErr ortn;
    
    ortn = SecAsn1CoderCreate(&coder);
    if (ortn == noErr) {
        memset(&certSeq, 0, sizeof(certSeq));
        ortn = SecAsn1Decode(coder, bytes, length, NetscapeCertSequenceTemplate, &certSeq);
        if (ortn == noErr) {
            if (certSeq.contentType.Length == CSSMOID_PKCS7_SignedData.Length &&
                memcmp(certSeq.contentType.Data, CSSMOID_PKCS7_SignedData.Data, certSeq.contentType.Length) == 0) {
                return WebCertificateParseResultPKCS7;
            }
            /*
             * Last cert is a root, which we do NOT want to add
             * to the user's keychain.
             */
            unsigned numCerts = nssArraySize((const void **)certSeq.certs) - 1;
            unsigned i;
            for (i=0; i<numCerts; i++) {
                CSSM_DATA *cert = certSeq.certs[i];
                result = addCertificateToKeychainFromData(cert->Data, cert->Length, i) ? WebCertificateParseResultSucceeded : WebCertificateParseResultFailed;
            } 
        } else {
            /*
             * Didn't appear to be a NetscapeCertSequence; assume it's just 
             * a cert. FIXME: Netscape spec says the blob might also be PKCS7
             * format, which we're not handling here.
             */
            result = addCertificateToKeychainFromData(bytes, length, 0) ? WebCertificateParseResultSucceeded : WebCertificateParseResultFailed;
        }
    }
    
    if (coder != NULL) {
        SecAsn1CoderRelease(coder);
    }

    return result;
}

#endif /* USE_NEW_KEY_GENERATION */
