/* Copyright (c) 2023, Google LLC
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

#ifndef OPENSSL_HEADER_CRYPTO_SPX_PARAMS_H
#define OPENSSL_HEADER_CRYPTO_SPX_PARAMS_H

#if defined(__cplusplus)
extern "C" {
#endif


// Output length of the hash function.
#define SPX_N 16
// Total height of the tree structure.
#define SPX_FULL_HEIGHT 63
// Number of subtree layers.
#define SPX_D 7
// Height of the trees on each layer
#define SPX_TREE_HEIGHT 9
// Height of each individual FORS tree.
#define SPX_FORS_HEIGHT 12
// Total number of FORS tree used.
#define SPX_FORS_TREES 14
// Size of a FORS signature
#define SPX_FORS_BYTES ((SPX_FORS_HEIGHT + 1) * SPX_FORS_TREES * SPX_N)

// Winternitz parameter and derived values
#define SPX_WOTS_W 16
#define SPX_WOTS_LOG_W 4
#define SPX_WOTS_LEN1 32
#define SPX_WOTS_LEN2 3
#define SPX_WOTS_LEN 35
#define SPX_WOTS_BYTES (SPX_N * SPX_WOTS_LEN)

// XMSS sizes
#define SPX_XMSS_BYTES (SPX_WOTS_BYTES + (SPX_N * SPX_TREE_HEIGHT))

// Size of the message digest (NOTE: This is only correct for the SHA256 params
// here)
#define SPX_DIGEST_SIZE                                                      \
  (((SPX_FORS_TREES * SPX_FORS_HEIGHT) / 8) +                                \
   (((SPX_FULL_HEIGHT - SPX_TREE_HEIGHT) / 8) + 1) + (SPX_TREE_HEIGHT / 8) + \
   1)

// Compressed address size when using SHA256
#define SPX_SHA256_ADDR_BYTES 22

// Size of the FORS message hash
#define SPX_FORS_MSG_BYTES ((SPX_FORS_HEIGHT * SPX_FORS_TREES + 7) / 8)
#define SPX_TREE_BITS (SPX_TREE_HEIGHT * (SPX_D - 1))
#define SPX_TREE_BYTES ((SPX_TREE_BITS + 7) / 8)
#define SPX_LEAF_BITS SPX_TREE_HEIGHT
#define SPX_LEAF_BYTES ((SPX_LEAF_BITS + 7) / 8)


#if defined(__cplusplus)
}  // extern C
#endif

#endif  // OPENSSL_HEADER_CRYPTO_SPX_PARAMS_H
