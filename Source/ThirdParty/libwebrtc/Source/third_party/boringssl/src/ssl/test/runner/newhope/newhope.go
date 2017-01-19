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

// package newhope contains a post-quantum key agreement algorithm,
// reimplemented from the reference implementation at
// https://github.com/tpoeppelmann/newhope.
//
// Note that this package does not interoperate with the reference
// implementation.
package newhope

import (
	"crypto/aes"
	"crypto/cipher"
	"errors"
	"io"
)

const (
	// q is the prime that defines the field.
	q = 12289
	// n is the number of coefficients in polynomials.
	n = 1024
	// k is the width of the noise distribution.
	k = 16

	// These values are used in the NTT calculation. See the paper for
	// details about their origins.
	omega        = 49
	invOmega     = 1254
	sqrtOmega    = 7
	invSqrtOmega = 8778
	invN         = 12277

	// encodedPolyLen is the length, in bytes, of an encoded polynomial.  The
	// encoding uses 14 bits per coefficient.
	encodedPolyLen = (n * 14) / 8

	// offerMsgLen is the length, in bytes, of the offering (first) message of
	// the key exchange.
	OfferMsgLen = encodedPolyLen + 32

	// acceptMsgLen is the length, in bytes, of the accepting (second) message
	// of the key exchange.
	AcceptMsgLen = encodedPolyLen + 256
)

// count16Bits returns the number of '1' bits in v.
func count16Bits(v uint16) (sum uint16) {
	for i := 0; i < 16; i++ {
		sum += v & 1
		v >>= 1
	}

	return sum
}

// Poly is a polynomial of n coefficients.
type Poly [n]uint16

// Key is the result of a key agreement.
type Key [32]uint8

// sampleNoise returns a random polynomial where the coefficients are
// drawn from the noise distribution.
func sampleNoise(rand io.Reader) *Poly {
	poly := new(Poly)
	buf := make([]byte, 4)

	for i := range poly {
		if _, err := io.ReadFull(rand, buf); err != nil {
			panic(err)
		}
		a := count16Bits(uint16(buf[0])<<8 | uint16(buf[1]))
		b := count16Bits(uint16(buf[2])<<8 | uint16(buf[3]))
		poly[i] = (q + a - b) % q
	}

	return poly
}

// randomPolynomial returns a random polynomial where the coefficients are
// drawn uniformly at random from the underlying field.
func randomPolynomial(rand io.Reader) *Poly {
	poly := new(Poly)

	buf := make([]byte, 2)
	for i := range poly {
		for {
			if _, err := io.ReadFull(rand, buf); err != nil {
				panic(err)
			}

			v := uint16(buf[1])<<8 | uint16(buf[0])
			v &= 0x3fff

			if v < q {
				poly[i] = v
				break
			}
		}
	}

	return poly
}

type zeroReader struct {
	io.Reader
}

func (z *zeroReader) Read(dst []byte) (n int, err error) {
	for i := range dst {
		dst[i] = 0
	}
	return len(dst), nil
}

// seedToPolynomial uses AES-CTR to generate a pseudo-random polynomial given a
// 32-byte seed.
func seedToPolynomial(seed []byte) *Poly {
	aes, err := aes.NewCipher(seed[0:16])
	if err != nil {
		panic(err)
	}
	stream := cipher.NewCTR(aes, seed[16:32])
	reader := &cipher.StreamReader{S: stream, R: &zeroReader{}}
	return randomPolynomial(reader)
}

// forwardNTT converts |in| into the frequency domain.
func forwardNTT(in *Poly) *Poly {
	return ntt(in, omega, sqrtOmega, 1, 1)
}

// inverseNTT converts |in| into the time domain.
func inverseNTT(in *Poly) *Poly {
	return ntt(in, invOmega, 1, invSqrtOmega, invN)
}

// ntt performs the number-theoretic transform (a discrete Fourier transform in
// a field) on in. Significant magic is in effect here. See the paper for the
// details of how this works.
func ntt(in *Poly, omega, preScaleBase, postScaleBase, postScale uint16) *Poly {
	out := new(Poly)
	omega_to_the_i := uint64(1)

	for i := range out {
		omegaToTheIJ := uint64(1)
		preScale := uint64(1)
		sum := uint64(0)

		for j := range in {
			t := (uint64(in[j]) * preScale) % q
			sum += (t * omegaToTheIJ) % q
			omegaToTheIJ = (omegaToTheIJ * omega_to_the_i) % q
			preScale = (uint64(preScaleBase) * preScale) % q
		}

		out[i] = uint16((sum * uint64(postScale)) % q)

		omega_to_the_i = (omega_to_the_i * uint64(omega)) % q
		postScale = uint16((uint64(postScale) * uint64(postScaleBase)) % q)
	}

	return out
}

// encodeRec encodes the reconciliation data compactly, for use in the accept
// message.
func encodeRec(rec *reconciliationData) []byte {
	var ret [n / 4]byte

	for i := 0; i < n/4; i++ {
		ret[i] = rec[4*i] | rec[4*i+1]<<2 | rec[4*i+2]<<4 | rec[4*i+3]<<6
	}

	return ret[:]
}

