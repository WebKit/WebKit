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

package newhope

// This file contains the reconciliation algorithm for NewHope. This is simply a
// monkey-see-monkey-do version of the reference code, with the exception that
// the key resulting from reconciliation is whitened with SHA2 rather than SHA3.
//
// Thanks to the authors of the reference code for allowing us to release this
// under the BoringSSL license.

import (
	"crypto/sha256"
	"io"
)

func abs(v int32) int32 {
	mask := v >> 31
	return (v ^ mask) - mask
}

func f(x int32) (v0, v1, k int32) {
	// Next 6 lines compute t = x/q;
	b := x * 2730
	t := b >> 25
	b = x - t*12289
	b = 12288 - b
	b >>= 31
	t -= b

	r := t & 1
	xit := (t >> 1)
	v0 = xit + r // v0 = round(x/(2*q))

	t -= 1
	r = t & 1
	v1 = (t >> 1) + r

	k = abs(x - (v0 * 2 * q))
	return
}

// reconciliationData is the data needed for reconciliation. There are 2 bits
// per coefficient; this is the unpacked form.
type reconciliationData [n]uint8

func helprec(rand io.Reader, v *Poly) *reconciliationData {
	var randBits [n / (4 * 8)]byte
	if _, err := io.ReadFull(rand, randBits[:]); err != nil {
		panic(err)
	}

	ret := new(reconciliationData)

	for i := uint(0); i < n/4; i++ {
		rbit := int32((randBits[i>>3] >> (i & 7)) & 1)

		a0, b0, k0 := f(8*int32(v[i]) + 4*rbit)
		a1, b1, k1 := f(8*int32(v[256+i]) + 4*rbit)
		a2, b2, k2 := f(8*int32(v[512+i]) + 4*rbit)
		a3, b3, k3 := f(8*int32(v[768+i]) + 4*rbit)

		k := (2*q - 1 - (k0 + k1 + k2 + k3)) >> 31

		v0 := ((^k) & a0) ^ (k & b0)
		v1 := ((^k) & a1) ^ (k & b1)
		v2 := ((^k) & a2) ^ (k & b2)
		v3 := ((^k) & a3) ^ (k & b3)

		ret[i] = uint8((v0 - v3) & 3)
		ret[i+256] = uint8((v1 - v3) & 3)
		ret[i+512] = uint8((v2 - v3) & 3)
		ret[i+768] = uint8((-k + 2*v3) & 3)
	}

	return ret
}

func g(x int32) int32 {
	// Next 6 lines compute t = x/(4*q);
	b := x * 2730
	t := b >> 27
	b = x - t*49156
	b = 49155 - b
	b >>= 31
	t -= b

	c := t & 1
	t = (t >> 1) + c // t = round(x/(8*q))

	t *= 8 * q

	return abs(t - x)
}

func ldDecode(xi0, xi1, xi2, xi3 int32) uint8 {
	t := g(xi0)
	t += g(xi1)
	t += g(xi2)
	t += g(xi3)

	t -= 8 * q
	t >>= 31
	return uint8(t & 1)
}

func reconcile(v *Poly, reconciliation *reconciliationData) Key {
	key := new(Key)

	for i := uint(0); i < n/4; i++ {
		t0 := 16*q + 8*int32(v[i]) - q*(2*int32(reconciliation[i])+int32(reconciliation[i+768]))
		t1 := 16*q + 8*int32(v[i+256]) - q*(2*int32(reconciliation[256+i])+int32(reconciliation[i+768]))
		t2 := 16*q + 8*int32(v[i+512]) - q*(2*int32(reconciliation[512+i])+int32(reconciliation[i+768]))
		t3 := 16*q + 8*int32(v[i+768]) - q*int32(reconciliation[i+768])

		key[i>>3] |= ldDecode(t0, t1, t2, t3) << (i & 7)
	}

	return sha256.Sum256(key[:])
}
