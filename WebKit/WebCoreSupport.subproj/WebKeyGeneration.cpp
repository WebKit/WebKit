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

#import <WebKit/WebKeyGeneration.h>

#ifndef USE_NEW_KEY_GENERATION

#import <WebKit/WebAssertions.h>

#include <Security/cuCdsaUtils.h>               /* private libCdsaUtils.a */
#include <Security/cuFileIo.h>                  /* private libCdsaUtils.a */
#include <Security/cuEnc64.h>                   /* private libCdsaUtils.a */
#include <SecurityNssAsn1/SecNssCoder.h>	/* private libnssasn1.a */
#include <SecurityNssAsn1/nssUtils.h>		/* private libnssasn1.a */
#include <Security/Security.h>
#include <Security/SecKeyPriv.h>                /* Security.framework SPI */

#include <Security/KeyItem.h>
#include <Security/SecBridge.h>
#include <Security/wrapkey.h>
#include <Security/cfutilities.h>


/* hard coded params, some of which may come from the user in real life */
#define GNR_KEY_ALG			CSSM_ALGID_RSA
#define GNR_SIG_ALG			CSSM_ALGID_MD5WithRSA
#define GNR_SIG_ALGOID                  CSSMOID_MD5WithRSA

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




/* copied code */

// @@@ This needs to be shared.
static CSSM_DB_NAME_ATTR(kSecKeyPrintName, 1, (char *)"PrintName", 0, NULL, BLOB);
static CSSM_DB_NAME_ATTR(kSecKeyLabel, 6, (char *)"Label", 0, NULL, BLOB);

