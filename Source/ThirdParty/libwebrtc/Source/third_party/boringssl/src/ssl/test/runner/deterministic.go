// Copyright (c) 2016, Google Inc.
//
// Permission to use, copy, modify, and/or distribute this software for any
// purpose with or without fee is hereby granted, provided that the above
// copyright notice and this permission notice appear in all copies.
//
// THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
// WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
// SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
// WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
// OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
// CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

package runner

import (
	"encoding/binary"

	"golang.org/x/crypto/chacha20"
)

// Use a different key from crypto/rand/deterministic.c.
var deterministicRandKey = []byte("runner deterministic key 0123456")

type deterministicRand struct {
	numCalls uint64
}

func (d *deterministicRand) Read(buf []byte) (int, error) {
	for i := range buf {
		buf[i] = 0
	}
	var nonce [12]byte
	binary.LittleEndian.PutUint64(nonce[:8], d.numCalls)
	cipher, err := chacha20.NewUnauthenticatedCipher(deterministicRandKey, nonce[:])
	if err != nil {
		return 0, err
	}
	cipher.XORKeyStream(buf, buf)
	d.numCalls++
	return len(buf), nil
}