// decodeRec decodes reconciliation data from the accept message.
func decodeRec(message []byte) (rec *reconciliationData) {
	rec = new(reconciliationData)

	for i, b := range message {
		rec[4*i] = b & 0x03
		rec[4*i+1] = (b >> 2) & 0x3
		rec[4*i+2] = (b >> 4) & 0x3
		rec[4*i+3] = b >> 6
	}

	return rec
}

// encodePoly returns a byte array that encodes a polynomial compactly, with 14
// bits per coefficient.
func encodePoly(poly *Poly) []byte {
	ret := make([]byte, encodedPolyLen)

	for i := 0; i < n/4; i++ {
		t0 := poly[4*i]
		t1 := poly[4*i+1]
		t2 := poly[4*i+2]
		t3 := poly[4*i+3]

		ret[7*i] = byte(t0)
		ret[7*i+1] = byte(t0>>8) | byte(t1<<6)
		ret[7*i+2] = byte(t1 >> 2)
		ret[7*i+3] = byte(t1>>10) | byte(t2<<4)
		ret[7*i+4] = byte(t2 >> 4)
		ret[7*i+5] = byte(t2>>12) | byte(t3<<2)
		ret[7*i+6] = byte(t3 >> 6)
	}

	return ret
}

// decodePoly inverts encodePoly.
func decodePoly(encoded []byte) *Poly {
	ret := new(Poly)

	for i := 0; i < n/4; i++ {
		ret[4*i] = uint16(encoded[7*i]) | uint16(encoded[7*i+1]&0x3f)<<8
		ret[4*i+1] = uint16(encoded[7*i+1])>>6 | uint16(encoded[7*i+2])<<2 | uint16(encoded[7*i+3]&0x0f)<<10
		ret[4*i+2] = uint16(encoded[7*i+3])>>4 | uint16(encoded[7*i+4])<<4 | uint16(encoded[7*i+5]&0x03)<<12
		ret[4*i+3] = uint16(encoded[7*i+5])>>2 | uint16(encoded[7*i+6])<<6
	}

	return ret
}

// Offer starts a new key exchange. It returns a message that should be
// transmitted to the peer, and a polynomial that must be retained in order to
// complete the exchange.
func Offer(rand io.Reader) (offerMsg []byte, sFreq *Poly) {
	seed := make([]byte, 32)

	if _, err := io.ReadFull(rand, seed); err != nil {
		panic(err)
	}

	aFreq := seedToPolynomial(seed)
	sFreq = forwardNTT(sampleNoise(rand))
	eFreq := forwardNTT(sampleNoise(rand))

	bFreq := new(Poly)
	for i := range bFreq {
		bFreq[i] = uint16((uint64(sFreq[i])*uint64(aFreq[i]) + uint64(eFreq[i])) % q)
	}

	offerMsg = encodePoly(bFreq)
	offerMsg = append(offerMsg, seed[:]...)
	return offerMsg, sFreq
}

// Accept processes a message generated by |Offer| and returns a reply message
// and the shared key.
func Accept(rand io.Reader, offerMsg []byte) (sharedKey Key, acceptMsg []byte, err error) {
	if len(offerMsg) != OfferMsgLen {
		return sharedKey, nil, errors.New("newhope: offer message has incorrect length")
	}

	bFreq := decodePoly(offerMsg)
	seed := offerMsg[encodedPolyLen:]

	aFreq := seedToPolynomial(seed)
	sPrimeFreq := forwardNTT(sampleNoise(rand))
	ePrimeFreq := forwardNTT(sampleNoise(rand))

	uFreq := new(Poly)
	for i := range uFreq {
		uFreq[i] = uint16((uint64(sPrimeFreq[i])*uint64(aFreq[i]) + uint64(ePrimeFreq[i])) % q)
	}

	vFreq := new(Poly)
	for i := range vFreq {
		vFreq[i] = uint16((uint64(sPrimeFreq[i]) * uint64(bFreq[i])) % q)
	}

	v := inverseNTT(vFreq)
	ePrimePrime := sampleNoise(rand)
	for i := range v {
		v[i] = uint16((uint64(v[i]) + uint64(ePrimePrime[i])) % q)
	}

	rec := helprec(rand, v)

	sharedKey = reconcile(v, rec)
	acceptMsg = encodePoly(uFreq)
	acceptMsg = append(acceptMsg, encodeRec(rec)[:]...)
	return sharedKey, acceptMsg, nil
}

// Finish processes the reply from the peer and returns the shared key.
func (sk *Poly) Finish(acceptMsg []byte) (sharedKey Key, err error) {
	if len(acceptMsg) != AcceptMsgLen {
		return sharedKey, errors.New("newhope: accept message has incorrect length")
	}

	uFreq := decodePoly(acceptMsg[:encodedPolyLen])
	rec := decodeRec(acceptMsg[encodedPolyLen:])

	for i, u := range uFreq {
		uFreq[i] = uint16((uint64(u) * uint64(sk[i])) % q)
	}
	u := inverseNTT(uFreq)

	return reconcile(u, rec), nil
}
