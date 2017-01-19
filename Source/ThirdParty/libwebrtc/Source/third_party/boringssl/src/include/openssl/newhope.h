/* Copyright (c) 2016, Google Inc.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef OPENSSL_HEADER_NEWHOPE_H
#define OPENSSL_HEADER_NEWHOPE_H

#include <openssl/base.h>
#include <openssl/sha.h>

#if defined(__cplusplus)
extern "C" {
#endif


/* Post-quantum key agreement, based upon the reference
 * implementation. Note: this implementation does not interoperate
 * with the reference implementation!
 *
 * Source: https://github.com/tpoeppelmann/newhope
 *
 * The authors' permission to use their code is gratefully acknowledged. */


/* NEWHOPE_POLY_new returns a new |NEWHOPE_POLY| object, or NULL on error. */
OPENSSL_EXPORT NEWHOPE_POLY *NEWHOPE_POLY_new(void);

/* NEWHOPE_POLY_free frees |p|. */
OPENSSL_EXPORT void NEWHOPE_POLY_free(NEWHOPE_POLY *p);

/* NEWHOPE_POLY_LENGTH is the size in bytes of the packed representation of a
 * polynomial, encoded with 14 bits per coefficient. */
#define NEWHOPE_POLY_LENGTH ((1024 * 14) / 8)

/* NEWHOPE_RECONCILIATION_LENGTH is the size in bytes of the packed
 * representation of the reconciliation data, encoded as 2 bits per
 * coefficient. */
#define NEWHOPE_RECONCILIATION_LENGTH ((1024 * 2) / 8)

/* NEWHOPE_OFFERMSG_LENGTH is the length of the offering party's message to the
 * accepting party. */
#define NEWHOPE_OFFERMSG_LENGTH (NEWHOPE_POLY_LENGTH + 32)

/* NEWHOPE_ACCEPTMSG_LENGTH is the length of the accepting party's message to
 * the offering party. */
#define NEWHOPE_ACCEPTMSG_LENGTH \
  (NEWHOPE_POLY_LENGTH + NEWHOPE_RECONCILIATION_LENGTH)

/* NEWHOPE_KEY_LENGTH is the size of the result of the key agreement. This
 * result is not exposed to callers: instead, it is whitened with SHA-256, whose
 * output happens to be the same size. */
#define NEWHOPE_KEY_LENGTH 32

/* NEWHOPE_offer initializes |out_msg| and |out_sk| for a new key
 * exchange. |msg| must have room for |NEWHOPE_OFFERMSG_LENGTH| bytes. Neither
 * output may be cached. */
OPENSSL_EXPORT void NEWHOPE_offer(uint8_t out_msg[NEWHOPE_OFFERMSG_LENGTH],
                                  NEWHOPE_POLY *out_sk);

/* NEWHOPE_accept completes a key exchange given an offer message |msg|. The
 * result of the key exchange is written to |out_key|, which must have space for
 * |SHA256_DIGEST_LENGTH| bytes. The message to be send to the offering party is
 * written to |out_msg|, which must have room for |NEWHOPE_ACCEPTMSG_LENGTH|
 * bytes. Returns 1 on success and 0 on error. */
OPENSSL_EXPORT int NEWHOPE_accept(uint8_t out_key[SHA256_DIGEST_LENGTH],
                                  uint8_t out_msg[NEWHOPE_ACCEPTMSG_LENGTH],
                                  const uint8_t msg[NEWHOPE_OFFERMSG_LENGTH],
                                  size_t msg_len);

/* NEWHOPE_finish completes a key exchange for the offering party, given an
 * accept message |msg| and the previously generated secret |sk|. The result of
 * the key exchange is written to |out_key|, which must have space for
 * |SHA256_DIGEST_LENGTH| bytes. Returns 1 on success and 0 on error. */
