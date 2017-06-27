/* Copyright (C) 1995-1998 Eric Young (eay@cryptsoft.com)
 * All rights reserved.
 *
 * This package is an SSL implementation written
 * by Eric Young (eay@cryptsoft.com).
 * The implementation was written so as to conform with Netscapes SSL.
 * 
 * This library is free for commercial and non-commercial use as long as
 * the following conditions are aheared to.  The following conditions
 * apply to all code found in this distribution, be it the RC4, RSA,
 * lhash, DES, etc., code; not just the SSL code.  The SSL documentation
 * included with this distribution is covered by the same copyright terms
 * except that the holder is Tim Hudson (tjh@cryptsoft.com).
 * 
 * Copyright remains Eric Young's, and as such any Copyright notices in
 * the code are not to be removed.
 * If this package is used in a product, Eric Young should be given attribution
 * as the author of the parts of the library used.
 * This can be in the form of a textual message at program startup or
 * in documentation (online or textual) provided with the package.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *    "This product includes cryptographic software written by
 *     Eric Young (eay@cryptsoft.com)"
 *    The word 'cryptographic' can be left out if the rouines from the library
 *    being used are not cryptographic related :-).
 * 4. If you include any Windows specific code (or a derivative thereof) from 
 *    the apps directory (application code) you must include an acknowledgement:
 *    "This product includes software written by Tim Hudson (tjh@cryptsoft.com)"
 * 
 * THIS SOFTWARE IS PROVIDED BY ERIC YOUNG ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 * The licence and distribution terms for any publically available version or
 * derivative of this code cannot be changed.  i.e. this code cannot simply be
 * copied and put under another distribution licence
 * [including the GNU Public Licence.]
 */
/* ====================================================================
 * Copyright 2005 Nokia. All rights reserved.
 *
 * The portions of the attached software ("Contribution") is developed by
 * Nokia Corporation and is licensed pursuant to the OpenSSL open source
 * license.
 *
 * The Contribution, originally written by Mika Kousa and Pasi Eronen of
 * Nokia Corporation, consists of the "PSK" (Pre-Shared Key) ciphersuites
 * support (see RFC 4279) to OpenSSL.
 *
 * No patent licenses or other rights except those expressly stated in
 * the OpenSSL open source license shall be deemed granted or received
 * expressly, by implication, estoppel, or otherwise.
 *
 * No assurances are provided by Nokia that the Contribution does not
 * infringe the patent or other intellectual property rights of any third
 * party or that the license provides you with all the necessary rights
 * to make use of the Contribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND. IN
 * ADDITION TO THE DISCLAIMERS INCLUDED IN THE LICENSE, NOKIA
 * SPECIFICALLY DISCLAIMS ANY LIABILITY FOR CLAIMS BROUGHT BY YOU OR ANY
 * OTHER ENTITY BASED ON INFRINGEMENT OF INTELLECTUAL PROPERTY RIGHTS OR
 * OTHERWISE.
 */

#include <openssl/ssl.h>

#include <assert.h>

#include "internal.h"


static int ssl_state(const SSL *ssl) {
  if (ssl->s3->hs == NULL) {
    assert(ssl->s3->initial_handshake_complete);
    return SSL_ST_OK;
  }

  return ssl->s3->hs->state;
}

