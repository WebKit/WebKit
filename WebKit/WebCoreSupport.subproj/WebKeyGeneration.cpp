/*
 * parseNetscapeCerts - parse a blob containing one or more
 * downloaded netscape certificates. 
 *
 * Requires Apple-private API in libCdsaUtils.a and libnssasn1.a.
 * NOT FOR USE OUTSIDE OF APPLE COMTPUTER.
 *
 * Created 12/4/03 by dmitch
 */

#import <WebKit/WebKeyGeneration.h>

#import <WebKit/WebAssertions.h>

#include <Security/cuCdsaUtils.h>               /* private libCdsaUtils.a */
#include <Security/cuFileIo.h>                  /* private libCdsaUtils.a */
#include <Security/cuEnc64.h>                   /* private libCdsaUtils.a */
#include <SecurityNssAsn1/SecNssCoder.h>	/* private libnssasn1.a */
#include <SecurityNssAsn1/nssUtils.h>		/* private libnssasn1.a */
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>                /* Security.framework SPI */

/* hard coded params, some of which may come from the user in real life */
#define GNR_KEY_ALG			CSSM_ALGID_RSA
#define GNR_SIG_ALG			CSSM_ALGID_SHA1WithRSA
#define GNR_SIG_ALGOID                  CSSMOID_SHA1WithRSA

const SEC_ASN1Template NetscapeCertSequenceTemplate[] = {
{ SEC_ASN1_SEQUENCE,
    0, NULL, sizeof(NetscapeCertSequence) },
{ SEC_ASN1_OBJECT_ID,
    offsetof(NetscapeCertSequence, contentType), 0, 0 },
{ SEC_ASN1_EXPLICIT | SEC_ASN1_CONSTRUCTED | 
    SEC_ASN1_CONTEXT_SPECIFIC | 0 , 
    offsetof(NetscapeCertSequence, certs),
    SEC_SequenceOfAnyTemplate, 0 },
{ 0, 0, 0, 0 }
};

const SEC_ASN1Template PublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(PublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
        offsetof(PublicKeyAndChallenge, spki),
        NSS_SubjectPublicKeyInfoTemplate, 0},
    { SEC_ASN1_INLINE,
        offsetof(PublicKeyAndChallenge, challenge),
        SEC_IA5StringTemplate, 0 },
    { 0, 0, 0, 0}
};

extern const SEC_ASN1Template SignedPublicKeyAndChallengeTemplate[] = {
    { SEC_ASN1_SEQUENCE,
        0, NULL, sizeof(SignedPublicKeyAndChallenge) },
    { SEC_ASN1_INLINE,
        offsetof(SignedPublicKeyAndChallenge, pubKeyAndChallenge),
        PublicKeyAndChallengeTemplate, 0 },
    { SEC_ASN1_INLINE,
        offsetof(SignedPublicKeyAndChallenge, algId),
        NSS_AlgorithmIDTemplate, 0 },
    { SEC_ASN1_BIT_STRING,
        offsetof(SignedPublicKeyAndChallenge, signature), 0, 0 },
    { 0, 0, 0, 0 }
};

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
void gnrNullAlgParams(
                      CSSM_X509_ALGORITHM_IDENTIFIER	*algId)
{
    static const uint8 encNull[2] = { SEC_ASN1_NULL, 0 };
    algId->parameters.Data = (uint8 *)encNull;
    algId->parameters.Length = 2;
}

/*
 * Sign specified plaintext. Caller must free signature data via
 * gnrFreeCssmData().
 */
CSSM_RETURN gnrSign(
                    CSSM_CSP_HANDLE		cspHand,
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

char *signedPublicKeyAndChallengeString(unsigned keySize, const char *challenge)
{
    OSStatus 		ortn;
    CSSM_RETURN		crtn;
    SecKeyRef 		pubKey = NULL;
    SecKeyRef 		privKey = NULL;
    CSSM_KEY		subjectPubKey;
    bool			freeSubjPubKey = false;
    CSSM_CSP_HANDLE	cspHand;
    SecNssCoder		coder;
    SignedPublicKeyAndChallenge	spkc;
    PublicKeyAndChallenge		*pkc = &spkc.pubKeyAndChallenge;
    /* DER encoded spkc.pubKeyAndChallenge and spkc */
    CSSM_DATA		encodedPkc = {0, NULL};		
    CSSM_DATA		encodedSpkc = {0, NULL};
    CSSM_DATA		signature = {0, NULL};
    PRErrorCode		perr;
    unsigned char	*spkcB64 = NULL;		// base64 encoded encodedSpkc
    unsigned		spkcB64Len;
    
    /* Cook up a key pair, just use any old params for now */
    ortn = SecKeyCreatePair(nil,		// in default KC
                            GNR_KEY_ALG,					// normally spec'd by user
                            keySize,				// key size, ditto
                            0,								// ContextHandle
                            CSSM_KEYUSE_ANY,				// might want to restrict this
                            CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | 
                            CSSM_KEYATTR_RETURN_REF,	// pub attrs
                            CSSM_KEYUSE_ANY,				// might want to restrict this
                            CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF |
                            CSSM_KEYATTR_PERMANENT |CSSM_KEYATTR_EXTRACTABLE,
                            /*
                             * FIXME: should have a non-NULL initialAccess here, but
                             * I do not know any easy way of doing that. Ask Perry
                             * (perry@apple.com) or MIchael (mb@apple.com).
                             */
                            NULL,
                            &pubKey,
                            &privKey);
    if (ortn) {
        ERROR("***SecKeyCreatePair", ortn);
        goto errOut;
    }
    
    /* get handle of CSPDL for crypto ops */
    ortn = SecKeyGetCSPHandle(privKey, &cspHand);
    if (ortn) {
        ERROR("***SecKeyGetCSPHandle", ortn);
        goto errOut;
    }
    
    /*
     * Get the public key in encoded SubjectPublicKeyInfo form.
     */
    ortn = gnrGetSubjPubKey(cspHand, pubKey, &subjectPubKey);
    if (ortn) {
        goto errOut;
    }
    freeSubjPubKey = true;
    
    /*
     * Cook up PublicKeyAndChallenge and DER-encode it.
     * First, DER-decode the key's SubjectPublicKeyInfo.
     */
    memset(&spkc, 0, sizeof(spkc));
    perr = coder.decodeItem(subjectPubKey.KeyData, 
                            NSS_SubjectPublicKeyInfoTemplate,
                            &pkc->spki);
    if (perr) {
        /* should never happen */
        ERROR("***Error decoding subject public key info\n");
        goto errOut;
    }
    pkc->challenge.Data = (uint8 *)challenge;
    pkc->challenge.Length = strlen(challenge);
    perr = coder.encodeItem(pkc, 
                            PublicKeyAndChallengeTemplate,
                            encodedPkc);
    if (perr) {
        /* should never happen */
        ERROR("***Error enccoding PublicKeyAndChallenge\n");
        goto errOut;
    }
    
    /*
     * Sign the encoded PublicKeyAndChallenge.
     */
    crtn = gnrSign(cspHand, &encodedPkc, privKey,
                   GNR_SIG_ALG, &signature);
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
    perr = coder.encodeItem(&spkc, 
                            SignedPublicKeyAndChallengeTemplate,
                            encodedSpkc);
    if (perr) {
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
    return spkcB64;
}	