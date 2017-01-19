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

import (
	"bytes"
	"crypto/rand"
	"fmt"
	"io"
	"io/ioutil"
	"testing"
)

func TestNTTRoundTrip(t *testing.T) {
	var a Poly
	for i := range a {
		a[i] = uint16(i)
	}

	frequency := forwardNTT(&a)
	original := inverseNTT(frequency)

	for i, v := range a {
		if v != original[i] {
			t.Errorf("NTT didn't invert correctly: original[%d] = %d", i, original[i])
			break
		}
	}
}

func TestNTTInv(t *testing.T) {
	var a Poly
	for i := range a {
		a[i] = uint16(i)
	}

	result := ntt(&a, invOmega, 1, invSqrtOmega, invN)
	if result[0] != 6656 || result[1] != 1792 || result[2] != 1234 {
		t.Errorf("NTT^-1 gave bad result: %v", result[:8])
	}
}

func disabledTestNoise(t *testing.T) {
	var buckets [1 + 2*k]int
	numSamples := 100

	for i := 0; i < numSamples; i++ {
		noise := sampleNoise(rand.Reader)
		for _, v := range noise {
			value := (int(v) + k) % q
			buckets[value]++
		}
	}

	sum := 0
	squareSum := 0

	for i, count := range buckets {
		sum += (i - k) * count
		squareSum += (i - k) * (i - k) * count
	}

	mean := float64(sum) / float64(n*numSamples)
	if mean < -0.5 || 0.5 < mean {
		t.Errorf("mean out of range: %f", mean)
	}

	expectedVariance := 0.5 * 0.5 * float64(k*2) // I think?
	variance := float64(squareSum)/float64(n*numSamples) - mean*mean

	if variance < expectedVariance-1.0 || expectedVariance+1.0 < variance {
		t.Errorf("variance out of range: got %f, want %f", variance, expectedVariance)
	}

	file, err := ioutil.TempFile("", "noise")
	fmt.Printf("writing noise to %s\n", file.Name())
	if err != nil {
		t.Fatal(err)
	}
	for i, count := range buckets {
		dots := ""
		for i := 0; i < count/(3*numSamples); i++ {
			dots += "++"
		}
		fmt.Fprintf(file, "%+d\t%d\t%s\n", i-k, count, dots)
	}
	file.Close()
}

func TestSeedToPolynomial(t *testing.T) {
	seed := make([]byte, 32)
	seed[0] = 1
	seed[31] = 2

	poly := seedToPolynomial(seed)
	if poly[0] != 3313 || poly[1] != 9277 || poly[2] != 11020 {
		t.Errorf("bad result: %v", poly[:3])
	}
}

func TestEncodeDecodePoly(t *testing.T) {
	poly := randomPolynomial(rand.Reader)
	poly2 := decodePoly(encodePoly(poly))
	if *poly != *poly2 {
		t.Errorf("decodePoly(encodePoly) isn't the identity function")
	}
}

func TestEncodeDecodeRec(t *testing.T) {
	var r reconciliationData
	if _, err := io.ReadFull(rand.Reader, r[:]); err != nil {
		panic(err)
	}
	for i := range r {
		r[i] &= 3
	}

	encoded := encodeRec(&r)
	decoded := decodeRec(encoded)

	if *decoded != r {
		t.Errorf("bad decode of rec")
	}
}

func TestExchange(t *testing.T) {
	for count := 0; count < 64; count++ {
		offerMsg, state := Offer(rand.Reader)
		sharedKey1, acceptMsg, err := Accept(rand.Reader, offerMsg)
		if err != nil {
			t.Errorf("Accept: %v", err)
		}
		sharedKey2, err := state.Finish(acceptMsg)
		if err != nil {
			t.Fatal("Finish: %v", err)
		}

		if !bytes.Equal(sharedKey1[:], sharedKey2[:]) {
			t.Fatalf("keys mismatched on iteration %d: %x vs %x", count, sharedKey1, sharedKey2)
		}
	}
}