const char *SSL_state_string_long(const SSL *ssl) {
  switch (ssl_state(ssl)) {
    case SSL_ST_ACCEPT:
      return "before accept initialization";

    case SSL_ST_CONNECT:
      return "before connect initialization";

    case SSL_ST_OK:
      return "SSL negotiation finished successfully";

    case SSL_ST_RENEGOTIATE:
      return "SSL renegotiate ciphers";

    /* SSLv3 additions */
    case SSL3_ST_CW_CLNT_HELLO_A:
      return "SSLv3 write client hello A";

    case SSL3_ST_CR_SRVR_HELLO_A:
      return "SSLv3 read server hello A";

    case SSL3_ST_CR_CERT_A:
      return "SSLv3 read server certificate A";

    case SSL3_ST_CR_KEY_EXCH_A:
      return "SSLv3 read server key exchange A";

    case SSL3_ST_CR_CERT_REQ_A:
      return "SSLv3 read server certificate request A";

    case SSL3_ST_CR_SESSION_TICKET_A:
      return "SSLv3 read server session ticket A";

    case SSL3_ST_CR_SRVR_DONE_A:
      return "SSLv3 read server done A";

    case SSL3_ST_CW_CERT_A:
      return "SSLv3 write client certificate A";

    case SSL3_ST_CW_KEY_EXCH_A:
      return "SSLv3 write client key exchange A";

    case SSL3_ST_CW_CERT_VRFY_A:
      return "SSLv3 write certificate verify A";

    case SSL3_ST_CW_CERT_VRFY_B:
      return "SSLv3 write certificate verify B";

    case SSL3_ST_CW_CHANGE:
    case SSL3_ST_SW_CHANGE:
      return "SSLv3 write change cipher spec";

    case SSL3_ST_CW_FINISHED_A:
    case SSL3_ST_SW_FINISHED_A:
      return "SSLv3 write finished A";

    case SSL3_ST_CR_CHANGE:
    case SSL3_ST_SR_CHANGE:
      return "SSLv3 read change cipher spec";

    case SSL3_ST_CR_FINISHED_A:
    case SSL3_ST_SR_FINISHED_A:
      return "SSLv3 read finished A";

    case SSL3_ST_CW_FLUSH:
    case SSL3_ST_SW_FLUSH:
      return "SSLv3 flush data";

    case SSL3_ST_SR_CLNT_HELLO_A:
      return "SSLv3 read client hello A";

    case SSL3_ST_SR_CLNT_HELLO_B:
      return "SSLv3 read client hello B";

    case SSL3_ST_SR_CLNT_HELLO_C:
      return "SSLv3 read client hello C";

    case SSL3_ST_SW_SRVR_HELLO_A:
      return "SSLv3 write server hello A";

    case SSL3_ST_SW_CERT_A:
      return "SSLv3 write certificate A";

    case SSL3_ST_SW_KEY_EXCH_A:
      return "SSLv3 write key exchange A";

    case SSL3_ST_SW_CERT_REQ_A:
      return "SSLv3 write certificate request A";

    case SSL3_ST_SW_SESSION_TICKET_A:
      return "SSLv3 write session ticket A";

    case SSL3_ST_SW_SRVR_DONE_A:
      return "SSLv3 write server done A";

    case SSL3_ST_SR_CERT_A:
      return "SSLv3 read client certificate A";

    case SSL3_ST_SR_KEY_EXCH_A:
      return "SSLv3 read client key exchange A";

    case SSL3_ST_SR_KEY_EXCH_B:
      return "SSLv3 read client key exchange B";

    case SSL3_ST_SR_CERT_VRFY_A:
      return "SSLv3 read certificate verify A";

    /* DTLS */
    case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_A:
      return "DTLS1 read hello verify request A";

    default:
      return "unknown state";
  }
}

const char *SSL_state_string(const SSL *ssl) {
  switch (ssl_state(ssl)) {
    case SSL_ST_ACCEPT:
      return "AINIT ";

    case SSL_ST_CONNECT:
      return "CINIT ";

    case SSL_ST_OK:
      return "SSLOK ";

    /* SSLv3 additions */
    case SSL3_ST_SW_FLUSH:
    case SSL3_ST_CW_FLUSH:
      return "3FLUSH";

    case SSL3_ST_CW_CLNT_HELLO_A:
      return "3WCH_A";

    case SSL3_ST_CR_SRVR_HELLO_A:
      return "3RSH_A";

    case SSL3_ST_CR_CERT_A:
      return "3RSC_A";

    case SSL3_ST_CR_KEY_EXCH_A:
      return "3RSKEA";

    case SSL3_ST_CR_CERT_REQ_A:
      return "3RCR_A";

    case SSL3_ST_CR_SRVR_DONE_A:
      return "3RSD_A";

    case SSL3_ST_CW_CERT_A:
      return "3WCC_A";

    case SSL3_ST_CW_KEY_EXCH_A:
      return "3WCKEA";

    case SSL3_ST_CW_CERT_VRFY_A:
      return "3WCV_A";

    case SSL3_ST_CW_CERT_VRFY_B:
      return "3WCV_B";

    case SSL3_ST_SW_CHANGE:
    case SSL3_ST_CW_CHANGE:
      return "3WCCS_";

    case SSL3_ST_SW_FINISHED_A:
    case SSL3_ST_CW_FINISHED_A:
      return "3WFINA";

    case SSL3_ST_CR_CHANGE:
    case SSL3_ST_SR_CHANGE:
      return "3RCCS_";

    case SSL3_ST_SR_FINISHED_A:
    case SSL3_ST_CR_FINISHED_A:
      return "3RFINA";

    case SSL3_ST_SR_CLNT_HELLO_A:
      return "3RCH_A";

    case SSL3_ST_SR_CLNT_HELLO_B:
      return "3RCH_B";

    case SSL3_ST_SR_CLNT_HELLO_C:
      return "3RCH_C";

    case SSL3_ST_SW_SRVR_HELLO_A:
      return "3WSH_A";

    case SSL3_ST_SW_CERT_A:
      return "3WSC_A";

    case SSL3_ST_SW_KEY_EXCH_A:
      return "3WSKEA";

    case SSL3_ST_SW_KEY_EXCH_B:
      return "3WSKEB";

    case SSL3_ST_SW_CERT_REQ_A:
      return "3WCR_A";

    case SSL3_ST_SW_SRVR_DONE_A:
      return "3WSD_A";

    case SSL3_ST_SR_CERT_A:
      return "3RCC_A";

    case SSL3_ST_SR_KEY_EXCH_A:
      return "3RCKEA";

    case SSL3_ST_SR_CERT_VRFY_A:
      return "3RCV_A";

    /* DTLS */
    case DTLS1_ST_CR_HELLO_VERIFY_REQUEST_A:
      return "DRCHVA";

    default:
      return "UNKWN ";
  }
}

