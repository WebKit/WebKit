// Copyright (c) 2021, Google Inc.
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

package main

import (
	"bytes"
	"crypto/aes"
	"crypto/rand"
	"encoding/hex"
	"testing"
)

func TestCTSRoundTrip(t *testing.T) {
	var buf [aes.BlockSize * 8]byte
	var key, iv [16]byte
	rand.Reader.Read(buf[:])
	rand.Reader.Read(key[:])
	rand.Reader.Read(iv[:])

	for i := aes.BlockSize; i < len(buf); i++ {
		in := buf[:i]
		ciphertext := doCTSEncrypt(key[:], in[:], iv[:])
		if len(ciphertext) != len(in) {
			t.Errorf("incorrect ciphertext length for input length %d", len(in))
			continue
		}
		out := doCTSDecrypt(key[:], ciphertext, iv[:])

		if !bytes.Equal(in[:], out) {
			t.Errorf("did not round trip for length %d", len(in))
		}
	}
}

func TestCTSVectors(t *testing.T) {
	tests := []struct {
		plaintextHex  string
		ciphertextHex string
		ivHex         string
	}{
		// Test vectors from OpenSSL.
		{
			"4920776f756c64206c696b652074686520",
			"c6353568f2bf8cb4d8a580362da7ff7f97",
			"00000000000000000000000000000000",
		},
		{
			"4920776f756c64206c696b65207468652047656e6572616c20476175277320",
			"fc00783e0efdb2c1d445d4c8eff7ed2297687268d6ecccc0c07b25e25ecfe5",
			"00000000000000000000000000000000",
		},
		{
			"4920776f756c64206c696b65207468652047656e6572616c2047617527732043",
			"39312523a78662d5be7fcbcc98ebf5a897687268d6ecccc0c07b25e25ecfe584",
			"00000000000000000000000000000000",
		},
		{
			"4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c",
			"97687268d6ecccc0c07b25e25ecfe584b3fffd940c16a18c1b5549d2f838029e39312523a78662d5be7fcbcc98ebf5",
			"00000000000000000000000000000000",
		},
		{
			"4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c",
			"5432a630742dee7beb70f9f1400ee6a0426da5c54a9990f5ae0b7825f51f0060b557cfb581949a4bdf3bb67dedd472",
			"000102030405060708090a0b0c0d0e0f",
		},
	}

	key := fromHex("636869636b656e207465726979616b69")

	for i, test := range tests {
		plaintext := fromHex(test.plaintextHex)
		iv := fromHex(test.ivHex)
		ciphertext := doCTSEncrypt(key, plaintext, iv)
		if got := hex.EncodeToString(ciphertext); got != test.ciphertextHex {
			t.Errorf("#%d: unexpected ciphertext %s, want %s", i, got, test.ciphertextHex)
		}
		plaintextAgain := doCTSDecrypt(key, ciphertext, iv)
		if !bytes.Equal(plaintext, plaintextAgain) {
			t.Errorf("#%d: did not round trip", i)
		}
	}
}
