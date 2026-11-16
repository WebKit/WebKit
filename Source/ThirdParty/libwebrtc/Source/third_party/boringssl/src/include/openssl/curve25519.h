// Copyright 2015 The BoringSSL Authors
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPENSSL_HEADER_CURVE25519_H
#define OPENSSL_HEADER_CURVE25519_H

#include <openssl/base.h>  // IWYU pragma: export
#include <openssl/sha2.h>

#if defined(__cplusplus)
extern "C" {
#endif


// Curve25519.
//
// Curve25519 is an elliptic curve. See https://tools.ietf.org/html/rfc7748.


// X25519.
//
// X25519 is the Diffie-Hellman primitive built from curve25519. It is
// sometimes referred to as “curve25519”, but “X25519” is a more precise name.
// See http://cr.yp.to/ecdh.html and https://tools.ietf.org/html/rfc7748.

#define X25519_PRIVATE_KEY_LEN 32
#define X25519_PUBLIC_VALUE_LEN 32
#define X25519_SHARED_KEY_LEN 32

// X25519_keypair sets `out_public_value` and `out_private_key` to a freshly
// generated, public–private key pair.
OPENSSL_EXPORT void X25519_keypair(uint8_t out_public_value[32],
                                   uint8_t out_private_key[32]);

// X25519 writes a shared key to `out_shared_key` that is calculated from the
// given private key and the peer's public value. It returns one on success and
// zero on error.
//
// Don't use the shared key directly, rather use a KDF and also include the two
// public values as inputs.
OPENSSL_EXPORT int X25519(uint8_t out_shared_key[32],
                          const uint8_t private_key[32],
                          const uint8_t peer_public_value[32]);

// X25519_public_from_private calculates a Diffie-Hellman public value from the
// given private key and writes it to `out_public_value`.
OPENSSL_EXPORT void X25519_public_from_private(uint8_t out_public_value[32],
                                               const uint8_t private_key[32]);


// Ed25519.
//
// Ed25519 is a signature scheme using a twisted-Edwards curve that is
// birationally equivalent to curve25519.
//
// Note that, unlike RFC 8032's formulation, our private key representation
// includes a public key suffix to make multiple key signing operations with the
// same key more efficient. The RFC 8032 private key is referred to in this
// implementation as the "seed" and is the first 32 bytes of our private key.

#define ED25519_PRIVATE_KEY_LEN 64
#define ED25519_PUBLIC_KEY_LEN 32
#define ED25519_SIGNATURE_LEN 64

// ED25519_keypair sets `out_public_key` and `out_private_key` to a freshly
// generated, public–private key pair.
OPENSSL_EXPORT void ED25519_keypair(uint8_t out_public_key[32],
                                    uint8_t out_private_key[64]);

// ED25519_sign sets `out_sig` to be a signature of `message_len` bytes from
// `message` using `private_key`. It returns one on success or zero on
// allocation failure.
OPENSSL_EXPORT int ED25519_sign(uint8_t out_sig[64], const uint8_t *message,
                                size_t message_len,
                                const uint8_t private_key[64]);

// ED25519_verify returns one iff `signature` is a valid signature, by
// `public_key` of `message_len` bytes from `message`. It returns zero
// otherwise.
OPENSSL_EXPORT int ED25519_verify(const uint8_t *message, size_t message_len,
                                  const uint8_t signature[64],
                                  const uint8_t public_key[32]);

// ED25519_sign_prehashed sets `out_sig` to be a signature of `sha512_digest`
// using `private_key` following the Ed25519ph algorithm. It returns one on
// success, or zero otherwise. `sha512_digest` must be the 64-byte result of
// hashing the input message with SHA-512. `context` allows for domain
// separation; see https://www.rfc-editor.org/info/rfc8032/#section-8.3.
//
// This function can return zero either on allocation failure, or if
// `context_len` is greater than 255.
//
// Note that Ed25519ph (pre-hashed) and "pure" Ed25519 are different algorithms,
// calling `ED25519_sign` with the digest of a message will *not* return the
// same output as `ED25519_sign_prehashed`.
OPENSSL_EXPORT int ED25519_sign_prehashed(uint8_t out_sig[64],
                                          const uint8_t *context,
                                          size_t context_len,
                                          const uint8_t sha512_digest[64],
                                          const uint8_t private_key[64]);

// ED25519_verify_prehashed returns one if `signature` is a valid signature, by
// `public_key`, of `sha512_digest` following the Ed25519ph algorithm. It
// returns zero otherwise. `sha512_digest` must be the 64-byte result of hashing
// the input message with SHA-512. `context` allows for domain separation; see
// https://www.rfc-editor.org/info/rfc8032/#section-8.3.
//
// This function can return zero either if the signature is invalid, or if
// `context_len` is greater than 255.
//
// Note that Ed25519ph (pre-hashed) and "pure" Ed25519 are different algorithms,
// calling `ED25519_verify` with the digest of a message will *not* return the
// same output as `ED25519_verify_prehashed`.
OPENSSL_EXPORT int ED25519_verify_prehashed(const uint8_t *context,
                                            size_t context_len,
                                            const uint8_t sha512_digest[64],
                                            const uint8_t signature[64],
                                            const uint8_t public_key[32]);

// ED25519_keypair_from_seed calculates a public and private key from an
// Ed25519 “seed”. Seed values are not exposed by this API (although they
// happen to be the first 32 bytes of a private key) so this function is for
// interoperating with systems that may store just a seed instead of a full
// private key.
OPENSSL_EXPORT void ED25519_keypair_from_seed(uint8_t out_public_key[32],
                                              uint8_t out_private_key[64],
                                              const uint8_t seed[32]);


// SPAKE2.
//
// SPAKE2 is a password-authenticated key-exchange. It allows two parties,
// who share a low-entropy secret (i.e. password), to agree on a shared key.
// An attacker can only make one guess of the password per execution of the
// protocol.
//
// See https://tools.ietf.org/html/draft-irtf-cfrg-spake2-02.

// spake2_role_t enumerates the different “roles” in SPAKE2. The protocol
// requires that the symmetry of the two parties be broken so one participant
// must be “Alice” and the other be “Bob”.
enum spake2_role_t {
  spake2_role_alice,
  spake2_role_bob,
};

// SPAKE2_CTX_new creates a new `SPAKE2_CTX` (which can only be used for a
// single execution of the protocol). SPAKE2 requires the symmetry of the two
// parties to be broken which is indicated via `my_role` – each party must pass
// a different value for this argument.
//
// The `my_name` and `their_name` arguments allow optional, opaque names to be
// bound into the protocol. For example MAC addresses, hostnames, usernames
// etc. These values are not exposed and can avoid context-confusion attacks
// when a password is shared between several devices.
OPENSSL_EXPORT SPAKE2_CTX *SPAKE2_CTX_new(enum spake2_role_t my_role,
                                          const uint8_t *my_name,
                                          size_t my_name_len,
                                          const uint8_t *their_name,
                                          size_t their_name_len);

// SPAKE2_CTX_free frees `ctx` and all the resources that it has allocated.
OPENSSL_EXPORT void SPAKE2_CTX_free(SPAKE2_CTX *ctx);

// SPAKE2_MAX_MSG_SIZE is the maximum size of a SPAKE2 message.
#define SPAKE2_MAX_MSG_SIZE 32

// SPAKE2_generate_msg generates a SPAKE2 message given `password`, writes
// it to `out` and sets `*out_len` to the number of bytes written.
//
// At most `max_out_len` bytes are written to `out` and, in order to ensure
// success, `max_out_len` should be at least `SPAKE2_MAX_MSG_SIZE` bytes.
//
// This function can only be called once for a given `SPAKE2_CTX`.
//
// It returns one on success and zero on error.
OPENSSL_EXPORT int SPAKE2_generate_msg(SPAKE2_CTX *ctx, uint8_t *out,
                                       size_t *out_len, size_t max_out_len,
                                       const uint8_t *password,
                                       size_t password_len);

// SPAKE2_MAX_KEY_SIZE is the maximum amount of key material that SPAKE2 will
// produce.
#define SPAKE2_MAX_KEY_SIZE 64

// SPAKE2_process_msg completes the SPAKE2 exchange given the peer's message in
// `their_msg`, writes at most `max_out_key_len` bytes to `out_key` and sets
// `*out_key_len` to the number of bytes written.
//
// The resulting keying material is suitable for:
//    - Using directly in a key-confirmation step: i.e. each side could
//      transmit a hash of their role, a channel-binding value and the key
//      material to prove to the other side that they know the shared key.
//   -  Using as input keying material to HKDF to generate a variety of subkeys
//      for encryption etc.
//
// If `max_out_key_key` is smaller than the amount of key material generated
// then the key is silently truncated. If you want to ensure that no truncation
// occurs then `max_out_key` should be at least `SPAKE2_MAX_KEY_SIZE`.
//
// You must call `SPAKE2_generate_msg` on a given `SPAKE2_CTX` before calling
// this function. On successful return, `ctx` is complete and calling
// `SPAKE2_CTX_free` is the only acceptable operation on it.
//
// Returns one on success or zero on error.
OPENSSL_EXPORT int SPAKE2_process_msg(SPAKE2_CTX *ctx, uint8_t *out_key,
                                      size_t *out_key_len,
                                      size_t max_out_key_len,
                                      const uint8_t *their_msg,
                                      size_t their_msg_len);


// CPace.
//
// See https://datatracker.ietf.org/doc/draft-irtf-cfrg-cpace/

// The role that a party plays in the CPACE key agreement protocol.
enum cpace_role_t {
  // This role corresponds to the party A in the specification.
  cpace_role_initiator,
  // This role corresponds to the party B in the specification.
  cpace_role_responder,
};

// CPACE_CTX_new returns a newly-allocated `CPACE_CTX` object that implements
// one side of the CPace protocol, or nullptr on error. This function implements
// CPACE-X25519-SHA512 in initiator-responder mode, acting as `role` and with
// `password` as the password-related string (PRS).
//
// `channel_id` and `session_id` are the channel identifier (CI) and session
// identifier (sid) values, respectively. Both values must match for the
// exchange to proceed. `assoc_data` is the cleartext associated data (ADa or
// ADb) to be sent to the peer. These values may be empty if not used. See
// Section 4.1 of draft-irtf-cfrg-cpace-21 for details.
//
// The application-level identities of each party in the protocol should be
// incorporated into either `channel_id` or each party's respective `assoc_data`
// values. See Section 10.1 of draft-irtf-cfrg-cpace-21.
//
// Once a `CPACE_CTX` is successfully created, the caller should call
// `CPACE_generate_msg` to generate the message to send to the peer.
OPENSSL_EXPORT CPACE_CTX *CPACE_CTX_new(
    enum cpace_role_t role, const uint8_t *password, size_t password_len,
    const uint8_t *channel_id, size_t channel_id_len, const uint8_t *assoc_data,
    size_t assoc_data_len, const uint8_t *session_id, size_t session_id_len);

// CPACE_CTX_free frees `ctx` and all the resources that it has allocated.
OPENSSL_EXPORT void CPACE_CTX_free(CPACE_CTX *ctx);

#define CPACE_MSG_LEN 32
#define CPACE_SHARED_SECRET_LEN 64

// CPACE_generate_msg generates the message to be sent to the peer. On success,
// it writes the message to `out_msg` and returns one. Otherwise, it returns
// zero. `ctx` must have been allocated by `CPACE_CTX_new`, with no subsequent
// functions called on it.
//
// `out_msg` only contains the Ya or Yb value. It does not include ADa or ADb.
// On success, the caller should separately send `out_msg` and their
// `assoc_data` value to the peer. It is the caller's responsibility to send
// `assoc_data` together with `out_msg`. After receiving the peer's reply, the
// caller should call `CPACE_process_msg` to complete the protocol.
OPENSSL_EXPORT int CPACE_generate_msg(CPACE_CTX *ctx,
                                      uint8_t out_msg[CPACE_MSG_LEN]);

// CPACE_process_msg processes `msg` and `peer_assoc_data` from the peer and
// computes the shared secret, or intermediate session key (ISK). On success, it
// writes the secret to `out_shared_secret` and returns one; it also writes a
// derived session ID if `out_sid` is not NULL. Otherwise, it returns zero.
// `ctx` must have successfully been passed to `CPACE_generate_msg`, with no
// subsequent functions called on it.
//
// `msg` should contain the peer's Ya or Yb value, and `peer_assoc_data` the
// peer's ADa or ADb value.
OPENSSL_EXPORT int CPACE_process_msg(
    CPACE_CTX *ctx, const uint8_t in_msg[CPACE_MSG_LEN],
    const uint8_t *peer_assoc_data, size_t peer_assoc_data_len,
    uint8_t shared_secret[CPACE_SHARED_SECRET_LEN],
    uint8_t sid_out[SHA512_DIGEST_LENGTH]);


#if defined(__cplusplus)
}  // extern C

extern "C++" {

BSSL_NAMESPACE_BEGIN

BORINGSSL_MAKE_DELETER(CPACE_CTX, CPACE_CTX_free)
BORINGSSL_MAKE_DELETER(SPAKE2_CTX, SPAKE2_CTX_free)

BSSL_NAMESPACE_END

}  // extern C++

#endif

#endif  // OPENSSL_HEADER_CURVE25519_H
