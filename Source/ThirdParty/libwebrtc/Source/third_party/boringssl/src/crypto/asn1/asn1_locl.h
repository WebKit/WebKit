/* asn1t.h */
/*
 * Written by Dr Stephen N Henson (steve@openssl.org) for the OpenSSL project
 * 2006.
 */
/* ====================================================================
 * Copyright (c) 2006 The OpenSSL Project.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * 3. All advertising materials mentioning features or use of this
 *    software must display the following acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit. (http://www.OpenSSL.org/)"
 *
 * 4. The names "OpenSSL Toolkit" and "OpenSSL Project" must not be used to
 *    endorse or promote products derived from this software without
 *    prior written permission. For written permission, please contact
 *    licensing@OpenSSL.org.
 *
 * 5. Products derived from this software may not be called "OpenSSL"
 *    nor may "OpenSSL" appear in their names without prior written
 *    permission of the OpenSSL Project.
 *
 * 6. Redistributions of any form whatsoever must retain the following
 *    acknowledgment:
 *    "This product includes software developed by the OpenSSL Project
 *    for use in the OpenSSL Toolkit (http://www.OpenSSL.org/)"
 *
 * THIS SOFTWARE IS PROVIDED BY THE OpenSSL PROJECT ``AS IS'' AND ANY
 * EXPRESSED OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE OpenSSL PROJECT OR
 * ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 * ====================================================================
 *
 * This product includes cryptographic software written by Eric Young
 * (eay@cryptsoft.com).  This product includes software written by Tim
 * Hudson (tjh@cryptsoft.com).
 *
 */

#ifndef OPENSSL_HEADER_ASN1_ASN1_LOCL_H
#define OPENSSL_HEADER_ASN1_ASN1_LOCL_H

#include <time.h>

#include <openssl/asn1.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Wrapper functions for time functions. */

/* OPENSSL_gmtime wraps |gmtime_r|. See the manual page for that function. */
struct tm *OPENSSL_gmtime(const time_t *time, struct tm *result);

/* OPENSSL_gmtime_adj updates |tm| by adding |offset_day| days and |offset_sec|
 * seconds. */
int OPENSSL_gmtime_adj(struct tm *tm, int offset_day, long offset_sec);

/* OPENSSL_gmtime_diff calculates the difference between |from| and |to| and
 * outputs the difference as a number of days and seconds in |*out_days| and
 * |*out_secs|. */
int OPENSSL_gmtime_diff(int *out_days, int *out_secs, const struct tm *from,
                        const struct tm *to);


/* Internal ASN1 structures and functions: not for application use */

int asn1_utctime_to_tm(struct tm *tm, const ASN1_UTCTIME *d);
int asn1_generalizedtime_to_tm(struct tm *tm, const ASN1_GENERALIZEDTIME *d);

void asn1_item_combine_free(ASN1_VALUE **pval, const ASN1_ITEM *it,
                            int combine);

int UTF8_getc(const unsigned char *str, int len, uint32_t *val);
int UTF8_putc(unsigned char *str, int len, uint32_t value);

int ASN1_item_ex_new(ASN1_VALUE **pval, const ASN1_ITEM *it);
void ASN1_item_ex_free(ASN1_VALUE **pval, const ASN1_ITEM *it);

void ASN1_template_free(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);
int ASN1_item_ex_d2i(ASN1_VALUE **pval, const unsigned char **in, long len,
                     const ASN1_ITEM *it, int tag, int aclass, char opt,
                     ASN1_TLC *ctx);

int ASN1_item_ex_i2d(ASN1_VALUE **pval, unsigned char **out,
                     const ASN1_ITEM *it, int tag, int aclass);
void ASN1_primitive_free(ASN1_VALUE **pval, const ASN1_ITEM *it);

int asn1_get_choice_selector(ASN1_VALUE **pval, const ASN1_ITEM *it);
int asn1_set_choice_selector(ASN1_VALUE **pval, int value, const ASN1_ITEM *it);

ASN1_VALUE **asn1_get_field_ptr(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt);

const ASN1_TEMPLATE *asn1_do_adb(ASN1_VALUE **pval, const ASN1_TEMPLATE *tt,
                                 int nullerr);

void asn1_refcount_set_one(ASN1_VALUE **pval, const ASN1_ITEM *it);
int asn1_refcount_dec_and_test_zero(ASN1_VALUE **pval, const ASN1_ITEM *it);

void asn1_enc_init(ASN1_VALUE **pval, const ASN1_ITEM *it);
void asn1_enc_free(ASN1_VALUE **pval, const ASN1_ITEM *it);
int asn1_enc_restore(int *len, unsigned char **out, ASN1_VALUE **pval,
                     const ASN1_ITEM *it);
int asn1_enc_save(ASN1_VALUE **pval, const unsigned char *in, int inlen,
                  const ASN1_ITEM *it);

/* asn1_type_value_as_pointer returns |a|'s value in pointer form. This is
 * usually the value object but, for BOOLEAN values, is 0 or 0xff cast to
 * a pointer. */
const void *asn1_type_value_as_pointer(const ASN1_TYPE *a);


#if defined(__cplusplus)
}  /* extern C */
#endif

#endif  /* OPENSSL_HEADER_ASN1_ASN1_LOCL_H */