const char *SSL_alert_type_string_long(int value) {
  value >>= 8;
  if (value == SSL3_AL_WARNING) {
    return "warning";
  } else if (value == SSL3_AL_FATAL) {
    return "fatal";
  }

  return "unknown";
}

const char *SSL_alert_type_string(int value) {
  return "!";
}

const char *SSL_alert_desc_string(int value) {
  return "!!";
}

const char *SSL_alert_desc_string_long(int value) {
  switch (value & 0xff) {
    case SSL3_AD_CLOSE_NOTIFY:
      return "close notify";

    case SSL3_AD_UNEXPECTED_MESSAGE:
      return "unexpected_message";

    case SSL3_AD_BAD_RECORD_MAC:
      return "bad record mac";

    case SSL3_AD_DECOMPRESSION_FAILURE:
      return "decompression failure";

    case SSL3_AD_HANDSHAKE_FAILURE:
      return "handshake failure";

    case SSL3_AD_NO_CERTIFICATE:
      return "no certificate";

    case SSL3_AD_BAD_CERTIFICATE:
      return "bad certificate";

    case SSL3_AD_UNSUPPORTED_CERTIFICATE:
      return "unsupported certificate";

    case SSL3_AD_CERTIFICATE_REVOKED:
      return "certificate revoked";

    case SSL3_AD_CERTIFICATE_EXPIRED:
      return "certificate expired";

    case SSL3_AD_CERTIFICATE_UNKNOWN:
      return "certificate unknown";

    case SSL3_AD_ILLEGAL_PARAMETER:
      return "illegal parameter";

    case TLS1_AD_DECRYPTION_FAILED:
      return "decryption failed";

    case TLS1_AD_RECORD_OVERFLOW:
      return "record overflow";

    case TLS1_AD_UNKNOWN_CA:
      return "unknown CA";

    case TLS1_AD_ACCESS_DENIED:
      return "access denied";

    case TLS1_AD_DECODE_ERROR:
      return "decode error";

    case TLS1_AD_DECRYPT_ERROR:
      return "decrypt error";

    case TLS1_AD_EXPORT_RESTRICTION:
      return "export restriction";

    case TLS1_AD_PROTOCOL_VERSION:
      return "protocol version";

    case TLS1_AD_INSUFFICIENT_SECURITY:
      return "insufficient security";

    case TLS1_AD_INTERNAL_ERROR:
      return "internal error";

    case SSL3_AD_INAPPROPRIATE_FALLBACK:
      return "inappropriate fallback";

    case TLS1_AD_USER_CANCELLED:
      return "user canceled";

    case TLS1_AD_NO_RENEGOTIATION:
      return "no renegotiation";

    case TLS1_AD_UNSUPPORTED_EXTENSION:
      return "unsupported extension";

    case TLS1_AD_CERTIFICATE_UNOBTAINABLE:
      return "certificate unobtainable";

    case TLS1_AD_UNRECOGNIZED_NAME:
      return "unrecognized name";

    case TLS1_AD_BAD_CERTIFICATE_STATUS_RESPONSE:
      return "bad certificate status response";

    case TLS1_AD_BAD_CERTIFICATE_HASH_VALUE:
      return "bad certificate hash value";

    case TLS1_AD_UNKNOWN_PSK_IDENTITY:
      return "unknown PSK identity";

    case TLS1_AD_CERTIFICATE_REQUIRED:
      return "certificate required";

    default:
      return "unknown";
  }
}