static void 
createPair(
	Keychain keychain,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	CFStringRef description,
	SecPointer<KeyItem> &outPublicKey, 
	SecPointer<KeyItem> &outPrivateKey)
{
	bool freeKeys = false;
	bool deleteContext = false;

	if (!keychain->database()->dl()->subserviceMask() & CSSM_SERVICE_CSP)
		MacOSError::throwMe(errSecInvalidKeychain);

	SSDb ssDb(safe_cast<SSDbImpl *>(&(*keychain->database())));
	CssmClient::CSP csp(keychain->csp());
	CssmClient::CSP appleCsp(gGuidAppleCSP);

	// Generate a random label to use initially
	CssmClient::Random random(appleCsp, CSSM_ALGID_APPLE_YARROW);
	uint8 labelBytes[20];
	CssmData label(labelBytes, sizeof(labelBytes));
	random.generate(label, label.Length);

	CSSM_KEY publicCssmKey, privateCssmKey;
	memset(&publicCssmKey, 0, sizeof(publicCssmKey));
	memset(&privateCssmKey, 0, sizeof(privateCssmKey));

	CSSM_CC_HANDLE ccHandle = 0;

	try
	{
		CSSM_RETURN status;
		if (contextHandle)
				ccHandle = contextHandle;
		else
		{
			status = CSSM_CSP_CreateKeyGenContext(csp->handle(), algorithm, keySizeInBits, NULL, NULL, NULL, NULL, NULL, &ccHandle);
			if (status)
				CssmError::throwMe(status);
			deleteContext = true;
		}
	
		CSSM_DL_DB_HANDLE dldbHandle = ssDb->handle();
		CSSM_DL_DB_HANDLE_PTR dldbHandlePtr = &dldbHandle;
		CSSM_CONTEXT_ATTRIBUTE contextAttributes = { CSSM_ATTRIBUTE_DL_DB_HANDLE, sizeof(dldbHandle), { (char *)dldbHandlePtr } };
		status = CSSM_UpdateContextAttributes(ccHandle, 1, &contextAttributes);
		if (status)
			CssmError::throwMe(status);
	
		// Generate the keypair
		status = CSSM_GenerateKeyPair(ccHandle, publicKeyUsage, publicKeyAttr, &label, &publicCssmKey, privateKeyUsage, privateKeyAttr, &label, NULL, &privateCssmKey);
		if (status)
			CssmError::throwMe(status);
		freeKeys = true;

		// Find the keys we just generated in the DL to get SecKeyRef's to them
		// so we can change the label to be the hash of the public key, and
		// fix up other attributes.

		// Look up public key in the DLDB.
		DbAttributes pubDbAttributes;
		DbUniqueRecord pubUniqueId;
		SSDbCursor dbPubCursor(ssDb, 1);
		dbPubCursor->recordType(CSSM_DL_DB_RECORD_PUBLIC_KEY);
		dbPubCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, label);
		CssmClient::Key publicKey;
		if (!dbPubCursor->nextKey(&pubDbAttributes, publicKey, pubUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Look up private key in the DLDB.
		DbAttributes privDbAttributes;
		DbUniqueRecord privUniqueId;
		SSDbCursor dbPrivCursor(ssDb, 1);
		dbPrivCursor->recordType(CSSM_DL_DB_RECORD_PRIVATE_KEY);
		dbPrivCursor->add(CSSM_DB_EQUAL, kSecKeyLabel, label);
		CssmClient::Key privateKey;
		if (!dbPrivCursor->nextKey(&privDbAttributes, privateKey, privUniqueId))
			MacOSError::throwMe(errSecItemNotFound);

		// Convert reference public key to a raw key so we can use it
		// in the appleCsp.
		CssmClient::WrapKey wrap(csp, CSSM_ALGID_NONE);
		CssmClient::Key rawPubKey = wrap(publicKey);

		// Calculate the hash of the public key using the appleCSP.
		CssmClient::PassThrough passThrough(appleCsp);
		void *outData;
		CssmData *cssmData;

		/* Given a CSSM_KEY_PTR in any format, obtain the SHA-1 hash of the 
		* associated key blob. 
		* Key is specified in CSSM_CSP_CreatePassThroughContext.
		* Hash is allocated bythe CSP, in the App's memory, and returned
		* in *outData. */
		passThrough.key(rawPubKey);
		passThrough(CSSM_APPLECSP_KEYDIGEST, NULL, &outData);
		cssmData = reinterpret_cast<CssmData *>(outData);
		CssmData &pubKeyHash = *cssmData;

		std::string desc = cfString(description);
		// Set the label of the public key to the public key hash.
		// Set the PrintName of the public key to the description in the acl.
		pubDbAttributes.add(kSecKeyLabel, pubKeyHash);
		pubDbAttributes.add(kSecKeyPrintName, desc);
		pubUniqueId->modify(CSSM_DL_DB_RECORD_PUBLIC_KEY, &pubDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// Set the label of the private key to the public key hash.
		// Set the PrintName of the private key to the description in the acl.
		privDbAttributes.add(kSecKeyLabel, pubKeyHash);
		privDbAttributes.add(kSecKeyPrintName, desc);
		privUniqueId->modify(CSSM_DL_DB_RECORD_PRIVATE_KEY, &privDbAttributes, NULL, CSSM_DB_MODIFY_ATTRIBUTE_REPLACE);

		// @@@ Not exception safe!
		csp.allocator().free(cssmData->Data);
		csp.allocator().free(cssmData);

		// Create keychain items which will represent the keys.
		outPublicKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PUBLIC_KEY, pubUniqueId)));
		outPrivateKey = safe_cast<KeyItem*>(&(*keychain->item(CSSM_DL_DB_RECORD_PRIVATE_KEY, privUniqueId)));
	}
	catch (...)
	{
		if (freeKeys)
		{
			// Delete the keys if something goes wrong so we don't end up with inaccesable keys in the database.
			CSSM_FreeKey(csp->handle(), NULL, &publicCssmKey, TRUE);
			CSSM_FreeKey(csp->handle(), NULL, &privateCssmKey, TRUE);
		}
		
		if (deleteContext)
			CSSM_DeleteContext(ccHandle);

		throw;
	}

	if (freeKeys)
	{
		CSSM_FreeKey(csp->handle(), NULL, &publicCssmKey, FALSE);
		CSSM_FreeKey(csp->handle(), NULL, &privateCssmKey, FALSE);
	}

	if (deleteContext)
		CSSM_DeleteContext(ccHandle);
}