OPENSSL_EXPORT int NEWHOPE_finish(uint8_t out_key[SHA256_DIGEST_LENGTH],
                                  const NEWHOPE_POLY *sk,
                                  const uint8_t msg[NEWHOPE_ACCEPTMSG_LENGTH],
                                  size_t msg_len);


/* Lower-level functions. */

/* NEWHOPE_POLY_noise sets |r| to a random polynomial where the coefficients are
 * sampled from the noise distribution. */
OPENSSL_EXPORT void NEWHOPE_POLY_noise(NEWHOPE_POLY* r);

/* NEWHOPE_POLY_noise_ntt sets |r| to an output of NEWHOPE_POLY_noise, and then
 * applies NTT(r) in-place. */
OPENSSL_EXPORT void NEWHOPE_POLY_noise_ntt(NEWHOPE_POLY* r);

/* NEWHOPE_offer_computation is the work of |NEWHOPE_offer|, less the encoding
 * parts.  The inputs are the noise polynomials |s| and |e|, and random
 * polynomial |a|. The output is the polynomial |pk|. */
OPENSSL_EXPORT void NEWHOPE_offer_computation(
    NEWHOPE_POLY *out_pk,
    const NEWHOPE_POLY *s, const NEWHOPE_POLY *e, const NEWHOPE_POLY *a);

/* NEWHOPE_accept_computation is the work of |NEWHOPE_accept|, less the encoding
 * parts. The inputs from the peer are |pk| and |a|. The locally-generated
 * inputs are the noise polynomials |sp|, |ep|, and |epp|, and the random bytes
 * |rand|. The outputs are |out_bp| and |out_reconciliation|, and the result of
 * key agreement |key|. Returns 1 on success and 0 on failure. */
OPENSSL_EXPORT void NEWHOPE_accept_computation(
    uint8_t out_key[NEWHOPE_KEY_LENGTH], NEWHOPE_POLY *out_bp,
    NEWHOPE_POLY *out_reconciliation,
    const NEWHOPE_POLY *sp, const NEWHOPE_POLY *ep, const NEWHOPE_POLY *epp,
    const uint8_t rand[32],
    const NEWHOPE_POLY *pk, const NEWHOPE_POLY *a);

/* NEWHOPE_finish_computation is the work of |NEWHOPE_finish|, less the encoding
 * parts. Given the peer's |bp| and |reconciliation|, and locally-generated
 * noise |noise|, the result of the key agreement is written to out_key.
 * Returns 1 on success and 0 on failure. */
OPENSSL_EXPORT void NEWHOPE_finish_computation(
    uint8_t out_key[NEWHOPE_KEY_LENGTH], const NEWHOPE_POLY *noise,
    const NEWHOPE_POLY *bp, const NEWHOPE_POLY *reconciliation);

/* NEWHOPE_POLY_frombytes decodes |a| into |r|. */
OPENSSL_EXPORT void NEWHOPE_POLY_frombytes(
    NEWHOPE_POLY *r, const uint8_t a[NEWHOPE_POLY_LENGTH]);

/* NEWHOPE_POLY_tobytes packs the polynomial |p| into the compact representation
 * |r|. */
OPENSSL_EXPORT void NEWHOPE_POLY_tobytes(uint8_t r[NEWHOPE_POLY_LENGTH],
                                         const NEWHOPE_POLY* p);

/* NEWHOPE_offer_frommsg decodes an offer message |msg| into its constituent
 * polynomials |out_pk| and |a|. */
OPENSSL_EXPORT void NEWHOPE_offer_frommsg(
    NEWHOPE_POLY *out_pk, NEWHOPE_POLY *out_a,
    const uint8_t msg[NEWHOPE_OFFERMSG_LENGTH]);


#if defined(__cplusplus)
} /* extern "C" */

extern "C++" {

namespace bssl {

BORINGSSL_MAKE_DELETER(NEWHOPE_POLY, NEWHOPE_POLY_free)

}  // namespace bssl

}  /* extern C++ */

#endif

#endif /* OPENSSL_HEADER_NEWHOPE_H */