static OSStatus
Safari_SecKeyCreatePair(
	SecKeychainRef keychainRef,
	CSSM_ALGORITHMS algorithm,
	uint32 keySizeInBits,
	CSSM_CC_HANDLE contextHandle,
	CSSM_KEYUSE publicKeyUsage,
	uint32 publicKeyAttr,
	CSSM_KEYUSE privateKeyUsage,
	uint32 privateKeyAttr,
	CFStringRef description,
	SecKeyRef* publicKeyRef, 
	SecKeyRef* privateKeyRef)
{
	BEGIN_SECAPI

	Keychain keychain = Keychain::optional(keychainRef);
	SecPointer<KeyItem> pubItem, privItem;

	createPair(keychain,
        algorithm,
        keySizeInBits,
        contextHandle,
        publicKeyUsage,
        publicKeyAttr,
        privateKeyUsage,
        privateKeyAttr,
	description,
        pubItem,
        privItem);

	// Return the generated keys.
	if (publicKeyRef)
		*publicKeyRef = pubItem->handle();
	if (privateKeyRef)
		*privateKeyRef = privItem->handle();

	END_SECAPI
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
    CFStringRef         result = NULL;

    // Cook up a key pair, just use any old params for now
    ortn = Safari_SecKeyCreatePair(nil,                                        // in default KC
                            GNR_KEY_ALG,                                // normally spec'd by user
                            keySize,                                    // key size, ditto
                            0,                                          // ContextHandle
                            CSSM_KEYUSE_ANY,                            // might want to restrict this
                            CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE | 
                            CSSM_KEYATTR_RETURN_REF,                    // pub attrs
                            CSSM_KEYUSE_ANY,				// might want to restrict this
                            CSSM_KEYATTR_SENSITIVE | CSSM_KEYATTR_RETURN_REF |
                            CSSM_KEYATTR_PERMANENT | CSSM_KEYATTR_EXTRACTABLE,
			    keyDescription,
                            &pubKey,
                            &privKey);
    if (ortn) {
        ERROR("***SecKeyCreatePair %d", ortn);
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
    perr = coder.decodeItem(subjectPubKey.KeyData, NSS_SubjectPublicKeyInfoTemplate, &pkc->spki);
    if (perr) {
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
    perr = coder.encodeItem(pkc, PublicKeyAndChallengeTemplate, encodedPkc);
    if (perr) {
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
    if (ortn) {
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
    SecNssCoder coder;
    NetscapeCertSequence certSeq;
    
    memset(&certSeq, 0, sizeof(certSeq));
    PRErrorCode perr = coder.decode(bytes, length, NetscapeCertSequenceTemplate, &certSeq);
    if (perr == 0) {
        if (certSeq.contentType.Length == CSSMOID_PKCS7_SignedData.Length &&
            memcmp(certSeq.contentType.Data, CSSMOID_PKCS7_SignedData.Data, certSeq.contentType.Length) == 0) {
            return WebCertificateParseResultPKCS7;
        }
        /*
         * Last cert is a root, which we do NOT want to add
         * to the user's keychain.
         */
        unsigned numCerts = nssArraySize((const void **)certSeq.certs) - 1;
        for (unsigned i=0; i<numCerts; i++) {
            CSSM_DATA *cert = certSeq.certs[i];
            result = addCertificateToKeychainFromData(cert->Data, cert->Length, i) ? WebCertificateParseResultSucceeded : WebCertificateParseResultFailed;
        } 
    } else {
        /*
         * Didn't appear to be a NetscapeCertSequence; assume it's just 
         * a cert. FIXME: Netscape spec says the blob might also be PKCS7
         * format, which we're not handling here.
         */
        result = addCertificateToKeychainFromData(static_cast<const unsigned char *>(bytes), length, 0) ? WebCertificateParseResultSucceeded : WebCertificateParseResultFailed;
    }

    return result;
}

#endif /* USE_NEW_KEY_GENERATION */
